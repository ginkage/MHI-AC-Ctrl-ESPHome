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

    Sensor* error_code_;
    Sensor* outdoor_temperature_;
    Sensor* return_air_temperature_;
    Sensor* outdoor_unit_fan_speed_;
    Sensor* indoor_unit_fan_speed_;
    Sensor* compressor_frequency_;
    Sensor* indoor_unit_total_run_time_;
    Sensor* compressor_total_run_time_;
    Sensor* current_power_;
    Sensor* vanes_pos_;
    Sensor* energy_used_;
    Sensor* indoor_unit_thi_r1_;
    Sensor* indoor_unit_thi_r2_;
    Sensor* indoor_unit_thi_r3_;
    Sensor* outdoor_unit_tho_r1_;
    Sensor* outdoor_unit_expansion_valve_;
    Sensor* outdoor_unit_discharge_pipe_;
    Sensor* outdoor_unit_discharge_pipe_super_heat_;
    Sensor* protection_state_number_;
    Sensor* vanesLR_pos_;
};

}
}
