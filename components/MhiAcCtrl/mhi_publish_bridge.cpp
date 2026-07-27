#include "mhi_publish_bridge.h"

#include <cstdio>

namespace esphome {
namespace mhi_ac_ctrl {

namespace {
constexpr float kClimateCurrentTempHighPriorityDeltaC = 0.5f;
constexpr uint32_t kClimateCurrentTempLowPriorityIntervalMs = 15000U;
}  // namespace

void MhiPublishBridge::publish(const MhiStateStore& state) {
  this->publish_status(state.status());
  this->publish_opdata(state.opdata());
}

void MhiPublishBridge::publish_status(const MhiStatusState& status) {
  if (!status.valid) {
    return;
  }

  const bool first_publish = !has_last_status_;

  if (targets_.climate != nullptr) {
    bool changed = first_publish;

    const climate::ClimateMode mode = map_climate_mode(status.power, status.mode);

    if (targets_.climate->mode != mode) {
      targets_.climate->mode = mode;
      changed = true;
    }

    if (float_changed(targets_.climate->target_temperature, status.target_temp_c)) {
      targets_.climate->target_temperature = status.target_temp_c;
      changed = true;
    }

    if (this->should_publish_climate_current_temperature_(status.room_temp_c, first_publish, status.last_update_ms)) {
      targets_.climate->current_temperature = status.room_temp_c;
      changed = true;
    }

    const climate::ClimateFanMode fan_mode = map_climate_fan(status.fan);
    if (!targets_.climate->fan_mode.has_value() || targets_.climate->fan_mode.value() != fan_mode) {
      targets_.climate->fan_mode = fan_mode;
      changed = true;
    }

    const climate::ClimateSwingMode swing_mode = map_climate_swing(status);
    if (targets_.climate->swing_mode != swing_mode) {
      targets_.climate->swing_mode = swing_mode;
      changed = true;
    }

    if (changed) {
      targets_.climate->publish_state();
    }
  }

  if (targets_.power_binary_sensor != nullptr && (first_publish || last_status_.power != status.power)) {
    targets_.power_binary_sensor->publish_state(status.power);
  }

  if (targets_.room_temp_sensor != nullptr &&
      (first_publish || float_changed(last_status_.room_temp_c, status.room_temp_c))) {
    targets_.room_temp_sensor->publish_state(status.room_temp_c);
  }

  if (targets_.target_temp_sensor != nullptr &&
      (first_publish || float_changed(last_status_.target_temp_c, status.target_temp_c))) {
    targets_.target_temp_sensor->publish_state(status.target_temp_c);
  }

  if (targets_.error_code_text_sensor != nullptr && (first_publish || last_status_.error_code != status.error_code)) {
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "%u", status.error_code);
    targets_.error_code_text_sensor->publish_state(buffer);
  }

  if (targets_.vertical_vanes_select != nullptr &&
      (first_publish || last_status_.vertical_vane != status.vertical_vane ||
       last_status_.vanes_swing != status.vanes_swing)) {
    targets_.vertical_vanes_select->publish_state(map_vertical_vane_option(status));
  }

  if (status.has_horizontal_vane && targets_.horizontal_vanes_select != nullptr &&
      (first_publish || !last_status_.has_horizontal_vane || last_status_.horizontal_vane != status.horizontal_vane ||
       last_status_.horizontal_vane_swing != status.horizontal_vane_swing)) {
    targets_.horizontal_vanes_select->publish_state(map_horizontal_vane_option(status));
  }

  if (status.has_3d_auto && targets_.vanes_3d_auto_enabled_binary_sensor != nullptr &&
      (first_publish || !last_status_.has_3d_auto || last_status_.three_d_auto != status.three_d_auto)) {
    targets_.vanes_3d_auto_enabled_binary_sensor->publish_state(status.three_d_auto);
  }

  if (status.has_3d_auto && targets_.vanes_3d_auto_switch != nullptr &&
      (first_publish || !last_status_.has_3d_auto || last_status_.three_d_auto != status.three_d_auto)) {
    targets_.vanes_3d_auto_switch->publish_state(status.three_d_auto);
  }

  if (targets_.fan_speed_select != nullptr && (first_publish || last_status_.fan != status.fan)) {
    const char* fan_option = map_fan_option(status.fan);
    if (fan_option != nullptr) {
      targets_.fan_speed_select->publish_state(fan_option);
    }
  }

