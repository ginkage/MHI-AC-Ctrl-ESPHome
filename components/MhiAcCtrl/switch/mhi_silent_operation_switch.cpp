#include "esphome/core/log.h"
#include "mhi_silent_operation_switch.h"

namespace esphome {
namespace mhi {

static const char *TAG = "mhi.switch";

void MhiSilentOperationSwitch::setup() {
  this->parent_->add_listener(this);
}

void MhiSilentOperationSwitch::write_state(bool state) {
  this->parent_->set_silent_operation(state);
  this->publish_state(state);
}

void MhiSilentOperationSwitch::dump_config() {
  ESP_LOGCONFIG(TAG, "MHI Silent Operation switch");
}

void MhiSilentOperationSwitch::update_status(ACStatus status, int value) {
  if (status == opdata_silent_operation)
    this->publish_state(value != 0);
}

}  // namespace mhi
}  // namespace esphome
