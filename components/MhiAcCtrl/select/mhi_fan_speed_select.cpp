#include "mhi_fan_speed_select.h"

#include "esphome/core/log.h"

namespace esphome {
namespace mhi_ac_ctrl {

static const char* const TAG = "mhi.select.fan";

namespace {

static constexpr const char* const kFanAuto = "Auto";
static constexpr const char* const kFanQuiet = "Quiet";
static constexpr const char* const kFanLow = "Low";
static constexpr const char* const kFanMedium = "Medium";
static constexpr const char* const kFanHigh = "High";

}  // namespace

void MhiFanSpeedSelect::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MHI fan speed select");

  if (this->parent_ != nullptr) {
    if (this->parent_->fan_profile_supports_quiet()) {
      this->traits.set_options({kFanAuto, kFanQuiet, kFanLow, kFanMedium, kFanHigh});
    } else {
      this->traits.set_options({kFanAuto, kFanLow, kFanMedium, kFanHigh});
    }
    this->parent_->set_fan_speed_select(this);
  }
}

void MhiFanSpeedSelect::dump_config() {
  ESP_LOGCONFIG(TAG, "MHI Fan Speed Select");
}

void MhiFanSpeedSelect::control(const std::string& value) {
  if (this->parent_ == nullptr) {
    ESP_LOGW(TAG, "Ignoring fan speed command because parent is not set");
    return;
  }

  uint8_t fan_code = 0U;
  if (!fan_code_from_name_(this->parent_->fan_profile(), value, fan_code)) {
    ESP_LOGW(TAG, "Ignoring unsupported fan speed option for configured fan profile: %s", value.c_str());
    return;
  }

  this->parent_->request_fan_command(fan_code);

  ESP_LOGD(TAG, "Fan speed command staged: %s; waiting for confirmed MOSI state", value.c_str());
}

bool MhiFanSpeedSelect::fan_code_from_name_(MhiFanProfile profile, const std::string& value, uint8_t& out) {
  MhiFanMode mode = MhiFanMode::UNKNOWN;

  if (value == kFanQuiet) {
    mode = MhiFanMode::QUIET;
  } else if (value == kFanLow) {
    mode = MhiFanMode::LOW;
  } else if (value == kFanMedium) {
    mode = MhiFanMode::MEDIUM;
  } else if (value == kFanHigh) {
    mode = MhiFanMode::HIGH;
  } else if (value == kFanAuto) {
    mode = MhiFanMode::AUTO;
  }

  return mhi_fan_code_from_mode(profile, mode, out);
}

}  // namespace mhi_ac_ctrl
}  // namespace esphome