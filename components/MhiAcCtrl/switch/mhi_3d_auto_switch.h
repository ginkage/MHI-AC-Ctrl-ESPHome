#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "../mhi_platform.h"

using namespace esphome;
using namespace esphome::mhi;

namespace esphome {
namespace mhi {

class Mhi3dAutoSwitch : 
    public switch_::Switch, 
    public Component,
    public Parented<MhiPlatform>, 
    protected MhiStatusListener {
public:
  void write_state(bool state);
  void setup() override;
  void dump_config() override;
  
protected:
  void update_status(ACStatus status, int value) override;
};

} //namespace mhi
} //namespace esphome