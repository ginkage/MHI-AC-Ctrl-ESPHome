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
        this->target_temperature = roundf(clamp(
            this->current_temperature, this->minimum_temperature_, this->maximum_temperature_));
        this->fan_mode = climate::CLIMATE_FAN_AUTO;
        this->swing_mode = climate::CLIMATE_SWING_OFF;
    }
    if (isnan(this->target_temperature))
        this->target_temperature = 20;

    this->vanesLR_pos_old_state_ = 4;
    this->vanes_pos_old_state_ = 4;

    this->platform_ = this->parent_;
    this->platform_->add_listener(this);
}

void MhiClimate::dump_config() {
    ESP_LOGCONFIG(TAG, "MHI Climate", this);  
}

void MhiClimate::update_status(ACStatus status, int value) {
    static int mode_tmp = 0xff;

    if (this->power_ == power_off) {
        this->mode = climate::CLIMATE_MODE_OFF;
        this->publish_state();
    }

    switch (status) {
    case status_power:
        if (value == power_on) {
            this->power_ = power_on;
            update_status(status_mode, mode_tmp);
        } else {
            this->power_ = power_off;
            this->mode = climate::CLIMATE_MODE_OFF;
            this->publish_state();
        }
        break;
    case status_mode:
        mode_tmp = value;
    case opdata_mode:
    case erropdata_mode:
        switch (value) {
        case mode_auto:          
            if (status != erropdata_mode && this->power_ > 0) {
                this->mode = climate::CLIMATE_MODE_HEAT_COOL;
            } else {
                this->mode = climate::CLIMATE_MODE_OFF;
            }
            break;
        case mode_dry:
            this->mode = climate::CLIMATE_MODE_DRY;
            break;
        case mode_cool:
            this->mode = climate::CLIMATE_MODE_COOL;
            this->platform_->set_vanes(vanes_1); // SMART LOGIC: Coanda (Oben)
            break;
        case mode_fan:
            this->mode = climate::CLIMATE_MODE_FAN_ONLY;
            break;
        case mode_heat:
            this->mode = climate::CLIMATE_MODE_HEAT;
            this->platform_->set_vanes(vanes_4); // SMART LOGIC: Konvektion (Unten)
            break;
        default:
            ESP_LOGD(TAG, "unknown status mode value %i", value);
        }
        this->publish_state();
        break;
    case status_fan:
        switch (value) {
        case 0: this->fan_mode = climate::CLIMATE_FAN_QUIET; break;
        case 1: this->fan_mode = climate::CLIMATE_FAN_LOW; break;
        case 2: this->fan_mode = climate::CLIMATE_FAN_MEDIUM; break;
        case 6: this->fan_mode = climate::CLIMATE_FAN_HIGH; break;
        case 7: this->fan_mode = climate::CLIMATE_FAN_AUTO; break;
        }
        this->publish_state();
        break;
    case status_vanes:
        if (this->vanesLR_pos_state_ == vanesLR_swing) {
            switch (value) {
                case vanes_unknown: case vanes_1: case vanes_2: case vanes_3: case vanes_4:
                    this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
                    this->vanes_pos_old_state_ = value;
                    break;
                case vanes_swing:
                    this->swing_mode = climate::CLIMATE_SWING_BOTH;
                    break;
            }
        } else {
            switch (value) {
                case vanes_unknown: case vanes_1: case vanes_2: case vanes_3: case vanes_4:
                    this->swing_mode = climate::CLIMATE_SWING_OFF;
                    this->vanes_pos_old_state_ = value;
                    break;
                case vanes_swing:
                    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
                    break;
            }
        }
        this->vanes_pos_state_ = value;
        this->publish_state();
        break;
    case status_vanesLR:
        if (this->vanes_pos_state_ == vanes_swing) {
            switch (value) {
                case vanesLR_1: case vanesLR_2: case vanesLR_3: case vanesLR_4: case vanesLR_5: case vanesLR_6: case vanesLR_7:
                    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
                    this->vanesLR_pos_old_state_ = value;
                    break;
                case vanesLR_swing:
                    this->swing_mode = climate::CLIMATE_SWING_BOTH;
                    break;
            }
        } else {
            switch (value) {
                case vanesLR_1: case vanesLR_2: case vanesLR_3: case vanesLR_4: case vanesLR_5: case vanesLR_6: case vanesLR_7:
                    this->swing_mode = climate::CLIMATE_SWING_OFF;
                    this->vanesLR_pos_old_state_ = value;
                    break;
                case vanesLR_swing:
                    this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
                    break;
            }
        }
        this->publish_state();
        this->vanesLR_pos_state_ = value;
        break;
    case status_troom:
        this->current_temperature = ((value - 61) / 4.0) - this->temperature_offset_;
        this->publish_state();
        break;
    case status_tsetpoint: {
        float ac_setpoint = (value & 0x7f) / 2.0;
        if (this->temperature_offset_ != 0.0 && 
            fabs(ac_setpoint - (this->target_temperature + this->temperature_offset_)) < 0.1) {
            break;
        }
        this->target_temperature = ac_setpoint;
        this->temperature_offset_ = 0.0;
        this->publish_state();
        break;
    }
    default:
        break;
    }
}

