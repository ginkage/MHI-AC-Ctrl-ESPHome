#include "mhi_test_common.h"

namespace mhi_unit_tests {

void publish_bridge_republishes_cached_state_after_targets_are_registered() {
  MhiStateStore state{};

  auto& status = state.status();
  status.valid = true;
  status.power = true;
  status.mode = 2U;
  status.fan = 2U;
  status.target_temp_c = 22.0f;
  status.room_temp_c = 24.0f;
  status.vertical_vane = 3U;
  status.error_code = 0U;

  auto& opdata = state.opdata();
  opdata.valid = true;
  opdata.has_outdoor_temp = true;
  opdata.outdoor_temp_c = 12.0f;
  opdata.has_current = true;
  opdata.current_a = 1.25f;

  MhiPublishBridge bridge{};

  // Simulate the parent decoding stable frames before ESPHome child entities
  // have registered their publish targets.
  bridge.publish(state);

  esphome::climate::Climate climate{};
  esphome::binary_sensor::BinarySensor power{};
  esphome::sensor::Sensor room{};
  esphome::sensor::Sensor target{};
  esphome::sensor::Sensor outdoor{};
  esphome::sensor::Sensor current{};
  esphome::text_sensor::TextSensor error{};
  esphome::select::Select vertical{};
  esphome::select::Select fan{};

  MhiPublishTargets targets{};
  targets.climate = &climate;
  targets.power_binary_sensor = &power;
  targets.room_temp_sensor = &room;
  targets.target_temp_sensor = &target;
  targets.outdoor_temp_sensor = &outdoor;
  targets.current_sensor = &current;
  targets.error_code_text_sensor = &error;
  targets.vertical_vanes_select = &vertical;
  targets.fan_speed_select = &fan;

  bridge.set_targets(targets);
  bridge.publish(state);

  EXPECT_EQ(climate.publish_count, 1U);
  EXPECT_EQ(power.publish_count, 1U);
  EXPECT_EQ(room.publish_count, 1U);
  EXPECT_EQ(target.publish_count, 1U);
  EXPECT_EQ(outdoor.publish_count, 1U);
  EXPECT_EQ(current.publish_count, 1U);
  EXPECT_EQ(error.publish_count, 1U);
  EXPECT_EQ(vertical.publish_count, 1U);
  EXPECT_EQ(fan.publish_count, 1U);

  EXPECT_TRUE(power.state);
  expect_near(room.state, 24.0f);
  expect_near(target.state, 22.0f);
  expect_near(outdoor.state, 12.0f);
  expect_near(current.state, 1.25f);
  EXPECT_TRUE(error.state == "0");
  EXPECT_TRUE(vertical.state == "Center/Down");
  EXPECT_TRUE(fan.state == "Medium");
}

}  // namespace mhi_unit_tests

namespace mhi_unit_tests {

void publish_bridge_publishes_sensor_parity_slice1_on_first_opdata_publish() {
  MhiStateStore state{};

  auto& opdata = state.opdata();
  opdata.valid = true;
  opdata.has_indoor_unit_fan_speed = true;
  opdata.indoor_unit_fan_speed = 7U;
  opdata.has_outdoor_unit_fan_speed = true;
  opdata.outdoor_unit_fan_speed = 4U;
  opdata.has_indoor_unit_total_run_time = true;
  opdata.indoor_unit_total_run_time_hours = 1200U;
  opdata.has_compressor_total_run_time = true;
  opdata.compressor_total_run_time_hours = 900U;
  opdata.has_energy_used = true;
  opdata.energy_used_kwh = 42.5f;

  esphome::sensor::Sensor indoor_fan{};
  esphome::sensor::Sensor outdoor_fan{};
  esphome::sensor::Sensor indoor_runtime{};
  esphome::sensor::Sensor compressor_runtime{};
  esphome::sensor::Sensor energy{};

  MhiPublishTargets targets{};
  targets.indoor_unit_fan_speed_sensor = &indoor_fan;
  targets.outdoor_unit_fan_speed_sensor = &outdoor_fan;
  targets.indoor_unit_total_run_time_sensor = &indoor_runtime;
  targets.compressor_total_run_time_sensor = &compressor_runtime;
  targets.energy_used_sensor = &energy;

  MhiPublishBridge bridge{};
  bridge.set_targets(targets);
  bridge.publish(state);

  EXPECT_EQ(indoor_fan.publish_count, 1U);
  EXPECT_EQ(outdoor_fan.publish_count, 1U);
  EXPECT_EQ(indoor_runtime.publish_count, 1U);
  EXPECT_EQ(compressor_runtime.publish_count, 1U);
  EXPECT_EQ(energy.publish_count, 1U);

  expect_near(indoor_fan.state, 7.0f);
  expect_near(outdoor_fan.state, 4.0f);
  expect_near(indoor_runtime.state, 1200.0f);
  expect_near(compressor_runtime.state, 900.0f);
  expect_near(energy.state, 42.5f);
}

}  // namespace mhi_unit_tests


