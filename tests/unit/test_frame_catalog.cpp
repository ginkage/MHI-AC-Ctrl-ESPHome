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
  EXPECT_EQ(opdata_classification.opdata_key, 0x0080U);

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
  EXPECT_EQ(high_db6_response_marker_classification.opdata_key, 0x011EU);
}

void frame_classifier_uses_stable_opdata_keys_when_value_bits_change() {
  const MhiFrameBuffer fan_stopped = make_legacy_opdata_frame(0x00, 0x1F, 0x10, 0x00);
  const MhiFrameBuffer fan_running = make_legacy_opdata_frame(0x00, 0x1F, 0x14, 0x00);
  const MhiFrameBuffer compressor_low = make_legacy_opdata_frame(0x00, 0x11, 0x10, 0x20);
  const MhiFrameBuffer compressor_high = make_legacy_opdata_frame(0x00, 0x11, 0x19, 0x40);
  const MhiFrameBuffer return_air = make_legacy_opdata_frame(0x80, 0x80, 0x20, 0x80);
  const MhiFrameBuffer outdoor_air = make_legacy_opdata_frame(0x00, 0x80, 0x10, 0x80);

  const auto stopped = classify_mhi_mosi_frame(fan_stopped.view());
  const auto running = classify_mhi_mosi_frame(fan_running.view());
  const auto low = classify_mhi_mosi_frame(compressor_low.view());
  const auto high = classify_mhi_mosi_frame(compressor_high.view());
  const auto indoor = classify_mhi_mosi_frame(return_air.view());
  const auto outdoor = classify_mhi_mosi_frame(outdoor_air.view());

  EXPECT_EQ(stopped.opdata_key, running.opdata_key);
  EXPECT_EQ(stopped.opdata_key, 0x001FU);
  EXPECT_EQ(low.opdata_key, high.opdata_key);
  EXPECT_EQ(low.opdata_key, 0x0011U);
  EXPECT_TRUE(indoor.opdata_key != outdoor.opdata_key);
  EXPECT_EQ(indoor.opdata_key, 0x0180U);
  EXPECT_EQ(outdoor.opdata_key, 0x0080U);
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

void frame_catalog_overwrites_changing_values_for_same_opdata_field() {
  MhiFrameCatalog catalog{};

  const MhiFrameBuffer stopped = make_legacy_opdata_frame(0x00, 0x1F, 0x10, 0x00);
  const MhiFrameBuffer running = make_legacy_opdata_frame(0x00, 0x1F, 0x14, 0x00);

  const auto first = catalog.ingest_mosi_frame(stopped.view(), 1U, 1000U);
  const auto second = catalog.ingest_mosi_frame(running.view(), 2U, 1010U);

  EXPECT_TRUE(first.stored);
  EXPECT_TRUE(second.stored);
  EXPECT_TRUE(second.overwritten);
  EXPECT_EQ(first.opdata_key, second.opdata_key);
  EXPECT_EQ(catalog.stats().overwritten_frames, 1U);
  EXPECT_EQ(catalog.stats().dropped_opdata_slots_full, 0U);

  MhiCatalogedFrame latest{};
  EXPECT_TRUE(catalog.take_latest_opdata(first.opdata_key, latest));
  EXPECT_EQ(latest.sequence, 2U);
  EXPECT_EQ(latest.frame.data[DB10], 0x14U);
}

void frame_catalog_reuses_consumed_opdata_slots() {
  MhiFrameCatalog catalog{};

  for (std::size_t i = 0; i < kMhiCatalogOpDataSlots; i++) {
    const uint8_t group = static_cast<uint8_t>(i + 1U);
    const MhiFrameBuffer frame = make_legacy_opdata_frame(0x00, group, 0x10, static_cast<uint8_t>(i));
    const auto result = catalog.ingest_mosi_frame(frame.view(), static_cast<uint32_t>(i + 1U), 1000U);
    EXPECT_TRUE(result.stored);
  }

  MhiCatalogedFrame consumed{};
  std::size_t consumed_count = 0U;
  while (catalog.take_next_opdata(consumed)) {
    consumed_count++;
  }
  EXPECT_EQ(consumed_count, kMhiCatalogOpDataSlots);

  for (std::size_t i = 0; i < kMhiCatalogOpDataSlots; i++) {
    const uint8_t group = static_cast<uint8_t>(i + kMhiCatalogOpDataSlots + 1U);
    const MhiFrameBuffer frame = make_legacy_opdata_frame(0x00, group, 0x10, static_cast<uint8_t>(i));
    const auto result = catalog.ingest_mosi_frame(frame.view(), static_cast<uint32_t>(100U + i), 2000U);
    EXPECT_TRUE(result.stored);
  }

  EXPECT_EQ(catalog.stats().dropped_opdata_slots_full, 0U);
}

void frame_catalog_buffers_all_supported_opdata_groups() {
  struct OpdataIdentity {
    uint8_t db6;
    uint8_t group;
  };

  constexpr OpdataIdentity identities[] = {
      {0x80U, 0x02U}, {0x80U, 0x05U}, {0x80U, 0x80U}, {0x80U, 0x81U}, {0x00U, 0x81U},
      {0x80U, 0x87U}, {0x80U, 0x1FU}, {0x80U, 0x1EU}, {0x00U, 0x80U}, {0x00U, 0x82U},
      {0x00U, 0x11U}, {0x00U, 0x85U}, {0x00U, 0x90U}, {0x00U, 0xB1U}, {0x00U, 0x7CU},
      {0x00U, 0x1FU}, {0x00U, 0x0CU}, {0x00U, 0x1EU}, {0x00U, 0x13U}, {0x80U, 0x94U},
  };

  MhiFrameCatalog catalog{};
  uint32_t sequence = 1U;
  for (const auto& identity : identities) {
    const uint8_t item = identity.db6 == 0x80U ? 0x20U : 0x10U;
    const MhiFrameBuffer frame = make_legacy_opdata_frame(identity.db6, identity.group, item, 0x01U);
    const auto result = catalog.ingest_mosi_frame(frame.view(), sequence++, 1000U);
    EXPECT_TRUE(result.stored);
  }

  EXPECT_EQ(catalog.stats().opdata_frames, static_cast<uint32_t>(sizeof(identities) / sizeof(identities[0])));
  EXPECT_EQ(catalog.stats().dropped_opdata_slots_full, 0U);
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
