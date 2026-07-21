
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

  command_coordinator_starts_confirmation_after_tx_completion();
  command_coordinator_restores_command_when_stage_is_rejected();
  command_coordinator_requeues_failed_command();
  command_coordinator_restores_vertical_vane_after_stage_rejection();
  command_coordinator_restores_horizontal_vane_after_tx_failure();
  command_coordinator_combines_vertical_and_horizontal_vanes();
  command_coordinator_preserves_3d_auto_louver_context();
  command_coordinator_restores_3d_auto_after_tx_failure();
  command_coordinator_ignores_background_and_stale_completions();
  command_coordinator_blocks_prepare_while_in_flight_or_confirming();
  command_coordinator_preserves_newer_same_field_after_tx_failure();
  command_coordinator_preserves_newer_same_field_after_stage_rejection();
  command_patch_merges_combined_climate_fields();
  command_coordinator_encodes_combined_climate_patch_in_one_envelope();
  command_patch_applies_allowed_fields_without_losing_existing_state();
  command_patch_rejects_invalid_vanes_but_keeps_valid_fields();
  command_coordinator_restores_failed_field_without_losing_new_unrelated_command();
  command_coordinator_assigns_increasing_generations();
  command_coordinator_supersedes_pending_confirmation_with_newer_value();
  command_coordinator_does_not_confirm_old_value_when_newer_request_is_queued();
  command_coordinator_retries_only_remaining_fields_and_caps_attempts();
  command_coordinator_reports_staged_timeout_once();
  tx_completion_queue_preserves_order_and_reports_overwrite();

  worker_decoded_store_latest_status_overwrites_stale_status();
  worker_decoded_store_keeps_command_candidate_separate();
  worker_decoded_store_merges_distinct_opdata_fields();
  worker_decoded_store_overwrites_only_repeated_opdata_field();
  worker_decoded_store_unknown_ring_is_bounded();

  frame_classifier_classifies_status_opdata_and_extended_status();
  frame_catalog_overwrites_repeated_status_with_latest();
  frame_catalog_keeps_opdata_slots_separate_by_key();
  frame_catalog_keeps_command_candidate_side_slot_latest_only();
  frame_catalog_reports_unknown_frames();
  frame_catalog_reuses_consumed_opdata_slots();

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
  publish_bridge_uses_configured_room_temperature_limits();

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
  command_confirmation_accepts_3d_auto_when_louver_context_changes();
  command_confirmation_supersedes_older_pending_value();
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
  worker_policy_allows_queue_backed_rx_drivers();
  worker_policy_keeps_synchronous_rx_in_main_loop();

  return 0;
}
