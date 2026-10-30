#include "esphome/core/log.h"
#include "mhi_power_switch.h"

namespace esphome {
namespace mhi {

static const char *TAG = "mhi.switch";

void MhiPowerSwitch::setup() {
  this->parent_->add_listener(this);
}

void MhiPowerSwitch::write_state(bool state) {
  // Power-on restores the persisted operating settings in MhiPlatform.
  this->parent_->set_power(state ? power_on : power_off);
}

void MhiPowerSwitch::dump_config() {
  ESP_LOGCONFIG(TAG, "MHI Power switch (restores saved operating settings)");
}

void MhiPowerSwitch::update_status(ACStatus status, int value) {
  if (status == status_power)
    this->publish_state(value == power_on);
}

}  // namespace mhi
}  // namespace esphome
