#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "mhi_command_coordinator.h"
#include "mhi_defs.h"
#include "mhi_diag.h"
#include "mhi_fan_profile.h"
#include "mhi_opdata_decoder.h"
#include "mhi_opdata_freshness.h"
#include "mhi_opdata_freshness_publisher.h"
#include "mhi_power_estimator.h"
#include "mhi_publish_bridge.h"
#include "mhi_rx_runtime.h"
#include "mhi_state.h"
#include "mhi_status_decoder.h"
#include "mhi_transport_diagnostics_publisher.h"
#include "mhi_transport_manager.h"
#include "mhi_tx_builder.h"

namespace esphome {
namespace mhi_ac_ctrl {

class MhiAcCtrl : public Component, public IMhiTransportTransitionListener {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  bool teardown() override;

  void set_frame_size(int frame_size) {
    this->frame_size_ = frame_size;
  }

  void set_primary_transport(IMhiTransport* transport) {
    this->transport_.set_primary(transport);
  }

  void set_recovery_transport(IMhiTransport* transport) {
    this->transport_.set_recovery(transport);
  }

  bool set_active_mode(bool enabled);
  bool active_mode() const {
    return active_mode_enabled_.load(std::memory_order_acquire);
  }

  void set_active_mode_switch(switch_::Switch* sw) {
    active_mode_switch_ = sw;
    this->publish_active_mode_state_();
  }

  void set_fan_profile(const std::string& profile) {
    this->fan_profile_ = mhi_fan_profile_from_name(profile);
    this->publish_bridge_.set_fan_profile(this->fan_profile_);
  }

  MhiFanProfile fan_profile() const {
    return this->fan_profile_;
  }

  bool fan_profile_supports_quiet() const {
    return mhi_fan_profile_supports_quiet(this->fan_profile_);
  }

  void set_tx_background_interval_ms(int interval_ms) {
    if (interval_ms >= 0) {
      this->tx_background_interval_ms_ = static_cast<uint32_t>(interval_ms);
    }
  }

  void set_command_confirmation_timeout_ms(uint32_t timeout_ms) {
    this->command_confirmation_timeout_ms_ = timeout_ms;
    this->command_coordinator_.set_confirmation_timeout_ms(timeout_ms);
  }

  void set_command_final_confirmation_grace_ms(uint32_t grace_ms) {
    this->command_final_confirmation_grace_ms_ = grace_ms;
    this->command_coordinator_.set_final_confirmation_grace_ms(grace_ms);
  }

  void set_command_worker(bool enabled) {
    this->command_worker_enabled_ = enabled;
  }

  void set_command_worker_start_delay_ms(int delay_ms) {
    if (delay_ms >= 0) {
      this->command_worker_start_delay_ms_ = static_cast<uint32_t>(delay_ms);
    }
  }

  void set_command_worker_stack_size(int stack_size) {
    if (stack_size >= 4096) {
      this->command_worker_stack_size_ = static_cast<uint32_t>(stack_size);
    }
  }

  void set_command_worker_priority(int priority) {
    if (priority > 0) {
      this->command_worker_priority_ = static_cast<uint32_t>(priority);
    }
  }

  void set_command_worker_core_id(int core_id) {
    if (core_id >= -1 && core_id <= 1) {
      this->command_worker_core_id_ = core_id;
    }
  }

  void set_room_temp_api_timeout(int timeout_s) {
    this->room_temp_api_timeout_s_ = timeout_s;
  }

  void set_room_temperature_publish_interval_ms(uint32_t interval_ms) {
    this->room_temperature_publish_interval_ms_ = interval_ms;
    this->publish_bridge_.set_room_temperature_publish_interval_ms(interval_ms);
  }

  void set_room_temperature_immediate_delta(float delta_c) {
    this->room_temperature_immediate_delta_c_ = delta_c;
    this->publish_bridge_.set_room_temperature_immediate_delta(delta_c);
  }

  void set_external_voltage_sensor(sensor::Sensor* sensor) {
    this->external_voltage_sensor_ = sensor;
  }

  void set_external_voltage_timeout_ms(uint32_t timeout_ms) {
    this->power_estimator_.set_external_voltage_timeout_ms(timeout_ms);
  }

