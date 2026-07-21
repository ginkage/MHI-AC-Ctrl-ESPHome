#include "mhi_test_common.h"

namespace mhi_unit_tests {

void command_confirmation_confirms_power_mode_and_vertical_vane() {
  MhiCommandConfirmation confirmation{};

  MhiCommandIntent intent{};
  intent.mask = MHI_COMMAND_POWER | MHI_COMMAND_MODE | MHI_COMMAND_VERTICAL_VANE;
  intent.power = true;
  intent.mode = 3U;
  intent.vertical_vane = 3U;

  confirmation.stage(intent, intent.mask, 1000U);
  EXPECT_EQ(confirmation.pending_mask(), intent.mask);

  MhiStatusState status{};
  status.valid = true;
  status.power = true;
  status.mode = 3U;
  status.vertical_vane = 3U;
  status.vanes_swing = false;

  const uint32_t confirmed = confirmation.observe_status(status);
  EXPECT_EQ(confirmed, intent.mask);
  EXPECT_FALSE(confirmation.has_pending());
}

void command_confirmation_keeps_partial_pending_until_later_status() {
  MhiCommandConfirmation confirmation{};

  MhiCommandIntent intent{};
  intent.mask = MHI_COMMAND_POWER | MHI_COMMAND_MODE;
  intent.power = true;
  intent.mode = 2U;

  confirmation.stage(intent, intent.mask, 1000U);

  MhiStatusState status{};
  status.valid = true;
  status.power = true;
  status.mode = 1U;

  EXPECT_EQ(confirmation.observe_status(status), static_cast<uint32_t>(MHI_COMMAND_POWER));
  EXPECT_EQ(confirmation.pending_mask(), static_cast<uint32_t>(MHI_COMMAND_MODE));

  status.mode = 2U;
  EXPECT_EQ(confirmation.observe_status(status), static_cast<uint32_t>(MHI_COMMAND_MODE));
  EXPECT_FALSE(confirmation.has_pending());
}

void command_confirmation_confirms_auto_fan() {
  MhiCommandConfirmation confirmation{};

  MhiCommandIntent intent{};
  intent.mask = MHI_COMMAND_FAN;
  intent.fan = 7U;

  confirmation.stage(intent, intent.mask, 1000U);
  EXPECT_EQ(confirmation.pending_mask(), static_cast<uint32_t>(MHI_COMMAND_FAN));

  MhiStatusState status{};
  status.valid = true;
  status.fan = 7U;

  EXPECT_EQ(confirmation.observe_status(status), static_cast<uint32_t>(MHI_COMMAND_FAN));
  EXPECT_FALSE(confirmation.has_pending());
}

void command_confirmation_confirms_supported_fan_codes() {
  MhiCommandConfirmation confirmation{};

  MhiCommandIntent intent{};
  intent.mask = MHI_COMMAND_FAN;
  intent.fan = 6U;

  confirmation.stage(intent, intent.mask, 1000U);
  EXPECT_EQ(confirmation.pending_mask(), static_cast<uint32_t>(MHI_COMMAND_FAN));

  MhiStatusState status{};
  status.valid = true;
  status.fan = 6U;

  EXPECT_EQ(confirmation.observe_status(status), static_cast<uint32_t>(MHI_COMMAND_FAN));
  EXPECT_FALSE(confirmation.has_pending());
}


void command_confirmation_confirms_quiet_fan_code_zero() {
  MhiCommandConfirmation confirmation{};

  MhiCommandIntent intent{};
  intent.mask = MHI_COMMAND_FAN;
  intent.fan = 0U;
  confirmation.stage(intent, intent.mask, 1000U);

  MhiStatusState status{};
  status.valid = true;
  status.fan = 0U;

  EXPECT_EQ(confirmation.observe_status(status), static_cast<uint32_t>(MHI_COMMAND_FAN));
  EXPECT_FALSE(confirmation.has_pending());
}

void command_confirmation_times_out_unconfirmed_commands() {
  MhiCommandConfirmation confirmation{};

  MhiCommandIntent intent{};
  intent.mask = MHI_COMMAND_TARGET_TEMP;
  intent.target_temp_c = 22.5f;

  confirmation.stage(intent, intent.mask, 1000U);
  EXPECT_EQ(confirmation.expire(1000U + kMhiCommandConfirmationTimeoutMs - 1U).mask, 0U);
  const MhiCommandExpiration expiration = confirmation.expire(1000U + kMhiCommandConfirmationTimeoutMs);
  EXPECT_EQ(expiration.mask, static_cast<uint32_t>(MHI_COMMAND_TARGET_TEMP));
  EXPECT_EQ(expiration.intent.target_temp_c, 22.5f);
  EXPECT_FALSE(confirmation.has_pending());
}



}  // namespace mhi_unit_tests

