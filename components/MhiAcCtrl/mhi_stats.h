#pragma once

#include <cstddef>
#include <cstdint>

#ifdef USE_ESP_IDF
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#endif

namespace esphome {
namespace mhi_ac_ctrl {

struct MhiStatsSnapshot {
  uint32_t rx_bytes{0};
  uint32_t rx_chunks{0};

  uint32_t candidate_frames{0};
  uint32_t valid_frames{0};
  uint32_t invalid_frames{0};

  uint32_t checksum_failures{0};
  uint32_t signature_misses{0};
  uint32_t sync_losses{0};
  uint32_t dropped_bytes{0};

  uint32_t tx_frames{0};
  uint32_t tx_failures{0};

  uint32_t tx_command_frames{0};
  uint32_t tx_command_failures{0};
  uint32_t unsupported_commands{0};

  uint32_t command_confirmations{0};
  uint32_t command_confirmation_timeouts{0};
  uint32_t command_retries{0};
  uint32_t command_retry_exhaustions{0};
  uint32_t command_staged_timeouts{0};

  uint32_t loop_iterations{0};
  uint32_t loop_over_budget{0};
  uint32_t loop_budget_us{0};

  uint32_t loop_last_us{0};
  uint32_t loop_avg_us{0};
  uint32_t loop_max_us{0};

  uint32_t transport_loop_last_us{0};
  uint32_t transport_loop_avg_us{0};
  uint32_t transport_loop_max_us{0};

  uint32_t tx_stage_last_us{0};
  uint32_t tx_stage_avg_us{0};
  uint32_t tx_stage_max_us{0};

  uint32_t rx_read_sync_last_us{0};
  uint32_t rx_read_sync_avg_us{0};
  uint32_t rx_read_sync_max_us{0};

  uint32_t publish_last_us{0};
  uint32_t publish_avg_us{0};
  uint32_t publish_max_us{0};

  uint32_t command_housekeeping_last_us{0};
  uint32_t command_housekeeping_avg_us{0};
  uint32_t command_housekeeping_max_us{0};

  uint32_t frame_sync_resets{0};
  uint32_t queue_overflows{0};

  uint32_t last_valid_frame_ms{0};
  uint32_t last_rx_byte_ms{0};
  uint32_t last_tx_frame_ms{0};
  uint32_t last_tx_command_frame_ms{0};
  uint32_t last_unsupported_command_ms{0};
  uint32_t last_command_confirmation_ms{0};
  uint32_t last_command_confirmation_timeout_ms{0};
  uint32_t last_command_retry_ms{0};
  uint32_t last_command_retry_exhaustion_ms{0};
  uint32_t last_command_staged_timeout_ms{0};
  uint32_t last_loop_over_budget_ms{0};

  uint32_t last_tx_command_mask{0};
  uint32_t last_unsupported_command_mask{0};
  uint32_t last_confirmed_command_mask{0};
  uint32_t last_command_confirmation_timeout_mask{0};
  uint32_t last_command_retry_mask{0};
  uint32_t last_command_retry_exhaustion_mask{0};
  uint32_t last_command_staged_timeout_mask{0};

  uint8_t last_error_code{0};

  uint32_t checksum_failure_samples{0};
  uint32_t signature_miss_samples{0};
  uint8_t last_checksum_failure_sample_len{0};
  uint8_t last_checksum_failure_sample[33]{};
  uint16_t last_checksum_expected_20{0};
  uint16_t last_checksum_actual_20{0};
  uint16_t last_checksum_expected_33{0};
  uint8_t last_checksum_actual_33{0};
  uint8_t last_checksum_signature_offset{255};
  uint8_t last_checksum_next_signature_offset{255};
  uint8_t last_signature_miss_sample_len{0};
  uint8_t last_signature_miss_sample[12]{};
  uint8_t last_signature_miss_signature_offset{255};
};

class MhiStats {
 public:
  void reset();

  void on_rx_bytes(uint32_t count, uint32_t now_ms);
  void on_rx_chunk(uint32_t now_ms);

  void on_candidate_frame();
  void on_valid_frame(uint32_t now_ms);
  void on_invalid_frame();

  void on_checksum_failure();
  void on_signature_miss();
  void on_sync_loss();
  void on_dropped_bytes(uint32_t count);
  void on_checksum_failure_sample(const uint8_t* data, std::size_t len, uint16_t expected_20, uint16_t actual_20,
                                  uint16_t expected_33, uint8_t actual_33);
  void on_signature_miss_sample(const uint8_t* data, std::size_t len);

  void on_tx_frame(uint32_t now_ms);
  void on_tx_failure();

  void on_tx_command_frame(uint32_t command_mask, uint32_t now_ms);
  void on_tx_command_failure(uint32_t command_mask, uint32_t now_ms);
  void on_unsupported_command(uint32_t command_mask, uint32_t now_ms);

  void on_command_confirmed(uint32_t command_mask, uint32_t now_ms);
  void on_command_confirmation_timeout(uint32_t command_mask, uint32_t now_ms);
  void on_command_retry(uint32_t command_mask, uint32_t now_ms);
  void on_command_retry_exhausted(uint32_t command_mask, uint32_t now_ms);
  void on_command_staged_timeout(uint32_t command_mask, uint32_t now_ms);

  void on_loop_timing(uint32_t loop_us, uint32_t transport_loop_us, uint32_t tx_stage_us, uint32_t rx_read_sync_us,
                      uint32_t publish_us, uint32_t command_housekeeping_us, uint32_t budget_us, uint32_t now_ms);

  void on_frame_sync_reset();
  void on_queue_overflow();

  void set_last_error_code(uint8_t error_code);

  MhiStatsSnapshot snapshot() const;

 private:
  void lock_() const;
  void unlock_() const;

  static uint32_t updated_average(uint32_t current_average, uint32_t sample_count, uint32_t sample_us);
  static void update_timing_sample(uint32_t sample_us, uint32_t sample_count, uint32_t& last_us, uint32_t& avg_us,
                                   uint32_t& max_us);

#ifdef USE_ESP_IDF
  mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
#endif
  MhiStatsSnapshot stats_{};
};

}  // namespace mhi_ac_ctrl
}  // namespace esphome