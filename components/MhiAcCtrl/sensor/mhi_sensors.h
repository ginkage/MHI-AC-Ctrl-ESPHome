#pragma once

#include "esphome/components/sensor/sensor.h"
#include "../mhi_platform.h"

using namespace esphome::sensor;

namespace esphome {
namespace mhi {

class MhiSensors : 
    public Component, 
    public Parented<MhiPlatform>, 
    protected MhiStatusListener {

public:
    void set_error_code(Sensor* sensor);
    void set_outdoor_temperature(Sensor* sensor);
    void set_return_air_temperature(Sensor* sensor);
    void set_outdoor_unit_fan_speed(Sensor* sensor);
    void set_indoor_unit_fan_speed(Sensor* sensor);
    void set_compressor_frequency(Sensor* sensor);
    void set_indoor_unit_total_run_time(Sensor* sensor);
    void set_compressor_total_run_time(Sensor* sensor);
    void set_current_power(Sensor* sensor);
    void set_vanes_pos(Sensor* sensor);
    void set_energy_used(Sensor* sensor);
    void set_indoor_unit_thi_r1(Sensor* sensor);
    void set_indoor_unit_thi_r2(Sensor* sensor);
    void set_indoor_unit_thi_r3(Sensor* sensor);
    void set_outdoor_unit_tho_r1(Sensor* sensor);
    void set_outdoor_unit_expansion_valve(Sensor* sensor);
    void set_outdoor_unit_discharge_pipe(Sensor* sensor);
    void set_outdoor_unit_discharge_pipe_super_heat(Sensor* sensor);
    void set_protection_state_number(Sensor* sensor);
    void set_vanesLR_pos(Sensor* sensor);

protected:
    void setup() override;
    void dump_config() override;
    void update_status(ACStatus status, int value) override;
private:

    Sensor* error_code_{nullptr};
    Sensor* outdoor_temperature_{nullptr};
    Sensor* return_air_temperature_{nullptr};
    Sensor* outdoor_unit_fan_speed_{nullptr};
    Sensor* indoor_unit_fan_speed_{nullptr};
    Sensor* compressor_frequency_{nullptr};
    Sensor* indoor_unit_total_run_time_{nullptr};
    Sensor* compressor_total_run_time_{nullptr};
    Sensor* current_power_{nullptr};
    Sensor* vanes_pos_{nullptr};
    Sensor* energy_used_{nullptr};
    Sensor* indoor_unit_thi_r1_{nullptr};
    Sensor* indoor_unit_thi_r2_{nullptr};
    Sensor* indoor_unit_thi_r3_{nullptr};
    Sensor* outdoor_unit_tho_r1_{nullptr};
    Sensor* outdoor_unit_expansion_valve_{nullptr};
    Sensor* outdoor_unit_discharge_pipe_{nullptr};
    Sensor* outdoor_unit_discharge_pipe_super_heat_{nullptr};
    Sensor* protection_state_number_{nullptr};
    Sensor* vanesLR_pos_{nullptr};
    MhiPlatform* platform_{nullptr};
};

}
}
