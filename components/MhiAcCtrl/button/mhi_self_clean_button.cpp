#include "esphome/core/log.h"
#include "mhi_self_clean_button.h"

namespace esphome {
namespace mhi {

static const char *TAG = "mhi.button";

void MhiSelfCleanButton::press_action() {
  this->parent_->start_self_clean();
}

void MhiSelfCleanButton::dump_config() {
  ESP_LOGCONFIG(TAG, "MHI Self Clean button (emulated)");
}

}  // namespace mhi
}  // namespace esphome