  void set_external_power_factor_sensor(sensor::Sensor* sensor) {
    this->external_power_factor_sensor_ = sensor;
  }

  void set_external_power_factor_timeout_ms(uint32_t timeout_ms) {
    this->power_estimator_.set_external_power_factor_timeout_ms(timeout_ms);
  }

  void set_external_room_temperature_sensor(sensor::Sensor* sensor) {
    this->external_room_temperature_sensor_ = sensor;
  }

  void set_external_temperature_fallback_on_homeassistant_disconnect(bool enabled) {
    this->external_temperature_fallback_on_homeassistant_disconnect_ = enabled;
  }

  void set_external_temperature_disconnect_grace_period_ms(uint32_t grace_period_ms) {
    this->external_temperature_disconnect_grace_period_ms_ = grace_period_ms;
  }

  void set_external_room_temperature(float value);

  // Compatibility aliases for old config names.
  void set_vanes(int position) {
    this->initial_vertical_vanes_position_ = position;
  }

  void set_vanesLR(int position) {
    this->initial_horizontal_vanes_position_ = position;
  }

  void set_initial_vertical_vanes_position(int position) {
    this->initial_vertical_vanes_position_ = position;
  }

  void set_initial_horizontal_vanes_position(int position) {
    this->initial_horizontal_vanes_position_ = position;
  }

  void set_opdata_poll_interval_ms(uint32_t request_mask, uint32_t interval_ms) {
    for (std::size_t i = 0; i < kMhiOpdataRequestCount; ++i) {
      if (request_mask == (1UL << i)) {
        this->tx_config_.opdata_poll_intervals_ms[i] = interval_ms;
        // Explicit polling enables the request even without an attached sensor.
        this->add_opdata_mask(request_mask);
        return;
      }
    }
  }

  void add_opdata_mask(uint32_t mask) {
    this->opdata_mask_ |= mask;
    this->tx_config_.enabled_opdata_mask = this->opdata_mask_;
    this->opdata_freshness_.set_enabled_mask(this->opdata_mask_);
  }

  void set_publish_targets(const MhiPublishTargets& targets) {
    this->publish_targets_ = targets;
    this->refresh_publish_targets_();
  }

  void set_climate_target(climate::Climate* climate) {
    this->publish_targets_.climate = climate;
    this->refresh_publish_targets_();
  }

  void set_power_binary_sensor(binary_sensor::BinarySensor* sensor) {
    this->publish_targets_.power_binary_sensor = sensor;
    this->refresh_publish_targets_();
  }

  void set_room_temp_sensor(sensor::Sensor* sensor) {
    this->publish_targets_.room_temp_sensor = sensor;
    this->refresh_publish_targets_();
  }

  void set_target_temp_sensor(sensor::Sensor* sensor) {
    this->publish_targets_.target_temp_sensor = sensor;
    this->refresh_publish_targets_();
  }

  void set_outdoor_temp_sensor(sensor::Sensor* sensor) {
    this->publish_targets_.outdoor_temp_sensor = sensor;
    this->refresh_publish_targets_();
  }

  void set_return_air_sensor(sensor::Sensor* sensor) {
    this->publish_targets_.return_air_sensor = sensor;
    this->refresh_publish_targets_();
  }

  void set_compressor_frequency_sensor(sensor::Sensor* sensor) {
    this->publish_targets_.compressor_frequency_sensor = sensor;
    this->refresh_publish_targets_();
  }

  void set_current_sensor(sensor::Sensor* sensor) {
    this->publish_targets_.current_sensor = sensor;
    this->refresh_publish_targets_();
  }

  void set_indoor_unit_fan_speed_sensor(sensor::Sensor* sensor) {
    this->publish_targets_.indoor_unit_fan_speed_sensor = sensor;
    this->refresh_publish_targets_();
  }

  void set_outdoor_unit_fan_speed_sensor(sensor::Sensor* sensor) {
    this->publish_targets_.outdoor_unit_fan_speed_sensor = sensor;
    this->refresh_publish_targets_();
  }

  void set_indoor_unit_total_run_time_sensor(sensor::Sensor* sensor) {
    this->publish_targets_.indoor_unit_total_run_time_sensor = sensor;
    this->refresh_publish_targets_();
  }

