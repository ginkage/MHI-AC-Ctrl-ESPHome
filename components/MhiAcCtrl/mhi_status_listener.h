#pragma once

#include "MHI-AC-Ctrl-core.h"

namespace esphome {
namespace mhi {

class MhiStatusListener {
public:
    virtual void update_status(ACStatus status, int value) = 0;
};

} //namespace mhi
} //namespace esphome