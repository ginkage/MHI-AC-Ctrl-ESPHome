#pragma once

#include <string>

namespace esphome {
namespace mhi_ac_ctrl {

// Only queue/DMA/ISR-backed RX implementations may be drained from the
// combined worker. FastGPIO RX performs timing-critical synchronous sampling
// inside read(), so it must remain in the ESPHome main loop.
inline bool mhi_rx_driver_supports_classified_worker(const std::string& driver_name) {
  return driver_name == "external_clock_rx" || driver_name == "rmt_spi_rx" || driver_name == "rmt_cs_spi";
}

}  // namespace mhi_ac_ctrl
}  // namespace esphome
