#pragma once

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "mhi_checksum.h"
#include "mhi_command.h"
#include "mhi_command_confirmation.h"
#include "mhi_defs.h"
#include "mhi_diag.h"
#include "mhi_duplex_tx_mailbox.h"
#include "mhi_frame.h"
#include "mhi_frame_queue.h"
#include "mhi_frame_catalog.h"
#include "mhi_frame_classifier.h"
#include "mhi_frame_sync.h"
#include "mhi_fan_profile.h"
#include "mhi_opdata_decoder.h"
#include "mhi_publish_bridge.h"
#include "mhi_status_decoder.h"
#include "mhi_tx_builder.h"

#define EXPECT_TRUE(expr)                                                                            \
  do {                                                                                               \
    if (!(expr)) {                                                                                   \
      std::cerr << "EXPECT_TRUE failed: " #expr << " at " << __FILE__ << ':' << __LINE__ << '\n';   \
      std::exit(1);                                                                                  \
    }                                                                                                \
  } while (false)

#define EXPECT_FALSE(expr) EXPECT_TRUE(!(expr))

#define EXPECT_EQ(actual, expected)                                                                  \
  do {                                                                                               \
    const auto actual_value = (actual);                                                              \
    const auto expected_value = (expected);                                                          \
    if (!(actual_value == expected_value)) {                                                         \
      std::cerr << "EXPECT_EQ failed: " #actual " == " #expected << " at " << __FILE__ << ':'       \
                << __LINE__ << " actual=" << +actual_value << " expected=" << +expected_value       \
                << '\n';                                                                             \
      std::exit(1);                                                                                  \
    }                                                                                                \
  } while (false)

