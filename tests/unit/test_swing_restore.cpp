#include "mhi_swing_restore.h"

#include "mhi_test_common.h"

namespace mhi_unit_tests {
namespace {
using esphome::mhi_ac_ctrl::MHI_COMMAND_HORIZONTAL_VANE;
using esphome::mhi_ac_ctrl::MHI_COMMAND_VERTICAL_VANE;
using esphome::mhi_ac_ctrl::MhiStatusState;
using esphome::mhi_ac_ctrl::MhiSwingRestore;
using esphome::mhi_ac_ctrl::MhiSwingSelection;

MhiStatusState fixed_status(uint8_t vertical = 1U, uint8_t horizontal = 6U) {
  MhiStatusState status{};
  status.valid = true;
  status.vertical_vane = vertical;
  status.has_horizontal_vane = true;
  status.horizontal_vane = horizontal;
  return status;
}
}  // namespace

void swing_restore_ignores_off_when_already_fixed() {
  MhiSwingRestore controller{};
  const auto plan = controller.plan(MhiSwingSelection::OFF, fixed_status(1U, 6U));
  EXPECT_FALSE(plan.patch.vertical_vane_set);
  EXPECT_FALSE(plan.patch.horizontal_vane_set);
  EXPECT_FALSE(plan.missing_vertical_restore);
  EXPECT_FALSE(plan.missing_horizontal_restore);
}

void swing_restore_snapshots_both_axes_and_restores_them() {
  MhiSwingRestore controller{};
  auto status = fixed_status(2U, 7U);
  const auto start = controller.plan(MhiSwingSelection::BOTH, status);
  EXPECT_EQ(start.saved_vertical, 2U);
  EXPECT_EQ(start.saved_horizontal, 7U);
  EXPECT_EQ(start.patch.vertical_vane, 5U);
  EXPECT_EQ(start.patch.horizontal_vane, 8U);
  controller.commit(start, MHI_COMMAND_VERTICAL_VANE | MHI_COMMAND_HORIZONTAL_VANE);

  status.vanes_swing = true;
  status.vertical_vane = 0U;
  status.horizontal_vane_swing = true;
  status.horizontal_vane = 0U;
  const auto repeat = controller.plan(MhiSwingSelection::BOTH, status);
  EXPECT_FALSE(repeat.patch.has_pending_command());

  const auto stop = controller.plan(MhiSwingSelection::OFF, status);
  EXPECT_TRUE(stop.patch.vertical_vane_set);
  EXPECT_TRUE(stop.patch.horizontal_vane_set);
  EXPECT_EQ(stop.patch.vertical_vane, 2U);
  EXPECT_EQ(stop.patch.horizontal_vane, 7U);
  controller.commit(stop, MHI_COMMAND_VERTICAL_VANE | MHI_COMMAND_HORIZONTAL_VANE);
  status = fixed_status(2U, 7U);
  EXPECT_FALSE(controller.plan(MhiSwingSelection::OFF, status).patch.has_pending_command());
}

void swing_restore_changes_axes_independently() {
  MhiSwingRestore controller{};
  auto status = fixed_status(3U, 5U);
  const auto vertical = controller.plan(MhiSwingSelection::VERTICAL, status);
  EXPECT_TRUE(vertical.patch.vertical_vane_set);
  EXPECT_FALSE(vertical.patch.horizontal_vane_set);
  controller.commit(vertical, MHI_COMMAND_VERTICAL_VANE);

  status.vanes_swing = true;
  status.vertical_vane = 0U;
  const auto horizontal = controller.plan(MhiSwingSelection::HORIZONTAL, status);
  EXPECT_TRUE(horizontal.patch.vertical_vane_set);
  EXPECT_EQ(horizontal.patch.vertical_vane, 3U);
  EXPECT_TRUE(horizontal.patch.horizontal_vane_set);
  EXPECT_EQ(horizontal.patch.horizontal_vane, 8U);
  controller.commit(horizontal, MHI_COMMAND_VERTICAL_VANE | MHI_COMMAND_HORIZONTAL_VANE);

  status = fixed_status(3U, 5U);
  status.horizontal_vane_swing = true;
  status.horizontal_vane = 0U;
  const auto stop = controller.plan(MhiSwingSelection::OFF, status);
  EXPECT_FALSE(stop.patch.vertical_vane_set);
  EXPECT_TRUE(stop.patch.horizontal_vane_set);
  EXPECT_EQ(stop.patch.horizontal_vane, 5U);
}

void swing_restore_never_fabricates_unknown_position() {
  MhiSwingRestore controller{};
  auto status = fixed_status();
  status.vanes_swing = true;
  status.vertical_vane = 0U;
  status.horizontal_vane_swing = true;
  status.horizontal_vane = 0U;
  const auto stop = controller.plan(MhiSwingSelection::OFF, status);
  EXPECT_FALSE(stop.patch.has_pending_command());
  EXPECT_TRUE(stop.missing_vertical_restore);
  EXPECT_TRUE(stop.missing_horizontal_restore);
}

void swing_restore_ignores_rejected_swing_and_supports_fast_toggle() {
  MhiSwingRestore controller{};
  const auto status = fixed_status(1U, 4U);
  const auto start = controller.plan(MhiSwingSelection::VERTICAL, status);
  controller.commit(start, 0U);
  EXPECT_FALSE(controller.plan(MhiSwingSelection::OFF, status).patch.has_pending_command());

  controller.commit(start, MHI_COMMAND_VERTICAL_VANE);
  // An OFF request may arrive before the swing command is confirmed on MOSI.
  const auto stop = controller.plan(MhiSwingSelection::OFF, status);
  EXPECT_TRUE(stop.patch.vertical_vane_set);
  EXPECT_EQ(stop.patch.vertical_vane, 1U);
}

void swing_restore_requires_valid_fixed_feedback() {
  MhiSwingRestore controller{};
  MhiStatusState status{};
  const auto start = controller.plan(MhiSwingSelection::BOTH, status);
  EXPECT_TRUE(start.patch.vertical_vane_set);
  EXPECT_TRUE(start.patch.horizontal_vane_set);
  controller.commit(start, MHI_COMMAND_VERTICAL_VANE | MHI_COMMAND_HORIZONTAL_VANE);
  const auto stop = controller.plan(MhiSwingSelection::OFF, status);
  EXPECT_FALSE(stop.patch.has_pending_command());
  EXPECT_TRUE(stop.missing_vertical_restore);
  EXPECT_TRUE(stop.missing_horizontal_restore);
}

}  // namespace mhi_unit_tests

