#include "mhi_test_common.h"

#include "mhi_command_coordinator.h"

namespace mhi_unit_tests {
namespace {

MhiTxEnvelope prepare_command_envelope(MhiCommandCoordinator &coordinator, MhiCommandState &command,
                                       MhiTxRuntime &runtime, const MhiTxBuildConfig &config,
                                       MhiCommandState &before, MhiTxBuildResult *build_result = nullptr) {
  MhiFrameBuffer frame{};
  MhiTxBuildResult result{};
  MhiTxEnvelope envelope{};

  for (unsigned attempt = 0U; attempt < 2U; attempt++) {
    before = command;
    EXPECT_TRUE(coordinator.prepare_next(command, runtime, config, frame, result, envelope));
    if (envelope.is_command()) {
      if (build_result != nullptr) {
        *build_result = result;
      }
      return envelope;
    }
  }

  EXPECT_TRUE(false);
  return {};
}

MhiTxCompletion completion_for(const MhiTxEnvelope &envelope, bool success, uint32_t completed_at_ms) {
  MhiTxCompletion completion{};
  completion.generation = envelope.generation;
  completion.kind = envelope.kind;
  completion.command_mask = envelope.command_mask;
  completion.intent = envelope.intent;
  completion.success = success;
  completion.completed_at_ms = completed_at_ms;
  return completion;
}

}  // namespace

void command_coordinator_starts_confirmation_after_tx_completion() {
  MhiCommandCoordinator coordinator{};
  MhiCommandState command{};
  MhiTxRuntime runtime{};
  MhiTxBuildConfig config{};
  MhiCommandState before{};

  command.power_set = true;
  command.power = true;

  // Commands are encoded only on the command half of the alternating frame.
  EXPECT_TRUE(coordinator.prepare_next(command, runtime, config, frame, result, envelope));
  if (!envelope.is_command()) {
    EXPECT_TRUE(coordinator.prepare_next(command, runtime, config, frame, result, envelope));
  }

  EXPECT_TRUE(envelope.is_command());
  const MhiCommandState before{};
  coordinator.on_stage_result(envelope, before, command, true);
  EXPECT_TRUE(coordinator.has_command_in_flight());
  EXPECT_EQ(coordinator.pending_mask(), 0U);

  MhiTxCompletion completion{};
  completion.generation = envelope.generation;
  completion.kind = envelope.kind;
  completion.command_mask = envelope.command_mask;
  completion.intent = envelope.intent;
  completion.success = true;
  completion.completed_at_ms = 100U;

  EXPECT_TRUE(coordinator.on_tx_completion(completion, command));
  EXPECT_FALSE(coordinator.has_command_in_flight());
  EXPECT_EQ(coordinator.pending_mask(), MHI_COMMAND_POWER);
  EXPECT_EQ(coordinator.pending_age_ms(150U), 50U);
}

void command_coordinator_restores_command_when_stage_is_rejected() {
  MhiCommandCoordinator coordinator{};
  MhiCommandState command{};
  MhiTxRuntime runtime{};
  MhiTxBuildConfig config{};
  MhiFrameBuffer frame{};
  MhiTxBuildResult result{};
  MhiTxEnvelope envelope{};

  command.target_temp_set = true;
  command.target_temp_c = 23.5F;
  const MhiCommandState before = command;

  EXPECT_TRUE(coordinator.prepare_next(command, runtime, config, frame, result, envelope));
  if (!envelope.is_command()) {
    EXPECT_TRUE(coordinator.prepare_next(command, runtime, config, frame, result, envelope));
  }

  EXPECT_TRUE(envelope.is_command());
  EXPECT_FALSE(command.target_temp_set);
  coordinator.on_stage_result(envelope, before, command, false);
  EXPECT_TRUE(command.target_temp_set);
  expect_near(command.target_temp_c, 23.5F);
  EXPECT_FALSE(coordinator.has_command_in_flight());
  EXPECT_EQ(coordinator.pending_mask(), 0U);
}

void command_coordinator_requeues_failed_command() {
  MhiCommandCoordinator coordinator{};
  MhiCommandState command{};
  MhiTxRuntime runtime{};
  MhiTxBuildConfig config{};
  MhiFrameBuffer frame{};
  MhiTxBuildResult result{};
  MhiTxEnvelope envelope{};

  command.fan_set = true;
  command.fan = 6U;
  MhiCommandState before = command;

  EXPECT_TRUE(coordinator.prepare_next(command, runtime, config, frame, result, envelope));
  if (!envelope.is_command()) {
    EXPECT_TRUE(coordinator.prepare_next(command, runtime, config, frame, result, envelope));
  }
  EXPECT_TRUE(envelope.is_command());
  coordinator.on_stage_result(envelope, before, command, true);
  EXPECT_FALSE(command.fan_set);

  MhiTxCompletion completion{};
  completion.generation = envelope.generation;
  completion.kind = envelope.kind;
  completion.command_mask = envelope.command_mask;
  completion.intent = envelope.intent;
  completion.success = false;
  completion.completed_at_ms = 200U;

  EXPECT_TRUE(coordinator.on_tx_completion(completion, command));
  EXPECT_TRUE(command.fan_set);
  EXPECT_EQ(command.fan, 6U);
  EXPECT_EQ(coordinator.pending_mask(), 0U);
}

void command_coordinator_restores_vertical_vane_after_stage_rejection() {
  MhiCommandCoordinator coordinator{};
  MhiCommandState command{};
  MhiTxRuntime runtime{};
  MhiTxBuildConfig config{};
  MhiCommandState before{};

  command.vertical_vane_set = true;
  command.vertical_vane = 5U;

  const MhiTxEnvelope envelope = prepare_command_envelope(coordinator, command, runtime, config, before);
  EXPECT_EQ(envelope.command_mask, static_cast<uint32_t>(MHI_COMMAND_VERTICAL_VANE));
  EXPECT_EQ(envelope.intent.vertical_vane, 5U);
  EXPECT_FALSE(command.vertical_vane_set);

  coordinator.on_stage_result(envelope, before, command, false);
  EXPECT_TRUE(command.vertical_vane_set);
  EXPECT_EQ(command.vertical_vane, 5U);
  EXPECT_FALSE(coordinator.has_command_in_flight());
}

void command_coordinator_restores_horizontal_vane_after_tx_failure() {
  MhiCommandCoordinator coordinator{};
  MhiCommandState command{};
  MhiTxRuntime runtime{};
  MhiTxBuildConfig config{};
  MhiCommandState before{};
  config.frame_size = kMhiFrame33Bytes;

  command.horizontal_vane_set = true;
  command.horizontal_vane = 8U;

  const MhiTxEnvelope envelope = prepare_command_envelope(coordinator, command, runtime, config, before);
  EXPECT_EQ(envelope.command_mask, static_cast<uint32_t>(MHI_COMMAND_HORIZONTAL_VANE));
  EXPECT_EQ(envelope.intent.horizontal_vane, 8U);
  coordinator.on_stage_result(envelope, before, command, true);
  EXPECT_FALSE(command.horizontal_vane_set);

  EXPECT_TRUE(coordinator.on_tx_completion(completion_for(envelope, false, 300U), command));
  EXPECT_TRUE(command.horizontal_vane_set);
  EXPECT_EQ(command.horizontal_vane, 8U);
  EXPECT_FALSE(coordinator.has_pending_confirmation());
}

void command_coordinator_combines_vertical_and_horizontal_vanes() {
  MhiCommandCoordinator coordinator{};
  MhiCommandState command{};
  MhiTxRuntime runtime{};
  MhiTxBuildConfig config{};
  MhiCommandState before{};
  config.frame_size = kMhiFrame33Bytes;

  command.vertical_vane_set = true;
  command.vertical_vane = 3U;
  command.horizontal_vane_set = true;
  command.horizontal_vane = 6U;

  const MhiTxEnvelope envelope = prepare_command_envelope(coordinator, command, runtime, config, before);
  const uint32_t expected_mask = MHI_COMMAND_VERTICAL_VANE | MHI_COMMAND_HORIZONTAL_VANE;
  EXPECT_EQ(envelope.command_mask, expected_mask);
  EXPECT_EQ(envelope.intent.mask, expected_mask);
  EXPECT_EQ(envelope.intent.vertical_vane, 3U);
  EXPECT_EQ(envelope.intent.horizontal_vane, 6U);
  EXPECT_TRUE(envelope.intent.has_extended_louver_context);
  EXPECT_FALSE(command.vertical_vane_set);
  EXPECT_FALSE(command.horizontal_vane_set);

  coordinator.on_stage_result(envelope, before, command, true);
  EXPECT_TRUE(coordinator.on_tx_completion(completion_for(envelope, true, 400U), command));
  EXPECT_EQ(coordinator.pending_mask(), expected_mask);

  MhiStatusState status{};
  status.valid = true;
  status.vertical_vane = 3U;
  status.vanes_swing = false;
  status.has_horizontal_vane = true;
  status.horizontal_vane_swing = false;
  status.horizontal_vane = 6U;
  EXPECT_EQ(coordinator.observe_status(status), expected_mask);
  EXPECT_FALSE(coordinator.has_pending_confirmation());
}

void command_coordinator_preserves_3d_auto_louver_context() {
  MhiCommandCoordinator coordinator{};
  MhiCommandState command{};
  MhiTxRuntime runtime{};
  MhiTxBuildConfig config{};
  MhiCommandState before{};
  config.frame_size = kMhiFrame33Bytes;
  config.has_extended_louver_state = true;
  config.extended_louver_db16 = 0x1FU;
  config.extended_louver_db17 = 0x0BU;
  config.extended_louver_horizontal_swing = true;
  config.extended_louver_horizontal_vane = 0U;
  config.extended_louver_three_d_auto = false;

  command.three_d_auto_set = true;
  command.three_d_auto = true;

  const MhiTxEnvelope envelope = prepare_command_envelope(coordinator, command, runtime, config, before);
  EXPECT_EQ(envelope.command_mask, static_cast<uint32_t>(MHI_COMMAND_THREE_D_AUTO));
  EXPECT_TRUE(envelope.intent.three_d_auto);
  EXPECT_TRUE(envelope.intent.has_extended_louver_context);
  EXPECT_EQ(envelope.intent.horizontal_vane, 8U);
  EXPECT_FALSE(command.three_d_auto_set);

  coordinator.on_stage_result(envelope, before, command, true);
  EXPECT_TRUE(coordinator.on_tx_completion(completion_for(envelope, true, 500U), command));
  EXPECT_EQ(coordinator.pending_mask(), static_cast<uint32_t>(MHI_COMMAND_THREE_D_AUTO));

  MhiStatusState status{};
  status.valid = true;
  status.has_3d_auto = true;
  status.three_d_auto = true;
  status.has_horizontal_vane = true;
  status.horizontal_vane_swing = true;
  EXPECT_EQ(coordinator.observe_status(status), static_cast<uint32_t>(MHI_COMMAND_THREE_D_AUTO));
  EXPECT_FALSE(coordinator.has_pending_confirmation());
}

void command_coordinator_restores_3d_auto_after_tx_failure() {
  MhiCommandCoordinator coordinator{};
  MhiCommandState command{};
  MhiTxRuntime runtime{};
  MhiTxBuildConfig config{};
  MhiCommandState before{};
  config.frame_size = kMhiFrame33Bytes;

  command.three_d_auto_set = true;
  command.three_d_auto = true;

  const MhiTxEnvelope envelope = prepare_command_envelope(coordinator, command, runtime, config, before);
  coordinator.on_stage_result(envelope, before, command, true);
  EXPECT_FALSE(command.three_d_auto_set);

  EXPECT_TRUE(coordinator.on_tx_completion(completion_for(envelope, false, 600U), command));
  EXPECT_TRUE(command.three_d_auto_set);
  EXPECT_TRUE(command.three_d_auto);
  EXPECT_EQ(coordinator.pending_mask(), 0U);
}

void command_coordinator_ignores_background_and_stale_completions() {
  MhiCommandCoordinator coordinator{};
  MhiCommandState command{};
  MhiTxRuntime runtime{};
  MhiTxBuildConfig config{};
  MhiCommandState before{};

  command.power_set = true;
  command.power = true;
  const MhiTxEnvelope envelope = prepare_command_envelope(coordinator, command, runtime, config, before);
  coordinator.on_stage_result(envelope, before, command, true);

  MhiTxCompletion background{};
  background.kind = MhiTxKind::BACKGROUND;
  background.success = true;
  background.completed_at_ms = 700U;
  EXPECT_FALSE(coordinator.on_tx_completion(background, command));
  EXPECT_TRUE(coordinator.has_command_in_flight());
  EXPECT_EQ(coordinator.pending_mask(), 0U);

  MhiTxCompletion stale = completion_for(envelope, true, 701U);
  stale.generation++;
  EXPECT_FALSE(coordinator.on_tx_completion(stale, command));
  EXPECT_TRUE(coordinator.has_command_in_flight());
  EXPECT_EQ(coordinator.pending_mask(), 0U);

  EXPECT_TRUE(coordinator.on_tx_completion(completion_for(envelope, true, 702U), command));
  EXPECT_FALSE(coordinator.has_command_in_flight());
  EXPECT_EQ(coordinator.pending_mask(), static_cast<uint32_t>(MHI_COMMAND_POWER));
}

void command_coordinator_blocks_prepare_while_in_flight_or_confirming() {
  MhiCommandCoordinator coordinator{};
  MhiCommandState command{};
  MhiTxRuntime runtime{};
  MhiTxBuildConfig config{};
  MhiCommandState before{};

  command.mode_set = true;
  command.mode = 4U;
  const MhiTxEnvelope envelope = prepare_command_envelope(coordinator, command, runtime, config, before);
  coordinator.on_stage_result(envelope, before, command, true);

  command.fan_set = true;
  command.fan = 2U;
  MhiFrameBuffer blocked_frame{};
  MhiTxBuildResult blocked_result{};
  MhiTxEnvelope blocked_envelope{};
  EXPECT_FALSE(coordinator.prepare_next(command, runtime, config, blocked_frame, blocked_result, blocked_envelope));
  EXPECT_TRUE(command.fan_set);

  EXPECT_TRUE(coordinator.on_tx_completion(completion_for(envelope, true, 800U), command));
  EXPECT_FALSE(coordinator.prepare_next(command, runtime, config, blocked_frame, blocked_result, blocked_envelope));
  EXPECT_TRUE(command.fan_set);

  MhiStatusState status{};
  status.valid = true;
  status.power = true;
  status.mode = 4U;
  EXPECT_EQ(coordinator.observe_status(status), static_cast<uint32_t>(MHI_COMMAND_MODE));
  EXPECT_FALSE(coordinator.has_pending_confirmation());

  MhiCommandState second_before{};
  const MhiTxEnvelope second = prepare_command_envelope(coordinator, command, runtime, config, second_before);
  EXPECT_EQ(second.command_mask, static_cast<uint32_t>(MHI_COMMAND_FAN));
  EXPECT_EQ(second.intent.fan, 2U);
}

void command_coordinator_preserves_newer_same_field_after_tx_failure() {
  MhiCommandCoordinator coordinator{};
  MhiCommandState command{};
  MhiTxRuntime runtime{};
  MhiTxBuildConfig config{};
  MhiCommandState before{};

  command.fan_set = true;
  command.fan = 2U;
  const MhiTxEnvelope envelope = prepare_command_envelope(coordinator, command, runtime, config, before);
  coordinator.on_stage_result(envelope, before, command, true);

  // A newer Home Assistant request arrives while the old frame is in flight.
  command.fan_set = true;
  command.fan = 6U;

  EXPECT_TRUE(coordinator.on_tx_completion(completion_for(envelope, false, 900U), command));
  EXPECT_TRUE(command.fan_set);
  EXPECT_EQ(command.fan, 6U);
}

void command_coordinator_restores_failed_field_without_losing_new_unrelated_command() {
  MhiCommandCoordinator coordinator{};
  MhiCommandState command{};
  MhiTxRuntime runtime{};
  MhiTxBuildConfig config{};
  MhiCommandState before{};

  command.power_set = true;
  command.power = true;
  const MhiTxEnvelope envelope = prepare_command_envelope(coordinator, command, runtime, config, before);
  coordinator.on_stage_result(envelope, before, command, true);

  command.target_temp_set = true;
  command.target_temp_c = 24.0F;

  EXPECT_TRUE(coordinator.on_tx_completion(completion_for(envelope, false, 1000U), command));
  EXPECT_TRUE(command.power_set);
  EXPECT_TRUE(command.power);
  EXPECT_TRUE(command.target_temp_set);
  expect_near(command.target_temp_c, 24.0F);
}

void command_coordinator_assigns_increasing_generations() {
  MhiCommandCoordinator coordinator{};
  MhiCommandState command{};
  MhiTxRuntime runtime{};
  MhiTxBuildConfig config{};
  MhiCommandState before{};

  command.power_set = true;
  command.power = true;
  const MhiTxEnvelope first = prepare_command_envelope(coordinator, command, runtime, config, before);
  coordinator.on_stage_result(first, before, command, true);
  EXPECT_TRUE(coordinator.on_tx_completion(completion_for(first, true, 1100U), command));

  MhiStatusState status{};
  status.valid = true;
  status.power = true;
  EXPECT_EQ(coordinator.observe_status(status), static_cast<uint32_t>(MHI_COMMAND_POWER));

  command.mode_set = true;
  command.mode = 2U;
  const MhiTxEnvelope second = prepare_command_envelope(coordinator, command, runtime, config, before);
  EXPECT_TRUE(second.generation > first.generation);
  EXPECT_EQ(second.command_mask, static_cast<uint32_t>(MHI_COMMAND_MODE));
}

void tx_completion_queue_preserves_order_and_reports_overwrite() {
  MhiTxCompletionQueue<2U> queue{};
  MhiTxCompletion one{};
  MhiTxCompletion two{};
  MhiTxCompletion three{};
  one.generation = 1U;
  two.generation = 2U;
  three.generation = 3U;

  queue.push(one);
  queue.push(two);
  queue.push(three);
  EXPECT_EQ(queue.overwritten(), 1U);
  EXPECT_EQ(queue.size(), 2U);

  MhiTxCompletion out{};
  EXPECT_TRUE(queue.pop(out));
  EXPECT_EQ(out.generation, 2U);
  EXPECT_TRUE(queue.pop(out));
  EXPECT_EQ(out.generation, 3U);
  EXPECT_FALSE(queue.pop(out));
}

}  // namespace mhi_unit_tests
