#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/version.h"
#include "mhi_platform.h"

namespace esphome {
namespace mhi {

template<typename... Ts> class SetVerticalVanesAction : public Action<Ts...> {
public:
  SetVerticalVanesAction(MhiPlatform *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(int, position);
  
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2025, 11, 0)
  void play(const Ts&... x) override
#else
  void play(Ts... x) override
#endif
  {
    int pos = this->position_.value(x...);
    if (pos > 0 && pos < 6) {    
      this->parent_->set_vanes(pos);
    }    
  }

protected:
  MhiPlatform *parent_;
};


template<typename... Ts> class SetHorizontalVanesAction : public Action<Ts...> {
public:
  SetHorizontalVanesAction(MhiPlatform *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(int, position);
  
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2025, 11, 0)
  void play(const Ts&... x) override
#else
  void play(Ts... x) override
#endif
  {
    int pos = this->position_.value(x...);
    if (pos > 0 && pos < 9) {    
      this->parent_->set_vanesLR(pos);
    }
  }
  
protected:
  MhiPlatform *parent_;
};

template<typename... Ts> class SetExternalRoomTemperatureAction : public Action<Ts...> {
public:
  SetExternalRoomTemperatureAction(MhiPlatform *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(float, temperature);
  
  

#if ESPHOME_VERSION_CODE >= VERSION_CODE(2025, 11, 0)
  void play(const Ts&... x) override
#else
  void play(Ts... x) override
#endif
  {
    float temp = this->temperature_.value(x...);
    this->parent_->set_room_temperature(temp);    
  }

protected:
  MhiPlatform *parent_;
};


} //namespace mhi
} //namespace esphome