namespace mhi_unit_tests {

void publish_bridge_publishes_sensor_parity_slice2_on_first_opdata_publish() {
  MhiStateStore state{};

  auto& opdata = state.opdata();
  opdata.valid = true;
  opdata.has_indoor_unit_thi_r1 = true;
  opdata.indoor_unit_thi_r1_c = 21.3f;
  opdata.has_indoor_unit_thi_r2 = true;
  opdata.indoor_unit_thi_r2_c = 8.0f;
  opdata.has_indoor_unit_thi_r3 = true;
  opdata.indoor_unit_thi_r3_c = 24.57f;
  opdata.has_outdoor_unit_tho_r1 = true;
  opdata.outdoor_unit_tho_r1_c = 27.84f;
  opdata.has_outdoor_unit_expansion_valve = true;
  opdata.outdoor_unit_expansion_valve_pulses = 0x1234U;
  opdata.has_outdoor_unit_discharge_pipe = true;
  opdata.outdoor_unit_discharge_pipe_c = 52.0f;
  opdata.has_outdoor_unit_discharge_pipe_super_heat = true;
  opdata.outdoor_unit_discharge_pipe_super_heat_c = 11.0f;
  opdata.has_protection_state_number = true;
  opdata.protection_state_number = 8U;
  opdata.has_defrost = true;
  opdata.defrost = true;

  esphome::sensor::Sensor thi_r1{};
  esphome::sensor::Sensor thi_r2{};
  esphome::sensor::Sensor thi_r3{};
  esphome::sensor::Sensor tho_r1{};
  esphome::sensor::Sensor expansion_valve{};
  esphome::sensor::Sensor discharge_pipe{};
  esphome::sensor::Sensor discharge_super_heat{};
  esphome::sensor::Sensor protection_number{};
  esphome::binary_sensor::BinarySensor defrost{};
  esphome::text_sensor::TextSensor protection_text{};

  MhiPublishTargets targets{};
  targets.indoor_unit_thi_r1_sensor = &thi_r1;
  targets.indoor_unit_thi_r2_sensor = &thi_r2;
  targets.indoor_unit_thi_r3_sensor = &thi_r3;
  targets.outdoor_unit_tho_r1_sensor = &tho_r1;
  targets.outdoor_unit_expansion_valve_sensor = &expansion_valve;
  targets.outdoor_unit_discharge_pipe_sensor = &discharge_pipe;
  targets.outdoor_unit_discharge_pipe_super_heat_sensor = &discharge_super_heat;
  targets.protection_state_number_sensor = &protection_number;
  targets.defrost_binary_sensor = &defrost;
  targets.protection_state_text_sensor = &protection_text;

  MhiPublishBridge bridge{};
  bridge.set_targets(targets);
  bridge.publish(state);

  EXPECT_EQ(thi_r1.publish_count, 1U);
  EXPECT_EQ(thi_r2.publish_count, 1U);
  EXPECT_EQ(thi_r3.publish_count, 1U);
  EXPECT_EQ(tho_r1.publish_count, 1U);
  EXPECT_EQ(expansion_valve.publish_count, 1U);
  EXPECT_EQ(discharge_pipe.publish_count, 1U);
  EXPECT_EQ(discharge_super_heat.publish_count, 1U);
  EXPECT_EQ(protection_number.publish_count, 1U);
  EXPECT_EQ(defrost.publish_count, 1U);
  EXPECT_EQ(protection_text.publish_count, 1U);

  expect_near(thi_r1.state, 21.3f);
  expect_near(thi_r2.state, 8.0f);
  expect_near(thi_r3.state, 24.57f);
  expect_near(tho_r1.state, 27.84f);
  expect_near(expansion_valve.state, 4660.0f);
  expect_near(discharge_pipe.state, 52.0f);
  expect_near(discharge_super_heat.state, 11.0f);
  expect_near(protection_number.state, 8.0f);
  EXPECT_TRUE(defrost.state);
  EXPECT_TRUE(protection_text.state == "Anti-frost prevention control");
}

void publish_bridge_maps_unknown_protection_state() {
  MhiStateStore state{};

  auto& opdata = state.opdata();
  opdata.valid = true;
  opdata.has_protection_state_number = true;
  opdata.protection_state_number = 99U;

  esphome::text_sensor::TextSensor protection_text{};

  MhiPublishTargets targets{};
  targets.protection_state_text_sensor = &protection_text;

  MhiPublishBridge bridge{};
  bridge.set_targets(targets);
  bridge.publish(state);

  EXPECT_EQ(protection_text.publish_count, 1U);
  EXPECT_TRUE(protection_text.state == "Unknown");
}

}  // namespace mhi_unit_tests

