#include "mhi_command_coordinator.h"

namespace esphome {
namespace mhi_ac_ctrl {

void MhiCommandCoordinator::reset() {
  confirmation_.reset();
  next_generation_ = 1U;
  command_in_flight_ = false;
  in_flight_envelope_ = {};
  in_flight_command_before_build_ = {};
}

bool MhiCommandCoordinator::prepare_next(MhiCommandState& command, MhiTxRuntime& runtime,
                                         const MhiTxBuildConfig& config, MhiFrameBuffer& frame,
                                         MhiTxBuildResult& result, MhiTxEnvelope& envelope) {
  if (command_in_flight_ || confirmation_.has_pending()) {
    return false;
  }

  if (!MhiTxBuilder::build_next_frame(command, runtime, config, frame, result) || frame.len == 0U) {
    return false;
  }

  envelope = {};
  if (!envelope.set_frame(frame.bytes(), frame.len)) {
    return false;
  }

  if (result.has_encoded_commands()) {
    envelope.kind = MhiTxKind::COMMAND;
    envelope.generation = next_generation_++;
    if (next_generation_ == 0U) {
      next_generation_ = 1U;
    }
    envelope.command_mask = result.encoded_command_mask;
    envelope.intent = result.intent;
  }

  return true;
}

void MhiCommandCoordinator::on_stage_result(const MhiTxEnvelope& envelope, const MhiCommandState& command_before_build,
                                            MhiCommandState& command, bool staged) {
  if (!envelope.is_command()) {
    return;
  }

  if (!staged) {
    restore_command_mask_(command, command_before_build, envelope.command_mask);
    return;
  }

  command_in_flight_ = true;
  in_flight_envelope_ = envelope;
  in_flight_command_before_build_ = command_before_build;
}

bool MhiCommandCoordinator::on_tx_completion(const MhiTxCompletion& completion, MhiCommandState& command) {
  if (!completion.is_command()) {
    return false;
  }

  if (!command_in_flight_ || completion.generation != in_flight_envelope_.generation) {
    return false;
  }

  if (completion.success) {
    confirmation_.stage(in_flight_envelope_.intent, in_flight_envelope_.command_mask, completion.completed_at_ms);
  } else {
    restore_command_mask_(command, in_flight_command_before_build_, in_flight_envelope_.command_mask);
  }

  command_in_flight_ = false;
  in_flight_envelope_ = {};
  in_flight_command_before_build_ = {};
  return true;
}

void MhiCommandCoordinator::restore_command_mask_(MhiCommandState& destination, const MhiCommandState& source,
                                                  uint32_t mask) {
  // Preserve any newer request queued while the older envelope was staged or in flight.
  if ((mask & MHI_COMMAND_POWER) != 0U && !destination.power_set) {
    destination.power_set = source.power_set;
    destination.power = source.power;
  }
  if ((mask & MHI_COMMAND_MODE) != 0U && !destination.mode_set) {
    destination.mode_set = source.mode_set;
    destination.mode = source.mode;
  }
  if ((mask & MHI_COMMAND_FAN) != 0U && !destination.fan_set) {
    destination.fan_set = source.fan_set;
    destination.fan = source.fan;
  }
  if ((mask & MHI_COMMAND_TARGET_TEMP) != 0U && !destination.target_temp_set) {
    destination.target_temp_set = source.target_temp_set;
    destination.target_temp_c = source.target_temp_c;
  }
  if ((mask & MHI_COMMAND_VERTICAL_VANE) != 0U && !destination.vertical_vane_set) {
    destination.vertical_vane_set = source.vertical_vane_set;
    destination.vertical_vane = source.vertical_vane;
  }
  if ((mask & MHI_COMMAND_HORIZONTAL_VANE) != 0U && !destination.horizontal_vane_set) {
    destination.horizontal_vane_set = source.horizontal_vane_set;
    destination.horizontal_vane = source.horizontal_vane;
  }
  if ((mask & MHI_COMMAND_THREE_D_AUTO) != 0U && !destination.three_d_auto_set) {
    destination.three_d_auto_set = source.three_d_auto_set;
    destination.three_d_auto = source.three_d_auto;
  }
  if ((mask & MHI_COMMAND_ROOM_TEMP_OVERRIDE) != 0U && !destination.room_temp_override_set) {
    destination.room_temp_override_set = source.room_temp_override_set;
    destination.room_temp_override_raw = source.room_temp_override_raw;
  }
  if ((mask & MHI_COMMAND_ERROR_OPDATA_REQUEST) != 0U && !destination.error_opdata_request) {
    destination.error_opdata_request = source.error_opdata_request;
  }
}

}  // namespace mhi_ac_ctrl
}  // namespace esphome
