
#include <iostream>

#include "mhi_test_common.h"

int main() {
  using namespace mhi_unit_tests;

  checksum_accepts_valid_20_byte_frame();
  checksum_rejects_bad_20_byte_frame();

  frame_sync_discards_garbage_and_extracts_valid_frame();
  frame_sync_waits_for_partial_frame();
  frame_sync_records_resync_stats();
  frame_sync_records_checksum_failure_stats();
  frame_sync_33_byte_mode_consumes_full_frame_without_tail_resync_noise();

  frame_queue_preserves_complete_frames();
  frame_queue_overwrites_oldest_complete_frame();

  duplex_tx_mailbox_stages_and_consumes_20_byte_frame();
  duplex_tx_mailbox_latest_stage_replaces_unclaimed_frame();
  duplex_tx_mailbox_rejects_invalid_frames_without_losing_pending_data();

  frame_classifier_classifies_status_opdata_and_extended_status();
  frame_catalog_overwrites_repeated_status_with_latest();
  frame_catalog_keeps_opdata_slots_separate_by_key();
  frame_catalog_keeps_command_candidate_side_slot_latest_only();
  frame_catalog_reports_unknown_frames();

  fan_profile_defaults_to_four_speed();
  fan_profile_three_speed_collapses_code_zero_to_low();
  fan_profile_four_speed_exposes_code_zero_as_quiet();
  fan_profile_encodes_quiet_only_for_four_speed();

  status_decoder_decodes_core_fields();
  status_decoder_preserves_protocol_fan_code_zero();
  status_decoder_decodes_33_byte_vane_feedback();
  status_decoder_ignores_unknown_horizontal_vane_feedback();
  status_decoder_ignores_33_byte_vane_feedback_on_opdata_frames();

  opdata_decoder_decodes_outdoor_temp();
  opdata_decoder_decodes_return_air_temp();
  opdata_decoder_decodes_compressor_frequency();
  opdata_decoder_decodes_current();
  opdata_decoder_decodes_indoor_unit_fan_speed();
  opdata_decoder_decodes_outdoor_unit_fan_speed();
  opdata_decoder_decodes_indoor_unit_total_run_time();
  opdata_decoder_decodes_compressor_total_run_time();
  opdata_decoder_decodes_energy_used();
  opdata_decoder_decodes_temperature_and_protection_slice2();

  publish_bridge_republishes_cached_state_after_targets_are_registered();
  publish_bridge_publishes_sensor_parity_slice1_on_first_opdata_publish();
  publish_bridge_publishes_sensor_parity_slice2_on_first_opdata_publish();
  publish_bridge_maps_unknown_protection_state();
  publish_bridge_maps_mhi_auto_to_heat_cool_for_ha_setpoint_ui();
  publish_bridge_three_speed_maps_code_zero_to_low();
  publish_bridge_four_speed_maps_code_zero_to_quiet();
  publish_bridge_publishes_sensor_parity_slice3_vane_feedback();
  publish_bridge_suppresses_unchanged_sensor_republishes();
  publish_bridge_suppresses_alternating_climate_current_temperature_chatter();
  publish_bridge_rate_limits_low_priority_climate_current_temperature_change();
  publish_bridge_does_not_force_low_priority_current_temp_when_other_climate_fields_change();
  publish_bridge_publishes_high_priority_climate_current_temperature_change_immediately();

  tx_builder_emits_valid_default_20_byte_frame();
  tx_builder_applies_pending_commands_once();
  tx_builder_uses_configured_sensor_parity_opdata_mask();
  tx_builder_uses_configured_sensor_parity_slice2_opdata_mask();
  tx_builder_reports_encoded_command_mask();
  tx_builder_encodes_quiet_fan_code_zero();
  tx_builder_keeps_double_frame_commands_pending_until_command_frame();
  tx_builder_drops_33_byte_only_commands_in_20_byte_mode();
  tx_builder_3d_auto_command_bits_regression_suite();
  tx_builder_applies_3d_auto_in_33_byte_frame();
  tx_builder_reports_horizontal_vane_intent_in_33_byte_frame();
  tx_builder_preserves_horizontal_context_for_3d_auto_command();
  tx_builder_persists_external_room_temperature_override();
  tx_builder_clears_external_room_temperature_override();

  command_confirmation_confirms_power_mode_and_vertical_vane();
  command_confirmation_keeps_partial_pending_until_later_status();
  command_confirmation_confirms_auto_fan();
  command_confirmation_confirms_supported_fan_codes();
  command_confirmation_confirms_quiet_fan_code_zero();
  command_confirmation_times_out_unconfirmed_commands();
  command_confirmation_detects_duplicate_pending_commands();
  command_confirmation_confirms_horizontal_vane_feedback();
  command_confirmation_confirms_horizontal_swing_feedback();
  command_confirmation_confirms_3d_auto_feedback();
  command_confirmation_requires_preserved_horizontal_context_for_3d_auto();
  command_confirmation_uses_longer_timeout_for_extended_louver_commands();
  command_confirmation_reports_pending_age_for_settle_window();
  command_confirmation_can_settle_extended_louver_pending_mask();
  command_state_clears_pending_mask();

  diagnostics_snapshot_reports_event_ages();
  diagnostics_snapshot_reports_command_event_ages();
  diagnostics_snapshot_reports_command_confirmation_event_ages();
  diagnostics_snapshot_reports_loop_budget_timing();
  diagnostics_snapshot_handles_missing_event_ages();


  fixture_valid_status_frame_decodes();
  fixture_bad_checksum_rejects();
  fixture_garbage_then_valid_frame_resyncs();
  fixture_opdata_outdoor_temp_decodes();
  fixture_opdata_current_decodes();

  std::cout << "MHI protocol unit tests passed\n";
  return 0;
}
