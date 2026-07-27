#include "mhi_ac_ctrl.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mhi_ac_ctrl {

static const char* const TAG = "mhi_ac_ctrl";
static const char* const DIAG_TAG = "mhi.diag";
static constexpr uint32_t kDiagLogIntervalMs = 30000U;
static constexpr uint32_t kLoopBudgetUs = 30000U;
static constexpr std::size_t kMaxRxChunksPerLoop = 4U;
static constexpr uint32_t kNoPendingExtendedFeedbackMask = MHI_COMMAND_HORIZONTAL_VANE | MHI_COMMAND_THREE_D_AUTO;

namespace {

bool in_range(float value, float min_value, float max_value) {
  return value >= min_value && value <= max_value;
}

}  // namespace

void MhiAcCtrl::set_external_room_temperature(float value) {
  if (std::isnan(value)) {
    this->room_temp_api_active_ = false;
    this->clear_external_room_temperature_();
    return;
  }

  this->room_temp_api_active_ = true;
  this->room_temp_api_timeout_start_ms_ = millis();
  this->apply_external_room_temperature_(value);
}

void MhiAcCtrl::apply_external_room_temperature_(float value) {
  if (std::isnan(value)) {
    this->clear_external_room_temperature_();
    return;
  }

  if (value <= -10.0f || value >= 48.0f) {
    ESP_LOGW(DIAG_TAG, "external room temperature ignored: %.2fC is outside the supported range", value);
    return;
  }

  if (!std::isnan(this->last_external_room_temperature_c_) &&
      std::fabs(value - this->last_external_room_temperature_c_) < 0.01f) {
    return;
  }

  auto& command = this->state_.command();
  command.room_temp_override_raw = static_cast<uint8_t>((value * 4.0f) + 61.0f);
  command.room_temp_override_set = true;
  this->last_external_room_temperature_c_ = value;

  ESP_LOGD(DIAG_TAG, "external room temperature staged: %.2fC raw=0x%02x", value,
           static_cast<unsigned int>(command.room_temp_override_raw));
}

void MhiAcCtrl::clear_external_room_temperature_() {
  if (std::isnan(this->last_external_room_temperature_c_) && this->tx_runtime_.room_temp_override_raw == 0xFFU) {
    return;
  }

  auto& command = this->state_.command();
  command.room_temp_override_raw = 0xFFU;
  command.room_temp_override_set = true;
  this->last_external_room_temperature_c_ = NAN;

  ESP_LOGD(DIAG_TAG, "external room temperature cleared; using indoor-unit sensor");
}

void MhiAcCtrl::check_external_room_temperature_timeout_() {
  if (!this->room_temp_api_active_ || this->room_temp_api_timeout_s_ <= 0) {
    return;
  }

  const uint32_t timeout_ms = static_cast<uint32_t>(this->room_temp_api_timeout_s_) * 1000U;
  const uint32_t now = millis();

  if ((now - this->room_temp_api_timeout_start_ms_) < timeout_ms) {
    return;
  }

  this->room_temp_api_active_ = false;
  this->clear_external_room_temperature_();
  ESP_LOGD(DIAG_TAG, "external room temperature timed out after %ds", this->room_temp_api_timeout_s_);
}

bool MhiAcCtrl::request_horizontal_vane_command(uint8_t horizontal_vane) {
  if (horizontal_vane < 1U || horizontal_vane > 8U) {
    ESP_LOGW(DIAG_TAG, "command: unsupported horizontal vane request value=%u",
             static_cast<unsigned int>(horizontal_vane));
    return false;
  }

  auto& command = this->state_.command();
  const uint32_t queued_mask = command.pending_command_mask();
  const uint32_t pending_mask = this->command_confirmation_.pending_mask();
  const auto& pending_intent = this->command_confirmation_.pending_intent();

  if (command.horizontal_vane_set && command.horizontal_vane == horizontal_vane) {
    ESP_LOGD(DIAG_TAG, "command: duplicate queued horizontal vane request ignored value=%u",
             static_cast<unsigned int>(horizontal_vane));
    return false;
  }

  if ((pending_mask & MHI_COMMAND_HORIZONTAL_VANE) != 0U && pending_intent.horizontal_vane == horizontal_vane) {
    ESP_LOGD(DIAG_TAG, "command: duplicate pending horizontal vane request ignored value=%u",
             static_cast<unsigned int>(horizontal_vane));
    return false;
  }

  const bool extended_louver_busy = ((queued_mask | pending_mask) & kNoPendingExtendedFeedbackMask) != 0U;
  if (!extended_louver_busy && this->confirmed_extended_louver_matches_horizontal_(horizontal_vane)) {
    ESP_LOGD(DIAG_TAG, "command: confirmed horizontal vane request ignored value=%u",
             static_cast<unsigned int>(horizontal_vane));
    return false;
  }

  command.horizontal_vane_set = true;
  command.horizontal_vane = horizontal_vane;
  return true;
}

bool MhiAcCtrl::request_three_d_auto_command(bool enabled) {
  auto& command = this->state_.command();
  const uint32_t queued_mask = command.pending_command_mask();
  const uint32_t pending_mask = this->command_confirmation_.pending_mask();
  const auto& pending_intent = this->command_confirmation_.pending_intent();

  if (command.three_d_auto_set && command.three_d_auto == enabled) {
    ESP_LOGD(DIAG_TAG, "command: duplicate queued 3D auto request ignored state=%s", enabled ? "ON" : "OFF");
    return false;
  }

  if ((pending_mask & MHI_COMMAND_THREE_D_AUTO) != 0U && pending_intent.three_d_auto == enabled) {
    ESP_LOGD(DIAG_TAG, "command: duplicate pending 3D auto request ignored state=%s", enabled ? "ON" : "OFF");
    return false;
  }

  const bool extended_louver_busy = ((queued_mask | pending_mask) & kNoPendingExtendedFeedbackMask) != 0U;
  if (!extended_louver_busy && this->confirmed_extended_louver_matches_three_d_auto_(enabled)) {
    ESP_LOGD(DIAG_TAG, "command: confirmed 3D auto request ignored state=%s", enabled ? "ON" : "OFF");
    return false;
  }

  command.three_d_auto_set = true;
  command.three_d_auto = enabled;
  return true;
}

void MhiAcCtrl::refresh_publish_targets_() {
  this->publish_bridge_.set_fan_profile(this->fan_profile_);
  this->publish_bridge_.set_targets(this->publish_targets_);
  this->publish_requested_ = true;
}

