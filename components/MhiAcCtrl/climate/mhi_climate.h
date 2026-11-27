#pragma once

#include <cstdint>

#include "../mhi_ac_ctrl.h"
#include "../mhi_swing_restore.h"
#include "esphome/components/climate/climate.h"
#include "esphome/core/component.h"

namespace esphome {
namespace mhi_ac_ctrl {

class MhiClimate : public climate::Climate, public Component, public Parented<MhiAcCtrl> {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

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
  uint8_t mode_to_mhi_(climate::ClimateMode mode) const;
  bool fan_to_mhi_(climate::ClimateFanMode fan, uint8_t& out) const;

  float minimum_temperature_{18.0f};
  float maximum_temperature_{30.0f};
  float temperature_step_{1.0f};

  MhiSwingRestore swing_restore_{};
};

}  // namespace mhi_ac_ctrl
}  // namespace esphome