// Version 4.0
#pragma once

#include "MHI-AC-Ctrl-core.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/time.h"
#include <vector>
#include <string>
#include "mhi_status_listener.h"

#define ROOM_TEMP_MQTT 1

using namespace esphome;
using namespace esphome::climate;
using namespace esphome::sensor;
using namespace esphome::text_sensor;
using namespace esphome::binary_sensor;

namespace esphome {
namespace mhi {

class MhiPlatform : public Component, public CallbackInterface_Status {

public:

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
    void set_external_room_temperature_sensor(Sensor* sensor);
    void add_listener(MhiStatusListener* listener);


private:
    void transfer_room_temperature(float value);

    int frame_size_;
    unsigned long room_temp_api_timeout_start_ = millis();
    unsigned long room_temp_api_timeout_;

    MHI_AC_Ctrl_Core mhi_ac_ctrl_core_;

    Sensor* external_temperature_sensor_;

    std::vector<MhiStatusListener*> listeners_;
};



} //namespace mhi
} //namespace esphome
