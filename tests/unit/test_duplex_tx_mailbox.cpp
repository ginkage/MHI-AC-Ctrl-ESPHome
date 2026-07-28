#include "mhi_test_common.h"

#include <array>

#include "mhi_duplex_tx_mailbox.h"

namespace mhi_unit_tests {

namespace {

MhiTxEnvelope make_envelope(const MhiFrameBuffer& frame, uint32_t generation, MhiTxKind kind = MhiTxKind::BACKGROUND,
                            uint32_t command_mask = 0U) {
  MhiTxEnvelope envelope{};
  EXPECT_TRUE(envelope.set_frame(frame.data, frame.len));
  envelope.generation = generation;
  envelope.kind = kind;
  envelope.command_mask = command_mask;
  envelope.intent.mask = command_mask;
  return envelope;
}

}  // namespace

void duplex_tx_mailbox_stages_and_consumes_20_byte_frame() {
  MhiDuplexTxMailbox mailbox{};
  const MhiFrameBuffer frame = make_mosi_status_frame();
  const MhiTxEnvelope staged = make_envelope(frame, 41U, MhiTxKind::COMMAND, MHI_COMMAND_POWER);

  EXPECT_TRUE(mailbox.stage(staged));
  EXPECT_TRUE(mailbox.pending());
  EXPECT_EQ(mailbox.pending_len(), kMhiFrame20Bytes);
  EXPECT_EQ(mailbox.generation(), 41U);

  std::array<uint8_t, kMhiMaxFrameBytes> destination{};
  MhiTxEnvelope taken{};

  EXPECT_TRUE(mailbox.take(destination.data(), destination.size(), taken));
  EXPECT_EQ(taken.len, kMhiFrame20Bytes);
  EXPECT_EQ(taken.generation, 41U);
  EXPECT_EQ(taken.command_mask, MHI_COMMAND_POWER);
  EXPECT_EQ(destination[SB0], frame.data[SB0]);
  EXPECT_EQ(destination[CBL], frame.data[CBL]);
  EXPECT_FALSE(mailbox.pending());
  EXPECT_FALSE(mailbox.take(destination.data(), destination.size(), taken));
}

void duplex_tx_mailbox_latest_stage_replaces_unclaimed_frame() {
  MhiDuplexTxMailbox mailbox{};
  MhiFrameBuffer first = make_mosi_status_frame();
  MhiFrameBuffer second = make_mosi_status_frame_33(4U, false, false);
  first.data[DB4] = 0x11U;
  second.data[DB4] = 0x22U;

  EXPECT_TRUE(mailbox.stage(make_envelope(first, 1U)));
  EXPECT_TRUE(mailbox.stage(make_envelope(second, 2U)));
  EXPECT_EQ(mailbox.overwritten_frames(), 1U);
  EXPECT_EQ(mailbox.generation(), 2U);
  EXPECT_EQ(mailbox.pending_len(), kMhiFrame33Bytes);

  std::array<uint8_t, kMhiMaxFrameBytes> destination{};
  MhiTxEnvelope taken{};

  EXPECT_TRUE(mailbox.take(destination.data(), destination.size(), taken));
  EXPECT_EQ(taken.len, kMhiFrame33Bytes);
  EXPECT_EQ(taken.generation, 2U);
  EXPECT_EQ(destination[DB4], 0x22U);
}

void duplex_tx_mailbox_rejects_invalid_frames_without_losing_pending_data() {
  MhiDuplexTxMailbox mailbox{};
  const MhiFrameBuffer frame = make_mosi_status_frame();
  MhiTxEnvelope invalid{};
  invalid.len = 21U;

  EXPECT_TRUE(mailbox.stage(make_envelope(frame, 1U)));
  EXPECT_FALSE(mailbox.stage(invalid));
  EXPECT_TRUE(mailbox.pending());
  EXPECT_EQ(mailbox.generation(), 1U);
  EXPECT_EQ(mailbox.overwritten_frames(), 0U);

  std::array<uint8_t, 10U> too_small{};
  MhiTxEnvelope taken{};
  EXPECT_FALSE(mailbox.take(too_small.data(), too_small.size(), taken));
  EXPECT_TRUE(mailbox.pending());
}

}  // namespace mhi_unit_tests
