#include "mhi_test_common.h"

#include "mhi_frame_catalog.h"
#include "mhi_frame_classifier.h"

namespace mhi_unit_tests {

using namespace esphome::mhi_ac_ctrl;

void frame_classifier_classifies_status_opdata_and_extended_status() {
  const MhiFrameBuffer status = make_mosi_status_frame();
  const MhiFrameClassification status_classification = classify_mhi_mosi_frame(status.view());
  EXPECT_EQ(static_cast<int>(status_classification.kind), static_cast<int>(MhiFrameKind::STATUS));
  EXPECT_EQ(status_classification.opdata_key, kMhiInvalidOpDataKey);

  const MhiFrameBuffer opdata = make_legacy_opdata_frame(0x00, 0x80, 0x10, 202);
  const MhiFrameClassification opdata_classification = classify_mhi_mosi_frame(opdata.view());
  EXPECT_EQ(static_cast<int>(opdata_classification.kind), static_cast<int>(MhiFrameKind::OPDATA));
  EXPECT_EQ(opdata_classification.opdata_key, 0x8010U);

  const MhiFrameBuffer extended = make_mosi_status_frame_33(2U, false, false);
  const MhiFrameClassification extended_classification = classify_mhi_mosi_frame(extended.view());
  EXPECT_EQ(static_cast<int>(extended_classification.kind), static_cast<int>(MhiFrameKind::EXTENDED_STATUS));
  EXPECT_EQ(extended_classification.opdata_key, kMhiInvalidOpDataKey);

  MhiFrameBuffer high_db6_status = make_mosi_status_frame();
  high_db6_status.data[DB6] = 0x80U;
  high_db6_status.data[DB9] = 0x81U;
  high_db6_status.data[DB10] = 0x01U;
  high_db6_status.data[DB11] = 0x00U;
  const uint16_t high_db6_status_checksum = mhi_calc_checksum(high_db6_status.data);
  high_db6_status.data[CBH] = static_cast<uint8_t>((high_db6_status_checksum >> 8U) & 0xFFU);
  high_db6_status.data[CBL] = static_cast<uint8_t>(high_db6_status_checksum & 0xFFU);

  const MhiFrameClassification high_db6_status_classification = classify_mhi_mosi_frame(high_db6_status.view());
  EXPECT_EQ(static_cast<int>(high_db6_status_classification.kind), static_cast<int>(MhiFrameKind::STATUS));
  EXPECT_EQ(high_db6_status_classification.opdata_key, kMhiInvalidOpDataKey);

  MhiFrameBuffer high_db6_response_marker = make_mosi_status_frame();
  high_db6_response_marker.data[DB6] = 0x80U;
  high_db6_response_marker.data[DB9] = 0x1EU;
  high_db6_response_marker.data[DB10] = 0x10U;
  high_db6_response_marker.data[DB11] = 0x0CU;
  const uint16_t high_db6_response_marker_checksum = mhi_calc_checksum(high_db6_response_marker.data);
  high_db6_response_marker.data[CBH] = static_cast<uint8_t>((high_db6_response_marker_checksum >> 8U) & 0xFFU);
  high_db6_response_marker.data[CBL] = static_cast<uint8_t>(high_db6_response_marker_checksum & 0xFFU);

  const MhiFrameClassification high_db6_response_marker_classification =
      classify_mhi_mosi_frame(high_db6_response_marker.view());
  EXPECT_EQ(static_cast<int>(high_db6_response_marker_classification.kind), static_cast<int>(MhiFrameKind::OPDATA));
  EXPECT_EQ(high_db6_response_marker_classification.opdata_key, 0x9E10U);
}

void frame_catalog_overwrites_repeated_status_with_latest() {
  MhiFrameCatalog catalog{};

  MhiFrameBuffer first = make_mosi_status_frame();
  first.data[DB3] = 100U;
  const uint16_t first_checksum = mhi_calc_checksum(first.data);
  first.data[CBH] = static_cast<uint8_t>((first_checksum >> 8U) & 0xFFU);
  first.data[CBL] = static_cast<uint8_t>(first_checksum & 0xFFU);

  MhiFrameBuffer second = make_mosi_status_frame();
  second.data[DB3] = 120U;
  const uint16_t second_checksum = mhi_calc_checksum(second.data);
  second.data[CBH] = static_cast<uint8_t>((second_checksum >> 8U) & 0xFFU);
  second.data[CBL] = static_cast<uint8_t>(second_checksum & 0xFFU);

  const auto first_result = catalog.ingest_mosi_frame(first.view(), 1U, 1000U);
  const auto second_result = catalog.ingest_mosi_frame(second.view(), 2U, 1010U);

  EXPECT_TRUE(first_result.stored);
  EXPECT_FALSE(first_result.overwritten);
  EXPECT_TRUE(second_result.stored);
  EXPECT_TRUE(second_result.overwritten);
  EXPECT_EQ(catalog.stats().status_frames, 2U);
  EXPECT_EQ(catalog.stats().overwritten_frames, 1U);

  MhiCatalogedFrame latest{};
  EXPECT_TRUE(catalog.take_latest_status(latest));
  EXPECT_EQ(latest.sequence, 2U);
  EXPECT_EQ(latest.last_update_ms, 1010U);
  EXPECT_EQ(latest.frame.data[DB3], 120U);
  EXPECT_FALSE(catalog.take_latest_status(latest));
}


void frame_catalog_keeps_command_candidate_side_slot_latest_only() {
  MhiFrameCatalog catalog{};

  MhiFrameBuffer before_command = make_mosi_status_frame_33(1U, false, false);
  MhiFrameBuffer first_candidate = make_mosi_status_frame_33(2U, false, false);
  MhiFrameBuffer second_candidate = make_mosi_status_frame_33(3U, false, false);

  const auto before = catalog.ingest_mosi_frame(before_command.view(), 1U, 1000U, false);
  const auto first = catalog.ingest_mosi_frame(first_candidate.view(), 2U, 1010U, true);
  const auto second = catalog.ingest_mosi_frame(second_candidate.view(), 3U, 1020U, true);

  EXPECT_TRUE(before.stored);
  EXPECT_TRUE(first.stored);
  EXPECT_TRUE(second.stored);
  EXPECT_EQ(catalog.stats().extended_status_frames, 3U);
  EXPECT_EQ(catalog.stats().command_candidate_frames, 2U);

  MhiCatalogedFrame command_candidate{};
  EXPECT_TRUE(catalog.take_latest_command_candidate(command_candidate));
  EXPECT_EQ(command_candidate.sequence, 3U);
  EXPECT_EQ(command_candidate.frame.data[DB16], second_candidate.data[DB16]);
  EXPECT_FALSE(catalog.take_latest_command_candidate(command_candidate));

  MhiCatalogedFrame latest_extended{};
  EXPECT_TRUE(catalog.take_latest_extended_status(latest_extended));
  EXPECT_EQ(latest_extended.sequence, 3U);
}

void frame_catalog_keeps_opdata_slots_separate_by_key() {
  MhiFrameCatalog catalog{};

  const MhiFrameBuffer outdoor = make_legacy_opdata_frame(0x00, 0x80, 0x10, 202);
  const MhiFrameBuffer return_air = make_legacy_opdata_frame(0x80, 0x80, 0x20, 168);
  const MhiFrameBuffer newer_outdoor = make_legacy_opdata_frame(0x00, 0x80, 0x10, 210);

  const auto first = catalog.ingest_mosi_frame(outdoor.view(), 1U, 1000U);
  const auto second = catalog.ingest_mosi_frame(return_air.view(), 2U, 1010U);
  const auto third = catalog.ingest_mosi_frame(newer_outdoor.view(), 3U, 1020U);

  EXPECT_TRUE(first.stored);
  EXPECT_TRUE(second.stored);
  EXPECT_TRUE(third.stored);
  EXPECT_TRUE(third.overwritten);
  EXPECT_EQ(catalog.stats().opdata_frames, 3U);
  EXPECT_EQ(catalog.stats().overwritten_frames, 1U);

  MhiCatalogedFrame latest_outdoor{};
  EXPECT_TRUE(catalog.take_latest_opdata(first.opdata_key, latest_outdoor));
  EXPECT_EQ(latest_outdoor.sequence, 3U);
  EXPECT_EQ(latest_outdoor.frame.data[DB11], 210U);

  MhiCatalogedFrame latest_return_air{};
  EXPECT_TRUE(catalog.take_latest_opdata(second.opdata_key, latest_return_air));
  EXPECT_EQ(latest_return_air.sequence, 2U);
  EXPECT_EQ(latest_return_air.frame.data[DB11], 168U);
}

void frame_catalog_reports_unknown_frames() {
  MhiFrameCatalog catalog{};
  MhiFrameBuffer unknown = make_mosi_status_frame();
  unknown.data[SB0] = 0x00U;

  const auto result = catalog.ingest_mosi_frame(unknown.view(), 10U, 2000U);
  EXPECT_TRUE(result.stored);
  EXPECT_EQ(static_cast<int>(result.kind), static_cast<int>(MhiFrameKind::UNKNOWN));
  EXPECT_EQ(catalog.stats().unknown_frames, 1U);

  MhiCatalogedFrame latest{};
  EXPECT_TRUE(catalog.take_latest_unknown(latest));
  EXPECT_EQ(static_cast<int>(latest.kind), static_cast<int>(MhiFrameKind::UNKNOWN));
  EXPECT_EQ(latest.sequence, 10U);
}

}  // namespace mhi_unit_tests

namespace mhi_unit_tests {

void frame_catalog_reuses_consumed_opdata_slots() {
  MhiFrameCatalog catalog{};
  catalog.reset();

  for (uint32_t i = 0U; i < 48U; i++) {
    const uint8_t group = static_cast<uint8_t>(i + 1U);
    auto frame = make_legacy_opdata_frame(0x00U, group, 0x10U, static_cast<uint8_t>(i));
    const auto result = catalog.ingest_mosi_frame(frame.view(), i + 1U, i * 10U);
    EXPECT_TRUE(result.stored);

    MhiCatalogedFrame out{};
    EXPECT_TRUE(catalog.take_next_opdata(out));
    EXPECT_EQ(out.sequence, i + 1U);
  }

  EXPECT_EQ(catalog.stats().dropped_opdata_slots_full, 0U);
}

}  // namespace mhi_unit_tests
