#include "mhi_command.h"

namespace esphome {
namespace mhi_ac_ctrl {

uint32_t merge_command_patch(MhiCommandState& destination, const MhiCommandState& patch, uint32_t allowed_mask) {
  uint32_t merged_mask = 0U;

  if (patch.power_set && (allowed_mask & MHI_COMMAND_POWER) != 0U) {
    destination.power_set = true;
    destination.power = patch.power;
    merged_mask |= MHI_COMMAND_POWER;
  }
  if (patch.mode_set && (allowed_mask & MHI_COMMAND_MODE) != 0U) {
    destination.mode_set = true;
    destination.mode = patch.mode;
    merged_mask |= MHI_COMMAND_MODE;
  }
  if (patch.fan_set && (allowed_mask & MHI_COMMAND_FAN) != 0U) {
    destination.fan_set = true;
    destination.fan = patch.fan;
    merged_mask |= MHI_COMMAND_FAN;
  }
  if (patch.target_temp_set && (allowed_mask & MHI_COMMAND_TARGET_TEMP) != 0U) {
    destination.target_temp_set = true;
    destination.target_temp_c = patch.target_temp_c;
    merged_mask |= MHI_COMMAND_TARGET_TEMP;
  }
  if (patch.vertical_vane_set && (allowed_mask & MHI_COMMAND_VERTICAL_VANE) != 0U && patch.vertical_vane >= 1U &&
      patch.vertical_vane <= 5U) {
    destination.vertical_vane_set = true;
    destination.vertical_vane = patch.vertical_vane;
    merged_mask |= MHI_COMMAND_VERTICAL_VANE;
  }
  if (patch.horizontal_vane_set && (allowed_mask & MHI_COMMAND_HORIZONTAL_VANE) != 0U && patch.horizontal_vane >= 1U &&
      patch.horizontal_vane <= 8U) {
    destination.horizontal_vane_set = true;
    destination.horizontal_vane = patch.horizontal_vane;
    merged_mask |= MHI_COMMAND_HORIZONTAL_VANE;
  }
  if (patch.three_d_auto_set && (allowed_mask & MHI_COMMAND_THREE_D_AUTO) != 0U) {
    destination.three_d_auto_set = true;
    destination.three_d_auto = patch.three_d_auto;
    merged_mask |= MHI_COMMAND_THREE_D_AUTO;
  }
  if (patch.room_temp_override_set && (allowed_mask & MHI_COMMAND_ROOM_TEMP_OVERRIDE) != 0U) {
    destination.room_temp_override_set = true;
    destination.room_temp_override_raw = patch.room_temp_override_raw;
    merged_mask |= MHI_COMMAND_ROOM_TEMP_OVERRIDE;
  }
  if (patch.error_opdata_request && (allowed_mask & MHI_COMMAND_ERROR_OPDATA_REQUEST) != 0U) {
    destination.error_opdata_request = true;
    merged_mask |= MHI_COMMAND_ERROR_OPDATA_REQUEST;
  }

  return merged_mask;
}

}  // namespace mhi_ac_ctrl
}  // namespace esphome