namespace mhi_unit_tests {

using namespace esphome::mhi_ac_ctrl;

inline void expect_near(float actual, float expected, float epsilon = 0.01f) {
  if (std::fabs(actual - expected) > epsilon) {
    std::cerr << "EXPECT_NEAR failed at " << __FILE__ << ':' << __LINE__ << " actual=" << actual
              << " expected=" << expected << " epsilon=" << epsilon << '\n';
    std::exit(1);
  }
}

inline MhiFrameBuffer make_mosi_status_frame() {
  MhiFrameBuffer frame{};
  frame.len = kMhiFrame20Bytes;

  frame.data[SB0] = kMhiMosiSignature0Default;
  frame.data[SB1] = kMhiMosiSignature1;
  frame.data[SB2] = kMhiMosiSignature2;

  frame.data[DB0] = 0x09;  // power on, mode cool: DB0[4:2] = 2.
  frame.data[DB1] = 0x22;  // raw fan code 2 / Medium, vertical vane position 3.
  frame.data[DB2] = 44;    // 22.0 C target.
  frame.data[DB3] = 157;   // 24.0 C room: (157 - 61) / 4.
  frame.data[DB4] = 0x00;
  frame.data[DB6] = 0x00;

  const uint16_t checksum = mhi_calc_checksum(frame.data);
  frame.data[CBH] = static_cast<uint8_t>((checksum >> 8U) & 0xFFU);
  frame.data[CBL] = static_cast<uint8_t>(checksum & 0xFFU);

  return frame;
}


inline MhiFrameBuffer make_mosi_status_frame_33(uint8_t horizontal_vane, bool horizontal_swing, bool three_d_auto) {
  MhiFrameBuffer frame = make_mosi_status_frame();
  frame.len = kMhiFrame33Bytes;

  if (horizontal_swing) {
    frame.data[DB17] |= 0x01U;
  } else if (horizontal_vane >= 1U && horizontal_vane <= 7U) {
    frame.data[DB16] = static_cast<uint8_t>(horizontal_vane - 1U);
  }

  if (three_d_auto) {
    frame.data[DB17] |= 0x04U;
  }

  const uint16_t checksum = mhi_calc_checksum_frame33(frame.data);
  frame.data[CBL2] = static_cast<uint8_t>(checksum & 0xFFU);

  return frame;
}

inline MhiFrameBuffer make_opdata_frame(uint8_t group, uint8_t item, uint8_t value) {
  MhiFrameBuffer frame = make_mosi_status_frame();

  frame.data[DB6] = 0x80;
  frame.data[DB9] = group;
  frame.data[DB10] = item;
  frame.data[DB11] = value;

  const uint16_t checksum = mhi_calc_checksum(frame.data);
  frame.data[CBH] = static_cast<uint8_t>((checksum >> 8U) & 0xFFU);
  frame.data[CBL] = static_cast<uint8_t>(checksum & 0xFFU);

  return frame;
}

inline MhiFrameBuffer make_legacy_opdata_frame(uint8_t db6, uint8_t group, uint8_t item, uint8_t value,
                                               uint8_t value_high = 0U) {
  MhiFrameBuffer frame = make_mosi_status_frame();

  frame.data[DB6] = db6;
  frame.data[DB9] = group;
  frame.data[DB10] = item;
  frame.data[DB11] = value;
  frame.data[DB12] = value_high;

  const uint16_t checksum = mhi_calc_checksum(frame.data);
  frame.data[CBH] = static_cast<uint8_t>((checksum >> 8U) & 0xFFU);
  frame.data[CBL] = static_cast<uint8_t>(checksum & 0xFFU);

  return frame;
}

inline std::vector<uint8_t> load_hex_fixture(const std::string &path) {
  std::ifstream file(path);
  if (!file) {
    std::cerr << "Failed to open fixture: " << path << '\n';
    std::exit(1);
  }

  std::vector<uint8_t> bytes{};
  std::string line{};

  while (std::getline(file, line)) {
    const auto comment_pos = line.find('#');
    if (comment_pos != std::string::npos) {
      line.resize(comment_pos);
    }

    std::istringstream stream(line);
    std::string token{};

    while (stream >> token) {
      const auto value = std::stoul(token, nullptr, 16);
      if (value > 0xFFU) {
        std::cerr << "Fixture byte out of range in " << path << ": " << token << '\n';
        std::exit(1);
      }

      bytes.push_back(static_cast<uint8_t>(value));
    }
  }

  return bytes;
}

inline MhiFrameBuffer load_frame_fixture(const std::string &path) {
  const auto bytes = load_hex_fixture(path);

  if (bytes.size() > kMhiMaxFrameBytes) {
    std::cerr << "Fixture too large: " << path << " size=" << bytes.size() << '\n';
    std::exit(1);
  }

  MhiFrameBuffer frame{};
  frame.len = bytes.size();

  for (std::size_t i = 0; i < bytes.size(); i++) {
    frame.data[i] = bytes[i];
  }

  return frame;
}

void checksum_accepts_valid_20_byte_frame();
void checksum_rejects_bad_20_byte_frame();

void frame_sync_discards_garbage_and_extracts_valid_frame();
void frame_sync_waits_for_partial_frame();
void frame_sync_records_resync_stats();
void frame_sync_records_checksum_failure_stats();
void frame_sync_33_byte_mode_consumes_full_frame_without_tail_resync_noise();

void frame_queue_preserves_complete_frames();
void frame_queue_overwrites_oldest_complete_frame();

void duplex_tx_mailbox_stages_and_consumes_20_byte_frame();
void duplex_tx_mailbox_latest_stage_replaces_unclaimed_frame();
void duplex_tx_mailbox_rejects_invalid_frames_without_losing_pending_data();

void frame_classifier_classifies_status_opdata_and_extended_status();
void frame_catalog_overwrites_repeated_status_with_latest();
void frame_catalog_keeps_opdata_slots_separate_by_key();
void frame_catalog_keeps_command_candidate_side_slot_latest_only();
void frame_catalog_reports_unknown_frames();

void status_decoder_decodes_core_fields();
void status_decoder_decodes_33_byte_vane_feedback();
void status_decoder_ignores_unknown_horizontal_vane_feedback();
void status_decoder_ignores_33_byte_vane_feedback_on_opdata_frames();

void opdata_decoder_decodes_outdoor_temp();
void opdata_decoder_decodes_return_air_temp();
void opdata_decoder_decodes_compressor_frequency();
void opdata_decoder_decodes_current();
void opdata_decoder_decodes_indoor_unit_fan_speed();
void opdata_decoder_decodes_outdoor_unit_fan_speed();
void opdata_decoder_decodes_indoor_unit_total_run_time();
void opdata_decoder_decodes_compressor_total_run_time();
void opdata_decoder_decodes_energy_used();
void opdata_decoder_decodes_temperature_and_protection_slice2();

void publish_bridge_republishes_cached_state_after_targets_are_registered();
void publish_bridge_publishes_sensor_parity_slice1_on_first_opdata_publish();
void publish_bridge_publishes_sensor_parity_slice2_on_first_opdata_publish();
void publish_bridge_maps_unknown_protection_state();
void publish_bridge_maps_mhi_auto_to_heat_cool_for_ha_setpoint_ui();
void publish_bridge_publishes_sensor_parity_slice3_vane_feedback();
void publish_bridge_suppresses_unchanged_sensor_republishes();
void publish_bridge_suppresses_alternating_climate_current_temperature_chatter();
void publish_bridge_rate_limits_low_priority_climate_current_temperature_change();
void publish_bridge_does_not_force_low_priority_current_temp_when_other_climate_fields_change();
void publish_bridge_publishes_high_priority_climate_current_temperature_change_immediately();

void tx_builder_emits_valid_default_20_byte_frame();
void tx_builder_applies_pending_commands_once();
void tx_builder_uses_configured_sensor_parity_opdata_mask();
void tx_builder_uses_configured_sensor_parity_slice2_opdata_mask();
void tx_builder_reports_encoded_command_mask();
void tx_builder_keeps_double_frame_commands_pending_until_command_frame();
void tx_builder_drops_33_byte_only_commands_in_20_byte_mode();
void tx_builder_applies_3d_auto_in_33_byte_frame();
void tx_builder_3d_auto_command_bits_regression_suite();
void tx_builder_reports_horizontal_vane_intent_in_33_byte_frame();
void tx_builder_preserves_horizontal_context_for_3d_auto_command();
void tx_builder_persists_external_room_temperature_override();
void tx_builder_clears_external_room_temperature_override();

void command_confirmation_confirms_power_mode_and_vertical_vane();
void command_confirmation_keeps_partial_pending_until_later_status();
void command_confirmation_confirms_auto_fan();
void command_confirmation_confirms_supported_fan_codes();
void command_confirmation_times_out_unconfirmed_commands();
void command_confirmation_detects_duplicate_pending_commands();
void command_confirmation_confirms_horizontal_vane_feedback();
void command_confirmation_confirms_horizontal_swing_feedback();
void command_confirmation_confirms_3d_auto_feedback();
void command_confirmation_requires_preserved_horizontal_context_for_3d_auto();
void command_confirmation_uses_longer_timeout_for_extended_louver_commands();
void command_confirmation_reports_pending_age_for_settle_window();
void command_confirmation_can_settle_extended_louver_pending_mask();
void command_state_clears_pending_mask();

void diagnostics_snapshot_reports_event_ages();
void diagnostics_snapshot_reports_command_event_ages();
void diagnostics_snapshot_reports_command_confirmation_event_ages();
void diagnostics_snapshot_reports_loop_budget_timing();
void diagnostics_snapshot_handles_missing_event_ages();



void fixture_valid_status_frame_decodes();
void fixture_bad_checksum_rejects();
void fixture_garbage_then_valid_frame_resyncs();
void fixture_opdata_outdoor_temp_decodes();
void fixture_opdata_current_decodes();


void fan_profile_defaults_to_four_speed();
void fan_profile_three_speed_collapses_code_zero_to_low();
void fan_profile_four_speed_exposes_code_zero_as_quiet();
void fan_profile_encodes_quiet_only_for_four_speed();
void status_decoder_preserves_protocol_fan_code_zero();
void publish_bridge_three_speed_maps_code_zero_to_low();
void publish_bridge_four_speed_maps_code_zero_to_quiet();
void tx_builder_encodes_quiet_fan_code_zero();
void command_confirmation_confirms_quiet_fan_code_zero();
}  // namespace mhi_unit_tests