  void set_compressor_total_run_time_sensor(sensor::Sensor* sensor) {
    this->publish_targets_.compressor_total_run_time_sensor = sensor;
    this->refresh_publish_targets_();
  }

  void set_energy_used_sensor(sensor::Sensor* sensor) {
    this->publish_targets_.energy_used_sensor = sensor;
    this->refresh_publish_targets_();
  }

  void set_estimated_power_sensor(sensor::Sensor* sensor) {
    this->publish_targets_.estimated_power_sensor = sensor;
    this->power_estimator_.set_enabled(true);
    this->refresh_publish_targets_();
  }

  void set_estimated_energy_sensor(sensor::Sensor* sensor) {
    this->publish_targets_.estimated_energy_sensor = sensor;
    this->power_estimator_.set_enabled(true);
    this->refresh_publish_targets_();
  }

  void configure_power_estimation(float nominal_voltage_v, float power_factor, float standby_power_w,
                                  uint32_t max_sample_interval_ms) {
    this->power_estimator_.configure(nominal_voltage_v, power_factor, standby_power_w, max_sample_interval_ms);
  }

  void set_indoor_unit_thi_r1_sensor(sensor::Sensor* sensor) {
    this->publish_targets_.indoor_unit_thi_r1_sensor = sensor;
    this->refresh_publish_targets_();
  }

  void set_indoor_unit_thi_r2_sensor(sensor::Sensor* sensor) {
    this->publish_targets_.indoor_unit_thi_r2_sensor = sensor;
    this->refresh_publish_targets_();
  }

  void set_indoor_unit_thi_r3_sensor(sensor::Sensor* sensor) {
    this->publish_targets_.indoor_unit_thi_r3_sensor = sensor;
    this->refresh_publish_targets_();
  }

  void set_outdoor_unit_tho_r1_sensor(sensor::Sensor* sensor) {
    this->publish_targets_.outdoor_unit_tho_r1_sensor = sensor;
    this->refresh_publish_targets_();
  }

  void set_outdoor_unit_expansion_valve_sensor(sensor::Sensor* sensor) {
    this->publish_targets_.outdoor_unit_expansion_valve_sensor = sensor;
    this->refresh_publish_targets_();
  }

  void set_outdoor_unit_discharge_pipe_sensor(sensor::Sensor* sensor) {
    this->publish_targets_.outdoor_unit_discharge_pipe_sensor = sensor;
    this->refresh_publish_targets_();
  }

  void set_outdoor_unit_discharge_pipe_super_heat_sensor(sensor::Sensor* sensor) {
    this->publish_targets_.outdoor_unit_discharge_pipe_super_heat_sensor = sensor;
    this->refresh_publish_targets_();
  }

  void set_protection_state_number_sensor(sensor::Sensor* sensor) {
    this->publish_targets_.protection_state_number_sensor = sensor;
    this->refresh_publish_targets_();
  }

  void set_defrost_binary_sensor(binary_sensor::BinarySensor* sensor) {
    this->publish_targets_.defrost_binary_sensor = sensor;
    this->refresh_publish_targets_();
  }

  void set_vanes_3d_auto_enabled_binary_sensor(binary_sensor::BinarySensor* sensor) {
    this->publish_targets_.vanes_3d_auto_enabled_binary_sensor = sensor;
    this->refresh_publish_targets_();
  }

  void set_vanes_3d_auto_switch(switch_::Switch* sw) {
    this->publish_targets_.vanes_3d_auto_switch = sw;
    this->refresh_publish_targets_();
  }

  void set_error_code_text_sensor(text_sensor::TextSensor* sensor) {
    this->publish_targets_.error_code_text_sensor = sensor;
    this->refresh_publish_targets_();
  }

  void set_protection_state_text_sensor(text_sensor::TextSensor* sensor) {
    this->publish_targets_.protection_state_text_sensor = sensor;
    this->refresh_publish_targets_();
  }

  void set_transport_healthy_binary_sensor(binary_sensor::BinarySensor* sensor) {
    this->transport_diagnostics_publisher_.set_healthy_binary_sensor(sensor);
  }

