#include "esphome/core/log.h"
#include "mhi_3d_auto_switch.h"

namespace esphome {
namespace mhi {

static const char *TAG = "mhi.switch";

void Mhi3dAutoSwitch::setup() {
    this->parent_->add_listener(this);
}

void Mhi3dAutoSwitch::write_state(bool state) {
    this->parent_->set_3Dauto(state);
    ESP_LOGD(TAG, "3d auto state written %d", state);
}

void Mhi3dAutoSwitch::dump_config(){
    ESP_LOGCONFIG(TAG, "3d Auto switch");
}

void Mhi3dAutoSwitch::update_status(ACStatus status, int value) {

    if (status == status_3Dauto) {
        switch (value) {
        case 0b00000000:
            this->publish_state(false);
            ESP_LOGD(TAG, "3d auto status updated: disabled");
            break;
        case 0b00000100:
            this->publish_state(true);
            ESP_LOGD(TAG, "3d auto status updated: enabled");
            break;
        }

    }
}

} //namespace mhi
} //namespace esphome