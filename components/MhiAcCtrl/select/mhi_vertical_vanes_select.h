#pragma once

#include "esphome/components/select/select.h"
#include "../mhi_platform.h"

namespace esphome {
namespace mhi {

class MhiVerticalVanesSelect : 
  public Component, 
  public select::Select, 
  public Parented<MhiPlatform>,
  protected MhiStatusListener {
public:
  MhiVerticalVanesSelect() = default;

protected:
  void control(const std::string &value) override;
  void setup() override;
  void update_status(ACStatus status, int value) override;
};

}  // namespace mhi
}  // namespace esphome