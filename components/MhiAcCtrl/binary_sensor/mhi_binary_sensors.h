#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "../mhi_platform.h"

using namespace esphome::binary_sensor;

namespace esphome {
namespace mhi {

class MhiBinarySensors : 
    public Component, 
    public Parented<MhiPlatform>, 
    protected MhiStatusListener {

public:
    void set_defrost(BinarySensor* sensor);    
    void set_vanes_3d_auto_enabled(BinarySensor* sensor);

protected:
    void setup() override;
    void dump_config() override;
    void update_status(ACStatus status, int value) override;
private:
    BinarySensor* defrost_;    
    BinarySensor* vanes_3d_auto_enabled_;
};

}
}
