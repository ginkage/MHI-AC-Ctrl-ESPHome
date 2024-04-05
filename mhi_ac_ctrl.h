// Version 2.2

#include "MHI-AC-Ctrl-core.h"
#define ROOM_TEMP_MQTT 1
#define ROOM_TEMP_SAMPLE_COUNT 1 //My AC reliably sends the temperature every time. If you experience fluctuations, change this setting.
#define ROOM_TEMP_SAMPLE_INTERVAL_MS 2500
#include <vector>
#include <string>

static const std::vector<std::string> protection_states = {
    "Normal",
    "Discharge pipe temperature protection control",
    "Discharge pipe temperature anomaly",
    "Current safe control of inverter primary current",
    "High pressure protection control",
    "High pressure anomaly",
    "Low pressure protection control",
    "Low pressure anomaly",
    "Anti-frost prevention control",
    "Current cut",
    "Power transistor protection control",
    "Power transistor anomaly (Overheat)",
    "Compression ratio control",
    "-",
    "Condensation prevention control",
    "Current safe control of inverter secondary current",
    "Stop by compressor rotor lock",
    "Stop by compressor startup failure"
};

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
            this->target_temperature = 18.0f;

        simulated_room_temperature_.set_icon("mdi:thermometer-lines");
        simulated_room_temperature_.set_unit_of_measurement("°C");
        simulated_room_temperature_.set_accuracy_decimals(2);

        room_temperature_offset_.set_icon("mdi:thermometer-chevron-up");
        room_temperature_offset_.set_unit_of_measurement("°C");
        room_temperature_offset_.set_accuracy_decimals(2);

        error_code_.set_icon("mdi:alert-circle");

        outdoor_temperature_.set_icon("mdi:home-thermometer-outline");
        outdoor_temperature_.set_unit_of_measurement("°C");
        outdoor_temperature_.set_accuracy_decimals(2);

        return_air_temperature_.set_icon("mdi:hvac");
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

        simulating_.set_icon("mdi:cached");

        vanes_pos_.set_icon("mdi:air-filter");

        indoor_unit_thi_r1_.set_icon("mdi:thermometer");
        indoor_unit_thi_r1_.set_unit_of_measurement("°C");
        indoor_unit_thi_r1_.set_accuracy_decimals(2);

        indoor_unit_thi_r2_.set_icon("mdi:thermometer");
        indoor_unit_thi_r2_.set_unit_of_measurement("°C");
        indoor_unit_thi_r2_.set_accuracy_decimals(2);

        indoor_unit_thi_r3_.set_icon("mdi:thermometer");
        indoor_unit_thi_r3_.set_unit_of_measurement("°C");
        indoor_unit_thi_r3_.set_accuracy_decimals(2);

        outdoor_unit_tho_r1_.set_icon("mdi:thermometer");
        outdoor_unit_tho_r1_.set_unit_of_measurement("°C");
        outdoor_unit_tho_r1_.set_accuracy_decimals(2);

        outdoor_unit_expansion_valve_.set_icon("mdi:valve");
        outdoor_unit_expansion_valve_.set_unit_of_measurement("pulse");
        outdoor_unit_expansion_valve_.set_accuracy_decimals(0);

        outdoor_unit_discharge_pipe_.set_icon("mdi:thermometer");
        outdoor_unit_discharge_pipe_.set_unit_of_measurement("°C");
        outdoor_unit_discharge_pipe_.set_accuracy_decimals(1);

        outdoor_unit_discharge_pipe_super_heat_.set_icon("mdi:thermometer");
        outdoor_unit_discharge_pipe_super_heat_.set_unit_of_measurement("°C");
        outdoor_unit_discharge_pipe_super_heat_.set_accuracy_decimals(1);

        protection_state_.set_icon("mdi:shield-alert-outline");

        protection_state_number_.set_icon("mdi:shield-alert-outline");

        energy_used_.set_icon("mdi:lightning-bolt");
        energy_used_.set_unit_of_measurement("kWh");
        energy_used_.set_accuracy_decimals(2);
        energy_used_.set_device_class("energy");
        energy_used_.set_state_class(STATE_CLASS_TOTAL_INCREASING);

        this->set_enable_offset(false, true, true);
        mhi_ac_ctrl_core.MHIAcCtrlStatus(this);
        mhi_ac_ctrl_core.init();
        mhi_ac_ctrl_core.set_frame_size(id(frame_size)); // set framesize. Only 20 (legacy) or 33 (includes 3D auto and vertical vanes) possible
    }

    void loop() override
    {

        if(enable_troom_offset){
            uint32_t now = millis();
            if(now - last_internal_sensor_timestamp > ROOM_TEMP_SAMPLE_INTERVAL_MS){
                last_internal_sensor_timestamp = now;
                set_enable_offset(false, false, false); //Sets troom to 0xff
                // Sends troom
                int ret = mhi_ac_ctrl_core.loop(100);
                if (ret < 0){
                    ESP_LOGW("mhi_ac_ctrl", "mhi_ac_ctrl_core.loop error: %i", ret);
                }
                // Retrieves the internal sensor value
                ret = mhi_ac_ctrl_core.loop(100);
                if (ret < 0){
                    ESP_LOGW("mhi_ac_ctrl", "mhi_ac_ctrl_core.loop error: %i", ret);
                }
                set_enable_offset(true, false, false);
                internal_sensor_averages[sample_count] = this->current_temperature_status;
                sample_count++;
                if(sample_count == ROOM_TEMP_SAMPLE_COUNT){
                    sample_count = 0;
                    float sum = 0.0f;
                    for(int i = 0; i<ROOM_TEMP_SAMPLE_COUNT; i++){
                        sum += internal_sensor_averages[i];
                    }
                    last_internal_sensor_temperature = sum / static_cast<float>(ROOM_TEMP_SAMPLE_COUNT);
                    this->current_temperature = last_internal_sensor_temperature;
                    this->publish_state();
                }

                // In case the AC also uses averages to calculate the room temperature, we're sending
                // back the temperature with twice the offset to nullify the sample we've just gotten.
                set_room_internal_temperature(last_internal_sensor_temperature + (internal_sensor_temperature_offset * 2.0f), false);
                ret = mhi_ac_ctrl_core.loop(100);
                if (ret < 0){
                    ESP_LOGW("mhi_ac_ctrl", "mhi_ac_ctrl_core.loop error: %i", ret);
                }
            }else{
                set_room_internal_temperature(last_internal_sensor_temperature + internal_sensor_temperature_offset, true);
                int ret = mhi_ac_ctrl_core.loop(100);
                if (ret < 0){
                    ESP_LOGW("mhi_ac_ctrl", "mhi_ac_ctrl_core.loop error: %i", ret);
                }
            }
        }else{
            if(millis() - room_temp_api_timeout_ms >= id(room_temp_api_timeout)*1000) {
                mhi_ac_ctrl_core.set_troom(0xff);  // use IU temperature sensor
                room_temp_api_timeout_ms = millis();
                ESP_LOGD("mhi_ac_ctrl", "did not receive a room_temp_api value, using IU temperature sensor");
            }
            else {
            int ret = mhi_ac_ctrl_core.loop(100);
            if (ret < 0){
                ESP_LOGW("mhi_ac_ctrl", "mhi_ac_ctrl_core.loop error: %i", ret);
            }
                // simulated_room_temperature_.publish_state(this->current_temperature);
            }
        }
    }

    void dump_config() override
    {
        LOG_CLIMATE("", "MHI-AC-Ctrl Climate", this);
        ESP_LOGCONFIG(TAG, "  Min. Temperature: %.1f°C", this->minimum_temperature_);
        ESP_LOGCONFIG(TAG, "  Max. Temperature: %.1f°C", this->maximum_temperature_);
        ESP_LOGCONFIG(TAG, "  Simulated min. Temperature: %.1f°C", this->adjusted_minimum_temperature_);
    }

    void cbiStatusFunction(ACStatus status, int value) override
    {
        static int mode_tmp = 0xff;
        ESP_LOGD("mhi_ac_ctrl", "received status=%i value=%i power=%i", status, value, this->power_);

        if (this->power_ == power_off) {
            // Workaround for status after reboot
            this->mode = climate::CLIMATE_MODE_OFF;
            this->publish_state();
        }
        int vanesLR_swing_value = vanesLR_swing;
        int vanesLR_sensor_value = vanesLR_pos_.state;
        int vanesUD_swing_value = vanes_swing;
        int vanesUD_sensor_value = vanes_pos_.state;
        
        switch (status) {
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
            if (vanesLR_sensor_value == vanesLR_swing_value) {
                switch (value) {
                    case vanes_unknown:
                    case vanes_1:
                    case vanes_2:
                    case vanes_3:
                    case vanes_4:
                        this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
                        vanes_pos_old_.publish_state(value);
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
                        vanes_pos_old_.publish_state(value);
                        break;
                    case vanes_swing:
                        this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
                        break;
                }

            }
            vanes_pos_.publish_state(value);
            this->publish_state();
            break;
        case status_vanesLR:
            if (vanesUD_sensor_value == vanesUD_swing_value) {
                switch (value) {
                    case vanesLR_1:
                    case vanesLR_2:
                    case vanesLR_3:
                    case vanesLR_4:
                    case vanesLR_5:
                    case vanesLR_6:
                    case vanesLR_7:
                        this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
                        vanesLR_pos_old_.publish_state(value);
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
                        vanesLR_pos_old_.publish_state(value);
                        break;
                    case vanesLR_swing:
                        this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
                        break;
                }

            }
            this->publish_state();
            vanesLR_pos_.publish_state(value);
            break;
        case status_3Dauto:
            switch (value) {
            case 0b00000000:
                Dauto_.publish_state(false);
                break;
            case 0b00000100:
                Dauto_.publish_state(true);
                break;
            }
            this->publish_state();
            break;
        case status_troom:
            // dtostrf((value - 61) / 4.0, 0, 2, strtmp);
            // output_P(status, PSTR(TOPIC_TROOM), strtmp);
            current_temperature_status = (value - 61) / 4.0;
            if(enable_troom_offset){
                this->current_temperature = last_internal_sensor_temperature;
                this->publish_state();
            }else if(enable_troom_processing){
                this->current_temperature = current_temperature_status;
                this->publish_state();
            }
            break;
        case status_tsetpoint:
            // itoa(value, strtmp, 10);
            // output_P(status, PSTR(TOPIC_TSETPOINT), strtmp);
            this->target_temperature = (value & 0x7f)/ 2.0;
            internal_sensor_temperature_offset = 0.0f;
            set_enable_offset(false, true, true);
            room_temperature_offset_.publish_state(internal_sensor_temperature_offset);
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
            // Indoor Heat exchanger temperature 1 (U-bend)
           indoor_unit_thi_r1_.publish_state(0.327f * value - 11.4f);
           break;
        case erropdata_thi_r1:
            // itoa(0.327f * value - 11.4f, strtmp, 10); // only rough approximation
            // output_P(status, PSTR(TOPIC_THI_R1), strtmp);
            break;
        case opdata_thi_r2:
            // Indoor Heat exchanger temperature 2 (capillary)
            indoor_unit_thi_r2_.publish_state(0.327f * value - 11.4f);
            break;
        case erropdata_thi_r2:
            // itoa(0.327f * value - 11.4f, strtmp, 10); // formula for calculation not known
            // output_P(status, PSTR(TOPIC_THI_R2), strtmp);
            break;
        case opdata_thi_r3:
            // Indoor Heat exchanger temperature 3 (suction header)
            indoor_unit_thi_r3_.publish_state(0.327f * value - 11.4f);
            break;
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
            // Indoor Heat exchanger temperature 3 (suction header)
            outdoor_unit_tho_r1_.publish_state(0.327f * value - 11.4f);
            break;
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
            if (value < 0x2)
                outdoor_unit_discharge_pipe_.publish_state(30);
            else
                outdoor_unit_discharge_pipe_.publish_state(value / 2 + 32);
            break;
        case opdata_ct:
        case erropdata_ct:
            // dtostrf(value * 14 / 51.0f, 0, 2, strtmp);
            // output_P(status, PSTR(TOPIC_CT), strtmp);
            current_power_.publish_state(value * 14 / 51.0f);
            break;
        case opdata_tdsh:
            outdoor_unit_discharge_pipe_super_heat_.publish_state(value);
            // itoa(value, strtmp, 10); // formula for calculation not known
            // output_P(status, PSTR(TOPIC_TDSH), strtmp);
            break;
        case opdata_protection_no:
            if (value < protection_states.size())
                protection_state_.publish_state(protection_states[value]);
            protection_state_number_.publish_state(value);
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
            outdoor_unit_expansion_valve_.publish_state(value);
            break;
        case erropdata_ou_eev1:
            // itoa(value, strtmp, 10);
            // output_P(status, PSTR(TOPIC_OU_EEV1), strtmp);
            break;
        case opdata_tsetpoint:
        case erropdata_tsetpoint:
            break;
        case opdata_kwh:
            // https://github.com/absalom-muc/MHI-AC-Ctrl/pull/135
            // This item is counting the kWh from the point where the AC is powered On
            energy_used_.publish_state(value * 0.25);
            break;
        case opdata_unknown:
            // skip these values as they are not used currently
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
            &compressor_total_run_time_,
            &vanes_pos_,
            &energy_used_,
            &indoor_unit_thi_r1_,
            &indoor_unit_thi_r2_,
            &indoor_unit_thi_r3_,
            &outdoor_unit_tho_r1_,
            &outdoor_unit_expansion_valve_,
            &outdoor_unit_discharge_pipe_,
            &outdoor_unit_discharge_pipe_super_heat_,
            &protection_state_number_,
            &vanesLR_pos_,
            &Dauto_,
            &simulated_room_temperature_,
            &room_temperature_offset_,
        };
    }

    std::vector<TextSensor *> get_text_sensors() {
        return { &protection_state_ };
    }

    std::vector<BinarySensor *> get_binary_sensors() {
        return { 
            &defrost_,
            &simulating_,
            };
    }

    void set_room_temperature(float value) {
        if ((value > -10) & (value < 48)) {
            room_temp_api_timeout_ms = millis();  // reset timeout
            byte tmp = value*4+61;
            mhi_ac_ctrl_core.set_troom(value*4+61);
            ESP_LOGD("mhi_ac_ctrl", "set room_temp_api: %f %i %i", value, (byte)(value*4+61), (byte)tmp);
        }
    }

    void set_room_internal_temperature(float value, bool publish) {
        if ((value > -10) && (value < 48)) {
            last_internal_sensor_timestamp = millis();
            byte tmp = value*4+61;
            mhi_ac_ctrl_core.set_troom(value*4+61);
            if(publish && (simulated_room_temperature_.state != value)){
                simulated_room_temperature_.publish_state(value);
            }
        }
    }

    void set_vanes(int value) {
        mhi_ac_ctrl_core.set_vanes(value);
        ESP_LOGD("mhi_ac_ctrl", "set vanes: %i", value);
    }

    void set_vanesLR(int value) {
        mhi_ac_ctrl_core.set_vanesLR(value);
        ESP_LOGD("mhi_ac_ctrl", "set vanes Left Right: %i", value);
    }

    void set_3Dauto(bool value) {
        // ESP_LOGD("mhi_ac_ctrl", "set 3D auto: %s", value);
        if (value){
            ESP_LOGD("mhi_ac_ctrl", "set 3D auto: on");
            mhi_ac_ctrl_core.set_3Dauto(AC3Dauto::Dauto_on); // Set swing to 3Dauto
        }
        else {
            ESP_LOGD("mhi_ac_ctrl", "set 3D auto: off");
            mhi_ac_ctrl_core.set_3Dauto(AC3Dauto::Dauto_off); // Set swing to 3Dauto
        }
        ESP_LOGD("mhi_ac_ctrl", "set vanes Left Right: %i", value);
    }

    void set_enable_offset(bool enabled, bool allow_processing, bool publish) {
        enable_troom_offset = enabled;
        if(publish && (simulating_.state != enable_troom_offset)){
            simulating_.publish_state(enable_troom_offset);
        }
        enable_troom_processing = allow_processing;
        if(!enable_troom_offset){
            // We removed the offset, we allow the use of the internal temperature sensor.
            mhi_ac_ctrl_core.set_troom(0xff);
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
            tsetpoint_ = clamp(this->target_temperature, minimum_temperature_, maximum_temperature_);
            // Check if target_temperature is smaller than minimum_temperature
            if (this->target_temperature < minimum_temperature_ ) {
                set_enable_offset(true, true, true);
                internal_sensor_temperature_offset = enable_troom_offset ? (minimum_temperature_ - this->target_temperature) : 0.0f;
            }
            // Check if target_temperature is bigger than maximum_temperature
            else if (this->target_temperature > maximum_temperature_) {
                set_enable_offset(true, true, true);
                internal_sensor_temperature_offset = enable_troom_offset ? (this->target_temperature - maximum_temperature_) : 0.0f;
            }

            // Check if target temperature is not a rounded value
            else if (this->target_temperature != round(this->target_temperature)) {
                tsetpoint_ = clamp(round(this->target_temperature), minimum_temperature_, maximum_temperature_);
                ESP_LOGD("mhi_ac_ctrl", "Target temperature is not a rounded value: %f", this->target_temperature);
                set_enable_offset(true, true, true);
                internal_sensor_temperature_offset = round(this->target_temperature) - this->target_temperature;  // Calculate offset when setpoint is changed
            }
            else {
                set_enable_offset(false, true, true);
                internal_sensor_temperature_offset = 0.0f;
            }
            room_temperature_offset_.publish_state(internal_sensor_temperature_offset);
            ESP_LOGD("mhi_ac_ctrl", "updated setpoint=%f offset=%f target=%f", tsetpoint_, internal_sensor_temperature_offset, this->target_temperature);
            mhi_ac_ctrl_core.set_tsetpoint((byte)(2 * tsetpoint_));
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

            mhi_ac_ctrl_core.set_fan(fan_);
        }

        if (call.get_swing_mode().has_value()) {
            this->swing_mode = *call.get_swing_mode();
            int vanesLR_pos_old_value = vanesLR_pos_old_.has_state() ? vanesLR_pos_old_.state : 4;
            int vanes_pos_old_value = vanes_pos_old_.has_state() ? vanes_pos_old_.state : 4;
            vanesLR_ = static_cast<ACVanesLR>(vanesLR_pos_old_value);
            vanes_ = static_cast<ACVanes>(vanes_pos_old_value);

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
            mhi_ac_ctrl_core.set_vanesLR(vanesLR_); // Set vanesLR to swing
            mhi_ac_ctrl_core.set_vanes(vanes_); // Set vanes to swing
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
        traits.set_visual_min_temperature(this->adjusted_minimum_temperature_);
        traits.set_visual_max_temperature(this->maximum_temperature_);
        traits.set_visual_temperature_step(this->temperature_step_);
        traits.set_supported_fan_modes({ CLIMATE_FAN_AUTO, CLIMATE_FAN_QUIET, CLIMATE_FAN_LOW, CLIMATE_FAN_MEDIUM, CLIMATE_FAN_HIGH });
        traits.set_supported_swing_modes({ CLIMATE_SWING_OFF, CLIMATE_SWING_BOTH, CLIMATE_SWING_VERTICAL, CLIMATE_SWING_HORIZONTAL });
        return traits;
    }

    float last_internal_sensor_temperature = 18.0f;
    uint32_t last_internal_sensor_timestamp = 0;
    float internal_sensor_averages[ROOM_TEMP_SAMPLE_COUNT] = {};
    int sample_count = 0;

    bool enable_troom_offset = false;
    bool enable_troom_processing = true;

    float current_temperature_status {18.0f};
    float internal_sensor_temperature_offset {0.0f};
    float adjusted_minimum_temperature_ {10.0f};
    float minimum_temperature_ { 18.0f };
    float maximum_temperature_ { 30.0f };
    float temperature_step_ { 0.5f };

    ACPower power_;
    ACMode mode_;
    float tsetpoint_;
    uint fan_;
    ACVanes vanes_;
    ACVanesLR vanesLR_;

    MHI_AC_Ctrl_Core mhi_ac_ctrl_core;

    Sensor simulated_room_temperature_;
    Sensor room_temperature_offset_;
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
    BinarySensor simulating_;
    Sensor vanes_pos_;
    Sensor vanes_pos_old_;
    Sensor energy_used_;
    Sensor indoor_unit_thi_r1_;
    Sensor indoor_unit_thi_r2_;
    Sensor indoor_unit_thi_r3_;
    Sensor outdoor_unit_tho_r1_;
    Sensor outdoor_unit_expansion_valve_;
    Sensor outdoor_unit_discharge_pipe_;
    Sensor outdoor_unit_discharge_pipe_super_heat_;
    Sensor protection_state_number_;
    TextSensor protection_state_;
    Sensor vanesLR_pos_;
    Sensor vanesLR_pos_old_;
    Sensor Dauto_;
};
