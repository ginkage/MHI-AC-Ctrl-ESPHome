#include "mhi_platform.h"

int SCK_PIN = 14;
int MOSI_PIN = 13;
int MISO_PIN = 12;
namespace esphome {
namespace mhi {

static const char* TAG = "mhi.platform";
static const uint8_t PERSISTED_STATE_VERSION = 1;
static const uint32_t PERSISTED_STATE_KEY = 0x4D484901;
static const uint32_t STATE_SAVE_DELAY_MS = 5000;
static const uint32_t ERROR_LOG_INTERVAL_MS = 10000;
static const uint32_t DIAGNOSTICS_PUBLISH_INTERVAL_MS = 30000;


void MhiPlatform::setup() {
    this->state_preference_ = global_preferences->make_preference<PersistedState>(PERSISTED_STATE_KEY, true);
    PersistedState restored;
    if (this->state_preference_.load(&restored) && restored.version == PERSISTED_STATE_VERSION) {
        this->persisted_state_ = restored;
        ESP_LOGI(TAG, "Restored saved operating settings (valid fields: 0x%02X)", restored.valid);
    } else {
        ESP_LOGI(TAG, "No saved operating settings found");
    }
    
    if (this->sck_pin_ >= 0) { //
      SCK_PIN = this->sck_pin_;
    }
    if (this->mosi_pin_ >= 0) { //
      MOSI_PIN = this->mosi_pin_;
    }
    if (this->miso_pin_ >= 0) { //
      MISO_PIN = this->miso_pin_;
    }

    this->mhi_ac_ctrl_core_.MHIAcCtrlStatus(this);
    this->mhi_ac_ctrl_core_.init();
    this->mhi_ac_ctrl_core_.set_frame_size(this->frame_size_); // set framesize. Only 20 (legacy) or 33 (includes 3D auto and vertical vanes) possible
    this->mhi_ac_ctrl_core_.set_command_retries(this->command_retries_);

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

void MhiPlatform::set_command_retries(int retries) {
    this->command_retries_ = retries;
}

void MhiPlatform::set_room_temp_api_timeout(int time_in_seconds) {
    this->room_temp_api_timeout_ = time_in_seconds;
}

void MhiPlatform::set_external_room_temperature_sensor(sensor::Sensor* sensor) {
    this->external_temperature_sensor_ = sensor;
}

void MhiPlatform::loop() {
    this->save_state_if_due_();
    if (this->external_temperature_sensor_ != nullptr) {
        this->transfer_room_temperature(this->external_temperature_sensor_->state);
        this->room_temp_api_active_ = false;
    }

    if(this->room_temp_api_active_ && millis() - this->room_temp_api_timeout_start_ >= room_temp_api_timeout_*1000) {

        mhi_ac_ctrl_core_.set_troom(0xff);  // use IU temperature sensor
        ESP_LOGD(TAG, "did not receive a room_temp_api value, using IU temperature sensor");
        this->room_temp_api_active_ = false;
    }

    int ret = mhi_ac_ctrl_core_.loop(100);
    if (ret < 0) {
        this->communication_errors_total_++;
        this->suppressed_error_count_++;
        this->last_loop_error_ = ret;
        if (millis() - this->last_error_log_at_ >= ERROR_LOG_INTERVAL_MS) {
            ESP_LOGW(TAG, "Communication error %i (%u occurrences since last report)", ret,
                     this->suppressed_error_count_);
            this->last_error_log_at_ = millis();
            this->suppressed_error_count_ = 0;
        }
    } else if (ret > 0) {
        this->successful_frames_total_++;
        this->last_successful_frame_at_ = millis();
    }

    if (static_cast<int32_t>(millis() - this->diagnostics_publish_at_) >= 0) {
        this->diagnostics_publish_at_ = millis() + DIAGNOSTICS_PUBLISH_INTERVAL_MS;
        if (this->communication_errors_sensor_ != nullptr)
            this->communication_errors_sensor_->publish_state(this->communication_errors_total_);
        if (this->successful_frames_sensor_ != nullptr)
            this->successful_frames_sensor_->publish_state(this->successful_frames_total_);
        if (this->communication_success_rate_sensor_ != nullptr) {
            const uint32_t total = this->communication_errors_total_ + this->successful_frames_total_;
            this->communication_success_rate_sensor_->publish_state(
                total == 0 ? NAN : 100.0f * this->successful_frames_total_ / total);
        }
        if (this->last_valid_frame_age_sensor_ != nullptr) {
            this->last_valid_frame_age_sensor_->publish_state(
                this->last_successful_frame_at_ == 0 ? NAN : (millis() - this->last_successful_frame_at_) / 1000.0f);
        }
    }
}

void MhiPlatform::dump_config() {

    ESP_LOGCONFIG(TAG, "MHI Platform");
    if (external_temperature_sensor_ != NULL) {
        ESP_LOGCONFIG(TAG, "  external_temperature_sensor enabled!");
    }

    ESP_LOGCONFIG(TAG, "  frame_size: %d", this->frame_size_);
    ESP_LOGCONFIG(TAG, "  command_retries: %d", this->command_retries_);
    ESP_LOGCONFIG(TAG, "  room_temp_api_timeout: %d", this->room_temp_api_timeout_);
    ESP_LOGCONFIG(TAG, "  listeners count: %d", this->listeners_.size());
}

void MhiPlatform:: cbiStatusFunction(ACStatus status, int value) {
    ESP_LOGD(TAG, "received status=%i value=%i", status, value);

    this->update_persisted_state_(status, value);

    for (MhiStatusListener* listener:this->listeners_) {
        listener->update_status(status, value);
    }
}

void MhiPlatform::set_room_temperature(float value) {
    this->room_temp_api_timeout_start_ = millis(); // reset timeout
    this->room_temp_api_active_ = true;
    this->transfer_room_temperature(value);
}

bool MhiPlatform::get_room_temp_api_active() {
    return this->room_temp_api_active_;
}

float MhiPlatform::get_room_temp_offset() {
    return this->temperature_offset_;
}

void MhiPlatform::transfer_room_temperature(float value) {
    if (isnan(value)) {
        if (!isnan(this->last_room_temperature_)) {
            ESP_LOGD(TAG, "set room_temp_api: value is NaN, using internal sensor");
            mhi_ac_ctrl_core_.set_troom(0xff); // reset target, use internal sensor
            this->last_room_temperature_ = NAN; // reset last room temperature
        }
        return;
    }

    if (this->temperature_offset_ > 0.0f) {  // if we have a offset value add this before setting troom value
        float orig_value = value;
        value = orig_value + this->temperature_offset_;
        //ESP_LOGD(TAG, "offset %f for original temp %f -> new troom %f", this->temperature_offset_ , orig_value, value);
    }

    //ESP_LOGD(TAG, "value: %f - last_room_temperature_ %f ", value, last_room_temperature_);
    if (fabs(value - last_room_temperature_) < 0.01) {
        //ESP_LOGD(TAG, "value %f - last_room_temperature_ %f < 0.01, returning", value, last_room_temperature_);
        return;
    }

    if ((value > -10) && (value < 48)) {
        byte tmp = value * 4 + 61;
        this->mhi_ac_ctrl_core_.set_troom(value * 4 + 61);
        this->last_room_temperature_ = value; // store last room temperature
        ESP_LOGD(TAG, "set room_temp_api: %f %i %i", value, (byte)(value * 4 + 61), (byte)tmp);
    }
}

void MhiPlatform::set_power(ACPower value) {
    this->cancel_timeout("self_clean");
    if (value == power_on) {
        this->restore_operating_state_();
    }
    this->mhi_ac_ctrl_core_.set_power(value);
}
void MhiPlatform::set_fan(int value) {
    if (this->persisted_state_.fan != value || !(this->persisted_state_.valid & FIELD_FAN)) {
        this->persisted_state_.fan = value;
        this->persisted_state_.valid |= FIELD_FAN;
        this->mark_state_changed_();
    }
    this->mhi_ac_ctrl_core_.set_fan(value);
}
void MhiPlatform::set_mode(ACMode value){
    if (this->persisted_state_.mode != value || !(this->persisted_state_.valid & FIELD_MODE)) {
        this->persisted_state_.mode = value;
        this->persisted_state_.valid |= FIELD_MODE;
        this->mark_state_changed_();
    }
    this->mhi_ac_ctrl_core_.set_mode(value);
}
void MhiPlatform::set_tsetpoint(float value) {
    uint8_t setpoint = static_cast<uint8_t>(2 * value);
    if (this->persisted_state_.setpoint != setpoint || !(this->persisted_state_.valid & FIELD_SETPOINT)) {
        this->persisted_state_.setpoint = setpoint;
        this->persisted_state_.valid |= FIELD_SETPOINT;
        this->mark_state_changed_();
    }
    this->mhi_ac_ctrl_core_.set_tsetpoint(setpoint);
    
    ESP_LOGD(TAG, "set setpoint: %f", value);

    if (this->room_temp_api_active_ && !isnan(this->last_room_temperature_)) {
        // send external sensor again if setpoint changed
        float last_room_temperature_to_send = this->last_room_temperature_;
        this->last_room_temperature_ = NAN;
        this->transfer_room_temperature(last_room_temperature_to_send);
        ESP_LOGD(TAG, "resending external troom to value: %f", last_room_temperature_to_send);
    }
}
void MhiPlatform::set_offset(float value) {
    if (value < 0.01f) {
        // if 0.5 offset is reset add this to the last known room temperature
        this->last_room_temperature_ -= this->temperature_offset_;
    }
    this->temperature_offset_ = value;
    ESP_LOGD(TAG, "set temperature offset: %f", value);
}

void MhiPlatform::set_vanes(int value) {
    if (this->persisted_state_.vanes != value || !(this->persisted_state_.valid & FIELD_VANES)) {
        this->persisted_state_.vanes = value;
        this->persisted_state_.valid |= FIELD_VANES;
        this->mark_state_changed_();
    }
    this->mhi_ac_ctrl_core_.set_vanes(value);
    ESP_LOGD(TAG, "set vanes: %i", value);
}

void MhiPlatform::set_vanesLR(int value) {

    if (this->frame_size_ == 33) {
        if (this->persisted_state_.vanes_lr != value || !(this->persisted_state_.valid & FIELD_VANES_LR)) {
            this->persisted_state_.vanes_lr = value;
            this->persisted_state_.valid |= FIELD_VANES_LR;
            this->mark_state_changed_();
        }
        ESP_LOGD(TAG, "setting vanesLR to: %i", value);
        this->mhi_ac_ctrl_core_.set_vanesLR(value);
        ESP_LOGD(TAG, "set vanes Left Right: %i", value);
    } else {
        ESP_LOGD("main", "Not setting vanesLR: %i", value);
    }
}


void MhiPlatform::set_3Dauto(bool value) {
    if (this->frame_size_ == 33) {
        if (this->persisted_state_.auto_3d != value || !(this->persisted_state_.valid & FIELD_3D_AUTO)) {
            this->persisted_state_.auto_3d = value;
            this->persisted_state_.valid |= FIELD_3D_AUTO;
            this->mark_state_changed_();
        }
        
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

void MhiPlatform::set_silent_operation(bool value) {
    if (this->persisted_state_.silent_operation != value ||
        !(this->persisted_state_.valid & FIELD_SILENT_OPERATION)) {
        this->persisted_state_.silent_operation = value;
        this->persisted_state_.valid |= FIELD_SILENT_OPERATION;
        this->mark_state_changed_();
    }
    this->mhi_ac_ctrl_core_.set_silent_operation(value);
}

void MhiPlatform::start_self_clean() {
    this->cancel_timeout("self_clean");
    ESP_LOGI(TAG, "Starting emulated self-clean cycle for %u minutes", this->self_clean_duration_ms_ / 60000);
    this->mhi_ac_ctrl_core_.set_mode(mode_fan);
    this->mhi_ac_ctrl_core_.set_fan(1);
    this->mhi_ac_ctrl_core_.set_power(power_on);
    this->set_timeout("self_clean", this->self_clean_duration_ms_, [this]() {
        ESP_LOGI(TAG, "Emulated self-clean cycle complete");
        this->mhi_ac_ctrl_core_.set_power(power_off);
    });
}

void MhiPlatform::mark_state_changed_() {
    this->state_dirty_ = true;
    this->state_save_at_ = millis() + STATE_SAVE_DELAY_MS;
}

void MhiPlatform::save_state_if_due_() {
    if (!this->state_dirty_ || static_cast<int32_t>(millis() - this->state_save_at_) < 0) {
        return;
    }
    if (this->state_preference_.save(&this->persisted_state_) && global_preferences->sync()) {
        ESP_LOGD(TAG, "Saved operating settings");
        this->state_dirty_ = false;
    } else {
        ESP_LOGW(TAG, "Failed to save operating settings");
        this->state_save_at_ = millis() + STATE_SAVE_DELAY_MS;
    }
}

void MhiPlatform::restore_operating_state_() {
    const uint8_t valid = this->persisted_state_.valid;
    ESP_LOGI(TAG, "Restoring operating settings before power on (valid fields: 0x%02X)", valid);
    if (valid & FIELD_MODE)
        this->mhi_ac_ctrl_core_.set_mode(static_cast<ACMode>(this->persisted_state_.mode));
    if (valid & FIELD_SETPOINT)
        this->mhi_ac_ctrl_core_.set_tsetpoint(this->persisted_state_.setpoint);
    if (valid & FIELD_FAN)
        this->mhi_ac_ctrl_core_.set_fan(this->persisted_state_.fan);
    if (valid & FIELD_VANES)
        this->mhi_ac_ctrl_core_.set_vanes(this->persisted_state_.vanes);
    if (this->frame_size_ == 33 && (valid & FIELD_VANES_LR))
        this->mhi_ac_ctrl_core_.set_vanesLR(this->persisted_state_.vanes_lr);
    if (this->frame_size_ == 33 && (valid & FIELD_3D_AUTO))
        this->mhi_ac_ctrl_core_.set_3Dauto(this->persisted_state_.auto_3d ? AC3Dauto::Dauto_on : AC3Dauto::Dauto_off);
    if (valid & FIELD_SILENT_OPERATION)
        this->mhi_ac_ctrl_core_.set_silent_operation(this->persisted_state_.silent_operation);
}

void MhiPlatform::update_persisted_state_(ACStatus status, int value) {
    uint8_t field = 0;
    uint8_t *destination = nullptr;
    switch (status) {
        case status_mode:
            field = FIELD_MODE;
            destination = &this->persisted_state_.mode;
            break;
        case status_fan:
            field = FIELD_FAN;
            destination = &this->persisted_state_.fan;
            break;
        case status_vanes:
            field = FIELD_VANES;
            destination = &this->persisted_state_.vanes;
            break;
        case status_vanesLR:
            field = FIELD_VANES_LR;
            destination = &this->persisted_state_.vanes_lr;
            break;
        case status_3Dauto:
            field = FIELD_3D_AUTO;
            destination = &this->persisted_state_.auto_3d;
            value = value == 0b00000100;
            break;
        case status_tsetpoint:
            field = FIELD_SETPOINT;
            destination = &this->persisted_state_.setpoint;
            value &= 0x7f;
            break;
        case opdata_silent_operation:
            field = FIELD_SILENT_OPERATION;
            destination = &this->persisted_state_.silent_operation;
            value = value != 0;
            break;
        default:
            return;
    }

    if (*destination != value || !(this->persisted_state_.valid & field)) {
        *destination = static_cast<uint8_t>(value);
        this->persisted_state_.valid |= field;
        this->mark_state_changed_();
    }
}

void MhiPlatform::add_listener(MhiStatusListener* listener) {
    this->listeners_.push_back(listener);
}

} //namespace mhi
} //namespace esphome
