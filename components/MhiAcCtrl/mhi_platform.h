// Version 4.0
#pragma once

#include "MHI-AC-Ctrl-core.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/preferences.h"
#include "esphome/core/time.h"
#include <vector>
#include <string>
#include "mhi_status_listener.h"

namespace esphome {
namespace mhi {

class MhiPlatform : 
    public Component, 
    public CallbackInterface_Status {

public:

    void setup() override;
    void set_frame_size(int framesize);
    void set_command_retries(int retries);
    void set_room_temp_api_timeout(int time_in_seconds);
    void loop() override;
    void dump_config() override;
    void cbiStatusFunction(ACStatus status, int value) override;

    void set_room_temperature(float value);

    bool get_room_temp_api_active();
    float get_room_temp_offset();
    
    void set_power(ACPower value);
    void set_fan(int value);
    void set_mode(ACMode value);
    void set_tsetpoint(float value);
    void set_offset(float value);
    void set_vanes(int value);
    void set_vanesLR(int value);
    void set_3Dauto(bool value);
    void set_silent_operation(bool value);
    void set_communication_errors_sensor(sensor::Sensor *sensor) { this->communication_errors_sensor_ = sensor; }
    void set_successful_frames_sensor(sensor::Sensor *sensor) { this->successful_frames_sensor_ = sensor; }
    void set_communication_success_rate_sensor(sensor::Sensor *sensor) {
        this->communication_success_rate_sensor_ = sensor;
    }
    void set_last_valid_frame_age_sensor(sensor::Sensor *sensor) { this->last_valid_frame_age_sensor_ = sensor; }
    void start_self_clean();
    void set_self_clean_duration(uint32_t duration_ms) { this->self_clean_duration_ms_ = duration_ms; }
    void set_external_room_temperature_sensor(sensor::Sensor* sensor);
    void set_sck_pin(int pin) { this->sck_pin_ = pin; };
    void set_mosi_pin(int pin) { this->mosi_pin_ = pin; };
    void set_miso_pin(int pin) { this->miso_pin_ = pin; };
    void add_listener(MhiStatusListener* listener);


private:
    struct PersistedState {
        uint8_t version;
        uint8_t valid;
        uint8_t mode;
        uint8_t fan;
        uint8_t vanes;
        uint8_t vanes_lr;
        uint8_t auto_3d;
        uint8_t setpoint;
        uint8_t silent_operation;
    };

    enum PersistedField : uint8_t {
        FIELD_MODE = 1 << 0,
        FIELD_FAN = 1 << 1,
        FIELD_VANES = 1 << 2,
        FIELD_VANES_LR = 1 << 3,
        FIELD_3D_AUTO = 1 << 4,
        FIELD_SETPOINT = 1 << 5,
        FIELD_SILENT_OPERATION = 1 << 6,
    };

    void mark_state_changed_();
    void save_state_if_due_();
    void restore_operating_state_();
    void update_persisted_state_(ACStatus status, int value);

    void transfer_room_temperature(float value);
    float last_room_temperature_ = NAN; 
    float temperature_offset_ = 0.0f;

    int frame_size_;
    int command_retries_{2};
    unsigned long room_temp_api_timeout_start_ = millis();
    unsigned long room_temp_api_timeout_;
    bool room_temp_api_active_ = false;
    int sck_pin_ = -1;
    int mosi_pin_ = -1;
    int miso_pin_ = -1;

    MHI_AC_Ctrl_Core mhi_ac_ctrl_core_;
    ESPPreferenceObject state_preference_;
    PersistedState persisted_state_{1, 0, 0, 7, 4, 4, 0, 40, 0};
    bool state_dirty_{false};
    uint32_t state_save_at_{0};
    int last_loop_error_{0};
    uint32_t last_error_log_at_{0};
    uint32_t suppressed_error_count_{0};
    uint32_t communication_errors_total_{0};
    uint32_t successful_frames_total_{0};
    uint32_t last_successful_frame_at_{0};
    uint32_t diagnostics_publish_at_{0};
    uint32_t self_clean_duration_ms_{7200000};

    sensor::Sensor* external_temperature_sensor_{nullptr};
    sensor::Sensor* communication_errors_sensor_{nullptr};
    sensor::Sensor* successful_frames_sensor_{nullptr};
    sensor::Sensor* communication_success_rate_sensor_{nullptr};
    sensor::Sensor* last_valid_frame_age_sensor_{nullptr};

    std::vector<MhiStatusListener*> listeners_;
};



} //namespace mhi
} //namespace esphome
