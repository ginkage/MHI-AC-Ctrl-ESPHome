#include "mhi_sensors.h"

namespace esphome {
namespace mhi {

static const char* TAG = "mhi.sensor";

void MhiSensors::set_error_code (Sensor* sensor) { error_code_ = sensor; }
void MhiSensors::set_outdoor_temperature (Sensor* sensor) { outdoor_temperature_ = sensor; }
void MhiSensors::set_return_air_temperature (Sensor* sensor) { return_air_temperature_ = sensor; }
void MhiSensors::set_outdoor_unit_fan_speed (Sensor* sensor) { outdoor_unit_fan_speed_ = sensor; }
void MhiSensors::set_indoor_unit_fan_speed (Sensor* sensor) { indoor_unit_fan_speed_ = sensor; }
void MhiSensors::set_compressor_frequency (Sensor* sensor) { compressor_frequency_ = sensor; }
void MhiSensors::set_indoor_unit_total_run_time (Sensor* sensor) { indoor_unit_total_run_time_ = sensor; }
void MhiSensors::set_compressor_total_run_time (Sensor* sensor) { compressor_total_run_time_ = sensor; }
void MhiSensors::set_current_power (Sensor* sensor) { current_power_ = sensor; }
void MhiSensors::set_vanes_pos (Sensor* sensor) { vanes_pos_ = sensor; }
void MhiSensors::set_energy_used (Sensor* sensor) { energy_used_ = sensor; }
void MhiSensors::set_indoor_unit_thi_r1 (Sensor* sensor) { indoor_unit_thi_r1_ = sensor; }
void MhiSensors::set_indoor_unit_thi_r2 (Sensor* sensor) { indoor_unit_thi_r2_ = sensor; }
void MhiSensors::set_indoor_unit_thi_r3 (Sensor* sensor) { indoor_unit_thi_r3_ = sensor; }
void MhiSensors::set_outdoor_unit_tho_r1 (Sensor* sensor) { outdoor_unit_tho_r1_ = sensor; }
void MhiSensors::set_outdoor_unit_expansion_valve (Sensor* sensor) { outdoor_unit_expansion_valve_ = sensor; }
void MhiSensors::set_outdoor_unit_discharge_pipe (Sensor* sensor) { outdoor_unit_discharge_pipe_ = sensor; }
void MhiSensors::set_outdoor_unit_discharge_pipe_super_heat (Sensor* sensor) { outdoor_unit_discharge_pipe_super_heat_ = sensor; }
void MhiSensors::set_protection_state_number (Sensor* sensor) { protection_state_number_ = sensor; }
void MhiSensors::set_vanesLR_pos (Sensor* sensor) { vanesLR_pos_ = sensor; }


void MhiSensors::setup() {
    this->parent_->add_listener(this);
}

void MhiSensors::dump_config() {

    ESP_LOGCONFIG(TAG, "MHI Sensors");
    if (error_code_ != NULL) {
        ESP_LOGCONFIG(TAG, "  error_code: %f", this->error_code_->state);
    }
    if (outdoor_temperature_ != NULL) {
        ESP_LOGCONFIG(TAG, "  outdoor_temperature: %f", this->outdoor_temperature_->state);
    }
    if (return_air_temperature_ != NULL) {
        ESP_LOGCONFIG(TAG, "  return_air_temperature: %f", this->return_air_temperature_->state);
    }
    if (outdoor_unit_fan_speed_ != NULL) {
        ESP_LOGCONFIG(TAG, "  outdoor_unit_fan_speed: %f", this->outdoor_unit_fan_speed_->state);
    }
    if (indoor_unit_fan_speed_ != NULL) {
        ESP_LOGCONFIG(TAG, "  indoor_unit_fan_speed: %f", this->indoor_unit_fan_speed_->state);
    }
    if (compressor_frequency_ != NULL) {
        ESP_LOGCONFIG(TAG, "  compressor_frequency: %f", this->compressor_frequency_->state);
    }
    if (indoor_unit_total_run_time_ != NULL) {
        ESP_LOGCONFIG(TAG, "  indoor_unit_total_run_time: %f", this->indoor_unit_total_run_time_->state);
    }
    if (compressor_total_run_time_ != NULL) {
        ESP_LOGCONFIG(TAG, "  compressor_total_run_time: %f", this->compressor_total_run_time_->state);
    }
    if (current_power_ != NULL) {
        ESP_LOGCONFIG(TAG, "  current_power: %f", this->current_power_->state);
    }
    if (vanes_pos_ != NULL) {
        ESP_LOGCONFIG(TAG, "  vanes_pos: %f", this->vanes_pos_->state);
    }
    if (energy_used_ != NULL) {
        ESP_LOGCONFIG(TAG, "  energy_used: %f", this->energy_used_->state);
    }
    if (indoor_unit_thi_r1_ != NULL) {
        ESP_LOGCONFIG(TAG, "  indoor_unit_thi_r1: %f", this->indoor_unit_thi_r1_->state);
    }
    if (indoor_unit_thi_r2_ != NULL) {
        ESP_LOGCONFIG(TAG, "  indoor_unit_thi_r2: %f", this->indoor_unit_thi_r2_->state);
    }
    if (indoor_unit_thi_r3_ != NULL) {
        ESP_LOGCONFIG(TAG, "  indoor_unit_thi_r3: %f", this->indoor_unit_thi_r3_->state);
    }
    if (outdoor_unit_tho_r1_ != NULL) {
        ESP_LOGCONFIG(TAG, "  outdoor_unit_tho_r1: %f", this->outdoor_unit_tho_r1_->state);
    }
    if (outdoor_unit_expansion_valve_ != NULL) {
        ESP_LOGCONFIG(TAG, "  outdoor_unit_expansion_valve: %f", this->outdoor_unit_expansion_valve_->state);
    }
    if (outdoor_unit_discharge_pipe_ != NULL) {
        ESP_LOGCONFIG(TAG, "  outdoor_unit_discharge_pipe: %f", this->outdoor_unit_discharge_pipe_->state);
    }
    if (outdoor_unit_discharge_pipe_super_heat_ != NULL) {
        ESP_LOGCONFIG(TAG, "  outdoor_unit_discharge_pipe_super_heat: %f", this->outdoor_unit_discharge_pipe_super_heat_->state);
    }
    if (protection_state_number_ != NULL) {
        ESP_LOGCONFIG(TAG, "  protection_state_number: %f", this->protection_state_number_->state);
    }
    if (vanesLR_pos_ != NULL) {
        ESP_LOGCONFIG(TAG, "  vanesLR_pos: %f", this->vanesLR_pos_->state);
    }
}

void MhiSensors::update_status(ACStatus status, int value) {

    switch (status) {

    case status_vanes:
        // Vanes Up Down, also known as Vertical
        if (this->vanes_pos_ != NULL) { 
            this->vanes_pos_ -> publish_state(value); 
        }
        break;
    case status_vanesLR:
        if (this->vanesLR_pos_ != NULL) { 
            this->vanesLR_pos_ -> publish_state(value); 
        }
        break;
    case status_errorcode:
    case erropdata_errorcode:
        // itoa(value, strtmp, 10);
        // output_P(status, PSTR(TOPIC_ERRORCODE), strtmp);
        if (this->error_code_ != NULL) { 
            this->error_code_ -> publish_state(value); 
        }
        break;
    case opdata_return_air:
    case erropdata_return_air:
        // dtostrf(value * 0.25f - 15, 0, 2, strtmp);
        // output_P(status, PSTR(TOPIC_RETURNAIR), strtmp);
        if (return_air_temperature_ != NULL) { 
            return_air_temperature_ -> publish_state(value * 0.25f - 15); 
        }
        break;
    case opdata_thi_r1:
        // Indoor Heat exchanger temperature 1 (U-bend)
        if (this->indoor_unit_thi_r1_ != NULL) { 
            this->indoor_unit_thi_r1_ -> publish_state(0.327f * value - 11.4f); 
        }
        break;
    case erropdata_thi_r1:
        // itoa(0.327f * value - 11.4f, strtmp, 10); // only rough approximation
        // output_P(status, PSTR(TOPIC_THI_R1), strtmp);
        break;
    case opdata_thi_r2:
        // Indoor Heat exchanger temperature 2 (capillary) 
        // 2025-07-08: Renewed calculation, needs to be confirmed
        // previously it was 0.327f * value - 11.4f, which is not correct
        if (this->indoor_unit_thi_r2_ != NULL) { 
            this->indoor_unit_thi_r2_ -> publish_state(0.275f * value - 47.0f); 
        }
        break;
    case erropdata_thi_r2:
        // itoa(0.327f * value - 11.4f, strtmp, 10); // formula for calculation not known
        // output_P(status, PSTR(TOPIC_THI_R2), strtmp);
        break;
    case opdata_thi_r3:
        // Indoor Heat exchanger temperature 3 (suction header)
        if (this->indoor_unit_thi_r3_ != NULL) { 
            this->indoor_unit_thi_r3_ -> publish_state(0.327f * value - 11.4f); 
        }
        break;
    case erropdata_thi_r3:
        // itoa(0.327f * value - 11.4f, strtmp, 10); // only rough approximation
        // output_P(status, PSTR(TOPIC_THI_R3), strtmp);
        break;
    case opdata_iu_fanspeed:
    case erropdata_iu_fanspeed:
        // itoa(value, strtmp, 10);
        // output_P(status, PSTR(TOPIC_IU_FANSPEED), strtmp);
        if (this->indoor_unit_fan_speed_ != NULL) { 
            this->indoor_unit_fan_speed_ -> publish_state(value); 
        }
        break;
    case opdata_total_iu_run:
    case erropdata_total_iu_run:
        // itoa(value * 100, strtmp, 10);
        // output_P(status, PSTR(TOPIC_TOTAL_IU_RUN), strtmp);
        if (this->indoor_unit_total_run_time_ != NULL) { 
            this->indoor_unit_total_run_time_ -> publish_state(value * 100); 
        }
        break;
    case erropdata_outdoor:
    case opdata_outdoor:
        // dtostrf((value - 94) * 0.25f, 0, 2, strtmp);
        // output_P(status, PSTR(TOPIC_OUTDOOR), strtmp);
        if (this->outdoor_temperature_ != NULL) { 
            this->outdoor_temperature_ -> publish_state((value - 94) * 0.25f); 
        }
        break;
    case opdata_tho_r1:
        // Indoor Heat exchanger temperature 3 (suction header)
        if (this->outdoor_unit_tho_r1_ != NULL) { 
            this->outdoor_unit_tho_r1_ -> publish_state(0.327f * value - 11.4f); 
        }
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
        if (this->compressor_frequency_ != NULL) { 
            this->compressor_frequency_ -> publish_state(highByte(value) * 25.6f + 0.1f * lowByte(value)); 
        }
        break;
    case erropdata_td:
    case opdata_td:
        if (value < 0x2) {
            if (this->outdoor_unit_discharge_pipe_ != NULL) { 
                this->outdoor_unit_discharge_pipe_ -> publish_state(30); 
            }
        }
        else {
            if (this->outdoor_unit_discharge_pipe_ != NULL) { 
                this->outdoor_unit_discharge_pipe_ -> publish_state(value / 2 + 32); 
            }
        }
        break;
    case opdata_ct:
    case erropdata_ct:
        // dtostrf(value * 14 / 51.0f, 0, 2, strtmp);
        // output_P(status, PSTR(TOPIC_CT), strtmp);
        if (this->current_power_ != NULL) { 
            this->current_power_ -> publish_state(value * 14 / 51.0f); 
        }
        break;
    case opdata_tdsh:
        if (this->outdoor_unit_discharge_pipe_super_heat_ != NULL) { 
            this->outdoor_unit_discharge_pipe_super_heat_ -> publish_state(value); 
        }
        // itoa(value, strtmp, 10); // formula for calculation not known
        // output_P(status, PSTR(TOPIC_TDSH), strtmp);
        break;
    case opdata_protection_no:
        if (this->protection_state_number_ != NULL) { 
            this->protection_state_number_ -> publish_state(value); 
        }
        // itoa(value, strtmp, 10);
        // output_P(status, PSTR(TOPIC_PROTECTION_NO), strtmp);
        break;
    case opdata_ou_fanspeed:
    case erropdata_ou_fanspeed:
        // itoa(value, strtmp, 10);
        // output_P(status, PSTR(TOPIC_OU_FANSPEED), strtmp);
        if (this->outdoor_unit_fan_speed_ != NULL) { 
            this->outdoor_unit_fan_speed_ -> publish_state(value); 
        }
        break;
    case opdata_total_comp_run:
    case erropdata_total_comp_run:
        // itoa(value * 100, strtmp, 10);
        // output_P(status, PSTR(TOPIC_TOTAL_COMP_RUN), strtmp);
        if (this->compressor_total_run_time_ != NULL) { 
            this->compressor_total_run_time_ -> publish_state(value * 100); 
        }
        break;
    case opdata_ou_eev1:
        if (this->outdoor_unit_expansion_valve_ != NULL) { 
            this->outdoor_unit_expansion_valve_ -> publish_state(value); 
        }
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
        if (this->energy_used_ != NULL) { 
            this->energy_used_ -> publish_state(value * 0.25); 
        }
        break;
    case opdata_unknown:
    default:
        // skip these values as they are not used currently
        break;
    }
}

} //namespace mhi
} //namespace esphome