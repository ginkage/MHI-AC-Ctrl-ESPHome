#include "esphome/core/log.h"
#include "mhi_climate.h"

namespace esphome {
namespace mhi {

static const char* TAG = "mhi.climate";

void MhiClimate::setup() {
    this->power_ = power_off;
    this->current_temperature = NAN;
    // restore set points
    auto restore = this->restore_state_();
    if (restore.has_value()) {
        restore->apply(this);
    } else {
        // restore from defaults
        this->mode = climate::CLIMATE_MODE_OFF;
        // initialize target temperature to some value so that it's not NAN
        this->target_temperature = roundf(clamp(
            this->current_temperature, this->minimum_temperature_, this->maximum_temperature_));
        this->fan_mode = climate::CLIMATE_FAN_AUTO;
        this->swing_mode = climate::CLIMATE_SWING_OFF;
    }
    // Never send nan to HA
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

    
    ESP_LOGD(TAG, "received status=%i value=%i power=%i", status, value, this->power_);

    if (this->power_ == power_off) {
        // Workaround for status after reboot
        this->mode = climate::CLIMATE_MODE_OFF;
        this->publish_state();
    }

    switch (status) {
    case status_power:
        if (value == power_on) {
            this->power_ = power_on;
            // output_P(status, (TOPIC_POWER), PSTR(PAYLOAD_POWER_ON));
            update_status(status_mode, mode_tmp);
        } else {
            // output_P(status, (TOPIC_POWER), (PAYLOAD_POWER_OFF));
            // output_P(status, PSTR(TOPIC_MODE), PSTR(PAYLOAD_MODE_OFF));
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
            // output_P(status, PSTR(TOPIC_MODE), PSTR(PAYLOAD_MODE_DRY));
            this->mode = climate::CLIMATE_MODE_DRY;
            break;
        case mode_cool:
            // output_P(status, PSTR(TOPIC_MODE), PSTR(PAYLOAD_MODE_COOL));
            this->mode = climate::CLIMATE_MODE_COOL;
            break;
        case mode_fan:
            // output_P(status, PSTR(TOPIC_MODE), PSTR(PAYLOAD_MODE_FAN));
            this->mode = climate::CLIMATE_MODE_FAN_ONLY;
            break;
        case mode_heat:
            // output_P(status, PSTR(TOPIC_MODE), PSTR(PAYLOAD_MODE_HEAT));
            this->mode = climate::CLIMATE_MODE_HEAT;
            break;
        default:
            ESP_LOGD(TAG, "unknown status mode value %i", value);
        }
        this->publish_state();
        break;
    case status_fan:
        switch (value) {
        case 0:
            this->fan_mode = climate::CLIMATE_FAN_QUIET;
            break;
        case 1:
            this->fan_mode = climate::CLIMATE_FAN_LOW;
            break;
        case 2:
            this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
            break;
        case 6:
            this->fan_mode = climate::CLIMATE_FAN_HIGH;
            break;
        case 7:
            this->fan_mode = climate::CLIMATE_FAN_AUTO;
            break;
        }
        this->publish_state();
        break;
    case status_vanes:
        // Vanes Up Down, also known as Vertical
        if (this->vanesLR_pos_state_ == vanesLR_swing) {
            switch (value) {
                case vanes_unknown:
                case vanes_1:
                case vanes_2:
                case vanes_3:
                case vanes_4:
                    this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
                    this->vanes_pos_old_state_ = value;
                    break;
                case vanes_swing:
                    this->swing_mode = climate::CLIMATE_SWING_BOTH;
                    break;
            }
        }
        else {
            switch (value) {
                case vanes_unknown:
                case vanes_1:
                case vanes_2:
                case vanes_3:
                case vanes_4:
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
                case vanesLR_1:
                case vanesLR_2:
                case vanesLR_3:
                case vanesLR_4:
                case vanesLR_5:
                case vanesLR_6:
                case vanesLR_7:
                    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
                    this->vanesLR_pos_old_state_ = value;
                    break;
                case vanesLR_swing:
                    this->swing_mode = climate::CLIMATE_SWING_BOTH;
                    break;
            }
        }
        else {
            switch (value) {
                case vanesLR_1:
                case vanesLR_2:
                case vanesLR_3:
                case vanesLR_4:
                case vanesLR_5:
                case vanesLR_6:
                case vanesLR_7:
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
        // Calculate the temperature and apply the offset for 0.5°C steps
        if (!this->platform_->get_room_temp_api_active()) {   // add offset only if not using an external value, as this is done there already
            this->current_temperature = ((value - 61) / 4.0) - this->temperature_offset_;
        } else {
            // offset is added when sending external sensor value to unit, no need to change the current_temperature value
            this->current_temperature = ((value - 61) / 4.0) + this->temperature_offset_;
        }
        
        this->publish_state();
        break;

    case status_tsetpoint: {
        float ac_setpoint = (value & 0x7f) / 2.0;

        // If we are using an offset, check if the AC's update is just an
        // echo of the command we already sent (our target + our offset).
        if (this->temperature_offset_ != 0.0 && 
            fabs(ac_setpoint - (this->target_temperature + this->temperature_offset_)) < 0.1) {
            
            // This is an expected echo. Do nothing to prevent overwriting our state.
            ESP_LOGD(TAG, "Ignoring tsetpoint echo from AC: %.1f", ac_setpoint);
            break;
        }

        // If it's not an echo, it's a new value from the remote.
        // Update the target temperature and reset the offset.
        ESP_LOGI(TAG, "Remote setpoint change detected. Updating target to %.1f", ac_setpoint);
        this->target_temperature = ac_setpoint;
        this->temperature_offset_ = 0.0;
        this->publish_state();
        break;
    }
    default:
        // skip these values as they are not used currently
        break;
    }
}

void MhiClimate::control(const climate::ClimateCall& call) {
    if (call.get_target_temperature().has_value()) {
        float target_temp = *call.get_target_temperature();
        this->target_temperature = target_temp; // Store the user's desired temp

        ESP_LOGD(TAG, "MhiClimate::control - get_target_temperature - New target_temperature: %.1f°C", target_temp);

        const float ac_unit_min_temp = 18.0f; // Hardware minimum for the AC unit

        // Scenario 1: User wants a temperature below the AC's hardware limit
        if (target_temp < ac_unit_min_temp) {
            ESP_LOGD(TAG, "Using low-temp workaround for target: %.1f°C", target_temp);
            this->platform_->set_tsetpoint(ac_unit_min_temp); // Send the lowest possible temp to the AC
            this->temperature_offset_ = ac_unit_min_temp - target_temp; // Set offset (e.g., 18 - 17 = 1.0)
        
        // Scenario 2: Handle 0.5°C steps (if enabled)
        } else if (this->temperature_offset_enabled_) {
            float fractional_part = target_temp - floor(target_temp);
            if (fractional_part >= 0.5) {
                this->platform_->set_offset(0.5); // set offset value to mhi_platform as well, we use it when sending external sensor values
                this->temperature_offset_ = 0.5; // Offset return temp by -0.5
                this->platform_->set_tsetpoint(ceil(target_temp)); // Set AC to x+1
                ESP_LOGD(TAG, "MhiClimate::control - get_target_temperature - set_tsetpoint %f, set_offset 0.5", ceil(target_temp));
            } else {
                this->platform_->set_offset(0.0);  // set offset value to mhi_platform as well, we use it when sending external sensor values
                this->temperature_offset_ = 0.0;
                this->platform_->set_tsetpoint(target_temp); // Set normally
                ESP_LOGD(TAG, "MhiClimate::control - get_target_temperature - set_tsetpoint %f, set_offset 0.0", target_temp);
            }

        // Scenario 3: Normal operation, no offsets needed
        } else {
            this->platform_->set_offset(0.0);  // set offset value to mhi_platform as well, we use it when sending external sensor values
            this->temperature_offset_ = 0.0;
            this->platform_->set_tsetpoint(target_temp);
            ESP_LOGD(TAG, "MhiClimate::control - get_target_temperature - set_tsetpoint %f, set_offset 0.0", target_temp);
        }
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
            break;
        case climate::CLIMATE_MODE_HEAT:
            mode_ = mode_heat;
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
        case climate::CLIMATE_FAN_QUIET:
            fan_ = 0;
            break;
        case climate::CLIMATE_FAN_LOW:
            fan_ = 1;
            break;
        case climate::CLIMATE_FAN_MEDIUM:
            fan_ = 2;
            break;
        case climate::CLIMATE_FAN_HIGH:
            fan_ = 6;
            break;
        case climate::CLIMATE_FAN_AUTO:
        default:
            fan_ = 7;
            break;
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
            // vanes_ = vanes_swing;
            vanesLR_ = vanesLR_swing;
            vanes_ = vanes_swing;
            break;
        }
        this->platform_->set_vanesLR(vanesLR_); // Set vanesLR to swing
        this->platform_->set_vanes(vanes_); // Set vanes to swing
    }

    this->publish_state();
}


/// Return the traits of this controller.
climate::ClimateTraits MhiClimate::traits() {
    auto traits = climate::ClimateTraits();
    traits.set_supports_current_temperature(true);
    traits.set_supported_modes({ CLIMATE_MODE_OFF, CLIMATE_MODE_HEAT_COOL, CLIMATE_MODE_COOL, CLIMATE_MODE_HEAT, CLIMATE_MODE_DRY, CLIMATE_MODE_FAN_ONLY });
    traits.set_supports_two_point_target_temperature(false);
    traits.set_visual_min_temperature(this->minimum_temperature_);
    traits.set_visual_max_temperature(this->maximum_temperature_);
    traits.set_visual_temperature_step(this->temperature_step_);
    traits.set_supported_fan_modes({ CLIMATE_FAN_AUTO, CLIMATE_FAN_QUIET, CLIMATE_FAN_LOW, CLIMATE_FAN_MEDIUM, CLIMATE_FAN_HIGH });
    traits.set_supported_swing_modes({ CLIMATE_SWING_OFF, CLIMATE_SWING_BOTH, CLIMATE_SWING_VERTICAL, CLIMATE_SWING_HORIZONTAL });
    return traits;
}


} //namespace mhi
} //namespace esphome