namespace mhi_unit_tests {
void swing_restore_tracks_feedback_and_external_vane_changes() {
  MhiSwingRestore controller{};
  auto status = fixed_status(1U, 2U);
  controller.observe(status);
  auto start = controller.plan(MhiSwingSelection::VERTICAL, status);
  controller.commit(start, MHI_COMMAND_VERTICAL_VANE);

  status.vanes_swing = true;
  status.vertical_vane = 0U;
  controller.observe(status);
  const auto restore = controller.plan(MhiSwingSelection::OFF, status);
  EXPECT_EQ(restore.patch.vertical_vane, 1U);

  // The AC or a separate select later changes the stationary position.
  status.vanes_swing = false;
  status.vertical_vane = 3U;
  controller.observe(status);
  EXPECT_FALSE(controller.plan(MhiSwingSelection::OFF, status).patch.vertical_vane_set);
  start = controller.plan(MhiSwingSelection::VERTICAL, status);
  EXPECT_TRUE(start.patch.vertical_vane_set);
  EXPECT_EQ(start.saved_vertical, 3U);
}

void swing_restore_can_restore_select_started_swing() {
  MhiSwingRestore controller{};
  auto status = fixed_status(2U, 6U);
  controller.observe(status);

  // Swing was initiated through a select entity, not a climate request.
  status.vanes_swing = true;
  status.vertical_vane = 0U;
  controller.observe(status);
  const auto stop = controller.plan(MhiSwingSelection::OFF, status);
  EXPECT_TRUE(stop.patch.vertical_vane_set);
  EXPECT_EQ(stop.patch.vertical_vane, 2U);
  EXPECT_FALSE(stop.patch.horizontal_vane_set);
}
}  // namespace mhi_unit_tests