namespace mhi_unit_tests {


void publish_bridge_maps_mhi_auto_to_heat_cool_for_ha_setpoint_ui() {
  MhiStateStore state{};

  auto& status = state.status();
  status.valid = true;
  status.power = true;
  status.mode = 0U;
  status.fan = 7U;
  status.target_temp_c = 21.0f;
  status.room_temp_c = 20.0f;

  esphome::climate::Climate climate{};

  MhiPublishTargets targets{};
  targets.climate = &climate;

  MhiPublishBridge bridge{};
  bridge.set_targets(targets);
  bridge.publish(state);

  EXPECT_EQ(climate.publish_count, 1U);
  EXPECT_EQ(climate.mode, esphome::climate::CLIMATE_MODE_HEAT_COOL);
  expect_near(climate.target_temperature, 21.0f);
}

void publish_bridge_publishes_sensor_parity_slice3_vane_feedback() {
  MhiStateStore state{};

  auto& status = state.status();
  status.valid = true;
  status.power = true;
  status.mode = 2U;
  status.fan = 3U;
  status.target_temp_c = 22.0f;
  status.room_temp_c = 24.0f;
  status.vertical_vane = 3U;
  status.vanes_swing = false;
  status.has_horizontal_vane = true;
  status.horizontal_vane = 6U;
  status.horizontal_vane_swing = false;
  status.has_3d_auto = true;
  status.three_d_auto = true;

  esphome::select::Select horizontal{};
  esphome::binary_sensor::BinarySensor auto3d_binary{};
  esphome::switch_::Switch auto3d_switch{};

  MhiPublishTargets targets{};
  targets.horizontal_vanes_select = &horizontal;
  targets.vanes_3d_auto_enabled_binary_sensor = &auto3d_binary;
  targets.vanes_3d_auto_switch = &auto3d_switch;

  MhiPublishBridge bridge{};
  bridge.set_targets(targets);
  bridge.publish(state);

  EXPECT_EQ(horizontal.publish_count, 1U);
  EXPECT_EQ(auto3d_binary.publish_count, 1U);
  EXPECT_EQ(auto3d_switch.publish_count, 1U);

  EXPECT_TRUE(horizontal.state == "Wide");
  EXPECT_TRUE(auto3d_binary.state);
  EXPECT_TRUE(auto3d_switch.state);
}

}  // namespace mhi_unit_tests