  void set_transport_recovery_active_binary_sensor(binary_sensor::BinarySensor* sensor) {
    this->transport_diagnostics_publisher_.set_recovery_active_binary_sensor(sensor);
  }

  void set_transport_safe_mode_binary_sensor(binary_sensor::BinarySensor* sensor) {
    this->transport_diagnostics_publisher_.set_safe_mode_binary_sensor(sensor);
  }

  void set_active_transport_text_sensor(text_sensor::TextSensor* sensor) {
    this->transport_diagnostics_publisher_.set_active_transport_text_sensor(sensor);
  }

  void set_transport_state_text_sensor(text_sensor::TextSensor* sensor) {
    this->transport_diagnostics_publisher_.set_state_text_sensor(sensor);
  }

  void set_last_transport_error_text_sensor(text_sensor::TextSensor* sensor) {
    this->transport_diagnostics_publisher_.set_last_error_text_sensor(sensor);
  }

  void set_opdata_freshness_timeout_ms(uint32_t timeout_ms) {
    this->opdata_freshness_timeout_ms_ = timeout_ms;
    this->opdata_freshness_.set_timeout_ms(timeout_ms);
  }

  void set_opdata_fresh_binary_sensor(binary_sensor::BinarySensor* sensor) {
    this->opdata_freshness_publisher_.set_fresh_binary_sensor(sensor);
  }

  void set_opdata_oldest_age_sensor(sensor::Sensor* sensor) {
    this->opdata_freshness_publisher_.set_oldest_age_sensor(sensor);
  }

  void set_opdata_stale_count_sensor(sensor::Sensor* sensor) {
    this->opdata_freshness_publisher_.set_stale_count_sensor(sensor);
  }

  void set_opdata_timeout_events_sensor(sensor::Sensor* sensor) {
    this->opdata_freshness_publisher_.set_timeout_events_sensor(sensor);
  }

  void set_vertical_vanes_select(select::Select* select) {
    this->publish_targets_.vertical_vanes_select = select;
    this->refresh_publish_targets_();
  }

  void set_horizontal_vanes_select(select::Select* select) {
    this->publish_targets_.horizontal_vanes_select = select;
    this->refresh_publish_targets_();
  }

  void set_fan_speed_select(select::Select* select) {
    this->publish_targets_.fan_speed_select = select;
    this->refresh_publish_targets_();
  }

  MhiStateStore& state() {
    return this->state_;
  }
  const MhiStateStore& state() const {
    return this->state_;
  }

  uint32_t request_command_patch(const MhiCommandState& patch);
  void request_power_command(bool power);
  void request_mode_command(uint8_t mode);
  void request_fan_command(uint8_t fan);
  void request_target_temperature_command(float target_temp_c);
  void request_vertical_vane_command(uint8_t vertical_vane);
  bool request_horizontal_vane_command(uint8_t horizontal_vane);
  bool request_three_d_auto_command(bool enabled);

  MhiDiagnostics& diagnostics() {
    return this->diagnostics_;
  }
  const MhiDiagnostics& diagnostics() const {
    return this->diagnostics_;
  }

 protected:
  void on_transport_switch_begin(const MhiTransportErrorDetail& reason) override;
  void on_transport_recovery_ready() override;
  void on_transport_safe_mode(const MhiTransportErrorDetail& reason) override;
  void reset_command_runtime_();
  void reset_runtime_for_transport_switch_();
  void publish_active_mode_state_();
  bool transport_command_path_ready_() const;
  void publish_transport_diagnostics_(bool force = false);
  void service_opdata_freshness_(bool force = false);

