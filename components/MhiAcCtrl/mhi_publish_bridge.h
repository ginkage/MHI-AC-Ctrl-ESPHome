#pragma once

#include <cmath>
#include <cstdint>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "mhi_fan_profile.h"
#include "mhi_state.h"

namespace esphome {
namespace mhi_ac_ctrl {

struct MhiPublishTargets {
  climate::Climate* climate{nullptr};

  binary_sensor::BinarySensor* power_binary_sensor{nullptr};

  sensor::Sensor* room_temp_sensor{nullptr};
  sensor::Sensor* target_temp_sensor{nullptr};
  sensor::Sensor* outdoor_temp_sensor{nullptr};
  sensor::Sensor* return_air_sensor{nullptr};
  sensor::Sensor* compressor_frequency_sensor{nullptr};
  sensor::Sensor* current_sensor{nullptr};
  sensor::Sensor* indoor_unit_fan_speed_sensor{nullptr};
  sensor::Sensor* outdoor_unit_fan_speed_sensor{nullptr};
  sensor::Sensor* indoor_unit_total_run_time_sensor{nullptr};
  sensor::Sensor* compressor_total_run_time_sensor{nullptr};
  sensor::Sensor* energy_used_sensor{nullptr};
  sensor::Sensor* indoor_unit_thi_r1_sensor{nullptr};
  sensor::Sensor* indoor_unit_thi_r2_sensor{nullptr};
  sensor::Sensor* indoor_unit_thi_r3_sensor{nullptr};
  sensor::Sensor* outdoor_unit_tho_r1_sensor{nullptr};
  sensor::Sensor* outdoor_unit_expansion_valve_sensor{nullptr};
  sensor::Sensor* outdoor_unit_discharge_pipe_sensor{nullptr};
  sensor::Sensor* outdoor_unit_discharge_pipe_super_heat_sensor{nullptr};
  sensor::Sensor* protection_state_number_sensor{nullptr};

  binary_sensor::BinarySensor* defrost_binary_sensor{nullptr};
  binary_sensor::BinarySensor* vanes_3d_auto_enabled_binary_sensor{nullptr};

  switch_::Switch* vanes_3d_auto_switch{nullptr};

  text_sensor::TextSensor* error_code_text_sensor{nullptr};
  text_sensor::TextSensor* protection_state_text_sensor{nullptr};

  select::Select* vertical_vanes_select{nullptr};
  select::Select* horizontal_vanes_select{nullptr};
  select::Select* fan_speed_select{nullptr};
};

class MhiPublishBridge {
 public:
  void set_targets(const MhiPublishTargets& targets) {
    targets_ = targets;

    // Targets are registered by ESPHome component setup, which can happen after
    // the parent has already decoded stable frames. Force the next decoded
    // state to be republished so newly registered entities do not stay at NA
    // when the AC state has not changed.
    this->reset_publish_cache_();
  }

  void set_fan_profile(MhiFanProfile profile) {
    if (this->fan_profile_ != profile) {
      this->fan_profile_ = profile;
      this->reset_publish_cache_();
    }
  }

  void publish(const MhiStateStore& state);

 private:
  static climate::ClimateMode map_climate_mode(bool power, uint8_t mode);
  climate::ClimateFanMode map_climate_fan(uint8_t fan) const;
  static climate::ClimateSwingMode map_climate_swing(const MhiStatusState& status);
  static const char* map_vertical_vane_option(const MhiStatusState& status);
  static const char* map_horizontal_vane_option(const MhiStatusState& status);
  const char* map_fan_option(uint8_t fan) const;
  static const char* map_protection_state(uint8_t state);
  static bool float_changed(float old_value, float new_value);

  void publish_status(const MhiStatusState& status);
  void publish_opdata(const MhiOpDataState& opdata);

  bool should_publish_climate_current_temperature_(float decoded_temp, bool first_publish, uint32_t now_ms);
  void reset_publish_cache_();

  MhiPublishTargets targets_{};
  MhiFanProfile fan_profile_{MhiFanProfile::FOUR_SPEED};

  bool has_last_status_{false};
  bool has_last_opdata_{false};

  MhiStatusState last_status_{};
  MhiOpDataState last_opdata_{};

  bool has_climate_published_current_temp_{false};
  float climate_published_current_temp_{0.0f};
  uint32_t last_climate_current_temp_publish_ms_{0};
};

}  // namespace mhi_ac_ctrl
}  // namespace esphome