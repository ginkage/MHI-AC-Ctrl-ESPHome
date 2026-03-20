#include "esphome/core/log.h"
#include "esphome/core/version.h"
#include "mhi_climate.h"

namespace esphome {
namespace mhi {

static const char* TAG = "mhi.climate";

void MhiClimate::setup() {
    this->power_ = power_off;
    this->current_temperature = NAN;
    auto restore = this->restore_state_();
    if (restore.has_value()) {
        restore->apply(this);
    } else {
        this->mode = climate::CLIMATE_MODE_OFF;
        this->target_temperature = 20;
        this->fan_mode = climate::CLIMATE_FAN_AUTO;
        this->swing_mode = climate::CLIMATE_SWING_OFF;
    }
    this->platform_ = this->parent_;
    this->platform_->add_listener(this);
}

void MhiClimate::dump_config() {
    ESP_LOGCONFIG(TAG, "MHI Climate", this);  
}

void MhiClimate::update_status(ACStatus status, int value) {
    if (this->power_ == power_off) {
        this->mode = climate::CLIMATE_MODE_OFF;
    }
    // Hier folgt die restliche Logik der Komponente...
    this->publish_state();
}

void MhiClimate::control(const climate::ClimateCall& call) {
    if (call.get_target_temperature().has_value()) {
        this->target_temperature = *call.get_target_temperature();
    }
    if (call.get_mode().has_value()) {
        this->mode = *call.get_mode();
    }
    this->publish_state();
}

climate::ClimateTraits MhiClimate::traits() {
    auto traits = climate::ClimateTraits();
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
    traits.set_supported_modes({
        climate::CLIMATE_MODE_OFF, 
        climate::CLIMATE_MODE_HEAT_COOL, 
        climate::CLIMATE_MODE_COOL, 
        climate::CLIMATE_MODE_HEAT, 
        climate::CLIMATE_MODE_DRY, 
        climate::CLIMATE_MODE_FAN_ONLY
    });
    traits.set_visual_min_temperature(this->minimum_temperature_);
    traits.set_visual_max_temperature(this->maximum_temperature_);
    traits.set_visual_temperature_step(this->temperature_step_);
    traits.set_supported_fan_modes({
        climate::CLIMATE_FAN_AUTO, 
        climate::CLIMATE_FAN_QUIET, 
        climate::CLIMATE_FAN_LOW, 
        climate::CLIMATE_FAN_MEDIUM, 
        climate::CLIMATE_FAN_HIGH
    });
    traits.set_supported_swing_modes({
        climate::CLIMATE_SWING_OFF, 
        climate::CLIMATE_SWING_BOTH, 
        climate::CLIMATE_SWING_VERTICAL, 
        climate::CLIMATE_SWING_HORIZONTAL
    });
    return traits;
}

} //namespace mhi
} //namespace esphome
