#include "mhi_test_common.h"

namespace mhi_unit_tests {

void worker_policy_allows_queue_backed_rx_drivers() {
  EXPECT_TRUE(mhi_rx_driver_supports_classified_worker("external_clock_rx"));
  EXPECT_TRUE(mhi_rx_driver_supports_classified_worker("rmt_spi_rx"));
  EXPECT_TRUE(mhi_rx_driver_supports_classified_worker("rmt_cs_spi"));
}

void worker_policy_keeps_synchronous_rx_in_main_loop() {
  EXPECT_FALSE(mhi_rx_driver_supports_classified_worker("fast_gpio_rx"));
  EXPECT_FALSE(mhi_rx_driver_supports_classified_worker("native_spi_rx"));
  EXPECT_FALSE(mhi_rx_driver_supports_classified_worker("none"));
}

}  // namespace mhi_unit_tests