namespace mhi_unit_tests {

void publish_bridge_suppresses_unchanged_sensor_republishes() {
  MhiStateStore state{};

  auto& opdata = state.opdata();
  opdata.valid = true;
  opdata.has_outdoor_temp = true;
  opdata.outdoor_temp_c = 19.50f;

  esphome::sensor::Sensor outdoor{};

  MhiPublishTargets targets{};
  targets.outdoor_temp_sensor = &outdoor;

  MhiPublishBridge bridge{};
  bridge.set_targets(targets);

  bridge.publish(state);
  bridge.publish(state);

  EXPECT_EQ(outdoor.publish_count, 1U);
  expect_near(outdoor.state, 19.50f);

  opdata.outdoor_temp_c = 19.51f;
  bridge.publish(state);

  EXPECT_EQ(outdoor.publish_count, 2U);
  expect_near(outdoor.state, 19.51f);
}

void publish_bridge_suppresses_alternating_climate_current_temperature_chatter() {
  MhiStateStore state{};

  auto& status = state.status();
  status.valid = true;
  status.power = false;
  status.mode = 0U;
  status.fan = 7U;
  status.target_temp_c = 19.0f;
  status.room_temp_c = 21.50f;
  status.last_update_ms = 1000U;

  esphome::climate::Climate climate{};

  MhiPublishTargets targets{};
  targets.climate = &climate;

  MhiPublishBridge bridge{};
  bridge.set_targets(targets);

  bridge.publish(state);
  EXPECT_EQ(climate.publish_count, 1U);
  expect_near(climate.current_temperature, 21.50f);

  status.room_temp_c = 21.25f;
  status.last_update_ms = 1200U;
  bridge.publish(state);
  EXPECT_EQ(climate.publish_count, 1U);
  expect_near(climate.current_temperature, 21.50f);

  status.room_temp_c = 21.50f;
  status.last_update_ms = 1400U;
  bridge.publish(state);
  EXPECT_EQ(climate.publish_count, 1U);
  expect_near(climate.current_temperature, 21.50f);

  status.room_temp_c = 21.25f;
  status.last_update_ms = 1600U;
  bridge.publish(state);
  EXPECT_EQ(climate.publish_count, 1U);
  expect_near(climate.current_temperature, 21.50f);
}

void publish_bridge_rate_limits_low_priority_climate_current_temperature_change() {
  MhiStateStore state{};

  auto& status = state.status();
  status.valid = true;
  status.power = false;
  status.mode = 0U;
  status.fan = 7U;
  status.target_temp_c = 19.0f;
  status.room_temp_c = 21.50f;
  status.last_update_ms = 1000U;

  esphome::climate::Climate climate{};

  MhiPublishTargets targets{};
  targets.climate = &climate;

  MhiPublishBridge bridge{};
  bridge.set_targets(targets);

  bridge.publish(state);
  EXPECT_EQ(climate.publish_count, 1U);

  // Adjacent 0.25 C current-temperature changes are low priority and should
  // not publish more than once per 15 seconds.
  status.room_temp_c = 21.25f;
  status.last_update_ms = 1200U;
  bridge.publish(state);
  EXPECT_EQ(climate.publish_count, 1U);
  expect_near(climate.current_temperature, 21.50f);

  status.room_temp_c = 21.25f;
  status.last_update_ms = 15999U;
  bridge.publish(state);
  EXPECT_EQ(climate.publish_count, 1U);
  expect_near(climate.current_temperature, 21.50f);

  status.room_temp_c = 21.25f;
  status.last_update_ms = 16000U;
  bridge.publish(state);
  EXPECT_EQ(climate.publish_count, 2U);
  expect_near(climate.current_temperature, 21.25f);
}


void publish_bridge_does_not_force_low_priority_current_temp_when_other_climate_fields_change() {
  MhiStateStore state{};

  auto& status = state.status();
  status.valid = true;
  status.power = true;
  status.mode = 2U;
  status.fan = 7U;
  status.target_temp_c = 19.0f;
  status.room_temp_c = 20.50f;
  status.last_update_ms = 1000U;

  esphome::climate::Climate climate{};

  MhiPublishTargets targets{};
  targets.climate = &climate;

  MhiPublishBridge bridge{};
  bridge.set_targets(targets);

  bridge.publish(state);
  EXPECT_EQ(climate.publish_count, 1U);
  expect_near(climate.current_temperature, 20.50f);

  // A non-temperature climate change should still publish immediately, but it
  // must not drag a low-priority 0.25 C current-temperature flicker with it.
  status.target_temp_c = 20.0f;
  status.room_temp_c = 20.25f;
  status.last_update_ms = 1200U;
  bridge.publish(state);

  EXPECT_EQ(climate.publish_count, 2U);
  expect_near(climate.target_temperature, 20.0f);
  expect_near(climate.current_temperature, 20.50f);

  status.room_temp_c = 20.25f;
  status.last_update_ms = 16000U;
  bridge.publish(state);

  EXPECT_EQ(climate.publish_count, 3U);
  expect_near(climate.current_temperature, 20.25f);
}

void publish_bridge_publishes_high_priority_climate_current_temperature_change_immediately() {
  MhiStateStore state{};

  auto& status = state.status();
  status.valid = true;
  status.power = false;
  status.mode = 0U;
  status.fan = 7U;
  status.target_temp_c = 19.0f;
  status.room_temp_c = 21.50f;
  status.last_update_ms = 1000U;

  esphome::climate::Climate climate{};

  MhiPublishTargets targets{};
  targets.climate = &climate;

  MhiPublishBridge bridge{};
  bridge.set_targets(targets);

  bridge.publish(state);
  EXPECT_EQ(climate.publish_count, 1U);

  status.room_temp_c = 22.00f;
  status.last_update_ms = 1200U;
  bridge.publish(state);
  EXPECT_EQ(climate.publish_count, 2U);
  expect_near(climate.current_temperature, 22.00f);
}


}  // namespace mhi_unit_tests


