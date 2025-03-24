#include "esphome/core/log.h"
#include "mhi_horizontal_vanes_select.h"

namespace esphome {
namespace mhi {

static const char *const TAG = "mhi.select";

void MhiHorizontalVanesSelect::setup() {
  this->parent_->add_listener(this);
}

void MhiHorizontalVanesSelect::control(const std::string &value) {
  auto idx = this->index_of(value);
  if (idx.has_value() && idx.value() < 8) {
    
    this->parent_->set_vanesLR(idx.value() + 1);
  }
  this->publish_state(value);
}

void MhiHorizontalVanesSelect::update_status(ACStatus status, int value) {

  if (status == status_vanesLR) {
    optional<std::string> opt = this->at(value - 1);
    if (opt.has_value()) {
      this->publish_state(opt.value());
      ESP_LOGD(TAG, "Vanes status updated");
    } else {
      ESP_LOGW(TAG, "Failed to map vanes status, value %i", value);
    }

  }
}

}  // namespace mhi
}  // name