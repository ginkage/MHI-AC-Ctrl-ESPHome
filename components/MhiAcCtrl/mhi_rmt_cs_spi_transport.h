#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "mhi_defs.h"
#include "mhi_duplex_transport.h"
#include "mhi_duplex_tx_mailbox.h"
#include "mhi_frame_queue.h"

#ifdef USE_ESP_IDF
#include <sdkconfig.h>
#endif

#if defined(USE_ESP_IDF) && defined(CONFIG_IDF_TARGET_ESP32S3)
#define MHI_RMT_CS_SPI_SUPPORTED 1
#include <driver/gpio_filter.h>
#include <driver/rmt_rx.h>
#include <driver/spi_slave.h>
#include <esp_attr.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <freertos/task.h>
#include <soc/soc_caps.h>
#else
#define MHI_RMT_CS_SPI_SUPPORTED 0
#endif

namespace esphome {
namespace mhi_ac_ctrl {

struct MhiRmtCsSpiConfig {
  uint8_t frame_size_hint{20U};
  uint32_t frame_gap_us{1000U};
  uint32_t task_stack_size{4096U};
  uint32_t task_priority{5U};
  int task_core_id{1};
};

// ESP32-S3 full-duplex backend for the no-CS MHI bus.
//
// RMT observes SCK and converts the inter-frame idle gap into a short pulse on
// the SPI slave peripheral's internal CS input. GP-SPI then captures MOSI and
// shifts MISO as one LSB-first, mode-3 transaction. A dedicated task is the
// sole owner of SPI transaction queue/result calls after setup.
class MhiRmtCsSpiTransport final : public IMhiDuplexTransport {
 public:
  ~MhiRmtCsSpiTransport() override;

  void set_config(const MhiRmtCsSpiConfig& config) {
    config_ = config;
  }

  bool setup(const MhiTransportPins& pins) override;
  void loop() override;
  void shutdown() override;
  std::size_t read(uint8_t* dst, std::size_t max_len) override;
  bool send(const MhiTxEnvelope& envelope) override;
  bool take_tx_completion(MhiTxCompletion& completion) override;

  const char* name() const override {
    return "rmt_cs_spi";
  }

  bool ready() const override {
    return ready_.load(std::memory_order_acquire);
  }

  uint32_t completed_tx_frames() const override;
  uint32_t tx_failures() const override;
  std::size_t tx_completion_queue_depth() const override;
  std::size_t tx_completion_queue_high_water() const override;
  uint32_t tx_completion_queue_dropped() const override;
  std::size_t rx_queue_depth() const override;
  std::size_t rx_queue_high_water() const override;
  uint32_t rx_queue_overwritten() const override;

 private:
  static constexpr std::size_t kTransactionQueueDepth = 1U;
  static constexpr std::size_t kCompletedFrameQueueDepth = 4U;
  static constexpr std::size_t kDmaTransferBytes = (kMhiMaxFrameBytes + 3U) & ~std::size_t{3U};
  static constexpr std::size_t kRmtSymbolBufferSize = (kMhiMaxFrameBytes * 8U) + 8U;

#if MHI_RMT_CS_SPI_SUPPORTED
  struct TransactionSlot {
    spi_slave_transaction_t transaction{};
    uint8_t* rx_buffer{nullptr};
    uint8_t* tx_buffer{nullptr};
    MhiTxEnvelope tx_envelope{};
  };

  static bool rmt_receive_done_callback_(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t* event_data,
                                         void* user_context);
  static void task_entry_(void* arg);

  bool validate_config_() const;
  bool allocate_transaction_buffers_();
  bool setup_spi_();
  bool setup_rmt_();
  bool setup_glitch_filter_();
  bool start_task_();
  bool queue_transaction_(TransactionSlot& slot);
  void task_loop_();
  void process_completed_transaction_(spi_slave_transaction_t* completed);
  void cleanup_();
  void connect_internal_cs_(bool inactive_high);
  void on_frame_boundary_from_isr_();

  spi_host_device_t host_{SPI2_HOST};
  std::array<TransactionSlot, kTransactionQueueDepth> transaction_slots_{};
  MhiFrameQueue<kCompletedFrameQueueDepth> completed_frames_{};
  MhiDuplexTxMailbox tx_mailbox_{};
  MhiTxCompletionQueue<8U> tx_completions_{};
  mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;

  rmt_channel_handle_t rmt_channel_{nullptr};
  rmt_receive_config_t rmt_receive_config_{};
  std::array<rmt_symbol_word_t, kRmtSymbolBufferSize> rmt_symbols_{};

#if SOC_GPIO_SUPPORT_PIN_GLITCH_FILTER
  gpio_glitch_filter_handle_t glitch_filter_{nullptr};
#endif

  TaskHandle_t task_handle_{nullptr};
  std::atomic<bool> task_running_{false};
  std::atomic<bool> stop_requested_{false};
  bool spi_initialized_{false};
  bool rmt_enabled_{false};

  volatile uint32_t marker_sequence_{0U};
  volatile uint32_t marker_frame_end_us_{0U};
  volatile uint32_t rmt_boundaries_{0U};
  volatile uint32_t rmt_rearm_errors_{0U};
#endif

  MhiRmtCsSpiConfig config_{};
  MhiTransportPins pins_{};
  std::atomic<bool> ready_{false};

  uint32_t completed_transactions_{0U};
  uint32_t completed_tx_frames_{0U};
  uint32_t tx_failures_{0U};
  uint32_t frames_20_{0U};
  uint32_t frames_33_{0U};
  uint32_t invalid_length_transactions_{0U};
  uint32_t transaction_result_errors_{0U};
  uint32_t transaction_queue_errors_{0U};
  uint32_t dropped_frames_{0U};
  uint32_t last_diag_log_ms_{0U};
};

}  // namespace mhi_ac_ctrl
}  // namespace esphome
