#include "mhi_vertical_vanes_select.h"

#include "esphome/core/log.h"

namespace esphome {
namespace mhi_ac_ctrl {

static const char* const TAG = "mhi.select.vertical_vanes";

void MhiVerticalVanesSelect::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MHI vertical vanes select");

  if (this->parent_ != nullptr) {
    this->parent_->set_vertical_vanes_select(this);
  }
}

void MhiVerticalVanesSelect::dump_config() {
  ESP_LOGCONFIG(TAG, "MHI Vertical Vanes Select");
}

void MhiVerticalVanesSelect::control(const std::string& value) {
  if (this->parent_ == nullptr) {
    ESP_LOGW(TAG, "Ignoring vertical vanes command because parent is not set");
    return;
  }

  auto index = this->index_of(value);
  if (!index.has_value()) {
    ESP_LOGW(TAG, "Unknown vertical vanes option: %s", value.c_str());
    return;
  }

  this->parent_->request_vertical_vane_command(static_cast<uint8_t>(index.value() + 1U));

  ESP_LOGD(TAG, "Vertical vanes command staged: %s; waiting for confirmed MOSI state", value.c_str());
}

}  // namespace mhi_ac_ctrl
}  // namespace esphome