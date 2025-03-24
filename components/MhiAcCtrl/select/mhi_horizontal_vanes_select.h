#pragma once

#include "../mhi_platform.h"
#include "esphome/components/select/select.h"

namespace esphome {
namespace mhi {

class MhiHorizontalVanesSelect : 
  public Component, 
  public select::Select, 
  public Parented<MhiPlatform>,
  protected MhiStatusListener {
public:
  MhiHorizontalVanesSelect() = default;

protected:
  void control(const std::string &value) override;
  void setup() override;
  void update_status(ACStatus status, int value) override;
};

}  // namespace mhi
}  // namespace esphome