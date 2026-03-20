#include "esphome/core/log.h"
#include "mhi_horizontal_vanes_select.h"

namespace esphome {
namespace mhi {

static const char *const TAG = "mhi.select";

void MhiHorizontalVanesSelect::setup() {
    this->parent_->add_listener(this);
}

void MhiHorizontalVanesSelect::dump_config(){
    ESP_LOGCONFIG(TAG, "MHI Horizontal Vanes Select");
    ESP_LOGCONFIG(TAG, "  state: %d", this->current_option());
}

void MhiHorizontalVanesSelect::control(const std::string &value) {
    auto idx = this->index_of(value);
    // Erhöht auf < 9, da du jetzt 9 Optionen hast (0 bis 8)
    if (idx.has_value() && idx.value() < 9) {
        // Die "+ 1" Rechnung ist weg! Index 0 (Stop) sendet jetzt auch direkt die 0.
        this->parent_->set_vanesLR(idx.value());
    }
    this->publish_state(value);
}

void MhiHorizontalVanesSelect::update_status(ACStatus status, int value) {
    if (status == status_vanesLR) {
        // Die "- 1" Rechnung ist weg! Empfängt der ESP die 0, wählt er auch Index 0 in der App.
        optional<std::string> opt = this->at(value);
        if (opt.has_value()) {
            this->publish_state(opt.value());
            ESP_LOGD(TAG, "Vanes status updated");
        } else {
            ESP_LOGW(TAG, "Failed to map vanes status, value %i", value);
        }
    }
}

}  // namespace mhi
}  // namespace esphome
