#pragma once

#include <cstdint>

#include "../mhi_ac_ctrl.h"
#include "esphome/components/climate/climate.h"
#include "esphome/core/component.h"

namespace esphome {
namespace mhi_ac_ctrl {

class MhiClimate : public climate::Climate, public Component, public Parented<MhiAcCtrl> {
 public:
  void setup() override;
  void dump_config() override;

  void set_temperature_offset_enabled(bool enabled) {
    this->temperature_offset_enabled_ = enabled;
  }

  void set_minimum_temperature(float temp) {
    this->minimum_temperature_ = temp;
  }

  void set_maximum_temperature(float temp) {
    this->maximum_temperature_ = temp;
  }

  void set_temperature_step(float step) {
    this->temperature_step_ = step;
  }

 protected:
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall& call) override;

 private:
  static constexpr float kAcMinimumSetpointC = 18.0f;

  uint8_t mode_to_mhi_(climate::ClimateMode mode) const;
  bool fan_to_mhi_(climate::ClimateFanMode fan, uint8_t& out) const;

  void apply_swing_command_(climate::ClimateSwingMode swing, MhiCommandState& patch);

  bool temperature_offset_enabled_{false};
  float temperature_offset_{0.0f};

  float minimum_temperature_{18.0f};
  float maximum_temperature_{30.0f};
  float temperature_step_{0.5f};

  // Used when switching swing off, so we return to a tangible vane position.
  uint8_t last_vertical_vane_position_{4};
  uint8_t last_horizontal_vane_position_{4};
};

}  // namespace mhi_ac_ctrl
}  // namespace esphome