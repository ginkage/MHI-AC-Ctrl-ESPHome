#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

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
#include "mhi_frame_catalog.h"
#include "mhi_frame_sync.h"
#include "mhi_opdata_decoder.h"
#include "mhi_publish_bridge.h"
#include "mhi_state.h"
#include "mhi_status_decoder.h"
#include "mhi_transport_manager.h"
#include "mhi_tx_builder.h"

namespace esphome {
namespace mhi_ac_ctrl {

struct MhiPins {
  int sck{-1};
  int mosi{-1};
  int miso{-1};
};

class MhiAcCtrl : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_frame_size(int frame_size) {
    this->frame_size_ = frame_size;
  }

  void set_sck_pin(int pin) {
    this->pins_.sck = pin;
  }
  void set_mosi_pin(int pin) {
    this->pins_.mosi = pin;
  }
  void set_miso_pin(int pin) {
    this->pins_.miso = pin;
  }

  void set_rx_driver(const std::string& driver) {
    this->rx_driver_ = driver;
  }
  void set_tx_driver(const std::string& driver) {
    this->tx_driver_ = driver;
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

  void set_frame_start_idle_ms(int idle_ms) {
    if (idle_ms > 0) {
      this->frame_start_idle_ms_ = static_cast<uint32_t>(idle_ms);
    }
  }

  void set_rmt_spi_frame_gap_us(int frame_gap_us) {
    if (frame_gap_us >= 500 && frame_gap_us <= 5000) {
      this->rmt_spi_frame_gap_us_ = static_cast<uint32_t>(frame_gap_us);
    }
  }

  void set_tx_background_interval_ms(int interval_ms) {
    if (interval_ms >= 0) {
      this->tx_background_interval_ms_ = static_cast<uint32_t>(interval_ms);
    }
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

  void set_external_room_temperature_sensor(sensor::Sensor* sensor) {
    this->external_room_temperature_sensor_ = sensor;
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

  void add_opdata_mask(uint32_t mask) {
    this->opdata_mask_ |= mask;
    this->tx_config_.enabled_opdata_mask = this->opdata_mask_;
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
  void refresh_publish_targets_();
  void record_tx_build_result_(const MhiTxBuildResult& result, const MhiFrameBuffer& frame, bool sent);
  bool read_and_sync_rx_frame_();
  bool ingest_rx_frame_(const MhiFrameBuffer& frame);
  bool decode_cataloged_frames_();
  bool decode_cataloged_frame_(const MhiCatalogedFrame& cataloged_frame);
  bool take_latest_extended_status_(MhiCatalogedFrame& out);
  bool take_latest_status_(MhiCatalogedFrame& out);
  bool take_latest_command_candidate_(MhiCatalogedFrame& out);
  void clear_command_candidate_();
  bool take_next_opdata_(MhiCatalogedFrame& out);
  bool take_latest_unknown_(MhiCatalogedFrame& out);
  MhiCatalogStats catalog_stats_snapshot_();
  void start_command_worker_();
  void stop_command_worker_();
  static void command_worker_task_entry_(void* arg);
  void command_worker_task_loop_();
  void notify_command_worker_();
  void service_command_pipeline_();
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
  void apply_external_room_temperature_(float value);
  void clear_external_room_temperature_();
  void check_external_room_temperature_timeout_();

  static uint32_t elapsed_us_(uint32_t start_us);
  static uint8_t detect_chip_core_count_();

  int frame_size_{20};
  int room_temp_api_timeout_s_{60};
  uint32_t room_temperature_publish_interval_ms_{15000U};
  float room_temperature_immediate_delta_c_{1.0f};
  bool room_temp_api_active_{false};
  uint32_t room_temp_api_timeout_start_ms_{0U};
  float last_external_room_temperature_c_{NAN};

  int initial_vertical_vanes_position_{0};
  int initial_horizontal_vanes_position_{0};

  MhiPins pins_{};

  std::string rx_driver_{"fast_gpio_rx"};
  std::string tx_driver_{"fast_gpio_tx"};
  MhiFanProfile fan_profile_{MhiFanProfile::FOUR_SPEED};

  sensor::Sensor* external_room_temperature_sensor_{nullptr};

  uint32_t opdata_mask_{kMhiDefaultOpdataMask};

  MhiStateStore state_{};
  MhiFrameSync frame_sync_{};
  MhiFrameCatalog frame_catalog_{};
  portMUX_TYPE frame_catalog_mux_ = portMUX_INITIALIZER_UNLOCKED;
  MhiTransportManager transport_{};
  MhiDiagnostics diagnostics_{};

  MhiPublishTargets publish_targets_{};
  MhiPublishBridge publish_bridge_{};

  MhiTxRuntime tx_runtime_{};
  MhiTxBuildConfig tx_config_{};
  MhiCommandCoordinator command_coordinator_{};
  SemaphoreHandle_t command_mutex_{nullptr};

  uint32_t frame_start_idle_ms_{10U};
  uint32_t rmt_spi_frame_gap_us_{1000U};
  uint32_t tx_background_interval_ms_{250U};
  uint32_t last_background_tx_ms_{0U};
  uint32_t tx_background_interval_deferrals_{0U};
  uint32_t tx_background_confirmation_deferrals_{0U};
  uint32_t tx_background_attempts_{0U};
  uint32_t tx_background_failures_{0U};
  uint32_t tx_command_priority_attempts_{0U};
  bool rx_byte_critical_sections_enabled_{true};
  bool publish_requested_{false};
  uint32_t frame_catalog_sequence_{0U};

  bool command_worker_enabled_{false};
  volatile bool command_worker_running_{false};
  volatile bool command_worker_stop_requested_{false};
  volatile bool command_worker_started_{false};
  TaskHandle_t command_worker_task_{nullptr};
  uint32_t command_worker_start_delay_ms_{0U};
  uint32_t command_worker_stack_size_{6144U};
  uint32_t command_worker_priority_{4U};
  int command_worker_core_id_{-1};
  volatile uint32_t command_worker_wakes_{0U};
  volatile uint32_t command_worker_service_runs_{0U};
  volatile uint32_t command_worker_idle_timeouts_{0U};
  volatile uint32_t command_worker_frames_staged_{0U};
  volatile uint32_t command_worker_completions_{0U};

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
