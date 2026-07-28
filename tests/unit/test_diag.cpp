#include "mhi_test_common.h"

namespace mhi_unit_tests {

void diagnostics_snapshot_reports_event_ages() {
  MhiDiagnostics diagnostics{};

  diagnostics.set_rx_driver_name("fast_gpio");
  diagnostics.set_tx_driver_name("fast_gpio");
  diagnostics.set_rx_driver_ready(true);
  diagnostics.set_tx_driver_ready(true);

  diagnostics.stats().on_rx_chunk(1000U);
  diagnostics.stats().on_rx_bytes(20U, 1000U);
  diagnostics.stats().on_candidate_frame();
  diagnostics.stats().on_valid_frame(1200U);
  diagnostics.stats().on_tx_frame(1300U);

  const auto snapshot = diagnostics.snapshot(1500U);

  EXPECT_EQ(snapshot.stats.rx_chunks, 1U);
  EXPECT_EQ(snapshot.stats.rx_bytes, 20U);
  EXPECT_EQ(snapshot.stats.candidate_frames, 1U);
  EXPECT_EQ(snapshot.stats.valid_frames, 1U);
  EXPECT_EQ(snapshot.stats.tx_frames, 1U);

  EXPECT_TRUE(std::string(snapshot.rx_driver_name) == "fast_gpio");
  EXPECT_TRUE(std::string(snapshot.tx_driver_name) == "fast_gpio");
  EXPECT_TRUE(snapshot.rx_driver_ready);
  EXPECT_TRUE(snapshot.tx_driver_ready);

  EXPECT_EQ(snapshot.last_rx_byte_age_ms, 500U);
  EXPECT_EQ(snapshot.last_valid_frame_age_ms, 300U);
  EXPECT_EQ(snapshot.last_tx_frame_age_ms, 200U);
}

void diagnostics_snapshot_handles_missing_event_ages() {
  const MhiDiagnostics diagnostics{};
  const auto snapshot = diagnostics.snapshot(1500U);

  EXPECT_EQ(snapshot.last_rx_byte_age_ms, 0U);
  EXPECT_EQ(snapshot.last_valid_frame_age_ms, 0U);
  EXPECT_EQ(snapshot.last_tx_frame_age_ms, 0U);
}

}  // namespace mhi_unit_tests

namespace mhi_unit_tests {

void diagnostics_snapshot_reports_command_event_ages() {
  MhiDiagnostics diagnostics{};

  diagnostics.stats().on_tx_command_frame(MHI_COMMAND_FAN, 1000U);
  diagnostics.stats().on_unsupported_command(MHI_COMMAND_THREE_D_AUTO, 1200U);

  const auto snapshot = diagnostics.snapshot(1500U);

  EXPECT_EQ(snapshot.stats.tx_command_frames, 1U);
  EXPECT_EQ(snapshot.stats.unsupported_commands, 1U);
  EXPECT_EQ(snapshot.stats.last_tx_command_mask, static_cast<uint32_t>(MHI_COMMAND_FAN));
  EXPECT_EQ(snapshot.stats.last_unsupported_command_mask, static_cast<uint32_t>(MHI_COMMAND_THREE_D_AUTO));
  EXPECT_EQ(snapshot.last_tx_command_frame_age_ms, 500U);
  EXPECT_EQ(snapshot.last_unsupported_command_age_ms, 300U);
}

}  // namespace mhi_unit_tests

