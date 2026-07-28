#include "mhi_test_common.h"

namespace mhi_unit_tests {

void worker_decoded_store_latest_status_overwrites_stale_status() {
  MhiWorkerDecodedStore store{};
  MhiDecodedStatus first{};
  first.valid = true;
  first.mode = 2U;
  MhiDecodedStatus second = first;
  second.mode = 4U;

  const auto frame = make_mosi_status_frame();
  store.store_status(first, frame, 1U, 10U, false, false);
  store.store_status(second, frame, 2U, 20U, false, false);

  MhiDecodedStatusSnapshot out{};
  EXPECT_TRUE(store.take_status(out));
  EXPECT_EQ(out.sequence, 2U);
  EXPECT_EQ(out.decoded.mode, 4U);
  EXPECT_FALSE(store.take_status(out));
  EXPECT_EQ(store.stats().status_writes, 2U);
  EXPECT_EQ(store.stats().status_overwrites, 1U);
  EXPECT_EQ(store.stats().pending_high_water, 1U);
}

void worker_decoded_store_keeps_command_candidate_separate() {
  MhiWorkerDecodedStore store{};
  MhiDecodedStatus decoded{};
  decoded.valid = true;
  decoded.fan = 6U;
  const auto frame = make_mosi_status_frame();

  store.store_status(decoded, frame, 3U, 30U, true, true);

  MhiDecodedStatusSnapshot candidate{};
  MhiDecodedStatusSnapshot extended{};
  EXPECT_TRUE(store.take_command_candidate(candidate));
  EXPECT_TRUE(store.take_extended_status(extended));
  EXPECT_EQ(candidate.sequence, 3U);
  EXPECT_EQ(extended.sequence, 3U);
}

void worker_decoded_store_merges_distinct_opdata_fields() {
  MhiWorkerDecodedStore store{};
  MhiDecodedOpData outdoor{};
  outdoor.valid = true;
  outdoor.has_outdoor_temp = true;
  outdoor.outdoor_temp_c = 14.5f;

  MhiDecodedOpData current{};
  current.valid = true;
  current.has_current = true;
  current.current_a = 2.5f;

  const auto frame = make_opdata_frame(0x80U, 0x10U, 14U);
  store.merge_opdata(outdoor, frame, 1U, 10U);
  store.merge_opdata(current, frame, 2U, 20U);

  MhiDecodedOpDataSnapshot out{};
  EXPECT_TRUE(store.take_opdata(out));
  EXPECT_TRUE(out.decoded.has_outdoor_temp);
  EXPECT_TRUE(out.decoded.has_current);
  expect_near(out.decoded.outdoor_temp_c, 14.5f);
  expect_near(out.decoded.current_a, 2.5f);
  EXPECT_EQ(store.stats().opdata_field_overwrites, 0U);
}

void worker_decoded_store_overwrites_only_repeated_opdata_field() {
  MhiWorkerDecodedStore store{};
  MhiDecodedOpData first{};
  first.valid = true;
  first.has_outdoor_temp = true;
  first.outdoor_temp_c = 10.0f;

  MhiDecodedOpData second = first;
  second.outdoor_temp_c = 11.0f;

  const auto frame = make_opdata_frame(0x80U, 0x10U, 11U);
  store.merge_opdata(first, frame, 1U, 10U);
  store.merge_opdata(second, frame, 2U, 20U);

  MhiDecodedOpDataSnapshot out{};
  EXPECT_TRUE(store.take_opdata(out));
  expect_near(out.decoded.outdoor_temp_c, 11.0f);
  EXPECT_EQ(store.stats().opdata_field_overwrites, 1U);
}

void worker_decoded_store_unknown_ring_is_bounded() {
  MhiWorkerDecodedStore store{};
  const auto frame = make_mosi_status_frame();

  for (uint32_t i = 0U; i < kMhiWorkerUnknownRingCapacity + 2U; i++) {
    store.store_unknown(frame, i + 1U, i * 10U);
  }

  MhiWorkerUnknownSnapshot out{};
  uint32_t count = 0U;
  uint32_t first_sequence = 0U;
  while (store.take_unknown(out)) {
    if (count == 0U) {
      first_sequence = out.sequence;
    }
    count++;
  }

  EXPECT_EQ(count, static_cast<uint32_t>(kMhiWorkerUnknownRingCapacity));
  EXPECT_EQ(first_sequence, 3U);
  EXPECT_EQ(store.stats().unknown_overwrites, 2U);
  EXPECT_EQ(store.stats().unknown_high_water, static_cast<uint32_t>(kMhiWorkerUnknownRingCapacity));
  EXPECT_EQ(store.stats().pending_high_water, static_cast<uint32_t>(kMhiWorkerUnknownRingCapacity));
}

}  // namespace mhi_unit_tests
