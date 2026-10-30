#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "../mhi_platform.h"

namespace esphome {
namespace mhi {

class MhiSilentOperationSwitch : public switch_::Switch,
                                 public Component,
                                 public Parented<MhiPlatform>,
                                 protected MhiStatusListener {
 public:
  void write_state(bool state) override;

 protected:
  void setup() override;
  void dump_config() override;
  void update_status(ACStatus status, int value) override;
};

}  // namespace mhi
}  // namespace esphome
