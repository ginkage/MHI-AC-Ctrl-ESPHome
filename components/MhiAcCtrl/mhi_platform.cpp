#include "mhi_platform.h"

namespace esphome {
namespace mhi {

static const char* TAG = "mhi.platform";

void MhiPlatform::setup() {
    
    this->mhi_ac_ctrl_core_.MHIAcCtrlStatus(this);
    this->mhi_ac_ctrl_core_.init();
    this->mhi_ac_ctrl_core_.set_frame_size(this->frame_size_); // set framesize. Only 20 (legacy) or 33 (includes 3D auto and vertical vanes) possible

    if (this->external_temperature_sensor_ != nullptr) {
        this->external_temperature_sensor_->add_on_state_callback([this](float state) {        
            this->transfer_room_temperature(state);
        });
        this->transfer_room_temperature(this->external_temperature_sensor_->state);
    }
}

void MhiPlatform::set_frame_size(int framesize) {
    this->frame_size_ = framesize;
}

void MhiPlatform::set_room_temp_api_timeout(int time_in_seconds) {
    this->room_temp_api_timeout_ = time_in_seconds;
}

void MhiPlatform::set_external_room_temperature_sensor(Sensor* sensor) {
    this->external_temperature_sensor_ = sensor;
}

void MhiPlatform::loop() {
    if(millis() - room_temp_api_timeout_start_ >= room_temp_api_timeout_*1000) {
        mhi_ac_ctrl_core_.set_troom(0xff);  // use IU temperature sensor
        ESP_LOGD(TAG, "did not receive a room_temp_api value, using IU temperature sensor");
    }

    int ret = mhi_ac_ctrl_core_.loop(100);
    if (ret < 0) {
        ESP_LOGE(TAG, "mhi_ac_ctrl_core,loop error: %i", ret);
    }
}

void MhiPlatform::dump_config() {

    //LOG_CLIMATE("", "MHI-AC-Ctrl Climate", this)
}

void MhiPlatform:: cbiStatusFunction(ACStatus status, int value) {

    //ESP_LOGE(TAG, "received status=%i value=%i power=%i", status, value, this->power_);
    ESP_LOGD(TAG, "received status=%i value=%i", status, value);

    ESP_LOGE(TAG, "listeners=%i", this->listeners_.size());

    for (MhiStatusListener* listener:this->listeners_) {
        listener->update_status(status, value);
    }
}

void MhiPlatform::set_room_temperature(float value) {
    this->room_temp_api_timeout_start_ = millis(); // reset timeout
    this->transfer_room_temperature(value);
}

void MhiPlatform::transfer_room_temperature(float value) {
    if (isnan(value)) {
        mhi_ac_ctrl_core_.set_troom(0xff); // reset target, use internal sensor
    }

    if ((value > -10) & (value < 48)) {
        byte tmp = value * 4 + 61;
        this->mhi_ac_ctrl_core_.set_troom(value * 4 + 61);        
        ESP_LOGD(TAG, "set room_temp_api: %f %i %i", value, (byte)(value * 4 + 61), (byte)tmp);
    }
}

void MhiPlatform::set_power(ACPower value) {
    this->mhi_ac_ctrl_core_.set_power(value);
}
void MhiPlatform::set_fan(int value) {
    this->mhi_ac_ctrl_core_.set_fan(value);
}
void MhiPlatform::set_mode(ACMode value){
    this->mhi_ac_ctrl_core_.set_mode(value);
}
void MhiPlatform::set_tsetpoint(float value) {
    this->mhi_ac_ctrl_core_.set_tsetpoint((byte)(2 * value));
    
    ESP_LOGD(TAG, "set setpoint: %f", value);
}

void MhiPlatform::set_vanes(int value) {
    
    this->mhi_ac_ctrl_core_.set_vanes(value);
    ESP_LOGD(TAG, "set vanes: %i", value);
}

void MhiPlatform::set_vanesLR(int value) {

    if (this->frame_size_ == 33) {
    
        ESP_LOGD(TAG, "setting vanesLR to: %i", value);
        this->mhi_ac_ctrl_core_.set_vanesLR(value);
        ESP_LOGD(TAG, "set vanes Left Right: %i", value);
    } else {
        ESP_LOGD("main", "Not setting vanesLR: %i", value);
    }
}


void MhiPlatform::set_3Dauto(bool value) {
    if (this->frame_size_ == 33) {
        
        if (value) {
            ESP_LOGD(TAG, "set 3D auto: on");
            this->mhi_ac_ctrl_core_.set_3Dauto(AC3Dauto::Dauto_on); // Set swing to 3Dauto
        } else {
            ESP_LOGD(TAG, "set 3D auto: off");
            this->mhi_ac_ctrl_core_.set_3Dauto(AC3Dauto::Dauto_off); // Set swing to 3Dauto
        }
        ESP_LOGD(TAG, "set 3d auto: %i", value);
    } else {
        ESP_LOGD("main", "Not setting 3d auto: %i because of frame_size", value);
    }
}

void MhiPlatform::add_listener(MhiStatusListener* listener) {
    this->listeners_.push_back(listener);
}

} //namespace mhi
} //namespace esphome