namespace mhi_unit_tests {

void command_confirmation_detects_duplicate_pending_commands() {
  MhiCommandConfirmation confirmation{};

  MhiCommandIntent intent{};
  intent.mask = MHI_COMMAND_FAN | MHI_COMMAND_TARGET_TEMP;
  intent.fan = 6U;
  intent.target_temp_c = 22.0f;

  confirmation.stage(intent, intent.mask, 1000U);

  MhiCommandState duplicate{};
  duplicate.fan_set = true;
  duplicate.fan = 6U;
  duplicate.target_temp_set = true;
  duplicate.target_temp_c = 22.0f;
  duplicate.mode_set = true;
  duplicate.mode = 2U;

  EXPECT_EQ(confirmation.duplicate_pending_mask(duplicate),
            static_cast<uint32_t>(MHI_COMMAND_FAN | MHI_COMMAND_TARGET_TEMP));

  duplicate.fan = 2U;
  EXPECT_EQ(confirmation.duplicate_pending_mask(duplicate), static_cast<uint32_t>(MHI_COMMAND_TARGET_TEMP));
}

void command_confirmation_reports_pending_age_for_settle_window() {
  MhiCommandConfirmation confirmation{};

  MhiCommandIntent intent{};
  intent.mask = MHI_COMMAND_HORIZONTAL_VANE;
  intent.horizontal_vane = 2U;

  confirmation.stage(intent, intent.mask, 1000U);

  EXPECT_EQ(confirmation.pending_age_ms(999U), 0U);
  EXPECT_EQ(confirmation.pending_age_ms(1000U), 0U);
  EXPECT_EQ(confirmation.pending_age_ms(1500U), 500U);
}

void command_confirmation_can_settle_extended_louver_pending_mask() {
  MhiCommandConfirmation confirmation{};

  MhiCommandIntent intent{};
  intent.mask = MHI_COMMAND_HORIZONTAL_VANE | MHI_COMMAND_THREE_D_AUTO;
  intent.horizontal_vane = 2U;
  intent.three_d_auto = true;

  confirmation.stage(intent, intent.mask, 1000U);

  EXPECT_EQ(confirmation.settle_pending_mask(MHI_COMMAND_HORIZONTAL_VANE),
            static_cast<uint32_t>(MHI_COMMAND_HORIZONTAL_VANE));
  EXPECT_EQ(confirmation.pending_mask(), static_cast<uint32_t>(MHI_COMMAND_THREE_D_AUTO));

  EXPECT_EQ(confirmation.settle_pending_mask(MHI_COMMAND_HORIZONTAL_VANE), 0U);
  EXPECT_EQ(confirmation.settle_pending_mask(MHI_COMMAND_THREE_D_AUTO),
            static_cast<uint32_t>(MHI_COMMAND_THREE_D_AUTO));
  EXPECT_FALSE(confirmation.has_pending());
}

void command_state_clears_pending_mask() {
  MhiCommandState command{};
  command.power_set = true;
  command.power = true;
  command.mode_set = true;
  command.mode = 2U;
  command.fan_set = true;
  command.fan = 6U;
  command.target_temp_set = true;
  command.target_temp_c = 22.0f;

  command.clear_pending_mask(MHI_COMMAND_FAN | MHI_COMMAND_TARGET_TEMP);

  EXPECT_TRUE(command.power_set);
  EXPECT_TRUE(command.mode_set);
  EXPECT_FALSE(command.fan_set);
  EXPECT_FALSE(command.target_temp_set);
  EXPECT_TRUE(command.has_pending_command());
  EXPECT_EQ(command.pending_command_mask(), static_cast<uint32_t>(MHI_COMMAND_POWER | MHI_COMMAND_MODE));
}

void command_confirmation_confirms_horizontal_vane_feedback() {
  MhiCommandConfirmation confirmation{};

  MhiCommandIntent intent{};
  intent.mask = MHI_COMMAND_HORIZONTAL_VANE;
  intent.horizontal_vane = 4U;

  confirmation.stage(intent, intent.mask, 1000U);
  EXPECT_EQ(confirmation.pending_mask(), static_cast<uint32_t>(MHI_COMMAND_HORIZONTAL_VANE));

  MhiStatusState status{};
  status.valid = true;
  status.has_horizontal_vane = true;
  status.horizontal_vane_swing = false;
  status.horizontal_vane = 3U;
  EXPECT_EQ(confirmation.observe_status(status), 0U);
  EXPECT_EQ(confirmation.pending_mask(), static_cast<uint32_t>(MHI_COMMAND_HORIZONTAL_VANE));

  status.horizontal_vane = 4U;
  EXPECT_EQ(confirmation.observe_status(status), static_cast<uint32_t>(MHI_COMMAND_HORIZONTAL_VANE));
  EXPECT_FALSE(confirmation.has_pending());
}

void command_confirmation_confirms_horizontal_swing_feedback() {
  MhiCommandConfirmation confirmation{};

  MhiCommandIntent intent{};
  intent.mask = MHI_COMMAND_HORIZONTAL_VANE;
  intent.horizontal_vane = 8U;

  confirmation.stage(intent, intent.mask, 1000U);
  EXPECT_EQ(confirmation.pending_mask(), static_cast<uint32_t>(MHI_COMMAND_HORIZONTAL_VANE));

  MhiStatusState status{};
  status.valid = true;
  status.has_horizontal_vane = true;
  status.horizontal_vane_swing = true;
  status.horizontal_vane = 0U;

  EXPECT_EQ(confirmation.observe_status(status), static_cast<uint32_t>(MHI_COMMAND_HORIZONTAL_VANE));
  EXPECT_FALSE(confirmation.has_pending());
}

void command_confirmation_confirms_3d_auto_feedback() {
  MhiCommandConfirmation confirmation{};

  MhiCommandIntent intent{};
  intent.mask = MHI_COMMAND_THREE_D_AUTO;
  intent.three_d_auto = true;

  confirmation.stage(intent, intent.mask, 1000U);
  EXPECT_EQ(confirmation.pending_mask(), static_cast<uint32_t>(MHI_COMMAND_THREE_D_AUTO));

  MhiStatusState status{};
  status.valid = true;
  status.has_3d_auto = true;
  status.three_d_auto = false;
  EXPECT_EQ(confirmation.observe_status(status), 0U);
  EXPECT_EQ(confirmation.pending_mask(), static_cast<uint32_t>(MHI_COMMAND_THREE_D_AUTO));

  status.three_d_auto = true;
  EXPECT_EQ(confirmation.observe_status(status), static_cast<uint32_t>(MHI_COMMAND_THREE_D_AUTO));
  EXPECT_FALSE(confirmation.has_pending());
}

void command_confirmation_accepts_3d_auto_when_louver_context_changes() {
  MhiCommandConfirmation confirmation{};

  MhiCommandIntent intent{};
  intent.mask = MHI_COMMAND_THREE_D_AUTO;
  intent.three_d_auto = true;
  intent.horizontal_vane = 8U;
  intent.has_extended_louver_context = true;

  confirmation.stage(intent, intent.mask, 1000U);

  MhiStatusState status{};
  status.valid = true;
  status.has_3d_auto = true;
  status.three_d_auto = true;
  status.has_horizontal_vane = true;
  status.horizontal_vane_swing = false;
  status.horizontal_vane = 2U;

  EXPECT_EQ(confirmation.observe_status(status), static_cast<uint32_t>(MHI_COMMAND_THREE_D_AUTO));
  EXPECT_FALSE(confirmation.has_pending());
}

void command_confirmation_supersedes_older_pending_value() {
  MhiCommandConfirmation confirmation{};

  MhiCommandIntent intent{};
  intent.mask = MHI_COMMAND_THREE_D_AUTO | MHI_COMMAND_TARGET_TEMP;
  intent.three_d_auto = true;
  intent.target_temp_c = 22.0f;
  confirmation.stage(intent, intent.mask, 1000U);

  MhiCommandState patch{};
  patch.three_d_auto_set = true;
  patch.three_d_auto = false;
  patch.target_temp_set = true;
  patch.target_temp_c = 24.0f;

  EXPECT_EQ(confirmation.supersede(patch),
            static_cast<uint32_t>(MHI_COMMAND_THREE_D_AUTO | MHI_COMMAND_TARGET_TEMP));
  EXPECT_FALSE(confirmation.has_pending());
}

void command_confirmation_uses_longer_timeout_for_extended_louver_commands() {
  MhiCommandConfirmation confirmation{};

  MhiCommandIntent intent{};
  intent.mask = MHI_COMMAND_THREE_D_AUTO;
  intent.three_d_auto = false;

  confirmation.stage(intent, intent.mask, 1000U);

  EXPECT_EQ(confirmation.expire(10999U).mask, 0U);
  EXPECT_TRUE(confirmation.has_pending());
  EXPECT_EQ(confirmation.expire(20999U).mask, 0U);
  EXPECT_TRUE(confirmation.has_pending());
  EXPECT_EQ(confirmation.expire(21000U).mask, static_cast<uint32_t>(MHI_COMMAND_THREE_D_AUTO));
  EXPECT_FALSE(confirmation.has_pending());
}

}  // namespace mhi_unit_tests