void MhiClimate::control(const climate::ClimateCall& call) {
    if (call.get_target_temperature().has_value()) {
        float target_temp = *call.get_target_temperature();
        this->target_temperature = target_temp; 
        const float ac_unit_min_temp = 18.0f; 
        float setpoint = ceil(target_temp);
        if (setpoint < ac_unit_min_temp)
            setpoint = ac_unit_min_temp;
        float offset = 0.0;
        if (this->temperature_offset_enabled_) {
            offset = setpoint - target_temp;
        }
        this->platform_->set_offset(offset);
        this->temperature_offset_ = offset;
        this->platform_->set_tsetpoint(setpoint);
    }

    if (call.get_mode().has_value()) {
        this->mode = *call.get_mode();
        this->power_ = power_on;
        switch (this->mode) {
        case climate::CLIMATE_MODE_OFF:
            power_ = power_off;
            break;
        case climate::CLIMATE_MODE_COOL:
            mode_ = mode_cool;
            this->platform_->set_vanes(vanes_1); // SMART LOGIC: Coanda (Oben)
            break;
        case climate::CLIMATE_MODE_HEAT:
            mode_ = mode_heat;
            this->platform_->set_vanes(vanes_4); // SMART LOGIC: Konvektion (Unten)
            break;
        case climate::CLIMATE_MODE_DRY:
            mode_ = mode_dry;
            break;
        case climate::CLIMATE_MODE_FAN_ONLY:
            mode_ = mode_fan;
            break;
        case climate::CLIMATE_MODE_HEAT_COOL:
        default:
            mode_ = mode_auto;
            break;
        }
        this->platform_->set_power(power_);
        this->platform_->set_mode(mode_);
    }

    if (call.get_fan_mode().has_value()) {
        this->fan_mode = *call.get_fan_mode();
        switch (*this->fan_mode) {
        case climate::CLIMATE_FAN_QUIET: fan_ = 0; break;
        case climate::CLIMATE_FAN_LOW: fan_ = 1; break;
        case climate::CLIMATE_FAN_MEDIUM: fan_ = 2; break;
        case climate::CLIMATE_FAN_HIGH: fan_ = 6; break;
        case climate::CLIMATE_FAN_AUTO:
        default: fan_ = 7; break;
        }
        this->platform_->set_fan(fan_);
    }

    if (call.get_swing_mode().has_value()) {
        this->swing_mode = *call.get_swing_mode();
        vanesLR_ = static_cast<ACVanesLR>(this->vanesLR_pos_old_state_);
        vanes_ = static_cast<ACVanes>(this->vanes_pos_old_state_);

        switch (this->swing_mode) {
        case climate::CLIMATE_SWING_OFF:
            break;
        case climate::CLIMATE_SWING_VERTICAL:
            vanes_ = vanes_swing;
            break;
        case climate::CLIMATE_SWING_HORIZONTAL:
            vanesLR_ = vanesLR_swing;
            break;
        default:
        case climate::CLIMATE_SWING_BOTH:
            vanesLR_ = vanesLR_swing;
            vanes_ = vanes_swing;
            break;
        }
        this->platform_->set_vanesLR(vanesLR_); 
        this->platform_->set_vanes(vanes_); 
    }

    this->publish_state();
}

/// Return the traits of this controller.
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2025, 11, 0)
climate::ClimateTraits MhiClimate::traits() {
    auto traits = climate::ClimateTraits();
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
    traits.set_supported_modes({ climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_HEAT_COOL, climate::CLIMATE_MODE_COOL, climate::CLIMATE_MODE_HEAT, climate::CLIMATE_MODE_DRY, climate::CLIMATE_MODE_FAN_ONLY });
    traits.set_visual_min_temperature(this->minimum_temperature_);
    traits.set_visual_max_temperature(this->maximum_temperature_);
    traits.set_visual_temperature_step(this->temperature_step_);
    traits.set_supported_fan_modes({ climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_QUIET, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH });
    traits.set_supported_swing_modes({ climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_BOTH, climate::CLIMATE_SWING_VERTICAL, climate::CLIMATE_SWING_HORIZONTAL });
    return traits;
}
#else
climate::ClimateTraits MhiClimate::traits() {
    auto traits = climate::ClimateTraits();
    traits.set_supports_current_temperature(true);
    traits.set_supported_modes({ climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_HEAT_COOL, climate::CLIMATE_MODE_COOL, climate::CLIMATE_MODE_HEAT, climate::CLIMATE_MODE_DRY, climate::CLIMATE_MODE_FAN_ONLY });
    traits.set_supports_two_point_target_temperature(false);
    traits.set_visual_min_temperature(this->minimum_temperature_);
    traits.set_visual_max_temperature(this->maximum_temperature_);
    traits.set_visual_temperature_step(this->temperature_step_);
    traits.set_supported_fan_modes({ climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_QUIET, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH });
    traits.set_supported_swing_modes({ climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_BOTH, climate::CLIMATE_SWING_VERTICAL, climate::CLIMATE_SWING_HORIZONTAL });
    return traits;
}
#endif

} //namespace mhi
} //namespace esphome
