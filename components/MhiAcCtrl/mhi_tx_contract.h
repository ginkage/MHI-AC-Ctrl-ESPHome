#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "mhi_command.h"
#include "mhi_defs.h"

namespace esphome {
namespace mhi_ac_ctrl {

enum class MhiTxKind : uint8_t {
  BACKGROUND = 0,
  COMMAND = 1,
};

struct MhiTxEnvelope {
  std::array<uint8_t, kMhiMaxFrameBytes> frame{};
  std::size_t len{0U};
  uint32_t generation{0U};
  MhiTxKind kind{MhiTxKind::BACKGROUND};
  uint32_t command_mask{0U};
  MhiCommandIntent intent{};

  bool valid() const {
    return len == kMhiFrame20Bytes || len == kMhiFrame33Bytes;
  }

  bool is_command() const {
    return kind == MhiTxKind::COMMAND && command_mask != 0U;
  }

  bool set_frame(const uint8_t* data, std::size_t frame_len) {
    if (data == nullptr || (frame_len != kMhiFrame20Bytes && frame_len != kMhiFrame33Bytes)) {
      return false;
    }

    frame.fill(0U);
    std::memcpy(frame.data(), data, frame_len);
    len = frame_len;
    return true;
  }
};

struct MhiTxCompletion {
  uint32_t generation{0U};
  MhiTxKind kind{MhiTxKind::BACKGROUND};
  uint32_t command_mask{0U};
  MhiCommandIntent intent{};
  bool success{false};
  uint32_t completed_at_ms{0U};

  bool is_command() const {
    return kind == MhiTxKind::COMMAND && command_mask != 0U;
  }
};

template <std::size_t Capacity>
class MhiTxCompletionQueue {
 public:
  static_assert(Capacity > 0U, "TX completion queue capacity must be non-zero");

  void reset() {
    head_ = 0U;
    size_ = 0U;
    high_water_mark_ = 0U;
    dropped_ = 0U;
  }

  bool push(const MhiTxCompletion& completion) {
    // A command completion is a lifecycle event, not a latest-value snapshot.
    // Never overwrite an older completion: reject the new entry and surface the
    // invariant failure through diagnostics instead.
    if (size_ == Capacity) {
      dropped_++;
      return false;
    }

    const std::size_t tail = (head_ + size_) % Capacity;
    entries_[tail] = completion;
    size_++;
    if (size_ > high_water_mark_) {
      high_water_mark_ = size_;
    }
    return true;
  }

  bool pop(MhiTxCompletion& completion) {
    if (size_ == 0U) {
      return false;
    }

    completion = entries_[head_];
    head_ = (head_ + 1U) % Capacity;
    size_--;
    return true;
  }

  std::size_t size() const {
    return size_;
  }

  constexpr std::size_t capacity() const {
    return Capacity;
  }

  std::size_t high_water_mark() const {
    return high_water_mark_;
  }

  uint32_t dropped() const {
    return dropped_;
  }

 private:
  std::array<MhiTxCompletion, Capacity> entries_{};
  std::size_t head_{0U};
  std::size_t size_{0U};
  std::size_t high_water_mark_{0U};
  uint32_t dropped_{0U};
};

}  // namespace mhi_ac_ctrl
}  // namespace esphome
