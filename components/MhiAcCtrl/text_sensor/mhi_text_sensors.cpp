#include "mhi_text_sensors.h"

namespace esphome {
namespace mhi {

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

static const char* TAG = "mhi.text_sensor";

void MhiTextSensors::set_protection_state(text_sensor::TextSensor* sensor) { protection_state_ = sensor; }


void MhiTextSensors::setup() {
    this->parent_->add_listener(this);
}

void MhiTextSensors::dump_config() {

    ESP_LOGCONFIG(TAG, "MHI Text Sensors");
    if (protection_state_ != NULL) {
        ESP_LOGCONFIG(TAG, "  protection_state: %s", this->protection_state_->state);
    }
}

void MhiTextSensors::update_status(ACStatus status, int value) {
    ESP_LOGD(TAG, "received status=%i value=%i", status, value);
    if (status == opdata_protection_no && value < protection_states.size() && this->protection_state_ != NULL) {
        this->protection_state_ -> publish_state(protection_states[value]); 
        ESP_LOGD(TAG, "Protection status updated");
    }
}

} //namespace mhi
} //namespace esphome