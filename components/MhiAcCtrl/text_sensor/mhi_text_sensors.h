#pragma once

#include "esphome/components/text_sensor/text_sensor.h"
#include <vector>
#include <string>
#include "../mhi_platform.h"

namespace esphome {
namespace mhi {

class MhiTextSensors : public Component, public Parented<MhiPlatform>, protected MhiStatusListener {

public:
    void set_protection_state(TextSensor* sensor);
    void setup() override;
    void dump_config() override;
protected:
    void update_status(ACStatus status, int value) override;
private:

    TextSensor* protection_state_;

};

} //namespace mhi
} //namespace esphome
