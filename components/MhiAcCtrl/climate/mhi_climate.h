#pragma once

#include "esphome/components/climate/climate.h"
#include "esphome/core/time.h"
#include <vector>
#include "../mhi_platform.h"

using namespace esphome::climate;

namespace esphome {
namespace mhi {

class MhiClimate : 
    public climate::Climate, 
    public Component, 
    public Parented<MhiPlatform>, 
    protected MhiStatusListener {

protected:

    void setup() override;
    void dump_config() override;
    void control(const climate::ClimateCall& call) override;
    climate::ClimateTraits traits() override;
    void update_status(ACStatus status, int value) override;

private:
    bool temperature_offset_enabled_{false};
    float temperature_offset_{0.0};
    float minimum_temperature_{18.0f};
    float maximum_temperature_{30.0f};
    float temperature_step_{0.5f};

    ACPower power_;
    ACMode mode_;
    float tsetpoint_;
    uint fan_;
    ACVanes vanes_;
    ACVanesLR vanesLR_;
    int vanesLR_pos_old_state_;
    int vanesLR_pos_state_;
    int vanes_pos_old_state_;
    int vanes_pos_state_;
    MhiPlatform* platform_;

public:
    void set_temperature_offset_enabled(bool enabled) { 
        this->temperature_offset_enabled_ = enabled; 
    }

    void set_minimum_temperature(float temp) { 
        this->minimum_temperature_ = temp; 
    }
};

}
}