namespace mhi_unit_tests {

void diagnostics_snapshot_reports_command_confirmation_event_ages() {
  MhiDiagnostics diagnostics{};

  diagnostics.stats().on_command_confirmed(MHI_COMMAND_POWER, 2000U);
  diagnostics.stats().on_command_confirmation_timeout(MHI_COMMAND_VERTICAL_VANE, 2500U);
  diagnostics.stats().on_command_retry(MHI_COMMAND_TARGET_TEMP, 2600U);
  diagnostics.stats().on_command_retry_exhausted(MHI_COMMAND_THREE_D_AUTO, 2700U);
  diagnostics.stats().on_command_staged_timeout(MHI_COMMAND_MODE, 2800U);

  const auto snapshot = diagnostics.snapshot(3000U);

  EXPECT_EQ(snapshot.stats.command_confirmations, 1U);
  EXPECT_EQ(snapshot.stats.command_confirmation_timeouts, 1U);
  EXPECT_EQ(snapshot.stats.last_confirmed_command_mask, static_cast<uint32_t>(MHI_COMMAND_POWER));
  EXPECT_EQ(snapshot.stats.last_command_confirmation_timeout_mask, static_cast<uint32_t>(MHI_COMMAND_VERTICAL_VANE));
  EXPECT_EQ(snapshot.stats.command_retries, 1U);
  EXPECT_EQ(snapshot.stats.command_retry_exhaustions, 1U);
  EXPECT_EQ(snapshot.stats.command_staged_timeouts, 1U);
  EXPECT_EQ(snapshot.stats.last_command_retry_mask, static_cast<uint32_t>(MHI_COMMAND_TARGET_TEMP));
  EXPECT_EQ(snapshot.stats.last_command_retry_exhaustion_mask, static_cast<uint32_t>(MHI_COMMAND_THREE_D_AUTO));
  EXPECT_EQ(snapshot.stats.last_command_staged_timeout_mask, static_cast<uint32_t>(MHI_COMMAND_MODE));
  EXPECT_EQ(snapshot.last_command_confirmation_age_ms, 1000U);
  EXPECT_EQ(snapshot.last_command_confirmation_timeout_age_ms, 500U);
}

}  // namespace mhi_unit_tests

namespace mhi_unit_tests {

void diagnostics_snapshot_reports_loop_budget_timing() {
  MhiDiagnostics diagnostics{};

  diagnostics.stats().on_loop_timing(1000U, 10U, 20U, 30U, 40U, 50U, 30000U, 1000U);
  diagnostics.stats().on_loop_timing(50000U, 110U, 220U, 330U, 440U, 550U, 30000U, 2000U);

  const auto snapshot = diagnostics.snapshot(3000U);

  EXPECT_EQ(snapshot.stats.loop_iterations, 2U);
  EXPECT_EQ(snapshot.stats.loop_over_budget, 1U);
  EXPECT_EQ(snapshot.stats.loop_budget_us, 30000U);

  EXPECT_EQ(snapshot.stats.loop_last_us, 50000U);
  EXPECT_EQ(snapshot.stats.loop_avg_us, 25500U);
  EXPECT_EQ(snapshot.stats.loop_max_us, 50000U);
  EXPECT_EQ(snapshot.last_loop_over_budget_age_ms, 1000U);

  EXPECT_EQ(snapshot.stats.transport_loop_last_us, 110U);
  EXPECT_EQ(snapshot.stats.transport_loop_avg_us, 60U);
  EXPECT_EQ(snapshot.stats.transport_loop_max_us, 110U);

  EXPECT_EQ(snapshot.stats.tx_stage_last_us, 220U);
  EXPECT_EQ(snapshot.stats.tx_stage_avg_us, 120U);
  EXPECT_EQ(snapshot.stats.tx_stage_max_us, 220U);

  EXPECT_EQ(snapshot.stats.rx_read_sync_last_us, 330U);
  EXPECT_EQ(snapshot.stats.rx_read_sync_avg_us, 180U);
  EXPECT_EQ(snapshot.stats.rx_read_sync_max_us, 330U);

  EXPECT_EQ(snapshot.stats.publish_last_us, 440U);
  EXPECT_EQ(snapshot.stats.publish_avg_us, 240U);
  EXPECT_EQ(snapshot.stats.publish_max_us, 440U);

  EXPECT_EQ(snapshot.stats.command_housekeeping_last_us, 550U);
  EXPECT_EQ(snapshot.stats.command_housekeeping_avg_us, 300U);
  EXPECT_EQ(snapshot.stats.command_housekeeping_max_us, 550U);
}

}  // namespace mhi_unit_tests