void MhiAcCtrl::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MHI AC Ctrl rewrite skeleton");

  this->diagnostics_.stats().reset();
  this->command_confirmation_.reset();
  this->frame_sync_.set_stats(&this->diagnostics_.stats());
  this->last_diag_log_ms_ = 0U;
  this->pending_extended_feedback_candidate_ = false;
  this->pending_extended_feedback_swing_ = false;
  this->pending_extended_feedback_vane_ = 0U;
  this->pending_extended_feedback_3d_auto_ = false;
  this->pending_extended_feedback_db16_ = 0U;
  this->pending_extended_feedback_db17_ = 0U;
  this->pending_extended_feedback_repeat_count_ = 0U;
  this->extended_louver_bootstrap_complete_ = false;
  this->last_protocol_health_valid_frames_ = 0U;
  this->last_protocol_health_invalid_frames_ = 0U;
  this->last_protocol_health_checksum_failures_ = 0U;
  this->last_protocol_health_signature_misses_ = 0U;
  this->last_protocol_health_sync_losses_ = 0U;
  this->last_protocol_health_dropped_bytes_ = 0U;
  this->last_background_tx_ms_ = 0U;
  this->tx_background_interval_deferrals_ = 0U;
  this->tx_background_confirmation_deferrals_ = 0U;
  this->tx_background_attempts_ = 0U;
  this->tx_background_failures_ = 0U;
  this->tx_command_priority_attempts_ = 0U;
  this->room_temp_api_active_ = false;
  this->room_temp_api_timeout_start_ms_ = 0U;
  this->last_external_room_temperature_c_ = NAN;
  this->rx_worker_running_ = false;
  this->rx_worker_stop_requested_ = false;
  this->rx_worker_started_ = false;
  this->rx_worker_task_ = nullptr;
  this->rx_worker_loops_ = 0U;
  this->rx_worker_idle_yields_ = 0U;
  this->rx_worker_ingested_frames_ = 0U;
  this->tx_worker_running_ = false;
  this->tx_worker_stop_requested_ = false;
  this->tx_worker_started_ = false;
  this->tx_worker_task_ = nullptr;
  this->tx_worker_loops_ = 0U;
  this->tx_worker_idle_yields_ = 0U;
  this->tx_worker_flush_attempts_ = 0U;
  this->tx_worker_flush_successes_ = 0U;

  this->tx_config_.frame_size = this->frame_size_ == 33 ? kMhiFrame33Bytes : kMhiFrame20Bytes;

  this->tx_config_.enabled_opdata_mask = this->opdata_mask_;

  this->frame_sync_.reset();
  this->rx_worker_frame_sync_.reset();
  portENTER_CRITICAL(&this->frame_catalog_mux_);
  this->frame_catalog_.reset();
  portEXIT_CRITICAL(&this->frame_catalog_mux_);
  this->frame_catalog_sequence_ = 0U;
  this->frame_sync_.set_mode(MhiFrameSyncMode::MOSI_ONLY);
  this->frame_sync_.set_33_byte_frames_enabled(this->frame_size_ == 33);
  this->rx_worker_frame_sync_.set_mode(MhiFrameSyncMode::MOSI_ONLY);
  this->rx_worker_frame_sync_.set_33_byte_frames_enabled(this->frame_size_ == 33);

  this->transport_.set_diagnostics(&this->diagnostics_);
  this->transport_.set_rmt_spi_frame_gap_us(this->rmt_spi_frame_gap_us_);

  this->transport_.configure(this->pins_.sck, this->pins_.mosi, this->pins_.miso, this->rx_driver_, this->tx_driver_,
                             static_cast<uint8_t>(this->frame_size_), this->frame_start_idle_ms_);

  this->rx_byte_critical_sections_enabled_ = true;
  this->transport_.set_rx_byte_critical_sections(this->rx_byte_critical_sections_enabled_);
  this->transport_.set_auto_tx_flush(!this->tx_worker_enabled_);

  this->transport_.setup();

  if (this->external_room_temperature_sensor_ != nullptr) {
    this->external_room_temperature_sensor_->add_on_state_callback([this](float state) {
      this->room_temp_api_active_ = false;
      this->apply_external_room_temperature_(state);
    });
    this->apply_external_room_temperature_(this->external_room_temperature_sensor_->state);
  }

  this->start_rx_worker_();
  this->start_tx_worker_();

  ESP_LOGCONFIG(TAG, "RX mode: %s",
                this->rx_worker_enabled_ ? "RX worker catalog writer" : "synchronous main-loop sampling");
  ESP_LOGCONFIG(TAG, "TX mode: %s", this->tx_worker_enabled_ ? "worker bus-marker flusher" : "main-loop auto flush");
}

void MhiAcCtrl::loop() {
  const uint32_t loop_start_us = micros();
  bool state_changed = false;
  uint32_t section_start_us = loop_start_us;
  this->transport_.loop();
  const uint32_t transport_loop_us = elapsed_us_(section_start_us);

  uint32_t tx_stage_us = 0U;
  uint32_t rx_read_sync_us = 0U;

  section_start_us = micros();
  this->check_external_room_temperature_timeout_();
  this->build_and_stage_tx_frame_();
  tx_stage_us = elapsed_us_(section_start_us);

  section_start_us = micros();
  state_changed = this->read_and_sync_rx_frame_();
  rx_read_sync_us = elapsed_us_(section_start_us);

  section_start_us = micros();
  if (state_changed || this->publish_requested_) {
    this->publish_bridge_.publish(this->state_);
    this->publish_requested_ = false;
  }
  const uint32_t publish_us = elapsed_us_(section_start_us);

  section_start_us = micros();
  this->check_command_confirmation_timeout_();
  const uint32_t command_housekeeping_us = elapsed_us_(section_start_us);

  const uint32_t loop_us = elapsed_us_(loop_start_us);
  this->diagnostics_.stats().on_loop_timing(loop_us, transport_loop_us, tx_stage_us, rx_read_sync_us, publish_us,
                                            command_housekeeping_us, kLoopBudgetUs, millis());

  this->log_runtime_diagnostics_();
}

