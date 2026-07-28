#pragma once

#include <cstddef>
#include <cstdint>

#include "mhi_frame.h"
#include "mhi_opdata_decoder.h"
#include "mhi_status_decoder.h"

namespace esphome {
namespace mhi_ac_ctrl {

constexpr std::size_t kMhiWorkerUnknownRingCapacity = 4U;

struct MhiDecodedStatusSnapshot {
  bool valid{false};
  uint32_t sequence{0};
  uint32_t last_update_ms{0};
  MhiDecodedStatus decoded{};
  MhiFrameBuffer frame{};
};

struct MhiDecodedOpDataSnapshot {
  bool valid{false};
  uint32_t sequence{0};
  uint32_t last_update_ms{0};
  MhiDecodedOpData decoded{};
  MhiFrameBuffer last_frame{};
};

struct MhiWorkerUnknownSnapshot {
  uint32_t sequence{0};
  uint32_t last_update_ms{0};
  MhiFrameBuffer frame{};
};

struct MhiWorkerDecodedStoreStats {
  uint32_t status_writes{0};
  uint32_t status_overwrites{0};
  uint32_t extended_status_writes{0};
  uint32_t extended_status_overwrites{0};
  uint32_t command_candidate_writes{0};
  uint32_t command_candidate_overwrites{0};
  uint32_t opdata_merges{0};
  uint32_t opdata_field_overwrites{0};
  uint32_t unknown_writes{0};
  uint32_t unknown_overwrites{0};
  uint32_t publish_batches{0};
  uint32_t pending_high_water{0};
  uint32_t unknown_high_water{0};
};

class MhiWorkerDecodedStore {
 public:
  void reset();

  void store_status(const MhiDecodedStatus& decoded, const MhiFrameBuffer& frame, uint32_t sequence, uint32_t now_ms,
                    bool extended, bool command_candidate);
  void merge_opdata(const MhiDecodedOpData& decoded, const MhiFrameBuffer& frame, uint32_t sequence, uint32_t now_ms);
  void store_unknown(const MhiFrameBuffer& frame, uint32_t sequence, uint32_t now_ms);

  bool take_command_candidate(MhiDecodedStatusSnapshot& out);
  void clear_command_candidate();
  bool take_extended_status(MhiDecodedStatusSnapshot& out);
  bool take_status(MhiDecodedStatusSnapshot& out);
  bool take_opdata(MhiDecodedOpDataSnapshot& out);
  bool take_unknown(MhiWorkerUnknownSnapshot& out);

  void on_publish_batch() {
    stats_.publish_batches++;
  }

  std::size_t pending_count() const;

  const MhiWorkerDecodedStoreStats& stats() const {
    return stats_;
  }

 private:
  void update_backlog_high_water_();
  static void write_status_slot_(MhiDecodedStatusSnapshot& slot, const MhiDecodedStatus& decoded,
                                 const MhiFrameBuffer& frame, uint32_t sequence, uint32_t now_ms, uint32_t& writes,
                                 uint32_t& overwrites);
  static uint32_t merge_opdata_fields_(MhiDecodedOpData& destination, const MhiDecodedOpData& source);

  MhiDecodedStatusSnapshot latest_status_{};
  MhiDecodedStatusSnapshot latest_extended_status_{};
  MhiDecodedStatusSnapshot latest_command_candidate_{};
  MhiDecodedOpDataSnapshot opdata_{};

  MhiWorkerUnknownSnapshot unknown_ring_[kMhiWorkerUnknownRingCapacity]{};
  std::size_t unknown_head_{0U};
  std::size_t unknown_size_{0U};

  MhiWorkerDecodedStoreStats stats_{};
};

}  // namespace mhi_ac_ctrl
}  // namespace esphome