namespace mhi_unit_tests {

void publish_bridge_three_speed_maps_code_zero_to_low() {
  MhiStateStore state{};
  auto& status = state.status();
  status.valid = true;
  status.fan = 0U;

  esphome::climate::Climate climate{};
  esphome::select::Select fan{};
  MhiPublishTargets targets{};
  targets.climate = &climate;
  targets.fan_speed_select = &fan;

  MhiPublishBridge bridge{};
  bridge.set_fan_profile(MhiFanProfile::THREE_SPEED);
  bridge.set_targets(targets);
  bridge.publish(state);

  EXPECT_TRUE(climate.fan_mode.has_value());
  EXPECT_EQ(climate.fan_mode.value(), esphome::climate::CLIMATE_FAN_LOW);
  EXPECT_TRUE(fan.state == "Low");
}

void publish_bridge_four_speed_maps_code_zero_to_quiet() {
  MhiStateStore state{};
  auto& status = state.status();
  status.valid = true;
  status.fan = 0U;

  esphome::climate::Climate climate{};
  esphome::select::Select fan{};
  MhiPublishTargets targets{};
  targets.climate = &climate;
  targets.fan_speed_select = &fan;

  MhiPublishBridge bridge{};
  bridge.set_fan_profile(MhiFanProfile::FOUR_SPEED);
  bridge.set_targets(targets);
  bridge.publish(state);

  EXPECT_TRUE(climate.fan_mode.has_value());
  EXPECT_EQ(climate.fan_mode.value(), esphome::climate::CLIMATE_FAN_QUIET);
  EXPECT_TRUE(fan.state == "Quiet");
}

}  // namespace mhi_unit_tests
