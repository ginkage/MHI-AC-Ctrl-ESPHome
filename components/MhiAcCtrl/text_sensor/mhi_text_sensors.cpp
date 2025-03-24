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

void MhiTextSensors::set_protection_state (TextSensor* sensor) { protection_state_ = sensor; }


void MhiTextSensors::setup() {
    this->parent_->add_listener(this);
}

void MhiTextSensors::dump_config() {

    //LOG_CLIMATE(TAG, "MHI-AC-Ctrl Text Sensors", this);
}

void MhiTextSensors:: update_status(ACStatus status, int value) {
    ESP_LOGE(TAG, "received status=%i value=%i", status, value);
    if (status == opdata_protection_no && value < protection_states.size() && this->protection_state_ != NULL) {
        this->protection_state_ -> publish_state(protection_states[value]); 
        ESP_LOGE(TAG, "Protection status updated");
    }
}

} //namespace mhi
} //namespace esphome