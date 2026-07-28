#include "mhi_command_coordinator.h"

namespace esphome {
namespace mhi_ac_ctrl {

void MhiCommandCoordinator::reset() {
  confirmation_.reset();
  next_generation_ = 1U;
  command_in_flight_ = false;
  in_flight_envelope_ = {};
  in_flight_command_before_build_ = {};
  in_flight_staged_ms_ = 0U;
  staged_timeout_reported_ = false;
  this->reset_attempts_();
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
                                            MhiCommandState& command, bool staged, uint32_t staged_at_ms) {
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
  in_flight_attempt_ = next_attempt_;
  in_flight_staged_ms_ = staged_at_ms;
  staged_timeout_reported_ = false;
}

bool MhiCommandCoordinator::on_tx_completion(const MhiTxCompletion& completion, MhiCommandState& command) {
  if (!completion.is_command()) {
    return false;
  }

  if (!command_in_flight_ || completion.generation != in_flight_envelope_.generation) {
    return false;
  }

  if (completion.success) {
    uint32_t confirm_mask = in_flight_envelope_.command_mask;

    // If a newer request for the same field arrived while this frame was in
    // flight, do not let feedback for the old value block the newer command.
    MhiCommandConfirmation in_flight_confirmation{};
    in_flight_confirmation.stage(in_flight_envelope_.intent, confirm_mask, completion.completed_at_ms);
    confirm_mask &= ~in_flight_confirmation.supersede(command);

    confirmation_.stage(in_flight_envelope_.intent, confirm_mask, completion.completed_at_ms);
    confirmation_attempt_ = in_flight_attempt_;
    if (!confirmation_.has_pending()) {
      this->reset_attempts_();
    }
  } else {
    restore_command_mask_(command, in_flight_command_before_build_, in_flight_envelope_.command_mask);
  }

  command_in_flight_ = false;
  in_flight_envelope_ = {};
  in_flight_command_before_build_ = {};
  in_flight_attempt_ = 0U;
  in_flight_staged_ms_ = 0U;
  staged_timeout_reported_ = false;
  return true;
}

uint32_t MhiCommandCoordinator::observe_status(const MhiStatusState& status) {
  const uint32_t confirmed = confirmation_.observe_status(status);
  if (!confirmation_.has_pending()) {
    this->reset_attempts_();
  }
  return confirmed;
}

uint32_t MhiCommandCoordinator::settle_pending_mask(uint32_t mask) {
  const uint32_t settled = confirmation_.settle_pending_mask(mask);
  if (!confirmation_.has_pending()) {
    this->reset_attempts_();
  }
  return settled;
}

uint32_t MhiCommandCoordinator::supersede_pending(const MhiCommandState& patch) {
  const uint32_t superseded = confirmation_.supersede(patch);
  if (!confirmation_.has_pending()) {
    this->reset_attempts_();
  }
  return superseded;
}

MhiCommandTimeoutResult MhiCommandCoordinator::expire(uint32_t now_ms, MhiCommandState& command) {
  MhiCommandTimeoutResult result{};
  const MhiCommandExpiration expiration = confirmation_.expire(now_ms);
  if (!expiration.expired()) {
    return result;
  }

  result.timed_out_mask = expiration.mask;
  result.attempt = confirmation_attempt_ == 0U ? 1U : confirmation_attempt_;
  const uint32_t already_queued = command.pending_command_mask() & expiration.mask;
  result.superseded_mask = already_queued;

  if (result.attempt < kMhiMaxCommandAttempts) {
    result.retry_mask = restore_intent_mask_(command, expiration.intent, expiration.mask & ~already_queued);
    result.superseded_mask |= expiration.mask & ~(result.retry_mask | result.superseded_mask);
    if (result.retry_mask != 0U) {
      next_attempt_ = static_cast<uint8_t>(result.attempt + 1U);
    } else {
      this->reset_attempts_();
    }
  } else {
    result.exhausted_mask = expiration.mask & ~already_queued;
    this->reset_attempts_();
  }

  return result;
}

uint32_t MhiCommandCoordinator::staged_timeout_mask(uint32_t now_ms, uint32_t timeout_ms) {
  if (!command_in_flight_ || staged_timeout_reported_ || in_flight_staged_ms_ == 0U || now_ms < in_flight_staged_ms_ ||
      (now_ms - in_flight_staged_ms_) < timeout_ms) {
    return 0U;
  }

  staged_timeout_reported_ = true;
  return in_flight_envelope_.command_mask;
}

void MhiCommandCoordinator::restore_command_mask_(MhiCommandState& destination, const MhiCommandState& source,
                                                  uint32_t mask) {
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
  if ((mask & MHI_COMMAND_ERROR_OPDATA_REQUEST) != 0U) {
    destination.error_opdata_request = source.error_opdata_request;
  }
}

uint32_t MhiCommandCoordinator::restore_intent_mask_(MhiCommandState& destination, const MhiCommandIntent& intent,
                                                     uint32_t mask) {
  uint32_t restored = 0U;

  if ((mask & MHI_COMMAND_POWER) != 0U && !destination.power_set) {
    destination.power_set = true;
    destination.power = intent.power;
    restored |= MHI_COMMAND_POWER;
  }
  if ((mask & MHI_COMMAND_MODE) != 0U && !destination.mode_set) {
    destination.mode_set = true;
    destination.mode = intent.mode;
    restored |= MHI_COMMAND_MODE;
  }
  if ((mask & MHI_COMMAND_FAN) != 0U && !destination.fan_set) {
    destination.fan_set = true;
    destination.fan = intent.fan;
    restored |= MHI_COMMAND_FAN;
  }
  if ((mask & MHI_COMMAND_TARGET_TEMP) != 0U && !destination.target_temp_set) {
    destination.target_temp_set = true;
    destination.target_temp_c = intent.target_temp_c;
    restored |= MHI_COMMAND_TARGET_TEMP;
  }
  if ((mask & MHI_COMMAND_VERTICAL_VANE) != 0U && !destination.vertical_vane_set) {
    destination.vertical_vane_set = true;
    destination.vertical_vane = intent.vertical_vane;
    restored |= MHI_COMMAND_VERTICAL_VANE;
  }
  if ((mask & MHI_COMMAND_HORIZONTAL_VANE) != 0U && !destination.horizontal_vane_set) {
    destination.horizontal_vane_set = true;
    destination.horizontal_vane = intent.horizontal_vane;
    restored |= MHI_COMMAND_HORIZONTAL_VANE;
  }
  if ((mask & MHI_COMMAND_THREE_D_AUTO) != 0U && !destination.three_d_auto_set) {
    destination.three_d_auto_set = true;
    destination.three_d_auto = intent.three_d_auto;
    restored |= MHI_COMMAND_THREE_D_AUTO;
  }

  return restored;
}

void MhiCommandCoordinator::reset_attempts_() {
  next_attempt_ = 1U;
  confirmation_attempt_ = 0U;
}

}  // namespace mhi_ac_ctrl
}  // namespace esphome
