#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/version.h"
#include "mhi_ac_ctrl.h"

namespace esphome {
namespace mhi_ac_ctrl {

template <typename... Ts>
class SetVerticalVanesAction : public Action<Ts...> {
 public:
  explicit SetVerticalVanesAction(MhiAcCtrl* parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(int, position);

#if ESPHOME_VERSION_CODE >= VERSION_CODE(2025, 11, 0)
  void play(const Ts&... x) override
#else
  void play(Ts... x) override
#endif
  {
    if (this->parent_ == nullptr) {
      return;
    }

    const int position = this->position_.value(x...);
    if (position < 1 || position > 5) {
      return;
    }

    this->parent_->request_vertical_vane_command(static_cast<uint8_t>(position));
  }

 protected:
  MhiAcCtrl* parent_;
};

template <typename... Ts>
class SetHorizontalVanesAction : public Action<Ts...> {
 public:
  explicit SetHorizontalVanesAction(MhiAcCtrl* parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(int, position);

#if ESPHOME_VERSION_CODE >= VERSION_CODE(2025, 11, 0)
  void play(const Ts&... x) override
#else
  void play(Ts... x) override
#endif
  {
    if (this->parent_ == nullptr) {
      return;
    }

    const int position = this->position_.value(x...);
    if (position >= 1 && position <= 8) {
      this->parent_->request_horizontal_vane_command(static_cast<uint8_t>(position));
    }
  }

 protected:
  MhiAcCtrl* parent_;
};

template <typename... Ts>
class SetExternalRoomTemperatureAction : public Action<Ts...> {
 public:
  explicit SetExternalRoomTemperatureAction(MhiAcCtrl* parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(float, temperature);

#if ESPHOME_VERSION_CODE >= VERSION_CODE(2025, 11, 0)
  void play(const Ts&... x) override
#else
  void play(Ts... x) override
#endif
  {
    if (this->parent_ != nullptr) {
      this->parent_->set_external_room_temperature(this->temperature_.value(x...));
    }
  }

 protected:
  MhiAcCtrl* parent_;
};

}  // namespace mhi_ac_ctrl
}  // namespace esphome