  void refresh_publish_targets_();
  void record_tx_build_result_(const MhiTxBuildResult& result, const MhiFrameBuffer& frame, bool sent);
  bool read_and_sync_rx_frame_();
  bool service_classified_rx_pipeline_();
  bool decode_cataloged_frames_();
  bool decode_cataloged_frame_(const MhiCatalogedFrame& cataloged_frame);
  bool apply_worker_decoded_snapshots_();
  void start_command_worker_();
  void stop_command_worker_();
  static void command_worker_task_entry_(void* arg);
  void command_worker_task_loop_();
  void notify_command_worker_();
  void service_command_pipeline_();
  void service_silent_mode_confirmation_schedule_locked_(uint32_t now_ms);
  void reset_silent_mode_confirmation_schedule_locked_();
  bool worker_handles_rx_() const;
  void drain_tx_completions_();
  bool decode_frame_(const MhiFrameBuffer& frame);
  bool apply_status_update_(const MhiDecodedStatus& decoded_status, const MhiFrameBuffer& frame);
  bool apply_opdata_update_(const MhiDecodedOpData& decoded_opdata, const MhiFrameBuffer& frame);
  bool is_sane_status_(const MhiDecodedStatus& decoded_status, const MhiFrameBuffer& frame) const;
  bool accept_extended_feedback_(const MhiDecodedStatus& decoded_status, const MhiFrameBuffer& frame);
  bool extended_feedback_matches_pending_(const MhiDecodedStatus& decoded_status) const;
  bool confirmed_extended_louver_matches_horizontal_(uint8_t horizontal_vane) const;
  bool confirmed_extended_louver_matches_three_d_auto_(bool enabled) const;
  void refresh_extended_louver_tx_context_();
  void log_suspicious_status_change_(const char* field, int old_value, int new_value,
                                     const MhiFrameBuffer& frame) const;
  void log_rejected_opdata_(const char* field, float value, const MhiFrameBuffer& frame) const;
  void log_runtime_diagnostics_();
  void update_command_confirmation_(const MhiStatusState& status);
  void check_command_confirmation_timeout_();
  void suppress_duplicate_pending_commands_();
  bool background_tx_due_(uint32_t now_ms) const;
  bool command_confirmation_pending_() const;
  bool background_tx_allowed_(uint32_t now_ms);
  bool apply_external_room_temperature_(float value);
  void handle_external_room_temperature_sensor_state_(float value);
  void clear_external_room_temperature_();
  void check_external_room_temperature_timeout_();
  bool is_homeassistant_api_connected_() const;
  void check_external_temperature_homeassistant_connection_();

  static uint32_t elapsed_us_(uint32_t start_us);
  static uint8_t detect_chip_core_count_();

  int frame_size_{20};
  int room_temp_api_timeout_s_{60};
  uint32_t room_temperature_publish_interval_ms_{15000U};
  float room_temperature_immediate_delta_c_{1.0f};
  bool room_temp_api_active_{false};
  bool external_room_temperature_sensor_active_{false};
  bool external_temperature_fallback_on_homeassistant_disconnect_{false};
  bool homeassistant_api_connected_{false};
  bool homeassistant_disconnect_grace_active_{false};
  uint32_t external_temperature_disconnect_grace_period_ms_{60000U};
  uint32_t homeassistant_disconnect_start_ms_{0U};
  uint32_t last_homeassistant_connection_check_ms_{0U};
  uint32_t room_temp_api_timeout_start_ms_{0U};
  float last_external_room_temperature_c_{NAN};

  int initial_vertical_vanes_position_{0};
  int initial_horizontal_vanes_position_{0};

  MhiFanProfile fan_profile_{MhiFanProfile::FOUR_SPEED};

  sensor::Sensor* external_room_temperature_sensor_{nullptr};
  sensor::Sensor* external_voltage_sensor_{nullptr};
  sensor::Sensor* external_power_factor_sensor_{nullptr};

  uint32_t opdata_mask_{kMhiDefaultOpdataMask};
  uint32_t opdata_freshness_timeout_ms_{120000U};
  uint32_t last_opdata_freshness_publish_ms_{0U};

  MhiStateStore state_{};
  MhiRxRuntime rx_runtime_{};
  MhiTransportManager transport_{};
  MhiDiagnostics diagnostics_{};
  MhiTransportDiagnosticsPublisher transport_diagnostics_publisher_{};
  MhiOpDataFreshnessTracker opdata_freshness_{};
  MhiOpDataFreshnessPublisher opdata_freshness_publisher_{};
  MhiPowerEstimator power_estimator_{};

  MhiPublishTargets publish_targets_{};
  MhiPublishBridge publish_bridge_{};

  MhiTxRuntime tx_runtime_{};
  MhiTxBuildConfig tx_config_{};
  MhiCommandCoordinator command_coordinator_{};
  SemaphoreHandle_t command_mutex_{nullptr};

