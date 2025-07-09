#include "esphome/core/log.h"
#include "mhi_fan_speed_select.h"

namespace esphome {
namespace mhi {

static const char *const TAG = "mhi.select";

void MhiFanSpeedSelect::setup() {
    this->parent_->add_listener(this);
}

void MhiFanSpeedSelect::dump_config(){
    ESP_LOGCONFIG(TAG, "MHI Fan Speed Select");
}

void MhiFanSpeedSelect::control(const std::string &value) {
    int fan_code = 7; // Default to Auto
    if (value == "Quiet") {
        fan_code = 0;
    } else if (value == "Low") {
        fan_code = 1;
    } else if (value == "Medium") {
        fan_code = 2;
    } else if (value == "High") {
        fan_code = 6;
    }
    
    this->parent_->set_fan(fan_code);
    this->publish_state(value);
}

void MhiFanSpeedSelect::update_status(ACStatus status, int value) {
    if (status == status_fan) {
        std::string fan_mode_str = "Auto";
        switch (value) {
            case 0: fan_mode_str = "Quiet"; break;
            case 1: fan_mode_str = "Low"; break;
            case 2: fan_mode_str = "Medium"; break;
            case 6: fan_mode_str = "High"; break;
            case 7: fan_mode_str = "Auto"; break;
        }
        this->publish_state(fan_mode_str);
        ESP_LOGD(TAG, "Fan speed status updated to: %s", fan_mode_str.c_str());
    }
}

}  // namespace mhi
}  // namespace esphome