  last_status_ = status;
  has_last_status_ = true;
}

void MhiPublishBridge::publish_opdata(const MhiOpDataState& opdata) {
  if (!opdata.valid) {
    return;
  }

  const bool first_publish = !has_last_opdata_;

  if (opdata.has_outdoor_temp && targets_.outdoor_temp_sensor != nullptr &&
      (first_publish || !last_opdata_.has_outdoor_temp ||
       float_changed(last_opdata_.outdoor_temp_c, opdata.outdoor_temp_c))) {
    targets_.outdoor_temp_sensor->publish_state(opdata.outdoor_temp_c);
  }

  if (opdata.has_return_air && targets_.return_air_sensor != nullptr &&
      (first_publish || !last_opdata_.has_return_air ||
       float_changed(last_opdata_.return_air_c, opdata.return_air_c))) {
    targets_.return_air_sensor->publish_state(opdata.return_air_c);
  }

  if (opdata.has_compressor_frequency && targets_.compressor_frequency_sensor != nullptr &&
      (first_publish || !last_opdata_.has_compressor_frequency ||
       float_changed(last_opdata_.compressor_frequency_hz, opdata.compressor_frequency_hz))) {
    targets_.compressor_frequency_sensor->publish_state(opdata.compressor_frequency_hz);
  }

  if (opdata.has_current && targets_.current_sensor != nullptr &&
      (first_publish || !last_opdata_.has_current || float_changed(last_opdata_.current_a, opdata.current_a))) {
    targets_.current_sensor->publish_state(opdata.current_a);
  }

  if (opdata.has_indoor_unit_fan_speed && targets_.indoor_unit_fan_speed_sensor != nullptr &&
      (first_publish || !last_opdata_.has_indoor_unit_fan_speed ||
       last_opdata_.indoor_unit_fan_speed != opdata.indoor_unit_fan_speed)) {
    targets_.indoor_unit_fan_speed_sensor->publish_state(opdata.indoor_unit_fan_speed);
  }

  if (opdata.has_outdoor_unit_fan_speed && targets_.outdoor_unit_fan_speed_sensor != nullptr &&
      (first_publish || !last_opdata_.has_outdoor_unit_fan_speed ||
       last_opdata_.outdoor_unit_fan_speed != opdata.outdoor_unit_fan_speed)) {
    targets_.outdoor_unit_fan_speed_sensor->publish_state(opdata.outdoor_unit_fan_speed);
  }

  if (opdata.has_indoor_unit_total_run_time && targets_.indoor_unit_total_run_time_sensor != nullptr &&
      (first_publish || !last_opdata_.has_indoor_unit_total_run_time ||
       last_opdata_.indoor_unit_total_run_time_hours != opdata.indoor_unit_total_run_time_hours)) {
    targets_.indoor_unit_total_run_time_sensor->publish_state(opdata.indoor_unit_total_run_time_hours);
  }

  if (opdata.has_compressor_total_run_time && targets_.compressor_total_run_time_sensor != nullptr &&
      (first_publish || !last_opdata_.has_compressor_total_run_time ||
       last_opdata_.compressor_total_run_time_hours != opdata.compressor_total_run_time_hours)) {
    targets_.compressor_total_run_time_sensor->publish_state(opdata.compressor_total_run_time_hours);
  }

  if (opdata.has_energy_used && targets_.energy_used_sensor != nullptr &&
      (first_publish || !last_opdata_.has_energy_used ||
       float_changed(last_opdata_.energy_used_kwh, opdata.energy_used_kwh))) {
    targets_.energy_used_sensor->publish_state(opdata.energy_used_kwh);
  }

  if (opdata.has_indoor_unit_thi_r1 && targets_.indoor_unit_thi_r1_sensor != nullptr &&
      (first_publish || !last_opdata_.has_indoor_unit_thi_r1 ||
       float_changed(last_opdata_.indoor_unit_thi_r1_c, opdata.indoor_unit_thi_r1_c))) {
    targets_.indoor_unit_thi_r1_sensor->publish_state(opdata.indoor_unit_thi_r1_c);
  }

  if (opdata.has_indoor_unit_thi_r2 && targets_.indoor_unit_thi_r2_sensor != nullptr &&
      (first_publish || !last_opdata_.has_indoor_unit_thi_r2 ||
       float_changed(last_opdata_.indoor_unit_thi_r2_c, opdata.indoor_unit_thi_r2_c))) {
    targets_.indoor_unit_thi_r2_sensor->publish_state(opdata.indoor_unit_thi_r2_c);
  }

  if (opdata.has_indoor_unit_thi_r3 && targets_.indoor_unit_thi_r3_sensor != nullptr &&
      (first_publish || !last_opdata_.has_indoor_unit_thi_r3 ||
       float_changed(last_opdata_.indoor_unit_thi_r3_c, opdata.indoor_unit_thi_r3_c))) {
    targets_.indoor_unit_thi_r3_sensor->publish_state(opdata.indoor_unit_thi_r3_c);
  }

  if (opdata.has_outdoor_unit_tho_r1 && targets_.outdoor_unit_tho_r1_sensor != nullptr &&
      (first_publish || !last_opdata_.has_outdoor_unit_tho_r1 ||
       float_changed(last_opdata_.outdoor_unit_tho_r1_c, opdata.outdoor_unit_tho_r1_c))) {
    targets_.outdoor_unit_tho_r1_sensor->publish_state(opdata.outdoor_unit_tho_r1_c);
  }

  if (opdata.has_outdoor_unit_expansion_valve && targets_.outdoor_unit_expansion_valve_sensor != nullptr &&
      (first_publish || !last_opdata_.has_outdoor_unit_expansion_valve ||
       last_opdata_.outdoor_unit_expansion_valve_pulses != opdata.outdoor_unit_expansion_valve_pulses)) {
    targets_.outdoor_unit_expansion_valve_sensor->publish_state(opdata.outdoor_unit_expansion_valve_pulses);
  }

  if (opdata.has_outdoor_unit_discharge_pipe && targets_.outdoor_unit_discharge_pipe_sensor != nullptr &&
      (first_publish || !last_opdata_.has_outdoor_unit_discharge_pipe ||
       float_changed(last_opdata_.outdoor_unit_discharge_pipe_c, opdata.outdoor_unit_discharge_pipe_c))) {
    targets_.outdoor_unit_discharge_pipe_sensor->publish_state(opdata.outdoor_unit_discharge_pipe_c);
  }

  if (opdata.has_outdoor_unit_discharge_pipe_super_heat &&
      targets_.outdoor_unit_discharge_pipe_super_heat_sensor != nullptr &&
      (first_publish || !last_opdata_.has_outdoor_unit_discharge_pipe_super_heat ||
       float_changed(last_opdata_.outdoor_unit_discharge_pipe_super_heat_c,
                     opdata.outdoor_unit_discharge_pipe_super_heat_c))) {
    targets_.outdoor_unit_discharge_pipe_super_heat_sensor->publish_state(
        opdata.outdoor_unit_discharge_pipe_super_heat_c);
  }

  if (opdata.has_protection_state_number && targets_.protection_state_number_sensor != nullptr &&
      (first_publish || !last_opdata_.has_protection_state_number ||
       last_opdata_.protection_state_number != opdata.protection_state_number)) {
    targets_.protection_state_number_sensor->publish_state(opdata.protection_state_number);
  }

  if (opdata.has_protection_state_number && targets_.protection_state_text_sensor != nullptr &&
      (first_publish || !last_opdata_.has_protection_state_number ||
       last_opdata_.protection_state_number != opdata.protection_state_number)) {
    targets_.protection_state_text_sensor->publish_state(map_protection_state(opdata.protection_state_number));
  }

  if (opdata.has_defrost && targets_.defrost_binary_sensor != nullptr &&
      (first_publish || !last_opdata_.has_defrost || last_opdata_.defrost != opdata.defrost)) {
    targets_.defrost_binary_sensor->publish_state(opdata.defrost);
  }

  last_opdata_ = opdata;
  has_last_opdata_ = true;
}

climate::ClimateMode MhiPublishBridge::map_climate_mode(bool power, uint8_t mode) {
  if (!power) {
    return climate::CLIMATE_MODE_OFF;
  }

  switch (mode) {
    case 0:
      // MHI auto is exposed as HEAT_COOL in Home Assistant so the frontend
      // keeps temperature control available. HA AUTO is not advertised.
      return climate::CLIMATE_MODE_HEAT_COOL;

    case 1:
      return climate::CLIMATE_MODE_DRY;

    case 2:
      return climate::CLIMATE_MODE_COOL;

    case 3:
      return climate::CLIMATE_MODE_FAN_ONLY;

    case 4:
      return climate::CLIMATE_MODE_HEAT;

    default:
      return climate::CLIMATE_MODE_HEAT_COOL;
  }
}

climate::ClimateFanMode MhiPublishBridge::map_climate_fan(uint8_t fan) const {
  switch (mhi_fan_mode_from_code(this->fan_profile_, fan)) {
    case MhiFanMode::QUIET:
      return climate::CLIMATE_FAN_QUIET;
    case MhiFanMode::LOW:
      return climate::CLIMATE_FAN_LOW;
    case MhiFanMode::MEDIUM:
      return climate::CLIMATE_FAN_MEDIUM;
    case MhiFanMode::HIGH:
      return climate::CLIMATE_FAN_HIGH;
    case MhiFanMode::AUTO:
    case MhiFanMode::UNKNOWN:
    default:
      return climate::CLIMATE_FAN_AUTO;
  }
}

climate::ClimateSwingMode MhiPublishBridge::map_climate_swing(const MhiStatusState& status) {
  return status.vanes_swing ? climate::CLIMATE_SWING_VERTICAL : climate::CLIMATE_SWING_OFF;
}

const char* MhiPublishBridge::map_vertical_vane_option(const MhiStatusState& status) {
  if (status.vanes_swing) {
    return "Swing";
  }

  switch (status.vertical_vane) {
    case 1U:
      return "Up";
    case 2U:
      return "Up/Center";
    case 3U:
      return "Center/Down";
    case 4U:
      return "Down";
    default:
      return "Down";
  }
}

const char* MhiPublishBridge::map_horizontal_vane_option(const MhiStatusState& status) {
  if (status.horizontal_vane_swing) {
    return "Swing";
  }

  switch (status.horizontal_vane) {
    case 1U:
      return "Left";
    case 2U:
      return "Left/Center";
    case 3U:
      return "Center";
    case 4U:
      return "Center/Right";
    case 5U:
      return "Right";
    case 6U:
      return "Wide";
    case 7U:
      return "Spot";
    default:
      return "Center";
  }
}

const char* MhiPublishBridge::map_fan_option(uint8_t fan) const {
  return mhi_fan_mode_name(mhi_fan_mode_from_code(this->fan_profile_, fan));
}

const char* MhiPublishBridge::map_protection_state(uint8_t state) {
  static constexpr const char* kProtectionStates[] = {
      "Normal",
      "Discharge pipe temperature protection control",
      "Discharge pipe temperature anomaly",
      "Current safe control of inverter primary current",
      "High pressure protection control",
      "High pressure anomaly",
      "Low pressure protection control",
      "Low pressure anomaly",
      "Anti-frost prevention control",
      "Current cut",
      "Power transistor protection control",
      "Power transistor anomaly (Overheat)",
      "Compression ratio control",
      "-",
      "Condensation prevention control",
      "Current safe control of inverter secondary current",
      "Stop by compressor rotor lock",
      "Stop by compressor startup failure",
  };

  if (state < (sizeof(kProtectionStates) / sizeof(kProtectionStates[0]))) {
    return kProtectionStates[state];
  }

  return "Unknown";
}

bool MhiPublishBridge::float_changed(float old_value, float new_value) {
  if (std::isnan(old_value) || std::isnan(new_value)) {
    return true;
  }

  return old_value != new_value;
}

bool MhiPublishBridge::should_publish_climate_current_temperature_(float decoded_temp, bool first_publish,
                                                                   uint32_t now_ms) {
  if (first_publish || !this->has_climate_published_current_temp_) {
    this->has_climate_published_current_temp_ = true;
    this->climate_published_current_temp_ = decoded_temp;
    this->last_climate_current_temp_publish_ms_ = now_ms;
    return true;
  }

  if (!float_changed(this->climate_published_current_temp_, decoded_temp)) {
    return false;
  }

  const float delta = std::fabs(decoded_temp - this->climate_published_current_temp_);
  if (delta >= kClimateCurrentTempHighPriorityDeltaC) {
    this->climate_published_current_temp_ = decoded_temp;
    this->last_climate_current_temp_publish_ms_ = now_ms;
    return true;
  }

  const bool interval_elapsed =
      now_ms != 0U && this->last_climate_current_temp_publish_ms_ != 0U &&
      (now_ms - this->last_climate_current_temp_publish_ms_) >= kClimateCurrentTempLowPriorityIntervalMs;
  if (!interval_elapsed) {
    return false;
  }

  this->climate_published_current_temp_ = decoded_temp;
  this->last_climate_current_temp_publish_ms_ = now_ms;
  return true;
}

void MhiPublishBridge::reset_publish_cache_() {
  this->has_last_status_ = false;
  this->has_last_opdata_ = false;
  this->has_climate_published_current_temp_ = false;
  this->climate_published_current_temp_ = 0.0f;
  this->last_climate_current_temp_publish_ms_ = 0U;
}

}  // namespace mhi_ac_ctrl
}  // namespace esphome