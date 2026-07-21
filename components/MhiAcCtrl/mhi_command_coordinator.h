#pragma once

#include <cstdint>

#include "mhi_command_confirmation.h"
#include "mhi_tx_builder.h"
#include "mhi_tx_contract.h"

namespace esphome {
namespace mhi_ac_ctrl {

constexpr uint8_t kMhiMaxCommandAttempts = 3U;

struct MhiCommandTimeoutResult {
  uint32_t timed_out_mask{0U};
  uint32_t retry_mask{0U};
  uint32_t exhausted_mask{0U};
  uint32_t superseded_mask{0U};
  uint8_t attempt{0U};

  bool timed_out() const {
    return timed_out_mask != 0U;
  }
};

// Owns command generations and starts semantic confirmation only after the
// transport reports that a command frame was actually clocked onto the bus.
class MhiCommandCoordinator {
 public:
  void reset();

  bool prepare_next(MhiCommandState& command, MhiTxRuntime& runtime, const MhiTxBuildConfig& config,
                    MhiFrameBuffer& frame, MhiTxBuildResult& result, MhiTxEnvelope& envelope);

  void on_stage_result(const MhiTxEnvelope& envelope, const MhiCommandState& command_before_build,
                       MhiCommandState& command, bool staged, uint32_t staged_at_ms = 0U);

  bool on_tx_completion(const MhiTxCompletion& completion, MhiCommandState& command);

  uint32_t observe_status(const MhiStatusState& status);
  uint32_t settle_pending_mask(uint32_t mask);
  uint32_t supersede_pending(const MhiCommandState& patch);

  MhiCommandTimeoutResult expire(uint32_t now_ms, MhiCommandState& command);

  uint32_t staged_timeout_mask(uint32_t now_ms, uint32_t timeout_ms);

  uint32_t pending_age_ms(uint32_t now_ms) const {
    return confirmation_.pending_age_ms(now_ms);
  }

  uint32_t duplicate_pending_mask(const MhiCommandState& command) const {
    return confirmation_.duplicate_pending_mask(command);
  }

  uint32_t pending_mask() const {
    return confirmation_.pending_mask();
  }

  const MhiCommandIntent& pending_intent() const {
    return confirmation_.pending_intent();
  }

  bool has_pending_confirmation() const {
    return confirmation_.has_pending();
  }

  bool has_command_in_flight() const {
    return command_in_flight_;
  }

  uint32_t in_flight_generation() const {
    return in_flight_envelope_.generation;
  }

  uint8_t confirmation_attempt() const {
    return confirmation_attempt_;
  }

 private:
  static void restore_command_mask_(MhiCommandState& destination, const MhiCommandState& source, uint32_t mask);
  static uint32_t restore_intent_mask_(MhiCommandState& destination, const MhiCommandIntent& intent, uint32_t mask);
  void reset_attempts_();

  MhiCommandConfirmation confirmation_{};
  uint32_t next_generation_{1U};
  bool command_in_flight_{false};
  MhiTxEnvelope in_flight_envelope_{};
  MhiCommandState in_flight_command_before_build_{};
  uint8_t next_attempt_{1U};
  uint8_t in_flight_attempt_{0U};
  uint8_t confirmation_attempt_{0U};
  uint32_t in_flight_staged_ms_{0U};
  bool staged_timeout_reported_{false};
};

}  // namespace mhi_ac_ctrl
}  // namespace esphome
