#include "mhi_frame_catalog.h"

namespace esphome {
namespace mhi_ac_ctrl {

void MhiFrameCatalog::reset() {
  stats_ = {};
  latest_status_ = {};
  latest_extended_status_ = {};
  latest_unknown_ = {};
  latest_command_candidate_ = {};

  for (auto& slot : opdata_slots_) {
    slot = {};
  }
}

MhiCatalogIngestResult MhiFrameCatalog::ingest_mosi_frame(const MhiFrameView& frame, uint32_t sequence, uint32_t now_ms,
                                                          bool store_command_candidate) {
  const MhiFrameClassification classification = classify_mhi_mosi_frame(frame);

  stats_.ingested_frames++;

  switch (classification.kind) {
    case MhiFrameKind::STATUS:
      stats_.status_frames++;
      maybe_write_command_candidate_(frame, classification.kind, classification.opdata_key, sequence, now_ms,
                                     store_command_candidate);
      return write_slot_(latest_status_, frame, classification.kind, classification.opdata_key, sequence, now_ms);

    case MhiFrameKind::EXTENDED_STATUS:
      stats_.extended_status_frames++;
      maybe_write_command_candidate_(frame, classification.kind, classification.opdata_key, sequence, now_ms,
                                     store_command_candidate);
      return write_slot_(latest_extended_status_, frame, classification.kind, classification.opdata_key, sequence,
                         now_ms);

    case MhiFrameKind::OPDATA: {
      stats_.opdata_frames++;
      MhiCatalogSlot* slot = find_or_allocate_opdata_slot_(classification.opdata_key);
      if (slot == nullptr) {
        stats_.dropped_opdata_slots_full++;
        return MhiCatalogIngestResult{classification.kind, classification.opdata_key, false, false};
      }
      return write_slot_(*slot, frame, classification.kind, classification.opdata_key, sequence, now_ms);
    }

    case MhiFrameKind::UNKNOWN:
    default:
      stats_.unknown_frames++;
      return write_slot_(latest_unknown_, frame, classification.kind, classification.opdata_key, sequence, now_ms);
  }
}

bool MhiFrameCatalog::take_latest_status(MhiCatalogedFrame& out) {
  if (!latest_status_.valid) {
    return false;
  }

  copy_slot_(latest_status_, out);
  latest_status_.valid = false;
  return true;
}

bool MhiFrameCatalog::take_latest_extended_status(MhiCatalogedFrame& out) {
  if (!latest_extended_status_.valid) {
    return false;
  }

  copy_slot_(latest_extended_status_, out);
  latest_extended_status_.valid = false;
  return true;
}

bool MhiFrameCatalog::take_latest_command_candidate(MhiCatalogedFrame& out) {
  if (!latest_command_candidate_.valid) {
    return false;
  }

  copy_slot_(latest_command_candidate_, out);
  latest_command_candidate_.valid = false;
  return true;
}

void MhiFrameCatalog::clear_command_candidate() {
  latest_command_candidate_.valid = false;
}

bool MhiFrameCatalog::take_next_opdata(MhiCatalogedFrame& out) {
  MhiCatalogSlot* best = nullptr;

  for (auto& slot : opdata_slots_) {
    if (!slot.valid) {
      continue;
    }

    if (best == nullptr || slot.sequence < best->sequence) {
      best = &slot;
    }
  }

  if (best == nullptr) {
    return false;
  }

  copy_slot_(*best, out);
  best->valid = false;
  return true;
}

bool MhiFrameCatalog::take_latest_opdata(uint16_t opdata_key, MhiCatalogedFrame& out) {
  for (auto& slot : opdata_slots_) {
    if (!slot.valid || slot.opdata_key != opdata_key) {
      continue;
    }

    copy_slot_(slot, out);
    slot.valid = false;
    return true;
  }

  return false;
}

bool MhiFrameCatalog::take_latest_unknown(MhiCatalogedFrame& out) {
  if (!latest_unknown_.valid) {
    return false;
  }

  copy_slot_(latest_unknown_, out);
  latest_unknown_.valid = false;
  return true;
}

bool MhiFrameCatalog::copy_frame_(const MhiFrameView& frame, MhiFrameBuffer& out) {
  if (!frame.valid()) {
    out.clear();
    return false;
  }

  out.clear();
  out.len = frame.len;

  for (std::size_t i = 0; i < frame.len; i++) {
    out.data[i] = frame[i];
  }

  return true;
}

void MhiFrameCatalog::copy_slot_(const MhiCatalogSlot& slot, MhiCatalogedFrame& out) {
  out.kind = slot.kind;
  out.opdata_key = slot.opdata_key;
  out.sequence = slot.sequence;
  out.last_update_ms = slot.last_update_ms;
  out.frame = slot.frame;
}

MhiCatalogSlot* MhiFrameCatalog::find_or_allocate_opdata_slot_(uint16_t opdata_key) {
  MhiCatalogSlot* reusable = nullptr;

  for (auto& slot : opdata_slots_) {
    if (slot.kind == MhiFrameKind::OPDATA && slot.opdata_key == opdata_key) {
      return &slot;
    }

    // Once a decoded value has been consumed the raw slot can be reused for a
    // different key. A valid slot is never displaced, so distinct pending
    // opdata keys remain isolated.
    if (reusable == nullptr && !slot.valid) {
      reusable = &slot;
    }
  }

  return reusable;
}

MhiCatalogIngestResult MhiFrameCatalog::write_slot_(MhiCatalogSlot& slot, const MhiFrameView& frame, MhiFrameKind kind,
                                                    uint16_t opdata_key, uint32_t sequence, uint32_t now_ms,
                                                    bool count_overwrite) {
  const bool overwritten = slot.valid;

  if (!copy_frame_(frame, slot.frame)) {
    return MhiCatalogIngestResult{kind, opdata_key, false, false};
  }

  if (overwritten) {
    slot.overwritten_count++;
    if (count_overwrite) {
      stats_.overwritten_frames++;
    }
  }

  slot.valid = true;
  slot.kind = kind;
  slot.opdata_key = opdata_key;
  slot.sequence = sequence;
  slot.last_update_ms = now_ms;
  slot.writes++;

  return MhiCatalogIngestResult{kind, opdata_key, true, overwritten};
}

void MhiFrameCatalog::maybe_write_command_candidate_(const MhiFrameView& frame, MhiFrameKind kind, uint16_t opdata_key,
                                                     uint32_t sequence, uint32_t now_ms, bool store_command_candidate) {
  if (!store_command_candidate) {
    return;
  }

  if (kind != MhiFrameKind::STATUS && kind != MhiFrameKind::EXTENDED_STATUS) {
    return;
  }

  const auto result = write_slot_(latest_command_candidate_, frame, kind, opdata_key, sequence, now_ms, false);
  if (result.stored) {
    stats_.command_candidate_frames++;
  }
}

}  // namespace mhi_ac_ctrl
}  // namespace esphome
