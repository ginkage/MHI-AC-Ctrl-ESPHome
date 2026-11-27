#pragma once

#include <cstdint>

#include "mhi_command.h"
#include "mhi_state.h"

namespace esphome {
namespace mhi_ac_ctrl {

// Keep the position that was confirmed just before an axis was switched to swing.
// A missing snapshot is represented by zero; never invent a fallback position.
enum class MhiSwingSelection : uint8_t { OFF, VERTICAL, HORIZONTAL, BOTH };

struct MhiSwingRestorePlan {
  MhiCommandState patch{};
  uint8_t saved_vertical{0U};
  uint8_t saved_horizontal{0U};
  bool vertical_requested_swing{false};
  bool horizontal_requested_swing{false};
  bool missing_vertical_restore{false};
  bool missing_horizontal_restore{false};
};

class MhiSwingRestore {
 public:
  // Follow authoritative AC feedback even when the position was set via a
  // separate select entity or the AC itself. Never overwrite a snapshot while
  // that axis is swinging or while its swing command is still unconfirmed.
  void observe(const MhiStatusState& status) {
    if (!status.valid) {
      return;
    }
    if (status.vanes_swing) {
      this->vertical_swing_seen_ = true;
    } else {
      if (this->vertical_swing_seen_) {
        this->vertical_requested_swing_ = false;
      }
      this->vertical_swing_seen_ = false;
      if (!this->vertical_requested_swing_ && status.vertical_vane >= 1U && status.vertical_vane <= 4U) {
        this->saved_vertical_ = status.vertical_vane;
      }
    }

    if (status.has_horizontal_vane) {
      if (status.horizontal_vane_swing) {
        this->horizontal_swing_seen_ = true;
      } else {
        if (this->horizontal_swing_seen_) {
          this->horizontal_requested_swing_ = false;
        }
        this->horizontal_swing_seen_ = false;
        if (!this->horizontal_requested_swing_ && status.horizontal_vane >= 1U && status.horizontal_vane <= 7U) {
          this->saved_horizontal_ = status.horizontal_vane;
        }
      }
    }
  }

  MhiSwingRestorePlan plan(MhiSwingSelection selection, const MhiStatusState& status) const {
    MhiSwingRestorePlan next{};
    next.saved_vertical = this->saved_vertical_;
    next.saved_horizontal = this->saved_horizontal_;
    next.vertical_requested_swing = this->vertical_requested_swing_;
    next.horizontal_requested_swing = this->horizontal_requested_swing_;

    const bool want_vertical = selection == MhiSwingSelection::VERTICAL || selection == MhiSwingSelection::BOTH;
    const bool want_horizontal = selection == MhiSwingSelection::HORIZONTAL || selection == MhiSwingSelection::BOTH;
    const bool vertical_swing = status.valid && status.vanes_swing;
    const bool horizontal_swing = status.valid && status.has_horizontal_vane && status.horizontal_vane_swing;

    if (want_vertical) {
      if (!vertical_swing && !this->vertical_requested_swing_) {
        if (status.valid && status.vertical_vane >= 1U && status.vertical_vane <= 4U) {
          next.saved_vertical = status.vertical_vane;
        } else {
          next.saved_vertical = 0U;
        }
        next.patch.vertical_vane_set = true;
        next.patch.vertical_vane = 5U;
        next.vertical_requested_swing = true;
      }
    } else if (vertical_swing || this->vertical_requested_swing_) {
      if (this->saved_vertical_ >= 1U && this->saved_vertical_ <= 4U) {
        next.patch.vertical_vane_set = true;
        next.patch.vertical_vane = this->saved_vertical_;
        next.vertical_requested_swing = false;
      } else {
        next.missing_vertical_restore = true;
      }
    }

    if (want_horizontal) {
      if (!horizontal_swing && !this->horizontal_requested_swing_) {
        if (status.valid && status.has_horizontal_vane && !status.horizontal_vane_swing &&
            status.horizontal_vane >= 1U && status.horizontal_vane <= 7U) {
          next.saved_horizontal = status.horizontal_vane;
        } else {
          next.saved_horizontal = 0U;
        }
        next.patch.horizontal_vane_set = true;
        next.patch.horizontal_vane = 8U;
        next.horizontal_requested_swing = true;
      }
    } else if (horizontal_swing || this->horizontal_requested_swing_) {
      if (this->saved_horizontal_ >= 1U && this->saved_horizontal_ <= 7U) {
        next.patch.horizontal_vane_set = true;
        next.patch.horizontal_vane = this->saved_horizontal_;
        next.horizontal_requested_swing = false;
      } else {
        next.missing_horizontal_restore = true;
      }
    }

    return next;
  }

  void commit(const MhiSwingRestorePlan& plan, uint32_t accepted_mask) {
    if (plan.patch.vertical_vane_set && (accepted_mask & MHI_COMMAND_VERTICAL_VANE) != 0U) {
      this->saved_vertical_ = plan.saved_vertical;
      this->vertical_requested_swing_ = plan.vertical_requested_swing;
      this->vertical_swing_seen_ = false;
    }
    if (plan.patch.horizontal_vane_set && (accepted_mask & MHI_COMMAND_HORIZONTAL_VANE) != 0U) {
      this->saved_horizontal_ = plan.saved_horizontal;
      this->horizontal_requested_swing_ = plan.horizontal_requested_swing;
      this->horizontal_swing_seen_ = false;
    }
  }

 private:
  uint8_t saved_vertical_{0U};
  uint8_t saved_horizontal_{0U};
  bool vertical_requested_swing_{false};
  bool horizontal_requested_swing_{false};
  bool vertical_swing_seen_{false};
  bool horizontal_swing_seen_{false};
};

}  // namespace mhi_ac_ctrl
}  // namespace esphome