  uint32_t tx_background_interval_ms_{250U};
  uint32_t command_confirmation_timeout_ms_{kMhiCommandConfirmationTimeoutMs};
  uint32_t command_final_confirmation_grace_ms_{kMhiCommandFinalConfirmationGraceMs};
  uint32_t last_background_tx_ms_{0U};
  uint32_t tx_background_interval_deferrals_{0U};
  uint32_t tx_background_confirmation_deferrals_{0U};
  uint32_t tx_background_attempts_{0U};
  uint32_t tx_background_failures_{0U};
  uint32_t tx_command_priority_attempts_{0U};
  uint32_t tx_staged_replacement_attempts_{0U};
  uint32_t tx_staged_replacement_successes_{0U};
  uint32_t tx_staged_replacement_claimed_misses_{0U};
  uint32_t tx_staged_replacement_unsupported_{0U};
  uint32_t tx_staged_replacement_rejected_{0U};
  uint32_t command_request_revision_{0U};
  uint32_t staged_replacement_revision_{0U};

  // Silent Mode uses a one-shot write followed by passive feedback and, only
  // when needed, two explicit 0xDD probes.
  uint32_t silent_mode_confirmation_started_ms_{0U};
  bool silent_mode_first_poll_requested_{false};
  bool silent_mode_final_poll_requested_{false};

  std::atomic<bool> transport_commands_enabled_{true};
  std::atomic<bool> active_mode_enabled_{true};
  switch_::Switch* active_mode_switch_{nullptr};
  bool publish_requested_{false};

  bool command_worker_enabled_{false};
  bool command_worker_classified_rx_enabled_{false};
  std::atomic<bool> command_worker_running_{false};
  std::atomic<bool> command_worker_stop_requested_{false};
  std::atomic<bool> command_worker_started_{false};
  TaskHandle_t command_worker_task_{nullptr};
  uint32_t command_worker_start_delay_ms_{0U};
  uint32_t command_worker_stack_size_{6144U};
  uint32_t command_worker_priority_{4U};
  int command_worker_core_id_{-1};
  std::atomic<uint32_t> command_worker_wakes_{0U};
  std::atomic<uint32_t> command_worker_service_runs_{0U};
  std::atomic<uint32_t> command_worker_idle_polls_{0U};
  std::atomic<uint32_t> command_worker_frames_staged_{0U};
  std::atomic<uint32_t> command_worker_completions_{0U};
  std::atomic<uint32_t> command_worker_rx_polls_{0U};
  std::atomic<uint32_t> command_worker_rx_batches_{0U};
  std::atomic<uint32_t> command_worker_rx_chunks_{0U};
  std::atomic<uint32_t> command_worker_rx_frames_{0U};
  std::atomic<uint32_t> command_worker_rx_max_batch_{0U};
  std::atomic<uint32_t> command_worker_last_runtime_us_{0U};
  std::atomic<uint32_t> command_worker_max_runtime_us_{0U};
  std::atomic<uint32_t> command_worker_max_notify_batch_{0U};
  std::atomic<uint32_t> command_worker_stack_high_water_bytes_{0U};
  std::atomic<bool> shutting_down_{false};
  bool transport_shutdown_{false};

  bool pending_extended_feedback_candidate_{false};
  bool pending_extended_feedback_swing_{false};
  uint8_t pending_extended_feedback_vane_{0};
  bool pending_extended_feedback_3d_auto_{false};
  uint8_t pending_extended_feedback_db16_{0};
  uint8_t pending_extended_feedback_db17_{0};
  uint8_t pending_extended_feedback_repeat_count_{0};
  bool extended_louver_bootstrap_complete_{false};
  uint32_t settled_extended_confirmation_mask_{0};

  uint32_t last_protocol_health_valid_frames_{0};
  uint32_t last_protocol_health_invalid_frames_{0};
  uint32_t last_protocol_health_checksum_failures_{0};
  uint32_t last_protocol_health_signature_misses_{0};
  uint32_t last_protocol_health_sync_losses_{0};
  uint32_t last_protocol_health_dropped_bytes_{0};

  uint32_t last_diag_log_ms_{0};
};

}  // namespace mhi_ac_ctrl
}  // namespace esphome
