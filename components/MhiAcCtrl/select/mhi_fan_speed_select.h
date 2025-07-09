#pragma once

#include "esphome/components/select/select.h"
#include "../mhi_platform.h"

namespace esphome {
namespace mhi {

class MhiFanSpeedSelect : 
    public Component, 
    public select::Select, 
    public Parented<MhiPlatform>,
    protected MhiStatusListener {
public:
    MhiFanSpeedSelect() = default;

protected:
    void control(const std::string &value) override;
    void setup() override;
    void dump_config() override;
    void update_status(ACStatus status, int value) override;
};

}
}