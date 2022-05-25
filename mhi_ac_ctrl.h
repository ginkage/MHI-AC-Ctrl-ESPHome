#include "MHI-AC-Ctrl-core.h"
#define ROOM_TEMP_MQTT 1

static const char* TAG = "mhi_ac_ctrl";

unsigned long room_temp_api_timeout_ms = millis();

class MhiAcCtrl : public climate::Climate,
                  public Component,
                  public CallbackInterface_Status {
public:
    void setup() override
    {
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

        error_code_.set_icon("mdi:alert-circle");

        outdoor_temperature_.set_icon("mdi:thermometer");
        outdoor_temperature_.set_unit_of_measurement("°C");
        outdoor_temperature_.set_accuracy_decimals(2);

        return_air_temperature_.set_icon("mdi:thermometer");
        return_air_temperature_.set_unit_of_measurement("°C");
        return_air_temperature_.set_accuracy_decimals(2);

        outdoor_unit_fan_speed_.set_icon("mdi:fan");

        indoor_unit_fan_speed_.set_icon("mdi:fan");

        compressor_frequency_.set_icon("mdi:sine-wave");
        compressor_frequency_.set_unit_of_measurement("Hz");
        compressor_frequency_.set_accuracy_decimals(1);

        indoor_unit_total_run_time_.set_icon("mdi:clock");
        indoor_unit_total_run_time_.set_unit_of_measurement("h");

        compressor_total_run_time_.set_icon("mdi:clock");
        compressor_total_run_time_.set_unit_of_measurement("h");

        current_power_.set_icon("mdi:current-ac");
        current_power_.set_unit_of_measurement("A");
        current_power_.set_accuracy_decimals(2);

        defrost_.set_icon("mdi:snowflake-melt");

        mhi_ac_ctrl_core.MHIAcCtrlStatus(this);
        mhi_ac_ctrl_core.init();
    }

    void loop() override
    {
        if(millis() - room_temp_api_timeout_ms >= id(room_temp_api_timeout)*1000) {
            mhi_ac_ctrl_core.set_troom(0xff);  // use IU temperature sensor
            room_temp_api_timeout_ms = millis();
            ESP_LOGD("mhi_ac_ctrl", "did not receive a room_temp_api value, using IU temperature sensor");
        }

        int ret = mhi_ac_ctrl_core.loop(100);
        if (ret < 0)
            ESP_LOGW("mhi_ac_ctrl", "mhi_ac_ctrl_core.loop error: %i", ret);
    }

    void dump_config() override
    {
        LOG_CLIMATE("", "MHI-AC-Ctrl Climate", this);
        ESP_LOGCONFIG(TAG, "  Min. Temperature: %.1f°C", this->minimum_temperature_);
        ESP_LOGCONFIG(TAG, "  Max. Temperature: %.1f°C", this->maximum_temperature_);
        ESP_LOGCONFIG(TAG, "  Supports HEAT: %s", YESNO(true));
        ESP_LOGCONFIG(TAG, "  Supports COOL: %s", YESNO(true));
    }

    void cbiStatusFunction(ACStatus status, int value) override
    {
        static int mode_tmp = 0xff;
        ESP_LOGD("mhi_ac_ctrl", "received status=%i value=%i power=%i", status, value, this->power_);
        switch (status) {
        case status_fsck:
            // itoa(value, strtmp, 10);
            // output_P(status, PSTR(TOPIC_FSCK), strtmp);
            break;
        case status_fmosi:
            // itoa(value, strtmp, 10);
            // output_P(status, PSTR(TOPIC_FMOSI), strtmp);
            break;
        case status_fmiso:
            // itoa(value, strtmp, 10);
            // output_P(status, PSTR(TOPIC_FMISO), strtmp);
            break;
        case status_power:
            if (value == power_on) {
                this->power_ = power_on;
                // output_P(status, (TOPIC_POWER), PSTR(PAYLOAD_POWER_ON));
                cbiStatusFunction(status_mode, mode_tmp);
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
                // if (status != erropdata_mode)
                //    output_P(status, PSTR(TOPIC_MODE), PSTR(PAYLOAD_MODE_AUTO));
                // else
                //    output_P(status, PSTR(TOPIC_MODE), PSTR(PAYLOAD_MODE_STOP));
                //    break;
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
                ESP_LOGD("mhi_ac_ctrl", "unknown status mode value %i", value);
            }
            this->publish_state();
            break;
        case status_fan:
            // itoa(value + 1, strtmp, 10);
            // output_P(status, TOPIC_FAN, strtmp);
            switch (value) {
            case 0:
                this->fan_mode = climate::CLIMATE_FAN_LOW;
                break;
            case 1:
                this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
                break;
            case 2:
                this->fan_mode = climate::CLIMATE_FAN_HIGH;
                break;
            case 3:
                this->fan_mode = climate::CLIMATE_FAN_AUTO;
                break;
            }
            this->publish_state();
            break;
        case status_vanes:
            switch (value) {
            case vanes_swing:
                // output_P(status, PSTR(TOPIC_VANES), PSTR(PAYLOAD_VANES_SWING));
                this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
                break;
            default:
                // itoa(value, strtmp, 10);
                // output_P(status, PSTR(TOPIC_VANES), strtmp);
                this->swing_mode = climate::CLIMATE_SWING_OFF;
            }
            this->publish_state();
            break;
        case status_troom:
            // dtostrf((value - 61) / 4.0, 0, 2, strtmp);
            // output_P(status, PSTR(TOPIC_TROOM), strtmp);
            this->current_temperature = (value - 61) / 4.0;
            this->publish_state();
            break;
        case status_tsetpoint:
        case opdata_tsetpoint:
        case erropdata_tsetpoint:
            // itoa(value, strtmp, 10);
            // output_P(status, PSTR(TOPIC_TSETPOINT), strtmp);
            this->target_temperature = (value & 0x7f)/ 2.0;
            this->publish_state();
            break;
        case status_errorcode:
        case erropdata_errorcode:
            // itoa(value, strtmp, 10);
            // output_P(status, PSTR(TOPIC_ERRORCODE), strtmp);
            error_code_.publish_state(value);
            break;
        case opdata_return_air:
        case erropdata_return_air:
            // dtostrf(value * 0.25f - 15, 0, 2, strtmp);
            // output_P(status, PSTR(TOPIC_RETURNAIR), strtmp);
            return_air_temperature_.publish_state(value * 0.25f - 15);
            break;
        case opdata_thi_r1:
        case erropdata_thi_r1:
            // itoa(0.327f * value - 11.4f, strtmp, 10); // only rough approximation
            // output_P(status, PSTR(TOPIC_THI_R1), strtmp);
            break;
        case opdata_thi_r2:
        case erropdata_thi_r2:
            // itoa(0.327f * value - 11.4f, strtmp, 10); // formula for calculation not known
            // output_P(status, PSTR(TOPIC_THI_R2), strtmp);
            break;
        case opdata_thi_r3:
        case erropdata_thi_r3:
            // itoa(0.327f * value - 11.4f, strtmp, 10); // only rough approximation
            // output_P(status, PSTR(TOPIC_THI_R3), strtmp);
            break;
        case opdata_iu_fanspeed:
        case erropdata_iu_fanspeed:
            // itoa(value, strtmp, 10);
            // output_P(status, PSTR(TOPIC_IU_FANSPEED), strtmp);
            indoor_unit_fan_speed_.publish_state(value);
            break;
        case opdata_total_iu_run:
        case erropdata_total_iu_run:
            // itoa(value * 100, strtmp, 10);
            // output_P(status, PSTR(TOPIC_TOTAL_IU_RUN), strtmp);
            indoor_unit_total_run_time_.publish_state(value * 100);
            break;
        case erropdata_outdoor:
        case opdata_outdoor:
            // dtostrf((value - 94) * 0.25f, 0, 2, strtmp);
            // output_P(status, PSTR(TOPIC_OUTDOOR), strtmp);
            outdoor_temperature_.publish_state((value - 94) * 0.25f);
            break;
        case opdata_tho_r1:
        case erropdata_tho_r1:
            // itoa(0.327f * value - 11.4f, strtmp, 10); // formula for calculation not known
            // output_P(status, PSTR(TOPIC_THO_R1), strtmp);
            break;
        case opdata_comp:
        case erropdata_comp:
            // dtostrf(
            //    highByte(value) * 25.6f + 0.1f * lowByte(value), 0, 2, strtmp); // to be confirmed
            // output_P(status, PSTR(TOPIC_COMP), strtmp);
            compressor_frequency_.publish_state(highByte(value) * 25.6f + 0.1f * lowByte(value));
            break;
        case erropdata_td:
        case opdata_td:
            // if (value < 0x12)
            //    strcpy(strtmp, "<=30");
            // else
             //   itoa(value / 2 + 32, strtmp, 10);
            // output_P(status, PSTR(TOPIC_TD), strtmp);
            break;
        case opdata_ct:
        case erropdata_ct:
            // dtostrf(value * 14 / 51.0f, 0, 2, strtmp);
            // output_P(status, PSTR(TOPIC_CT), strtmp);
            current_power_.publish_state(value * 14 / 51.0f);
            break;
        case opdata_tdsh:
            // itoa(value, strtmp, 10); // formula for calculation not known
            // output_P(status, PSTR(TOPIC_TDSH), strtmp);
            break;
        case opdata_protection_no:
            // itoa(value, strtmp, 10);
            // output_P(status, PSTR(TOPIC_PROTECTION_NO), strtmp);
            break;
        case opdata_ou_fanspeed:
        case erropdata_ou_fanspeed:
            // itoa(value, strtmp, 10);
            // output_P(status, PSTR(TOPIC_OU_FANSPEED), strtmp);
            outdoor_unit_fan_speed_.publish_state(value);
            break;
        case opdata_defrost:
            // if (value)
            //     output_P(status, PSTR(TOPIC_DEFROST), PSTR(PAYLOAD_OP_DEFROST_ON));
            // else
            //     output_P(status, PSTR(TOPIC_DEFROST), PSTR(PAYLOAD_OP_DEFROST_OFF));
            defrost_.publish_state(value != 0);
            break;
        case opdata_total_comp_run:
        case erropdata_total_comp_run:
            // itoa(value * 100, strtmp, 10);
            // output_P(status, PSTR(TOPIC_TOTAL_COMP_RUN), strtmp);
            compressor_total_run_time_.publish_state(value * 100);
            break;
        case opdata_ou_eev1:
        case erropdata_ou_eev1:
            // itoa(value, strtmp, 10);
            // output_P(status, PSTR(TOPIC_OU_EEV1), strtmp);
            break;
        case status_rssi:
        case status_connected:
        case status_cmd:
        case status_mqtt_lost:
        case status_wifi_lost:
        case status_tds1820:
        case opdata_unknwon:
            break;
        }
    }

    std::vector<Sensor *> get_sensors() {
        return {
            &error_code_,
            &outdoor_temperature_,
            &return_air_temperature_,
            &outdoor_unit_fan_speed_,
            &indoor_unit_fan_speed_,
            &current_power_,
            &compressor_frequency_,
            &indoor_unit_total_run_time_,
            &compressor_total_run_time_
        };
    }

    std::vector<BinarySensor *> get_binary_sensors() {
        return { &defrost_ };
    }

    void set_room_temperature(float value) {
        if ((value > -10) & (value < 48)) {
            room_temp_api_timeout_ms = millis();  // reset timeout
            byte tmp = value*4+61;
            mhi_ac_ctrl_core.set_troom(value*4+61);
            ESP_LOGD("mhi_ac_ctrl", "set room_temp_api: %f %i %i", value, (byte)(value*4+61), (byte)tmp);
        }
    }

protected:
    /// Transmit the state of this climate controller.
    void control(const climate::ClimateCall& call) override
    {
        if (call.get_mode().has_value()) {
            this->mode = *call.get_mode();

            power_ = power_on;
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

            mhi_ac_ctrl_core.set_power(power_);
            mhi_ac_ctrl_core.set_mode(mode_);
        }

        if (call.get_target_temperature().has_value()) {
            this->target_temperature = *call.get_target_temperature();

            tsetpoint_ = (uint)roundf(
                clamp(this->target_temperature, minimum_temperature_, maximum_temperature_));

            mhi_ac_ctrl_core.set_tsetpoint((byte)(2 * tsetpoint_));
        }

        if (call.get_fan_mode().has_value()) {
            this->fan_mode = *call.get_fan_mode();

            switch (*this->fan_mode) {
            case climate::CLIMATE_FAN_LOW:
                fan_ = 1;
                break;
            case climate::CLIMATE_FAN_MEDIUM:
                fan_ = 2;
                break;
            case climate::CLIMATE_FAN_HIGH:
                fan_ = 3;
                break;
            case climate::CLIMATE_FAN_AUTO:
            default:
                fan_ = 4;
                break;
            }

            mhi_ac_ctrl_core.set_fan(fan_);
        }

        if (call.get_swing_mode().has_value()) {
            this->swing_mode = *call.get_swing_mode();

            switch (this->swing_mode) {
            case climate::CLIMATE_SWING_OFF:
                vanes_ = vanes_unknown;
                break;
            case climate::CLIMATE_SWING_VERTICAL:
            default:
                vanes_ = vanes_swing;
                break;
            }

            mhi_ac_ctrl_core.set_vanes(vanes_);
        }

        this->publish_state();
    }

    /// Return the traits of this controller.
    climate::ClimateTraits traits() override
    {
        auto traits = climate::ClimateTraits();
        traits.set_supports_current_temperature(true);
        traits.set_supported_modes({ CLIMATE_MODE_OFF, CLIMATE_MODE_HEAT_COOL, CLIMATE_MODE_COOL, CLIMATE_MODE_HEAT, CLIMATE_MODE_DRY, CLIMATE_MODE_FAN_ONLY });
        traits.set_supports_two_point_target_temperature(false);
        traits.set_visual_min_temperature(this->minimum_temperature_);
        traits.set_visual_max_temperature(this->maximum_temperature_);
        traits.set_visual_temperature_step(this->temperature_step_);
        traits.set_supported_fan_modes({ CLIMATE_FAN_AUTO, CLIMATE_FAN_LOW, CLIMATE_FAN_MEDIUM, CLIMATE_FAN_HIGH });
        traits.set_supported_swing_modes({ CLIMATE_SWING_OFF, CLIMATE_SWING_VERTICAL });
        return traits;
    }

    float minimum_temperature_ { 18.0f };
    float maximum_temperature_ { 30.0f };
    float temperature_step_ { 1.0f };

    ACPower power_;
    ACMode mode_;
    uint tsetpoint_;
    uint fan_;
    ACVanes vanes_;

    MHI_AC_Ctrl_Core mhi_ac_ctrl_core;

    Sensor error_code_;
    Sensor outdoor_temperature_;
    Sensor return_air_temperature_;
    Sensor outdoor_unit_fan_speed_;
    Sensor indoor_unit_fan_speed_;
    Sensor compressor_frequency_;
    Sensor indoor_unit_total_run_time_;
    Sensor compressor_total_run_time_;
    Sensor current_power_;
    BinarySensor defrost_;
};
