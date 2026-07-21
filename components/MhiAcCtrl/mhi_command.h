#pragma once

#include <cstdint>

namespace esphome {
namespace mhi_ac_ctrl {

enum MhiCommandMask : uint32_t {
  MHI_COMMAND_POWER = (1UL << 0),
  MHI_COMMAND_MODE = (1UL << 1),
  MHI_COMMAND_FAN = (1UL << 2),
  MHI_COMMAND_TARGET_TEMP = (1UL << 3),
  MHI_COMMAND_VERTICAL_VANE = (1UL << 4),
  MHI_COMMAND_HORIZONTAL_VANE = (1UL << 5),
  MHI_COMMAND_THREE_D_AUTO = (1UL << 6),
  MHI_COMMAND_ROOM_TEMP_OVERRIDE = (1UL << 7),
  MHI_COMMAND_ERROR_OPDATA_REQUEST = (1UL << 8),
};

struct MhiCommandIntent {
  uint32_t mask{0};

  bool power{false};
  uint8_t mode{0};
  uint8_t fan{0};
  float target_temp_c{0.0f};
  uint8_t vertical_vane{0};
  uint8_t horizontal_vane{0};
  bool three_d_auto{false};

  // Extended-louver commands are a composite DB16/DB17 state on 33-byte frames.
  // For 3D-only commands this captures the horizontal vane/swing state that must
  // be preserved while changing only the 3D bit. horizontal_vane=8 means swing.
  bool has_extended_louver_context{false};
};

struct MhiCommandState {
  // DB0: power + mode
  bool power_set{false};
  bool power{false};

  bool mode_set{false};
  uint8_t mode{0};

  // DB1 / DB6: fan
  bool fan_set{false};
  uint8_t fan{0};

  // DB2: target temperature
  bool target_temp_set{false};
  float target_temp_c{0.0f};

  // DB0 / DB1: vertical vanes
  bool vertical_vane_set{false};
  uint8_t vertical_vane{0};

  // DB16 / DB17: horizontal vanes, extended 33-byte frame only
  bool horizontal_vane_set{false};
  uint8_t horizontal_vane{0};

  // DB17: 3D auto, extended 33-byte frame only
  bool three_d_auto_set{false};
  bool three_d_auto{false};

  // DB3: room temperature override.
  // 0xFF means no override / use internal AC sensor.
  bool room_temp_override_set{false};
  uint8_t room_temp_override_raw{0xFF};

  // DB6 / DB9: request last-error opdata sequence
  bool error_opdata_request{false};

  void clear_pending_flags() {
    power_set = false;
    mode_set = false;
    fan_set = false;
    target_temp_set = false;
    vertical_vane_set = false;
    horizontal_vane_set = false;
    three_d_auto_set = false;
    room_temp_override_set = false;
    error_opdata_request = false;
  }

  void clear_pending_mask(uint32_t mask) {
    if ((mask & MHI_COMMAND_POWER) != 0U) {
      power_set = false;
    }
    if ((mask & MHI_COMMAND_MODE) != 0U) {
      mode_set = false;
    }
    if ((mask & MHI_COMMAND_FAN) != 0U) {
      fan_set = false;
    }
    if ((mask & MHI_COMMAND_TARGET_TEMP) != 0U) {
      target_temp_set = false;
    }
    if ((mask & MHI_COMMAND_VERTICAL_VANE) != 0U) {
      vertical_vane_set = false;
    }
    if ((mask & MHI_COMMAND_HORIZONTAL_VANE) != 0U) {
      horizontal_vane_set = false;
    }
    if ((mask & MHI_COMMAND_THREE_D_AUTO) != 0U) {
      three_d_auto_set = false;
    }
    if ((mask & MHI_COMMAND_ROOM_TEMP_OVERRIDE) != 0U) {
      room_temp_override_set = false;
    }
    if ((mask & MHI_COMMAND_ERROR_OPDATA_REQUEST) != 0U) {
      error_opdata_request = false;
    }
  }

  bool has_pending_command() const {
    return pending_command_mask() != 0U;
  }

  uint32_t pending_command_mask() const {
    uint32_t mask = 0U;

    if (power_set) {
      mask |= MHI_COMMAND_POWER;
    }
    if (mode_set) {
      mask |= MHI_COMMAND_MODE;
    }
    if (fan_set) {
      mask |= MHI_COMMAND_FAN;
    }
    if (target_temp_set) {
      mask |= MHI_COMMAND_TARGET_TEMP;
    }
    if (vertical_vane_set) {
      mask |= MHI_COMMAND_VERTICAL_VANE;
    }
    if (horizontal_vane_set) {
      mask |= MHI_COMMAND_HORIZONTAL_VANE;
    }
    if (three_d_auto_set) {
      mask |= MHI_COMMAND_THREE_D_AUTO;
    }
    if (room_temp_override_set) {
      mask |= MHI_COMMAND_ROOM_TEMP_OVERRIDE;
    }
    if (error_opdata_request) {
      mask |= MHI_COMMAND_ERROR_OPDATA_REQUEST;
    }

    return mask;
  }
};

// Merge the selected fields from a command patch into the destination state.
// The patch uses the existing *_set flags as field-presence markers.
// allowed_mask lets the caller reject invalid or duplicate fields before the
// merge while still applying the rest of the batch atomically.
uint32_t merge_command_patch(MhiCommandState& destination, const MhiCommandState& patch,
                             uint32_t allowed_mask = 0xFFFFFFFFUL);

}  // namespace mhi_ac_ctrl
}  // namespace esphome
