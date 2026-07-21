#include "mhi_worker_decoded_store.h"

namespace esphome {
namespace mhi_ac_ctrl {
namespace {

template <typename T>
uint32_t merge_field(bool& destination_has, T& destination_value, bool source_has, const T& source_value) {
  if (!source_has) {
    return 0U;
  }

  const uint32_t overwritten = destination_has ? 1U : 0U;
  destination_has = true;
  destination_value = source_value;
  return overwritten;
}

}  // namespace

void MhiWorkerDecodedStore::reset() {
  latest_status_ = {};
  latest_extended_status_ = {};
  latest_command_candidate_ = {};
  opdata_ = {};
  unknown_head_ = 0U;
  unknown_size_ = 0U;
  stats_ = {};

  for (auto& entry : unknown_ring_) {
    entry = {};
  }
}

void MhiWorkerDecodedStore::write_status_slot_(MhiDecodedStatusSnapshot& slot, const MhiDecodedStatus& decoded,
                                               const MhiFrameBuffer& frame, uint32_t sequence, uint32_t now_ms,
                                               uint32_t& writes, uint32_t& overwrites) {
  if (slot.valid) {
    overwrites++;
  }

  slot.valid = true;
  slot.sequence = sequence;
  slot.last_update_ms = now_ms;
  slot.decoded = decoded;
  slot.frame = frame;
  writes++;
}

void MhiWorkerDecodedStore::store_status(const MhiDecodedStatus& decoded, const MhiFrameBuffer& frame,
                                         uint32_t sequence, uint32_t now_ms, bool extended, bool command_candidate) {
  if (extended) {
    write_status_slot_(latest_extended_status_, decoded, frame, sequence, now_ms, stats_.extended_status_writes,
                       stats_.extended_status_overwrites);
  } else {
    write_status_slot_(latest_status_, decoded, frame, sequence, now_ms, stats_.status_writes,
                       stats_.status_overwrites);
  }

  if (command_candidate) {
    write_status_slot_(latest_command_candidate_, decoded, frame, sequence, now_ms, stats_.command_candidate_writes,
                       stats_.command_candidate_overwrites);
  }
}

uint32_t MhiWorkerDecodedStore::merge_opdata_fields_(MhiDecodedOpData& destination, const MhiDecodedOpData& source) {
  uint32_t overwritten = 0U;

  overwritten += merge_field(destination.has_mode, destination.mode, source.has_mode, source.mode);
  overwritten += merge_field(destination.has_setpoint, destination.setpoint_c, source.has_setpoint, source.setpoint_c);
  overwritten +=
      merge_field(destination.has_return_air, destination.return_air_c, source.has_return_air, source.return_air_c);
  overwritten += merge_field(destination.has_outdoor_temp, destination.outdoor_temp_c, source.has_outdoor_temp,
                             source.outdoor_temp_c);
  overwritten += merge_field(destination.has_compressor_frequency, destination.compressor_frequency_hz,
                             source.has_compressor_frequency, source.compressor_frequency_hz);
  overwritten += merge_field(destination.has_current, destination.current_a, source.has_current, source.current_a);
  overwritten += merge_field(destination.has_indoor_unit_fan_speed, destination.indoor_unit_fan_speed,
                             source.has_indoor_unit_fan_speed, source.indoor_unit_fan_speed);
  overwritten += merge_field(destination.has_outdoor_unit_fan_speed, destination.outdoor_unit_fan_speed,
                             source.has_outdoor_unit_fan_speed, source.outdoor_unit_fan_speed);
  overwritten += merge_field(destination.has_total_indoor_runtime, destination.total_indoor_runtime_hours,
                             source.has_total_indoor_runtime, source.total_indoor_runtime_hours);
  overwritten += merge_field(destination.has_total_compressor_runtime, destination.total_compressor_runtime_hours,
                             source.has_total_compressor_runtime, source.total_compressor_runtime_hours);
  overwritten += merge_field(destination.has_energy_used, destination.energy_used_kwh, source.has_energy_used,
                             source.energy_used_kwh);
  overwritten += merge_field(destination.has_indoor_unit_thi_r1, destination.indoor_unit_thi_r1_c,
                             source.has_indoor_unit_thi_r1, source.indoor_unit_thi_r1_c);
  overwritten += merge_field(destination.has_indoor_unit_thi_r2, destination.indoor_unit_thi_r2_c,
                             source.has_indoor_unit_thi_r2, source.indoor_unit_thi_r2_c);
  overwritten += merge_field(destination.has_indoor_unit_thi_r3, destination.indoor_unit_thi_r3_c,
                             source.has_indoor_unit_thi_r3, source.indoor_unit_thi_r3_c);
  overwritten += merge_field(destination.has_outdoor_unit_tho_r1, destination.outdoor_unit_tho_r1_c,
                             source.has_outdoor_unit_tho_r1, source.outdoor_unit_tho_r1_c);
  overwritten +=
      merge_field(destination.has_outdoor_unit_expansion_valve, destination.outdoor_unit_expansion_valve_pulses,
                  source.has_outdoor_unit_expansion_valve, source.outdoor_unit_expansion_valve_pulses);
  overwritten += merge_field(destination.has_outdoor_unit_discharge_pipe, destination.outdoor_unit_discharge_pipe_c,
                             source.has_outdoor_unit_discharge_pipe, source.outdoor_unit_discharge_pipe_c);
  overwritten += merge_field(
      destination.has_outdoor_unit_discharge_pipe_super_heat, destination.outdoor_unit_discharge_pipe_super_heat_c,
      source.has_outdoor_unit_discharge_pipe_super_heat, source.outdoor_unit_discharge_pipe_super_heat_c);
  overwritten += merge_field(destination.has_protection_state_number, destination.protection_state_number,
                             source.has_protection_state_number, source.protection_state_number);
  overwritten += merge_field(destination.has_defrost, destination.defrost, source.has_defrost, source.defrost);
  overwritten += merge_field(destination.has_last_error, destination.last_error_code, source.has_last_error,
                             source.last_error_code);

  destination.valid = destination.valid || source.valid;
  return overwritten;
}

void MhiWorkerDecodedStore::merge_opdata(const MhiDecodedOpData& decoded, const MhiFrameBuffer& frame,
                                         uint32_t sequence, uint32_t now_ms) {
  if (!opdata_.valid) {
    opdata_.decoded = {};
  }

  stats_.opdata_field_overwrites += merge_opdata_fields_(opdata_.decoded, decoded);
  opdata_.valid = true;
  opdata_.sequence = sequence;
  opdata_.last_update_ms = now_ms;
  opdata_.last_frame = frame;
  stats_.opdata_merges++;
}

void MhiWorkerDecodedStore::store_unknown(const MhiFrameBuffer& frame, uint32_t sequence, uint32_t now_ms) {
  std::size_t index = 0U;
  if (unknown_size_ < kMhiWorkerUnknownRingCapacity) {
    index = (unknown_head_ + unknown_size_) % kMhiWorkerUnknownRingCapacity;
    unknown_size_++;
  } else {
    index = unknown_head_;
    unknown_head_ = (unknown_head_ + 1U) % kMhiWorkerUnknownRingCapacity;
    stats_.unknown_overwrites++;
  }

  unknown_ring_[index].sequence = sequence;
  unknown_ring_[index].last_update_ms = now_ms;
  unknown_ring_[index].frame = frame;
  stats_.unknown_writes++;
}

bool MhiWorkerDecodedStore::take_command_candidate(MhiDecodedStatusSnapshot& out) {
  if (!latest_command_candidate_.valid) {
    return false;
  }
  out = latest_command_candidate_;
  latest_command_candidate_.valid = false;
  return true;
}

void MhiWorkerDecodedStore::clear_command_candidate() {
  latest_command_candidate_.valid = false;
}

bool MhiWorkerDecodedStore::take_extended_status(MhiDecodedStatusSnapshot& out) {
  if (!latest_extended_status_.valid) {
    return false;
  }
  out = latest_extended_status_;
  latest_extended_status_.valid = false;
  return true;
}

bool MhiWorkerDecodedStore::take_status(MhiDecodedStatusSnapshot& out) {
  if (!latest_status_.valid) {
    return false;
  }
  out = latest_status_;
  latest_status_.valid = false;
  return true;
}

bool MhiWorkerDecodedStore::take_opdata(MhiDecodedOpDataSnapshot& out) {
  if (!opdata_.valid) {
    return false;
  }
  out = opdata_;
  opdata_ = {};
  return true;
}

bool MhiWorkerDecodedStore::take_unknown(MhiWorkerUnknownSnapshot& out) {
  if (unknown_size_ == 0U) {
    return false;
  }

  out = unknown_ring_[unknown_head_];
  unknown_ring_[unknown_head_] = {};
  unknown_head_ = (unknown_head_ + 1U) % kMhiWorkerUnknownRingCapacity;
  unknown_size_--;
  return true;
}

}  // namespace mhi_ac_ctrl
}  // namespace esphome
