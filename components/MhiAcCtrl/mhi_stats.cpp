#include "mhi_stats.h"

#include <algorithm>
#include <cstring>

#include "mhi_defs.h"

namespace esphome {
namespace mhi_ac_ctrl {

namespace {

constexpr uint8_t kNoSignatureOffset = 255U;

uint8_t find_mosi_signature_offset(const uint8_t* data, std::size_t len, std::size_t start_index = 0U) {
  if (data == nullptr || len < 3U || start_index >= len) {
    return kNoSignatureOffset;
  }

  for (std::size_t i = start_index; (i + 2U) < len; i++) {
    const bool sb0_ok = data[i + SB0] == kMhiMosiSignature0Default || data[i + SB0] == kMhiMosiSignature0Alt;
    if (sb0_ok && data[i + SB1] == kMhiMosiSignature1 && data[i + SB2] == kMhiMosiSignature2) {
      return static_cast<uint8_t>(i);
    }
  }

  return kNoSignatureOffset;
}

}  // namespace

void MhiStats::lock_() const {
#ifdef USE_ESP_IDF
  portENTER_CRITICAL(const_cast<portMUX_TYPE*>(&mux_));
#endif
}

void MhiStats::unlock_() const {
#ifdef USE_ESP_IDF
  portEXIT_CRITICAL(const_cast<portMUX_TYPE*>(&mux_));
#endif
}

void MhiStats::reset() {
  this->lock_();
  stats_ = {};
  this->unlock_();
}

void MhiStats::on_rx_bytes(uint32_t count, uint32_t now_ms) {
  this->lock_();
  stats_.rx_bytes += count;
  stats_.last_rx_byte_ms = now_ms;
  this->unlock_();
}

void MhiStats::on_rx_chunk(uint32_t now_ms) {
  this->lock_();
  stats_.rx_chunks++;
  stats_.last_rx_byte_ms = now_ms;
  this->unlock_();
}

void MhiStats::on_candidate_frame() {
  this->lock_();
  stats_.candidate_frames++;
  this->unlock_();
}

void MhiStats::on_valid_frame(uint32_t now_ms) {
  this->lock_();
  stats_.valid_frames++;
  stats_.last_valid_frame_ms = now_ms;
  this->unlock_();
}

void MhiStats::on_invalid_frame() {
  this->lock_();
  stats_.invalid_frames++;
  this->unlock_();
}

void MhiStats::on_checksum_failure() {
  this->lock_();
  stats_.checksum_failures++;
  this->unlock_();
}

void MhiStats::on_signature_miss() {
  this->lock_();
  stats_.signature_misses++;
  this->unlock_();
}

void MhiStats::on_sync_loss() {
  this->lock_();
  stats_.sync_losses++;
  this->unlock_();
}

void MhiStats::on_dropped_bytes(uint32_t count) {
  this->lock_();
  stats_.dropped_bytes += count;
  this->unlock_();
}

void MhiStats::on_checksum_failure_sample(const uint8_t* data, std::size_t len, uint16_t expected_20,
                                          uint16_t actual_20, uint16_t expected_33, uint8_t actual_33) {
  this->lock_();
  stats_.checksum_failure_samples++;
  const std::size_t copy_len = std::min<std::size_t>(len, sizeof(stats_.last_checksum_failure_sample));
  stats_.last_checksum_failure_sample_len = static_cast<uint8_t>(copy_len);
  std::memset(stats_.last_checksum_failure_sample, 0, sizeof(stats_.last_checksum_failure_sample));

  if (data != nullptr && copy_len > 0U) {
    std::memcpy(stats_.last_checksum_failure_sample, data, copy_len);
  }

  stats_.last_checksum_expected_20 = expected_20;
  stats_.last_checksum_actual_20 = actual_20;
  stats_.last_checksum_expected_33 = expected_33;
  stats_.last_checksum_actual_33 = actual_33;
  stats_.last_checksum_signature_offset = find_mosi_signature_offset(data, len, 0U);
  stats_.last_checksum_next_signature_offset = find_mosi_signature_offset(data, len, 1U);
  this->unlock_();
}

void MhiStats::on_signature_miss_sample(const uint8_t* data, std::size_t len) {
  this->lock_();
  stats_.signature_miss_samples++;
  const std::size_t copy_len = std::min<std::size_t>(len, sizeof(stats_.last_signature_miss_sample));
  stats_.last_signature_miss_sample_len = static_cast<uint8_t>(copy_len);
  std::memset(stats_.last_signature_miss_sample, 0, sizeof(stats_.last_signature_miss_sample));

  if (data != nullptr && copy_len > 0U) {
    std::memcpy(stats_.last_signature_miss_sample, data, copy_len);
  }

  stats_.last_signature_miss_signature_offset = find_mosi_signature_offset(data, len, 0U);
  this->unlock_();
}

void MhiStats::on_tx_frame(uint32_t now_ms) {
  this->lock_();
  stats_.tx_frames++;
  stats_.last_tx_frame_ms = now_ms;
  this->unlock_();
}

void MhiStats::on_tx_failure() {
  this->lock_();
  stats_.tx_failures++;
  this->unlock_();
}

void MhiStats::on_tx_command_frame(uint32_t command_mask, uint32_t now_ms) {
  this->lock_();
  stats_.tx_command_frames++;
  stats_.last_tx_command_frame_ms = now_ms;
  stats_.last_tx_command_mask = command_mask;
  this->unlock_();
}

void MhiStats::on_tx_command_failure(uint32_t command_mask, uint32_t now_ms) {
  this->lock_();
  stats_.tx_command_failures++;
  stats_.last_tx_command_frame_ms = now_ms;
  stats_.last_tx_command_mask = command_mask;
  this->unlock_();
}

void MhiStats::on_unsupported_command(uint32_t command_mask, uint32_t now_ms) {
  this->lock_();
  stats_.unsupported_commands++;
  stats_.last_unsupported_command_ms = now_ms;
  stats_.last_unsupported_command_mask = command_mask;
  this->unlock_();
}

void MhiStats::on_command_confirmed(uint32_t command_mask, uint32_t now_ms) {
  this->lock_();
  stats_.command_confirmations++;
  stats_.last_command_confirmation_ms = now_ms;
  stats_.last_confirmed_command_mask = command_mask;
  this->unlock_();
}

void MhiStats::on_command_confirmation_timeout(uint32_t command_mask, uint32_t now_ms) {
  this->lock_();
  stats_.command_confirmation_timeouts++;
  stats_.last_command_confirmation_timeout_ms = now_ms;
  stats_.last_command_confirmation_timeout_mask = command_mask;
  this->unlock_();
}

void MhiStats::on_loop_timing(uint32_t loop_us, uint32_t transport_loop_us, uint32_t tx_stage_us,
                              uint32_t rx_read_sync_us, uint32_t publish_us, uint32_t command_housekeeping_us,
                              uint32_t budget_us, uint32_t now_ms) {
  this->lock_();
  stats_.loop_iterations++;
  stats_.loop_budget_us = budget_us;

  const uint32_t sample_count = stats_.loop_iterations;

  update_timing_sample(loop_us, sample_count, stats_.loop_last_us, stats_.loop_avg_us, stats_.loop_max_us);
  update_timing_sample(transport_loop_us, sample_count, stats_.transport_loop_last_us, stats_.transport_loop_avg_us,
                       stats_.transport_loop_max_us);
  update_timing_sample(tx_stage_us, sample_count, stats_.tx_stage_last_us, stats_.tx_stage_avg_us,
                       stats_.tx_stage_max_us);
  update_timing_sample(rx_read_sync_us, sample_count, stats_.rx_read_sync_last_us, stats_.rx_read_sync_avg_us,
                       stats_.rx_read_sync_max_us);
  update_timing_sample(publish_us, sample_count, stats_.publish_last_us, stats_.publish_avg_us, stats_.publish_max_us);
  update_timing_sample(command_housekeeping_us, sample_count, stats_.command_housekeeping_last_us,
                       stats_.command_housekeeping_avg_us, stats_.command_housekeeping_max_us);

  if (budget_us > 0U && loop_us > budget_us) {
    stats_.loop_over_budget++;
    stats_.last_loop_over_budget_ms = now_ms;
  }
  this->unlock_();
}

void MhiStats::on_frame_sync_reset() {
  this->lock_();
  stats_.frame_sync_resets++;
  this->unlock_();
}

void MhiStats::on_queue_overflow() {
  this->lock_();
  stats_.queue_overflows++;
  this->unlock_();
}

void MhiStats::set_last_error_code(uint8_t error_code) {
  this->lock_();
  stats_.last_error_code = error_code;
  this->unlock_();
}

MhiStatsSnapshot MhiStats::snapshot() const {
  this->lock_();
  const MhiStatsSnapshot snapshot = stats_;
  this->unlock_();
  return snapshot;
}

uint32_t MhiStats::updated_average(uint32_t current_average, uint32_t sample_count, uint32_t sample_us) {
  if (sample_count <= 1U) {
    return sample_us;
  }

  const uint64_t previous_total = static_cast<uint64_t>(current_average) * static_cast<uint64_t>(sample_count - 1U);
  const uint64_t next_total = previous_total + static_cast<uint64_t>(sample_us);

  return static_cast<uint32_t>(next_total / static_cast<uint64_t>(sample_count));
}

void MhiStats::update_timing_sample(uint32_t sample_us, uint32_t sample_count, uint32_t& last_us, uint32_t& avg_us,
                                    uint32_t& max_us) {
  last_us = sample_us;
  avg_us = updated_average(avg_us, sample_count, sample_us);

  if (sample_us > max_us) {
    max_us = sample_us;
  }
}

}  // namespace mhi_ac_ctrl
}  // namespace esphome