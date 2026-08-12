#include "esphome/core/log.h"
#include "mhi_vertical_vanes_select.h"

namespace esphome {
namespace mhi {

static const char *const TAG = "mhi.select";

void MhiVerticalVanesSelect::setup() {
    this->parent_->add_listener(this);
}

void MhiVerticalVanesSelect::dump_config(){
    ESP_LOGCONFIG(TAG, "MHI Vertical Vanes Select");
    ESP_LOGCONFIG(TAG, "  state: %d", this->current_option());
}

void MhiVerticalVanesSelect::control(const std::string &value) {
    auto idx = this->index_of(value);
    // Limit auf < 7 erhöht, da wir jetzt 7 Optionen haben (Index 0 bis 6)
    if (idx.has_value() && idx.value() < 7) {
        // Das "+ 1" wurde entfernt. Index 0 (Stop) sendet jetzt auch den Befehl 0.
        this->parent_->set_vanes(idx.value());
    }
    this->publish_state(value);
}

void MhiVerticalVanesSelect::update_status(ACStatus status, int value) {
    if (status == status_vanes) {
        // Das "- 1" wurde entfernt. Wenn die Anlage Status 0 meldet, wird auch Index 0 (Stop) ausgewählt.
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
