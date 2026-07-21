#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "mhi_defs.h"
#include "mhi_diag.h"
#include "mhi_duplex_transport.h"
#include "mhi_fast_gpio_rx_driver.h"
#include "mhi_rx_driver.h"
#include "mhi_transport_pins.h"
#include "mhi_tx_contract.h"
#include "mhi_tx_driver.h"
#include "mhi_worker_policy.h"

#ifdef USE_ESP_IDF
#include <sdkconfig.h>
#endif

#if defined(USE_ESP_IDF) && defined(CONFIG_IDF_TARGET_ESP32S3)
#define MHI_ENABLE_EXPERIMENTAL_S3_DRIVER 1
#else
#define MHI_ENABLE_EXPERIMENTAL_S3_DRIVER 0
#endif

#if defined(USE_ESP_IDF) && (defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S3))
#define MHI_ENABLE_EXTERNAL_CLOCK_RX_DRIVER 1
#define MHI_ENABLE_SPLIT_TX_DRIVER 1
#include "mhi_external_clock_rx_driver.h"
#include "mhi_fast_gpio_tx_driver.h"
#include "mhi_null_tx_driver.h"
#else
#define MHI_ENABLE_EXTERNAL_CLOCK_RX_DRIVER 0
#define MHI_ENABLE_SPLIT_TX_DRIVER 0
#endif

#if MHI_ENABLE_EXPERIMENTAL_S3_DRIVER
#include "mhi_native_spi_rx_driver.h"
#include "mhi_rmt_cs_spi_transport.h"
#include "mhi_rmt_spi_rx_driver.h"
#endif

#define MHI_ENABLE_NATIVE_SPI_RX_DRIVER MHI_ENABLE_EXPERIMENTAL_S3_DRIVER
#define MHI_ENABLE_RMT_SPI_RX_DRIVER MHI_ENABLE_EXPERIMENTAL_S3_DRIVER
#define MHI_ENABLE_RMT_CS_SPI_TRANSPORT MHI_ENABLE_EXPERIMENTAL_S3_DRIVER

namespace esphome {
namespace mhi_ac_ctrl {

class MhiTransportManager {
 public:
  void configure(int sck_pin, int mosi_pin, int miso_pin, const std::string& rx_driver, const std::string& tx_driver,
                 uint8_t frame_size_hint = 20U, uint32_t frame_start_idle_ms = 10U,
                 uint32_t external_clock_byte_gap_us = 80U, uint32_t external_clock_frame_gap_us = 5000U,
                 uint32_t external_clock_min_edge_gap_us = 4U, const std::string& external_clock_edge = "falling",
                 uint32_t external_clock_sample_delay_nops = 0U);

  void set_rmt_spi_frame_gap_us(uint32_t frame_gap_us) {
    rmt_spi_frame_gap_us_ = frame_gap_us;
  }

  void set_diagnostics(MhiDiagnostics* diagnostics) {
    diagnostics_ = diagnostics;
  }

  bool setup();
  void loop();
  void shutdown();

  std::size_t read_rx(uint8_t* dst, std::size_t max_len);

  // Stages an immutable TX envelope. Real-time transmission remains owned by
  // the selected transport. Completion is reported only after the frame was
  // actually clocked onto the bus.
  bool queue_tx(const MhiTxEnvelope& envelope);
  bool take_tx_completion(MhiTxCompletion& completion);
  bool has_pending_tx() const;
  bool flush_tx_on_bus_marker();
  std::size_t tx_completion_queue_depth() const;
  std::size_t tx_completion_queue_high_water() const;
  uint32_t tx_completion_queue_dropped() const;
  std::size_t rx_queue_depth() const;
  std::size_t rx_queue_high_water() const;
  uint32_t rx_queue_overwritten() const;
  void set_auto_tx_flush(bool enabled) {
    auto_tx_flush_ = enabled;
  }
  bool auto_tx_flush() const {
    return auto_tx_flush_;
  }