void MhiAcCtrl::dump_config() {
  const auto diag = this->diagnostics_.snapshot(millis());

  ESP_LOGCONFIG(TAG, "MHI AC Ctrl rewrite skeleton:");
  ESP_LOGCONFIG(TAG, "  Frame size: %d", this->frame_size_);
  ESP_LOGCONFIG(TAG, "  Room temp timeout: %ds", this->room_temp_api_timeout_s_);
  ESP_LOGCONFIG(TAG, "  External room temperature sensor: %s",
                this->external_room_temperature_sensor_ != nullptr ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Pins: SCK=%d MOSI=%d MISO=%d", this->pins_.sck, this->pins_.mosi, this->pins_.miso);
  ESP_LOGCONFIG(TAG, "  RX driver configured: %s", this->rx_driver_.c_str());
  ESP_LOGCONFIG(TAG, "  TX driver configured: %s", this->tx_driver_.c_str());
  ESP_LOGCONFIG(TAG, "  Fan profile: %s", mhi_fan_profile_name(this->fan_profile_));
  ESP_LOGCONFIG(TAG, "  RX driver active: %s", diag.rx_driver_name);
  ESP_LOGCONFIG(TAG, "  TX driver active: %s", diag.tx_driver_name);
  ESP_LOGCONFIG(TAG, "  RX ready: %s", diag.rx_driver_ready ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  TX ready: %s", diag.tx_driver_ready ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  RX mode: %s", this->rx_worker_enabled_ ? "worker catalog writer" : "main-loop sampling");
  ESP_LOGCONFIG(TAG, "  RX worker: enabled=%s running=%s core=%d priority=%lu stack=%lu start_delay=%lums",
                this->rx_worker_enabled_ ? "YES" : "NO", this->rx_worker_running_ ? "YES" : "NO",
                this->rx_worker_core_id_, static_cast<unsigned long>(this->rx_worker_priority_),
                static_cast<unsigned long>(this->rx_worker_stack_size_),
                static_cast<unsigned long>(this->rx_worker_start_delay_ms_));
  ESP_LOGCONFIG(TAG, "  Frame start idle: %lums", static_cast<unsigned long>(this->frame_start_idle_ms_));
  ESP_LOGCONFIG(TAG, "  RMT/SPI frame gap: %luus", static_cast<unsigned long>(this->rmt_spi_frame_gap_us_));
  ESP_LOGCONFIG(TAG, "  TX background interval: %lums", static_cast<unsigned long>(this->tx_background_interval_ms_));
  ESP_LOGCONFIG(TAG, "  TX priority: commands bypass interval, background waits for no pending confirmation");
  ESP_LOGCONFIG(TAG, "  TX ownership: queue/flush split, auto_flush=%s",
                this->transport_.auto_tx_flush() ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  TX worker: enabled=%s running=%s core=%d priority=%lu stack=%lu start_delay=%lums",
                this->tx_worker_enabled_ ? "YES" : "NO", this->tx_worker_running_ ? "YES" : "NO",
                this->tx_worker_core_id_, static_cast<unsigned long>(this->tx_worker_priority_),
                static_cast<unsigned long>(this->tx_worker_stack_size_),
                static_cast<unsigned long>(this->tx_worker_start_delay_ms_));
  ESP_LOGCONFIG(TAG, "  RX byte critical sections: %s", this->rx_byte_critical_sections_enabled_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Opdata request mask: 0x%08lx", static_cast<unsigned long>(this->opdata_mask_));
  ESP_LOGCONFIG(TAG, "  Frame catalog: enabled latest-slot decode");
}

void MhiAcCtrl::log_runtime_diagnostics_() {
  const uint32_t now = millis();

  if (now < kDiagLogIntervalMs) {
    return;
  }

  if (this->last_diag_log_ms_ != 0U && (now - this->last_diag_log_ms_) < kDiagLogIntervalMs) {
    return;
  }

  this->last_diag_log_ms_ = now;

  const auto diag = this->diagnostics_.snapshot(now);
  const auto& stats = diag.stats;

  ESP_LOGI(DIAG_TAG,
           "runtime: rx_bytes=%lu rx_chunks=%lu candidate_frames=%lu valid_frames=%lu invalid_frames=%lu "
           "checksum_failures=%lu",
           static_cast<unsigned long>(stats.rx_bytes), static_cast<unsigned long>(stats.rx_chunks),
           static_cast<unsigned long>(stats.candidate_frames), static_cast<unsigned long>(stats.valid_frames),
           static_cast<unsigned long>(stats.invalid_frames), static_cast<unsigned long>(stats.checksum_failures));

  ESP_LOGI(DIAG_TAG,
           "runtime: signature_misses=%lu sync_losses=%lu dropped_bytes=%lu tx_frames=%lu tx_failures=%lu "
           "last_valid_frame_age_ms=%lu last_rx_byte_age_ms=%lu last_tx_frame_age_ms=%lu",
           static_cast<unsigned long>(stats.signature_misses), static_cast<unsigned long>(stats.sync_losses),
           static_cast<unsigned long>(stats.dropped_bytes), static_cast<unsigned long>(stats.tx_frames),
           static_cast<unsigned long>(stats.tx_failures), static_cast<unsigned long>(diag.last_valid_frame_age_ms),
           static_cast<unsigned long>(diag.last_rx_byte_age_ms), static_cast<unsigned long>(diag.last_tx_frame_age_ms));

  const uint32_t delta_valid_frames = stats.valid_frames - this->last_protocol_health_valid_frames_;
  const uint32_t delta_invalid_frames = stats.invalid_frames - this->last_protocol_health_invalid_frames_;
  const uint32_t delta_checksum_failures = stats.checksum_failures - this->last_protocol_health_checksum_failures_;
  const uint32_t delta_signature_misses = stats.signature_misses - this->last_protocol_health_signature_misses_;
  const uint32_t delta_sync_losses = stats.sync_losses - this->last_protocol_health_sync_losses_;
  const uint32_t delta_dropped_bytes = stats.dropped_bytes - this->last_protocol_health_dropped_bytes_;
  const bool protocol_healthy = delta_invalid_frames == 0U && delta_checksum_failures == 0U &&
                                delta_signature_misses == 0U && delta_sync_losses == 0U && delta_dropped_bytes == 0U;

  if (protocol_healthy) {
    ESP_LOGI(DIAG_TAG,
             "runtime: rx_protocol_health healthy=YES delta_valid=%lu delta_invalid=%lu delta_checksum_failures=%lu "
             "delta_signature_misses=%lu delta_sync_losses=%lu delta_dropped_bytes=%lu",
             static_cast<unsigned long>(delta_valid_frames), static_cast<unsigned long>(delta_invalid_frames),
             static_cast<unsigned long>(delta_checksum_failures), static_cast<unsigned long>(delta_signature_misses),
             static_cast<unsigned long>(delta_sync_losses), static_cast<unsigned long>(delta_dropped_bytes));
  } else {
    ESP_LOGW(DIAG_TAG,
             "runtime: rx_protocol_health healthy=NO delta_valid=%lu delta_invalid=%lu delta_checksum_failures=%lu "
             "delta_signature_misses=%lu delta_sync_losses=%lu delta_dropped_bytes=%lu",
             static_cast<unsigned long>(delta_valid_frames), static_cast<unsigned long>(delta_invalid_frames),
             static_cast<unsigned long>(delta_checksum_failures), static_cast<unsigned long>(delta_signature_misses),
             static_cast<unsigned long>(delta_sync_losses), static_cast<unsigned long>(delta_dropped_bytes));
  }

  if (delta_checksum_failures > 0U && stats.last_checksum_failure_sample_len > 0U) {
    ESP_LOGW(DIAG_TAG,
             "runtime: checksum_sample samples=%lu len=%u b=%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x "
             "%02x %02x expected20=0x%04x actual20=0x%04x expected33=0x%04x actual33=0x%02x",
             static_cast<unsigned long>(stats.checksum_failure_samples),
             static_cast<unsigned int>(stats.last_checksum_failure_sample_len),
             static_cast<unsigned int>(stats.last_checksum_failure_sample[0]),
             static_cast<unsigned int>(stats.last_checksum_failure_sample[1]),
             static_cast<unsigned int>(stats.last_checksum_failure_sample[2]),
             static_cast<unsigned int>(stats.last_checksum_failure_sample[3]),
             static_cast<unsigned int>(stats.last_checksum_failure_sample[4]),
             static_cast<unsigned int>(stats.last_checksum_failure_sample[5]),
             static_cast<unsigned int>(stats.last_checksum_failure_sample[6]),
             static_cast<unsigned int>(stats.last_checksum_failure_sample[7]),
             static_cast<unsigned int>(stats.last_checksum_failure_sample[8]),
             static_cast<unsigned int>(stats.last_checksum_failure_sample[9]),
             static_cast<unsigned int>(stats.last_checksum_failure_sample[10]),
             static_cast<unsigned int>(stats.last_checksum_failure_sample[11]),
             static_cast<unsigned int>(stats.last_checksum_expected_20),
             static_cast<unsigned int>(stats.last_checksum_actual_20),
             static_cast<unsigned int>(stats.last_checksum_expected_33),
             static_cast<unsigned int>(stats.last_checksum_actual_33));

    ESP_LOGW(DIAG_TAG,
             "runtime: checksum_detail sig0=%u next_sig=%u cbh=0x%02x cbl=0x%02x db15=0x%02x db16=0x%02x "
             "db17=0x%02x cbl2=0x%02x",
             static_cast<unsigned int>(stats.last_checksum_signature_offset),
             static_cast<unsigned int>(stats.last_checksum_next_signature_offset),
             static_cast<unsigned int>(stats.last_checksum_failure_sample[CBH]),
             static_cast<unsigned int>(stats.last_checksum_failure_sample[CBL]),
             static_cast<unsigned int>(stats.last_checksum_failure_sample[DB15]),
             static_cast<unsigned int>(stats.last_checksum_failure_sample[DB16]),
             static_cast<unsigned int>(stats.last_checksum_failure_sample[DB17]),
             static_cast<unsigned int>(stats.last_checksum_failure_sample[CBL2]));
  }

  if (delta_signature_misses > 0U && stats.last_signature_miss_sample_len > 0U) {
    ESP_LOGW(DIAG_TAG,
             "runtime: signature_sample samples=%lu len=%u b=%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x "
             "%02x %02x",
             static_cast<unsigned long>(stats.signature_miss_samples),
             static_cast<unsigned int>(stats.last_signature_miss_sample_len),
             static_cast<unsigned int>(stats.last_signature_miss_sample[0]),
             static_cast<unsigned int>(stats.last_signature_miss_sample[1]),
             static_cast<unsigned int>(stats.last_signature_miss_sample[2]),
             static_cast<unsigned int>(stats.last_signature_miss_sample[3]),
             static_cast<unsigned int>(stats.last_signature_miss_sample[4]),
             static_cast<unsigned int>(stats.last_signature_miss_sample[5]),
             static_cast<unsigned int>(stats.last_signature_miss_sample[6]),
             static_cast<unsigned int>(stats.last_signature_miss_sample[7]),
             static_cast<unsigned int>(stats.last_signature_miss_sample[8]),
             static_cast<unsigned int>(stats.last_signature_miss_sample[9]),
             static_cast<unsigned int>(stats.last_signature_miss_sample[10]),
             static_cast<unsigned int>(stats.last_signature_miss_sample[11]));
    ESP_LOGW(DIAG_TAG, "runtime: signature_detail sig_offset=%u",
             static_cast<unsigned int>(stats.last_signature_miss_signature_offset));
  }

  this->last_protocol_health_valid_frames_ = stats.valid_frames;
  this->last_protocol_health_invalid_frames_ = stats.invalid_frames;
  this->last_protocol_health_checksum_failures_ = stats.checksum_failures;
  this->last_protocol_health_signature_misses_ = stats.signature_misses;
  this->last_protocol_health_sync_losses_ = stats.sync_losses;
  this->last_protocol_health_dropped_bytes_ = stats.dropped_bytes;

  ESP_LOGI(DIAG_TAG,
           "runtime: tx_command_frames=%lu tx_command_failures=%lu unsupported_commands=%lu "
           "last_tx_command_mask=0x%08lx last_unsupported_command_mask=0x%08lx "
           "last_tx_command_age_ms=%lu last_unsupported_command_age_ms=%lu",
           static_cast<unsigned long>(stats.tx_command_frames), static_cast<unsigned long>(stats.tx_command_failures),
           static_cast<unsigned long>(stats.unsupported_commands),
           static_cast<unsigned long>(stats.last_tx_command_mask),
           static_cast<unsigned long>(stats.last_unsupported_command_mask),
           static_cast<unsigned long>(diag.last_tx_command_frame_age_ms),
           static_cast<unsigned long>(diag.last_unsupported_command_age_ms));

  ESP_LOGI(DIAG_TAG,
           "runtime: command_confirmations=%lu command_confirmation_timeouts=%lu pending_confirmation_mask=0x%08lx "
           "last_confirmed_mask=0x%08lx last_timeout_mask=0x%08lx "
           "last_confirmed_age_ms=%lu last_timeout_age_ms=%lu",
           static_cast<unsigned long>(stats.command_confirmations),
           static_cast<unsigned long>(stats.command_confirmation_timeouts),
           static_cast<unsigned long>(this->command_confirmation_.pending_mask()),
           static_cast<unsigned long>(stats.last_confirmed_command_mask),
           static_cast<unsigned long>(stats.last_command_confirmation_timeout_mask),
           static_cast<unsigned long>(diag.last_command_confirmation_age_ms),
           static_cast<unsigned long>(diag.last_command_confirmation_timeout_age_ms));

  const MhiCatalogStats catalog_stats = this->catalog_stats_snapshot_();
  ESP_LOGI(DIAG_TAG,
           "runtime: catalog ingested=%lu status=%lu extended=%lu opdata=%lu unknown=%lu overwritten=%lu "
           "opdata_slots_full=%lu command_candidates=%lu",
           static_cast<unsigned long>(catalog_stats.ingested_frames),
           static_cast<unsigned long>(catalog_stats.status_frames),
           static_cast<unsigned long>(catalog_stats.extended_status_frames),
           static_cast<unsigned long>(catalog_stats.opdata_frames),
           static_cast<unsigned long>(catalog_stats.unknown_frames),
           static_cast<unsigned long>(catalog_stats.overwritten_frames),
           static_cast<unsigned long>(catalog_stats.dropped_opdata_slots_full),
           static_cast<unsigned long>(catalog_stats.command_candidate_frames));

  ESP_LOGI(DIAG_TAG,
           "runtime: tx_priority command_attempts=%lu background_attempts=%lu background_failures=%lu "
           "interval_deferrals=%lu confirmation_deferrals=%lu",
           static_cast<unsigned long>(this->tx_command_priority_attempts_),
           static_cast<unsigned long>(this->tx_background_attempts_),
           static_cast<unsigned long>(this->tx_background_failures_),
           static_cast<unsigned long>(this->tx_background_interval_deferrals_),
           static_cast<unsigned long>(this->tx_background_confirmation_deferrals_));

  ESP_LOGI(DIAG_TAG, "runtime: rx_worker enabled=%s running=%s loops=%lu ingested=%lu idle_yields=%lu",
           this->rx_worker_enabled_ ? "YES" : "NO", this->rx_worker_running_ ? "YES" : "NO",
           static_cast<unsigned long>(this->rx_worker_loops_),
           static_cast<unsigned long>(this->rx_worker_ingested_frames_),
           static_cast<unsigned long>(this->rx_worker_idle_yields_));

  ESP_LOGI(DIAG_TAG,
           "runtime: tx_worker enabled=%s running=%s loops=%lu flush_attempts=%lu flush_successes=%lu idle_yields=%lu",
           this->tx_worker_enabled_ ? "YES" : "NO", this->tx_worker_running_ ? "YES" : "NO",
           static_cast<unsigned long>(this->tx_worker_loops_),
           static_cast<unsigned long>(this->tx_worker_flush_attempts_),
           static_cast<unsigned long>(this->tx_worker_flush_successes_),
           static_cast<unsigned long>(this->tx_worker_idle_yields_));

  ESP_LOGI(DIAG_TAG, "runtime: loop_us last=%lu avg=%lu max=%lu over_budget=%lu budget=%lu last_over_budget_age_ms=%lu",
           static_cast<unsigned long>(stats.loop_last_us), static_cast<unsigned long>(stats.loop_avg_us),
           static_cast<unsigned long>(stats.loop_max_us), static_cast<unsigned long>(stats.loop_over_budget),
           static_cast<unsigned long>(stats.loop_budget_us),
           static_cast<unsigned long>(diag.last_loop_over_budget_age_ms));

  ESP_LOGI(
      DIAG_TAG,
      "runtime: section_us transport=%lu/%lu/%lu tx=%lu/%lu/%lu rx=%lu/%lu/%lu "
      "publish=%lu/%lu/%lu command=%lu/%lu/%lu",
      static_cast<unsigned long>(stats.transport_loop_last_us), static_cast<unsigned long>(stats.transport_loop_avg_us),
      static_cast<unsigned long>(stats.transport_loop_max_us), static_cast<unsigned long>(stats.tx_stage_last_us),
      static_cast<unsigned long>(stats.tx_stage_avg_us), static_cast<unsigned long>(stats.tx_stage_max_us),
      static_cast<unsigned long>(stats.rx_read_sync_last_us), static_cast<unsigned long>(stats.rx_read_sync_avg_us),
      static_cast<unsigned long>(stats.rx_read_sync_max_us), static_cast<unsigned long>(stats.publish_last_us),
      static_cast<unsigned long>(stats.publish_avg_us), static_cast<unsigned long>(stats.publish_max_us),
      static_cast<unsigned long>(stats.command_housekeeping_last_us),
      static_cast<unsigned long>(stats.command_housekeeping_avg_us),
      static_cast<unsigned long>(stats.command_housekeeping_max_us));
}

uint32_t MhiAcCtrl::elapsed_us_(uint32_t start_us) {
  return static_cast<uint32_t>(micros() - start_us);
}

bool MhiAcCtrl::confirmed_extended_louver_matches_horizontal_(uint8_t horizontal_vane) const {
  const auto& status = this->state_.status();

  if (!this->extended_louver_bootstrap_complete_ || !status.has_extended_louver_raw || !status.has_horizontal_vane) {
    return false;
  }

  if (horizontal_vane == 8U) {
    return status.horizontal_vane_swing;
  }

  if (horizontal_vane >= 1U && horizontal_vane <= 7U) {
    return !status.horizontal_vane_swing && status.horizontal_vane == horizontal_vane;
  }

  return false;
}

bool MhiAcCtrl::confirmed_extended_louver_matches_three_d_auto_(bool enabled) const {
  const auto& status = this->state_.status();

  return this->extended_louver_bootstrap_complete_ && status.has_extended_louver_raw && status.has_3d_auto &&
         status.three_d_auto == enabled;
}

void MhiAcCtrl::refresh_extended_louver_tx_context_() {
  const auto& status = this->state_.status();

  this->tx_config_.has_extended_louver_state =
      status.has_horizontal_vane && status.has_3d_auto && status.has_extended_louver_raw;

  if (!this->tx_config_.has_extended_louver_state) {
    return;
  }

  this->tx_config_.extended_louver_db16 = status.extended_louver_db16;
  this->tx_config_.extended_louver_db17 = status.extended_louver_db17;
  this->tx_config_.extended_louver_horizontal_swing = status.horizontal_vane_swing;
  this->tx_config_.extended_louver_horizontal_vane = status.horizontal_vane;
  this->tx_config_.extended_louver_three_d_auto = status.three_d_auto;
}

void MhiAcCtrl::start_rx_worker_() {
  if (!this->rx_worker_enabled_) {
    return;
  }

  if (this->rx_worker_started_) {
    return;
  }

  this->rx_worker_stop_requested_ = false;
  this->rx_worker_running_ = false;

  TaskHandle_t task_handle = nullptr;
  const BaseType_t priority = static_cast<BaseType_t>(this->rx_worker_priority_);
  const uint32_t stack_size = this->rx_worker_stack_size_;

  BaseType_t created = pdFALSE;
  if (this->rx_worker_core_id_ >= 0) {
    created = xTaskCreatePinnedToCore(&MhiAcCtrl::rx_worker_task_entry_, "mhi_rx_worker", stack_size, this, priority,
                                      &task_handle, static_cast<BaseType_t>(this->rx_worker_core_id_));
  } else {
    created = xTaskCreate(&MhiAcCtrl::rx_worker_task_entry_, "mhi_rx_worker", stack_size, this, priority, &task_handle);
  }

  if (created != pdPASS || task_handle == nullptr) {
    ESP_LOGE(TAG, "RX worker start failed: stack=%lu priority=%lu core=%d", static_cast<unsigned long>(stack_size),
             static_cast<unsigned long>(this->rx_worker_priority_), this->rx_worker_core_id_);
    this->rx_worker_enabled_ = false;
    this->rx_worker_task_ = nullptr;
    return;
  }

  this->rx_worker_task_ = task_handle;
  this->rx_worker_started_ = true;
  ESP_LOGCONFIG(TAG, "RX worker started: stack=%lu priority=%lu core=%d start_delay=%lums",
                static_cast<unsigned long>(stack_size), static_cast<unsigned long>(this->rx_worker_priority_),
                this->rx_worker_core_id_, static_cast<unsigned long>(this->rx_worker_start_delay_ms_));
}

void MhiAcCtrl::stop_rx_worker_() {
  if (!this->rx_worker_started_) {
    return;
  }

  this->rx_worker_stop_requested_ = true;
}

void MhiAcCtrl::rx_worker_task_entry_(void* arg) {
  auto* self = static_cast<MhiAcCtrl*>(arg);
  if (self == nullptr) {
    vTaskDelete(nullptr);
    return;
  }

  self->rx_worker_task_loop_();
  vTaskDelete(nullptr);
}

void MhiAcCtrl::rx_worker_task_loop_() {
  this->rx_worker_running_ = true;

  if (this->rx_worker_start_delay_ms_ > 0U) {
    vTaskDelay(pdMS_TO_TICKS(this->rx_worker_start_delay_ms_));
  }

  uint8_t buffer[kMhiMaxFrameBytes]{};
  MhiFrameBuffer frame{};

  while (!this->rx_worker_stop_requested_) {
    this->rx_worker_loops_++;

    const std::size_t len = this->transport_.read_rx_for_worker(buffer, sizeof(buffer));
    if (len == 0U) {
      this->rx_worker_idle_yields_++;
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }

    this->rx_worker_frame_sync_.push_bytes(buffer, len);

    bool ingested_any = false;
    while (this->rx_worker_frame_sync_.pop_frame(frame)) {
      this->diagnostics_.stats().on_valid_frame(millis());
      if (this->ingest_rx_frame_(frame)) {
        this->rx_worker_ingested_frames_++;
        ingested_any = true;
      }
    }

    if (ingested_any) {
      taskYIELD();
    }
  }

  this->rx_worker_running_ = false;
  this->rx_worker_started_ = false;
  this->rx_worker_task_ = nullptr;
}

void MhiAcCtrl::start_tx_worker_() {
  if (!this->tx_worker_enabled_) {
    return;
  }

  if (this->tx_worker_started_) {
    return;
  }

  this->tx_worker_stop_requested_ = false;
  this->tx_worker_running_ = false;

  TaskHandle_t task_handle = nullptr;
  const BaseType_t priority = static_cast<BaseType_t>(this->tx_worker_priority_);
  const uint32_t stack_size = this->tx_worker_stack_size_;

  BaseType_t created = pdFALSE;
  if (this->tx_worker_core_id_ >= 0) {
    created = xTaskCreatePinnedToCore(&MhiAcCtrl::tx_worker_task_entry_, "mhi_tx_worker", stack_size, this, priority,
                                      &task_handle, static_cast<BaseType_t>(this->tx_worker_core_id_));
  } else {
    created = xTaskCreate(&MhiAcCtrl::tx_worker_task_entry_, "mhi_tx_worker", stack_size, this, priority, &task_handle);
  }

  if (created != pdPASS || task_handle == nullptr) {
    ESP_LOGE(TAG, "TX worker start failed: stack=%lu priority=%lu core=%d", static_cast<unsigned long>(stack_size),
             static_cast<unsigned long>(this->tx_worker_priority_), this->tx_worker_core_id_);
    this->tx_worker_enabled_ = false;
    this->transport_.set_auto_tx_flush(true);
    this->tx_worker_task_ = nullptr;
    return;
  }

  this->tx_worker_task_ = task_handle;
  this->tx_worker_started_ = true;
  ESP_LOGCONFIG(TAG, "TX worker started: stack=%lu priority=%lu core=%d start_delay=%lums",
                static_cast<unsigned long>(stack_size), static_cast<unsigned long>(this->tx_worker_priority_),
                this->tx_worker_core_id_, static_cast<unsigned long>(this->tx_worker_start_delay_ms_));
}

void MhiAcCtrl::stop_tx_worker_() {
  if (!this->tx_worker_started_) {
    return;
  }

  this->tx_worker_stop_requested_ = true;
}

void MhiAcCtrl::tx_worker_task_entry_(void* arg) {
  auto* self = static_cast<MhiAcCtrl*>(arg);
  if (self == nullptr) {
    vTaskDelete(nullptr);
    return;
  }

  self->tx_worker_task_loop_();
  vTaskDelete(nullptr);
}

void MhiAcCtrl::tx_worker_task_loop_() {
  this->tx_worker_running_ = true;

  if (this->tx_worker_start_delay_ms_ > 0U) {
    vTaskDelay(pdMS_TO_TICKS(this->tx_worker_start_delay_ms_));
  }

  while (!this->tx_worker_stop_requested_) {
    this->tx_worker_loops_++;
    this->tx_worker_flush_attempts_++;

    if (this->transport_.flush_tx_on_bus_marker()) {
      this->tx_worker_flush_successes_++;
      taskYIELD();
      continue;
    }

    this->tx_worker_idle_yields_++;
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  this->tx_worker_running_ = false;
  this->tx_worker_started_ = false;
  this->tx_worker_task_ = nullptr;
}

bool MhiAcCtrl::background_tx_due_(uint32_t now_ms) const {
  if (this->tx_background_interval_ms_ == 0U) {
    return false;
  }
  if (this->last_background_tx_ms_ == 0U) {
    return true;
  }
  return (now_ms - this->last_background_tx_ms_) >= this->tx_background_interval_ms_;
}

bool MhiAcCtrl::command_confirmation_pending_() const {
  return this->command_confirmation_.pending_mask() != 0U;
}

bool MhiAcCtrl::background_tx_allowed_(uint32_t now_ms) {
  if (!this->background_tx_due_(now_ms)) {
    this->tx_background_interval_deferrals_++;
    return false;
  }

  if (this->command_confirmation_pending_()) {
    this->tx_background_confirmation_deferrals_++;
    return false;
  }

  return true;
}

void MhiAcCtrl::build_and_stage_tx_frame_() {
  this->suppress_duplicate_pending_commands_();

  const bool has_pending_command = this->state_.command().has_pending_command();
  const uint32_t now = millis();

  if (!has_pending_command && !this->background_tx_allowed_(now)) {
    return;
  }

  this->refresh_extended_louver_tx_context_();

  MhiFrameBuffer tx_frame{};
  MhiTxBuildResult build_result{};

  const bool built = MhiTxBuilder::build_next_frame(this->state_.command(), this->tx_runtime_, this->tx_config_,
                                                    tx_frame, build_result);

  if (!built || tx_frame.len == 0U) {
    return;
  }

  const bool command_frame = build_result.has_encoded_commands();
  const bool background_frame = !has_pending_command && !command_frame;

  if (has_pending_command) {
    this->tx_command_priority_attempts_++;
  } else {
    this->tx_background_attempts_++;
  }

  const bool queued = this->transport_.queue_tx(tx_frame.bytes(), tx_frame.len);

  if (background_frame) {
    this->last_background_tx_ms_ = now;
    if (!queued) {
      this->tx_background_failures_++;
    }
  }

  this->record_tx_build_result_(build_result, tx_frame, queued);
}

void MhiAcCtrl::record_tx_build_result_(const MhiTxBuildResult& result, const MhiFrameBuffer& frame, bool sent) {
  const uint32_t now = millis();

  if (result.has_encoded_commands()) {
    if (sent) {
      this->diagnostics_.stats().on_tx_command_frame(result.encoded_command_mask, now);
      this->command_confirmation_.stage(result.intent, result.encoded_command_mask, now);
      if (this->command_confirmation_.pending_mask() != 0U) {
        this->clear_command_candidate_();
      }
    } else {
      this->diagnostics_.stats().on_tx_command_failure(result.encoded_command_mask, now);
    }

    ESP_LOGI(DIAG_TAG,
             "command: staged=%s mask=0x%08lx len=%u db0=0x%02x db1=0x%02x db2=0x%02x db6=0x%02x "
             "db9=0x%02x db16=0x%02x db17=0x%02x",
             sent ? "YES" : "NO", static_cast<unsigned long>(result.encoded_command_mask),
             static_cast<unsigned int>(frame.len), frame.data[DB0], frame.data[DB1], frame.data[DB2], frame.data[DB6],
             frame.data[DB9], frame.len > DB16 ? frame.data[DB16] : 0U, frame.len > DB17 ? frame.data[DB17] : 0U);
  }

  if (result.has_unsupported_commands()) {
    this->diagnostics_.stats().on_unsupported_command(result.unsupported_command_mask, now);

    ESP_LOGW(DIAG_TAG, "command: unsupported mask=0x%08lx frame_size=%u; command dropped",
             static_cast<unsigned long>(result.unsupported_command_mask), static_cast<unsigned int>(frame.len));
  }
}

bool MhiAcCtrl::read_and_sync_rx_frame_() {
  if (this->rx_worker_enabled_) {
    return this->decode_cataloged_frames_();
  }

  uint8_t buffer[kMhiMaxFrameBytes]{};
  MhiFrameBuffer frame{};

  for (std::size_t chunk = 0U; chunk < kMaxRxChunksPerLoop; chunk++) {
    const std::size_t len = this->transport_.read_rx(buffer, sizeof(buffer));
    if (len == 0U) {
      break;
    }

    this->frame_sync_.push_bytes(buffer, len);

    while (this->frame_sync_.pop_frame(frame)) {
      this->diagnostics_.stats().on_valid_frame(millis());
      this->ingest_rx_frame_(frame);
    }
  }

  return this->decode_cataloged_frames_();
}

bool MhiAcCtrl::ingest_rx_frame_(const MhiFrameBuffer& frame) {
  portENTER_CRITICAL(&this->frame_catalog_mux_);
  const bool store_command_candidate = this->command_confirmation_pending_();
  const MhiCatalogIngestResult result = this->frame_catalog_.ingest_mosi_frame(
      frame.view(), ++this->frame_catalog_sequence_, millis(), store_command_candidate);
  portEXIT_CRITICAL(&this->frame_catalog_mux_);

  if (!result.stored) {
    ESP_LOGVV(DIAG_TAG, "catalog: dropped kind=%s key=0x%04x len=%u", mhi_frame_kind_to_string(result.kind),
              static_cast<unsigned int>(result.opdata_key), static_cast<unsigned int>(frame.len));
    return false;
  }

  if (result.overwritten) {
    ESP_LOGVV(DIAG_TAG, "catalog: overwritten kind=%s key=0x%04x sequence=%lu", mhi_frame_kind_to_string(result.kind),
              static_cast<unsigned int>(result.opdata_key), static_cast<unsigned long>(this->frame_catalog_sequence_));
  }

  return true;
}

bool MhiAcCtrl::take_latest_extended_status_(MhiCatalogedFrame& out) {
  portENTER_CRITICAL(&this->frame_catalog_mux_);
  const bool taken = this->frame_catalog_.take_latest_extended_status(out);
  portEXIT_CRITICAL(&this->frame_catalog_mux_);
  return taken;
}

bool MhiAcCtrl::take_latest_status_(MhiCatalogedFrame& out) {
  portENTER_CRITICAL(&this->frame_catalog_mux_);
  const bool taken = this->frame_catalog_.take_latest_status(out);
  portEXIT_CRITICAL(&this->frame_catalog_mux_);
  return taken;
}

bool MhiAcCtrl::take_latest_command_candidate_(MhiCatalogedFrame& out) {
  portENTER_CRITICAL(&this->frame_catalog_mux_);
  const bool taken = this->frame_catalog_.take_latest_command_candidate(out);
  portEXIT_CRITICAL(&this->frame_catalog_mux_);
  return taken;
}

void MhiAcCtrl::clear_command_candidate_() {
  portENTER_CRITICAL(&this->frame_catalog_mux_);
  this->frame_catalog_.clear_command_candidate();
  portEXIT_CRITICAL(&this->frame_catalog_mux_);
}

bool MhiAcCtrl::take_next_opdata_(MhiCatalogedFrame& out) {
  portENTER_CRITICAL(&this->frame_catalog_mux_);
  const bool taken = this->frame_catalog_.take_next_opdata(out);
  portEXIT_CRITICAL(&this->frame_catalog_mux_);
  return taken;
}

bool MhiAcCtrl::take_latest_unknown_(MhiCatalogedFrame& out) {
  portENTER_CRITICAL(&this->frame_catalog_mux_);
  const bool taken = this->frame_catalog_.take_latest_unknown(out);
  portEXIT_CRITICAL(&this->frame_catalog_mux_);
  return taken;
}

MhiCatalogStats MhiAcCtrl::catalog_stats_snapshot_() {
  portENTER_CRITICAL(&this->frame_catalog_mux_);
  const MhiCatalogStats stats = this->frame_catalog_.stats();
  portEXIT_CRITICAL(&this->frame_catalog_mux_);
  return stats;
}

bool MhiAcCtrl::decode_cataloged_frames_() {
  bool decoded_anything = false;
  MhiCatalogedFrame cataloged{};

  // While a command is pending, preserve the latest status/extended feedback in a side slot so
  // the RX worker cannot overwrite a short-lived confirmation candidate before the main loop decodes it.
  if (this->command_confirmation_pending_() && this->take_latest_command_candidate_(cataloged)) {
    if (this->decode_cataloged_frame_(cataloged)) {
      decoded_anything = true;
    }
  }

  // Command/extended feedback can affect pending command confirmation, so drain it first.
  if (this->take_latest_extended_status_(cataloged)) {
    if (this->decode_cataloged_frame_(cataloged)) {
      decoded_anything = true;
    }
  }

  if (this->take_latest_status_(cataloged)) {
    if (this->decode_cataloged_frame_(cataloged)) {
      decoded_anything = true;
    }
  }

  while (this->take_next_opdata_(cataloged)) {
    if (this->decode_cataloged_frame_(cataloged)) {
      decoded_anything = true;
    }
  }

  // Keep the unknown slot from becoming permanently valid. Unknowns are still counted in catalog diagnostics.
  if (this->take_latest_unknown_(cataloged)) {
    ESP_LOGVV(DIAG_TAG, "catalog: ignored unknown sequence=%lu len=%u", static_cast<unsigned long>(cataloged.sequence),
              static_cast<unsigned int>(cataloged.frame.len));
  }

  return decoded_anything;
}

bool MhiAcCtrl::decode_cataloged_frame_(const MhiCatalogedFrame& cataloged_frame) {
  return this->decode_frame_(cataloged_frame.frame);
}

bool MhiAcCtrl::decode_frame_(const MhiFrameBuffer& frame) {
  bool decoded_anything = false;

  const MhiFrameView view = frame.view();

  MhiDecodedStatus decoded_status{};

  if (MhiStatusDecoder::decode_mosi(view, decoded_status)) {
    if (this->apply_status_update_(decoded_status, frame)) {
      decoded_anything = true;
    }
  }

  MhiDecodedOpData decoded_opdata{};

  if (MhiOpDataDecoder::decode_mosi(view, decoded_opdata)) {
    if (this->apply_opdata_update_(decoded_opdata, frame)) {
      decoded_anything = true;
    }
  }

  return decoded_anything;
}

bool MhiAcCtrl::apply_status_update_(const MhiDecodedStatus& decoded_status, const MhiFrameBuffer& frame) {
  if (!this->is_sane_status_(decoded_status, frame)) {
    return false;
  }

  auto& status = this->state_.status();
  const bool previous_valid = status.valid;
  const bool previous_horizontal_swing = status.horizontal_vane_swing;
  const uint8_t previous_horizontal_vane = status.horizontal_vane;
  const bool previous_3d_auto = status.three_d_auto;

  status.valid = decoded_status.valid;
  status.power = decoded_status.power;
  status.mode = decoded_status.mode;
  status.fan = decoded_status.fan;
  status.target_temp_c = decoded_status.target_temp_c;
  status.room_temp_c = decoded_status.room_temp_c;
  status.vertical_vane = decoded_status.vertical_vane;
  status.vanes_swing = decoded_status.vertical_swing;

  const bool accept_extended_feedback = (decoded_status.has_horizontal_vane || decoded_status.has_3d_auto) &&
                                        this->accept_extended_feedback_(decoded_status, frame);

  if (decoded_status.has_horizontal_vane && accept_extended_feedback) {
    status.has_horizontal_vane = true;
    status.horizontal_vane_swing = decoded_status.horizontal_swing;
    status.horizontal_vane = decoded_status.horizontal_vane;

    if (previous_valid && previous_horizontal_swing != status.horizontal_vane_swing) {
      this->log_suspicious_status_change_("horizontal_swing", previous_horizontal_swing ? 1 : 0,
                                          status.horizontal_vane_swing ? 1 : 0, frame);
    }

    if (previous_valid && previous_horizontal_vane != status.horizontal_vane) {
      this->log_suspicious_status_change_("horizontal_vane", previous_horizontal_vane, status.horizontal_vane, frame);
    }
  }

  if (decoded_status.has_3d_auto && accept_extended_feedback) {
    status.has_3d_auto = true;
    status.three_d_auto = decoded_status.three_d_auto;

    if (previous_valid && previous_3d_auto != status.three_d_auto) {
      this->log_suspicious_status_change_("three_d_auto", previous_3d_auto ? 1 : 0, status.three_d_auto ? 1 : 0, frame);
    }
  }

  if (accept_extended_feedback && decoded_status.has_extended_louver_raw) {
    status.has_extended_louver_raw = true;
    status.extended_louver_db16 = decoded_status.extended_louver_db16;
    status.extended_louver_db17 = decoded_status.extended_louver_db17;
  }

  status.error_code = decoded_status.error_code;
  status.last_update_ms = millis();

  this->diagnostics_.stats().set_last_error_code(decoded_status.error_code);
  this->update_command_confirmation_(status);

  return true;
}

bool MhiAcCtrl::apply_opdata_update_(const MhiDecodedOpData& decoded_opdata, const MhiFrameBuffer& frame) {
  auto& opdata = this->state_.opdata();
  bool accepted = false;

  opdata.valid = true;
  opdata.last_update_ms = millis();

  if (decoded_opdata.has_outdoor_temp) {
    if (in_range(decoded_opdata.outdoor_temp_c, -60.0f, 80.0f)) {
      opdata.has_outdoor_temp = true;
      opdata.outdoor_temp_c = decoded_opdata.outdoor_temp_c;
      accepted = true;
    } else {
      this->log_rejected_opdata_("outdoor_temp", decoded_opdata.outdoor_temp_c, frame);
    }
  }

  if (decoded_opdata.has_return_air) {
    if (in_range(decoded_opdata.return_air_c, -10.0f, 60.0f)) {
      opdata.has_return_air = true;
      opdata.return_air_c = decoded_opdata.return_air_c;
      accepted = true;
    } else {
      this->log_rejected_opdata_("return_air", decoded_opdata.return_air_c, frame);
    }
  }

  if (decoded_opdata.has_compressor_frequency) {
    if (in_range(decoded_opdata.compressor_frequency_hz, 0.0f, 250.0f)) {
      opdata.has_compressor_frequency = true;
      opdata.compressor_frequency_hz = decoded_opdata.compressor_frequency_hz;
      accepted = true;
    } else {
      this->log_rejected_opdata_("compressor_frequency", decoded_opdata.compressor_frequency_hz, frame);
    }
  }

  if (decoded_opdata.has_current) {
    if (in_range(decoded_opdata.current_a, 0.0f, 80.0f)) {
      opdata.has_current = true;
      opdata.current_a = decoded_opdata.current_a;
      accepted = true;
    } else {
      this->log_rejected_opdata_("current", decoded_opdata.current_a, frame);
    }
  }

  if (decoded_opdata.has_indoor_unit_fan_speed) {
    if (decoded_opdata.indoor_unit_fan_speed <= 15U) {
      opdata.has_indoor_unit_fan_speed = true;
      opdata.indoor_unit_fan_speed = decoded_opdata.indoor_unit_fan_speed;
      accepted = true;
    } else {
      this->log_rejected_opdata_("indoor_unit_fan_speed", decoded_opdata.indoor_unit_fan_speed, frame);
    }
  }

  if (decoded_opdata.has_outdoor_unit_fan_speed) {
    if (decoded_opdata.outdoor_unit_fan_speed <= 15U) {
      opdata.has_outdoor_unit_fan_speed = true;
      opdata.outdoor_unit_fan_speed = decoded_opdata.outdoor_unit_fan_speed;
      accepted = true;
    } else {
      this->log_rejected_opdata_("outdoor_unit_fan_speed", decoded_opdata.outdoor_unit_fan_speed, frame);
    }
  }

  if (decoded_opdata.has_total_indoor_runtime) {
    opdata.has_indoor_unit_total_run_time = true;
    opdata.indoor_unit_total_run_time_hours = decoded_opdata.total_indoor_runtime_hours;
    accepted = true;
  }

  if (decoded_opdata.has_total_compressor_runtime) {
    opdata.has_compressor_total_run_time = true;
    opdata.compressor_total_run_time_hours = decoded_opdata.total_compressor_runtime_hours;
    accepted = true;
  }

  if (decoded_opdata.has_energy_used) {
    if (in_range(decoded_opdata.energy_used_kwh, 0.0f, 1000000.0f)) {
      opdata.has_energy_used = true;
      opdata.energy_used_kwh = decoded_opdata.energy_used_kwh;
      accepted = true;
    } else {
      this->log_rejected_opdata_("energy_used", decoded_opdata.energy_used_kwh, frame);
    }
  }

  if (decoded_opdata.has_indoor_unit_thi_r1) {
    if (in_range(decoded_opdata.indoor_unit_thi_r1_c, -50.0f, 130.0f)) {
      opdata.has_indoor_unit_thi_r1 = true;
      opdata.indoor_unit_thi_r1_c = decoded_opdata.indoor_unit_thi_r1_c;
      accepted = true;
    } else {
      this->log_rejected_opdata_("indoor_unit_thi_r1", decoded_opdata.indoor_unit_thi_r1_c, frame);
    }
  }

  if (decoded_opdata.has_indoor_unit_thi_r2) {
    if (in_range(decoded_opdata.indoor_unit_thi_r2_c, -50.0f, 130.0f)) {
      opdata.has_indoor_unit_thi_r2 = true;
      opdata.indoor_unit_thi_r2_c = decoded_opdata.indoor_unit_thi_r2_c;
      accepted = true;
    } else {
      this->log_rejected_opdata_("indoor_unit_thi_r2", decoded_opdata.indoor_unit_thi_r2_c, frame);
    }
  }

  if (decoded_opdata.has_indoor_unit_thi_r3) {
    if (in_range(decoded_opdata.indoor_unit_thi_r3_c, -50.0f, 130.0f)) {
      opdata.has_indoor_unit_thi_r3 = true;
      opdata.indoor_unit_thi_r3_c = decoded_opdata.indoor_unit_thi_r3_c;
      accepted = true;
    } else {
      this->log_rejected_opdata_("indoor_unit_thi_r3", decoded_opdata.indoor_unit_thi_r3_c, frame);
    }
  }

  if (decoded_opdata.has_outdoor_unit_tho_r1) {
    if (in_range(decoded_opdata.outdoor_unit_tho_r1_c, -50.0f, 130.0f)) {
      opdata.has_outdoor_unit_tho_r1 = true;
      opdata.outdoor_unit_tho_r1_c = decoded_opdata.outdoor_unit_tho_r1_c;
      accepted = true;
    } else {
      this->log_rejected_opdata_("outdoor_unit_tho_r1", decoded_opdata.outdoor_unit_tho_r1_c, frame);
    }
  }

  if (decoded_opdata.has_outdoor_unit_expansion_valve) {
    opdata.has_outdoor_unit_expansion_valve = true;
    opdata.outdoor_unit_expansion_valve_pulses = decoded_opdata.outdoor_unit_expansion_valve_pulses;
    accepted = true;
  }

  if (decoded_opdata.has_outdoor_unit_discharge_pipe) {
    if (in_range(decoded_opdata.outdoor_unit_discharge_pipe_c, 0.0f, 140.0f)) {
      opdata.has_outdoor_unit_discharge_pipe = true;
      opdata.outdoor_unit_discharge_pipe_c = decoded_opdata.outdoor_unit_discharge_pipe_c;
      accepted = true;
    } else {
      this->log_rejected_opdata_("outdoor_unit_discharge_pipe", decoded_opdata.outdoor_unit_discharge_pipe_c, frame);
    }
  }

  if (decoded_opdata.has_outdoor_unit_discharge_pipe_super_heat) {
    if (in_range(decoded_opdata.outdoor_unit_discharge_pipe_super_heat_c, 0.0f, 120.0f)) {
      opdata.has_outdoor_unit_discharge_pipe_super_heat = true;
      opdata.outdoor_unit_discharge_pipe_super_heat_c = decoded_opdata.outdoor_unit_discharge_pipe_super_heat_c;
      accepted = true;
    } else {
      this->log_rejected_opdata_("outdoor_unit_discharge_pipe_super_heat",
                                 decoded_opdata.outdoor_unit_discharge_pipe_super_heat_c, frame);
    }
  }

  if (decoded_opdata.has_protection_state_number) {
    opdata.has_protection_state_number = true;
    opdata.protection_state_number = decoded_opdata.protection_state_number;
    accepted = true;
  }

  if (decoded_opdata.has_defrost) {
    opdata.has_defrost = true;
    opdata.defrost = decoded_opdata.defrost;
    accepted = true;
  }

  return accepted;
}

bool MhiAcCtrl::is_sane_status_(const MhiDecodedStatus& decoded_status, const MhiFrameBuffer& frame) const {
  if (!decoded_status.valid) {
    return false;
  }

  const bool fan_ok = decoded_status.fan == 0U || decoded_status.fan == 1U || decoded_status.fan == 2U ||
                      decoded_status.fan == 6U || decoded_status.fan == 7U;
  if (decoded_status.mode > 4U || !fan_ok || decoded_status.vertical_vane > 4U) {
    ESP_LOGW(DIAG_TAG,
             "decode_reject: field=status_enum mode=%u fan=%u vertical=%u len=%u db0=0x%02x "
             "db1=0x%02x db2=0x%02x db3=0x%02x db6=0x%02x db9=0x%02x",
             static_cast<unsigned int>(decoded_status.mode), static_cast<unsigned int>(decoded_status.fan),
             static_cast<unsigned int>(decoded_status.vertical_vane), static_cast<unsigned int>(frame.len),
             frame.data[DB0], frame.data[DB1], frame.data[DB2], frame.data[DB3], frame.data[DB6], frame.data[DB9]);
    return false;
  }

  if (!in_range(decoded_status.target_temp_c, 5.0f, 40.0f) || !in_range(decoded_status.room_temp_c, -10.0f, 60.0f)) {
    ESP_LOGW(DIAG_TAG,
             "decode_reject: field=status_temp target=%.2f room=%.2f len=%u db0=0x%02x db1=0x%02x "
             "db2=0x%02x db3=0x%02x db6=0x%02x db9=0x%02x",
             decoded_status.target_temp_c, decoded_status.room_temp_c, static_cast<unsigned int>(frame.len),
             frame.data[DB0], frame.data[DB1], frame.data[DB2], frame.data[DB3], frame.data[DB6], frame.data[DB9]);
    return false;
  }

  if (decoded_status.has_horizontal_vane && decoded_status.horizontal_vane > 7U) {
    ESP_LOGD(DIAG_TAG, "decode_reject: field=horizontal_vane value=%u len=%u db16=0x%02x db17=0x%02x db9=0x%02x",
             static_cast<unsigned int>(decoded_status.horizontal_vane), static_cast<unsigned int>(frame.len),
             frame.len > DB16 ? frame.data[DB16] : 0U, frame.len > DB17 ? frame.data[DB17] : 0U, frame.data[DB9]);
    return false;
  }

  return true;
}

bool MhiAcCtrl::accept_extended_feedback_(const MhiDecodedStatus& decoded_status, const MhiFrameBuffer& frame) {
  (void)frame;

  if (!decoded_status.has_horizontal_vane && !decoded_status.has_3d_auto) {
    this->pending_extended_feedback_candidate_ = false;
    this->pending_extended_feedback_repeat_count_ = 0U;
    return false;
  }

  this->pending_extended_feedback_candidate_ = false;
  this->pending_extended_feedback_repeat_count_ = 0U;
  this->extended_louver_bootstrap_complete_ = true;
  return true;
}
bool MhiAcCtrl::extended_feedback_matches_pending_(const MhiDecodedStatus& decoded_status) const {
  const uint32_t pending_mask = this->command_confirmation_.pending_mask();
  const auto& intent = this->command_confirmation_.pending_intent();

  if ((pending_mask & MHI_COMMAND_HORIZONTAL_VANE) != 0U) {
    if (!decoded_status.has_horizontal_vane) {
      return false;
    }

    if (intent.horizontal_vane == 8U) {
      if (!decoded_status.horizontal_swing) {
        return false;
      }
    } else if (intent.horizontal_vane >= 1U && intent.horizontal_vane <= 7U) {
      if (decoded_status.horizontal_swing || decoded_status.horizontal_vane != intent.horizontal_vane) {
        return false;
      }
    } else {
      return false;
    }
  }

  if ((pending_mask & MHI_COMMAND_THREE_D_AUTO) != 0U) {
    if (!decoded_status.has_3d_auto || decoded_status.three_d_auto != intent.three_d_auto) {
      return false;
    }

    if (intent.has_extended_louver_context) {
      if (!decoded_status.has_horizontal_vane) {
        return false;
      }

      if (intent.horizontal_vane == 8U) {
        if (!decoded_status.horizontal_swing) {
          return false;
        }
      } else if (intent.horizontal_vane >= 1U && intent.horizontal_vane <= 7U) {
        if (decoded_status.horizontal_swing || decoded_status.horizontal_vane != intent.horizontal_vane) {
          return false;
        }
      }
    }
  }

  return (pending_mask & kNoPendingExtendedFeedbackMask) != 0U;
}

void MhiAcCtrl::log_suspicious_status_change_(const char* field, int old_value, int new_value,
                                              const MhiFrameBuffer& frame) const {
  (void)field;
  (void)old_value;
  (void)new_value;
  (void)frame;
}
void MhiAcCtrl::log_rejected_opdata_(const char* field, float value, const MhiFrameBuffer& frame) const {
  ESP_LOGW(DIAG_TAG,
           "decode_reject: field=%s value=%.2f len=%u db6=0x%02x db9=0x%02x db10=0x%02x db11=0x%02x db12=0x%02x", field,
           value, static_cast<unsigned int>(frame.len), frame.data[DB6], frame.data[DB9],
           frame.len > DB10 ? frame.data[DB10] : 0U, frame.len > DB11 ? frame.data[DB11] : 0U,
           frame.len > DB12 ? frame.data[DB12] : 0U);
}

void MhiAcCtrl::update_command_confirmation_(const MhiStatusState& status) {
  uint32_t confirmed_mask = this->command_confirmation_.observe_status(status);

  if (this->settled_extended_confirmation_mask_ != 0U) {
    confirmed_mask |= this->command_confirmation_.settle_pending_mask(this->settled_extended_confirmation_mask_);
    this->settled_extended_confirmation_mask_ = 0U;
  }

  if (confirmed_mask == 0U) {
    return;
  }

  const uint32_t now = millis();
  this->diagnostics_.stats().on_command_confirmed(confirmed_mask, now);

  if (this->command_confirmation_.pending_mask() == 0U) {
    this->clear_command_candidate_();
  }

  if ((confirmed_mask & kNoPendingExtendedFeedbackMask) != 0U) {
    this->extended_louver_bootstrap_complete_ = true;
    this->pending_extended_feedback_candidate_ = false;
    this->pending_extended_feedback_repeat_count_ = 0U;
  }

  ESP_LOGI(DIAG_TAG, "command: confirmed mask=0x%08lx pending=0x%08lx", static_cast<unsigned long>(confirmed_mask),
           static_cast<unsigned long>(this->command_confirmation_.pending_mask()));
}

void MhiAcCtrl::check_command_confirmation_timeout_() {
  const uint32_t now = millis();
  const uint32_t timeout_mask = this->command_confirmation_.expire(now);

  if (timeout_mask == 0U) {
    return;
  }

  this->diagnostics_.stats().on_command_confirmation_timeout(timeout_mask, now);
  this->clear_command_candidate_();

  ESP_LOGW(DIAG_TAG, "command: confirmation timeout mask=0x%08lx", static_cast<unsigned long>(timeout_mask));
}

void MhiAcCtrl::suppress_duplicate_pending_commands_() {
  auto& command = this->state_.command();
  const uint32_t duplicate_mask = this->command_confirmation_.duplicate_pending_mask(command);

  if (duplicate_mask == 0U) {
    return;
  }

  command.clear_pending_mask(duplicate_mask);

  ESP_LOGD(DIAG_TAG, "command: duplicate pending request ignored mask=0x%08lx",
           static_cast<unsigned long>(duplicate_mask));
}

}  // namespace mhi_ac_ctrl
}  // namespace esphome
