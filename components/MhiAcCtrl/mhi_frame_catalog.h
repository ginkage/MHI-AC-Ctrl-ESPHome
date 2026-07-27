#pragma once

#include <cstddef>
#include <cstdint>

#include "mhi_frame.h"
#include "mhi_frame_classifier.h"

namespace esphome {
namespace mhi_ac_ctrl {

constexpr std::size_t kMhiCatalogOpDataSlots = 16U;

struct MhiCatalogStats {
  uint32_t ingested_frames{0};
  uint32_t status_frames{0};
  uint32_t extended_status_frames{0};
  uint32_t opdata_frames{0};
  uint32_t unknown_frames{0};
  uint32_t overwritten_frames{0};
  uint32_t dropped_opdata_slots_full{0};
  uint32_t command_candidate_frames{0};
};

struct MhiCatalogSlot {
  bool valid{false};
  MhiFrameKind kind{MhiFrameKind::UNKNOWN};
  uint16_t opdata_key{kMhiInvalidOpDataKey};
  uint32_t sequence{0};
  uint32_t last_update_ms{0};
  uint32_t writes{0};
  uint32_t overwritten_count{0};
  MhiFrameBuffer frame{};
};

struct MhiCatalogedFrame {
  MhiFrameKind kind{MhiFrameKind::UNKNOWN};
  uint16_t opdata_key{kMhiInvalidOpDataKey};
  uint32_t sequence{0};
  uint32_t last_update_ms{0};
  MhiFrameBuffer frame{};
};

struct MhiCatalogIngestResult {
  MhiFrameKind kind{MhiFrameKind::UNKNOWN};
  uint16_t opdata_key{kMhiInvalidOpDataKey};
  bool stored{false};
  bool overwritten{false};
};

class MhiFrameCatalog {
 public:
  void reset();

  MhiCatalogIngestResult ingest_mosi_frame(const MhiFrameView& frame, uint32_t sequence, uint32_t now_ms,
                                           bool store_command_candidate = false);

  bool take_latest_status(MhiCatalogedFrame& out);
  bool take_latest_extended_status(MhiCatalogedFrame& out);
  bool take_latest_command_candidate(MhiCatalogedFrame& out);
  void clear_command_candidate();
  bool take_next_opdata(MhiCatalogedFrame& out);
  bool take_latest_opdata(uint16_t opdata_key, MhiCatalogedFrame& out);
  bool take_latest_unknown(MhiCatalogedFrame& out);

  const MhiCatalogStats& stats() const {
    return stats_;
  }

  const MhiCatalogSlot& latest_status_slot() const {
    return latest_status_;
  }

  const MhiCatalogSlot& latest_extended_status_slot() const {
    return latest_extended_status_;
  }

 private:
  static bool copy_frame_(const MhiFrameView& frame, MhiFrameBuffer& out);
  static void copy_slot_(const MhiCatalogSlot& slot, MhiCatalogedFrame& out);

  MhiCatalogSlot* find_or_allocate_opdata_slot_(uint16_t opdata_key);
  MhiCatalogIngestResult write_slot_(MhiCatalogSlot& slot, const MhiFrameView& frame, MhiFrameKind kind,
                                     uint16_t opdata_key, uint32_t sequence, uint32_t now_ms,
                                     bool count_overwrite = true);
  void maybe_write_command_candidate_(const MhiFrameView& frame, MhiFrameKind kind, uint16_t opdata_key,
                                      uint32_t sequence, uint32_t now_ms, bool store_command_candidate);

  MhiCatalogStats stats_{};
  MhiCatalogSlot latest_status_{};
  MhiCatalogSlot latest_extended_status_{};
  MhiCatalogSlot latest_command_candidate_{};
  MhiCatalogSlot opdata_slots_[kMhiCatalogOpDataSlots]{};
  MhiCatalogSlot latest_unknown_{};
};

}  // namespace mhi_ac_ctrl
}  // namespace esphome