  void set_rx_byte_critical_sections(bool enabled);
  bool rx_byte_critical_sections() const;
  bool tx_uses_bus_marker() const;
  bool tx_uses_bus_window() const {
    return this->tx_uses_bus_marker();
  }

  const char* rx_name() const;
  const char* tx_name() const;

  bool rx_ready() const {
    return rx_ready_;
  }

  bool rx_supports_classified_worker() const {
    return rx_ready_ && mhi_rx_driver_supports_classified_worker(this->rx_name());
  }
  bool tx_ready() const {
    return tx_ready_;
  }

 private:
  void resolve_drivers();
  void update_duplex_diagnostics_();
  std::size_t read_rx_raw_(uint8_t* dst, std::size_t max_len);
  void queue_pending_tx_(const MhiTxEnvelope& envelope);
  bool pending_tx_available_() const;
  void clear_pending_tx_();
  bool flush_pending_tx_on_bus_marker_();

  MhiTransportPins pins_{};

  std::string transport_driver_name_{"split"};
  std::string rx_driver_name_{"fast_gpio_rx"};
  std::string tx_driver_name_{"fast_gpio_tx"};

  MhiFastGpioRxDriver fast_gpio_rx_{};
#if MHI_ENABLE_SPLIT_TX_DRIVER
  MhiFastGpioTxDriver fast_gpio_tx_{};
  MhiNullTxDriver null_tx_{};
#endif
#if MHI_ENABLE_NATIVE_SPI_RX_DRIVER
  MhiNativeSpiRxDriver native_spi_rx_{};
#endif
#if MHI_ENABLE_RMT_SPI_RX_DRIVER
  MhiRmtSpiRxDriver rmt_spi_rx_{};
#endif
#if MHI_ENABLE_RMT_CS_SPI_TRANSPORT
  MhiRmtCsSpiTransport rmt_cs_spi_{};
#endif
#if MHI_ENABLE_EXTERNAL_CLOCK_RX_DRIVER
  MhiExternalClockRxDriver external_clock_rx_{};
#endif

  IMhiDuplexTransport* duplex_{nullptr};
  IMhiRxDriver* rx_{&fast_gpio_rx_};
#if MHI_ENABLE_SPLIT_TX_DRIVER
  IMhiTxDriver* tx_{&fast_gpio_tx_};
#else
  IMhiTxDriver* tx_{nullptr};
#endif

  bool rx_ready_{false};
  bool tx_ready_{false};
  bool auto_tx_flush_{true};

  portMUX_TYPE tx_mux_ = portMUX_INITIALIZER_UNLOCKED;
  MhiTxEnvelope pending_tx_envelope_{};
  MhiTxCompletionQueue<8U> tx_completions_{};
  bool pending_tx_{false};
  bool tx_in_progress_{false};
  uint32_t pending_tx_generation_{0U};
  uint32_t pending_tx_queued_after_marker_sequence_{0U};
  uint32_t last_consumed_bus_marker_sequence_{0U};
  uint32_t last_stale_bus_marker_sequence_{0U};
  uint32_t tx_backoff_until_ms_{0U};

  // Arm TX from a new RX frame-end marker, then make one blocking TX attempt
  // against the real next SCK burst. This avoids age-window retry storms while
  // still giving the AC-owned clock enough time to arrive.
  uint32_t tx_marker_arm_max_age_us_{3000U};
  uint32_t tx_marker_timeout_ms_{60U};
  uint32_t tx_failure_backoff_ms_{250U};
  uint32_t rmt_spi_frame_gap_us_{1000U};
  uint32_t last_duplex_tx_completed_{0U};
  uint32_t last_duplex_tx_failures_{0U};

  MhiDiagnostics* diagnostics_{nullptr};
};

}  // namespace mhi_ac_ctrl
}  // namespace esphome
