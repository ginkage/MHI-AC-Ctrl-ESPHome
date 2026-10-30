#pragma once

#include "esphome/components/button/button.h"
#include "esphome/core/component.h"
#include "../mhi_platform.h"

namespace esphome {
namespace mhi {

class MhiSelfCleanButton : public button::Button,
                           public Component,
                           public Parented<MhiPlatform> {
 protected:
  void press_action() override;
  void dump_config() override;
};

}  // namespace mhi
}  // namespace esphome
