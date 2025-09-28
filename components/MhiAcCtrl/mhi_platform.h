// Version 4.0
#pragma once

#include "MHI-AC-Ctrl-core.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/hal.h"
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

    void set_sck_pin(GPIOPin *sck_pin) { sck_pin_ = sck_pin; }
    void set_mosi_pin(GPIOPin *mosi_pin) { mosi_pin_ = mosi_pin; }
    void set_miso_pin(GPIOPin *miso_pin) { miso_pin_ = miso_pin; }

    void setup() override;
    void set_frame_size(int framesize);
    void set_room_temp_api_timeout(int time_in_seconds);
    void loop() override;
    void dump_config() override;
    void cbiStatusFunction(ACStatus status, int value) override;

    void set_room_temperature(float value);

    void set_power(ACPower value);
    void set_fan(int value);
    void set_mode(ACMode value);
    void set_tsetpoint(float value);
    void set_vanes(int value);
    void set_vanesLR(int value);
    void set_3Dauto(bool value);
    void set_external_room_temperature_sensor(sensor::Sensor* sensor);
    void add_listener(MhiStatusListener* listener);


private:
    void transfer_room_temperature(float value);
    float last_room_temperature_ = NAN;

    int frame_size_;
    unsigned long room_temp_api_timeout_start_ = millis();
    unsigned long room_temp_api_timeout_;
    bool room_temp_api_active_ = false;
    GPIOPin * sck_pin_;
    GPIOPin * mosi_pin_;
    GPIOPin * miso_pin_;

    MHI_AC_Ctrl_Core mhi_ac_ctrl_core_;

    sensor::Sensor* external_temperature_sensor_{nullptr};

    std::vector<MhiStatusListener*> listeners_;
};



} //namespace mhi
} //namespace esphome
