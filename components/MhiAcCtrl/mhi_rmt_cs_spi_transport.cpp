#include "mhi_rmt_cs_spi_transport.h"

#include <algorithm>
#include <cstring>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#if MHI_RMT_CS_SPI_SUPPORTED
#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_rom_gpio.h>
#include <esp_timer.h>
#include <soc/gpio_sig_map.h>
#include <soc/spi_periph.h>
#endif

namespace esphome {
namespace mhi_ac_ctrl {

static const char* const TAG = "mhi_rmt_cs_spi";

MhiRmtCsSpiTransport::~MhiRmtCsSpiTransport() {
  this->shutdown();
}

bool MhiRmtCsSpiTransport::setup(const MhiTransportPins& pins) {
  pins_ = pins;

#if !MHI_RMT_CS_SPI_SUPPORTED
  ESP_LOGE(TAG, "rmt_cs_spi requires ESP-IDF on ESP32-S3");
  ready_ = false;
  return false;
#else
  this->cleanup_();

  if (!this->validate_config_()) {
    return false;
  }

  completed_transactions_ = 0U;
  completed_tx_frames_ = 0U;
  tx_failures_ = 0U;
  frames_20_ = 0U;
  frames_33_ = 0U;
  invalid_length_transactions_ = 0U;
  transaction_result_errors_ = 0U;
  transaction_queue_errors_ = 0U;
  dropped_frames_ = 0U;
  last_diag_log_ms_ = 0U;

  portENTER_CRITICAL(&mux_);
  completed_frames_.clear();
  tx_mailbox_.clear();
  tx_completions_.reset();
  marker_sequence_ = 0U;
  marker_frame_end_us_ = 0U;
  rmt_boundaries_ = 0U;
  rmt_rearm_errors_ = 0U;
  portEXIT_CRITICAL(&mux_);

  if (!this->allocate_transaction_buffers_() || !this->setup_spi_() || !this->setup_rmt_()) {
    this->cleanup_();
    return false;
  }

  if (!this->queue_transaction_(transaction_slots_[0])) {
    this->cleanup_();
    return false;
  }

  if (!this->start_task_()) {
    this->cleanup_();
    return false;
  }

  const esp_err_t receive_result =
      rmt_receive(rmt_channel_, rmt_symbols_.data(), sizeof(rmt_symbols_), &rmt_receive_config_);
  if (receive_result != ESP_OK) {
    ESP_LOGE(TAG, "rmt_receive failed: %s", esp_err_to_name(receive_result));
    this->cleanup_();
    return false;
  }

  // The transaction is queued while CS is inactive. Pull internal CS active
  // only after SPI, RMT, and the owner task are ready.
  this->connect_internal_cs_(false);
  ready_ = true;

  ESP_LOGW(TAG,
           "RMT-CS SPI duplex enabled: host=SPI2 SCK=%d MOSI=%d MISO=%d mode=3 LSB-first DMA=%u bytes "
           "frame=%u frame_gap=%luus task_core=%d task_priority=%lu task_stack=%lu",
           pins_.sck, pins_.mosi, pins_.miso, static_cast<unsigned int>(kDmaTransferBytes),
           static_cast<unsigned int>(config_.frame_size_hint), static_cast<unsigned long>(config_.frame_gap_us),
           config_.task_core_id, static_cast<unsigned long>(config_.task_priority),
           static_cast<unsigned long>(config_.task_stack_size));

  return true;
#endif
}

void MhiRmtCsSpiTransport::loop() {
#if MHI_RMT_CS_SPI_SUPPORTED
  if (!ready_.load(std::memory_order_acquire)) {
    return;
  }

  const uint32_t now = millis();
  if (last_diag_log_ms_ != 0U && (now - last_diag_log_ms_) < 30000U) {
    return;
  }
  last_diag_log_ms_ = now;

  uint32_t boundaries = 0U;
  uint32_t rearm_errors = 0U;
  uint32_t completed_transactions = 0U;
  uint32_t completed_tx_frames = 0U;
  uint32_t tx_failures = 0U;
  uint32_t frames_20 = 0U;
  uint32_t frames_33 = 0U;
  uint32_t invalid_length_transactions = 0U;
  uint32_t transaction_result_errors = 0U;
  uint32_t transaction_queue_errors = 0U;
  uint32_t dropped_frames = 0U;
  uint32_t overwritten_rx = 0U;
  uint32_t overwritten_tx = 0U;
  std::size_t queued_frames = 0U;
  std::size_t max_buffered_frames = 0U;

  portENTER_CRITICAL(&mux_);
  boundaries = rmt_boundaries_;
  rearm_errors = rmt_rearm_errors_;
  completed_transactions = completed_transactions_;
  completed_tx_frames = completed_tx_frames_;
  tx_failures = tx_failures_;
  frames_20 = frames_20_;
  frames_33 = frames_33_;
  invalid_length_transactions = invalid_length_transactions_;
  transaction_result_errors = transaction_result_errors_;
  transaction_queue_errors = transaction_queue_errors_;
  dropped_frames = dropped_frames_;
  overwritten_rx = completed_frames_.overwritten_frames();
  overwritten_tx = tx_mailbox_.overwritten_frames();
  queued_frames = completed_frames_.size();
  max_buffered_frames = completed_frames_.high_water_mark();
  portEXIT_CRITICAL(&mux_);

  ESP_LOGI(TAG,
           "runtime: boundaries=%lu completed=%lu tx_completed=%lu tx_failures=%lu frame20=%lu frame33=%lu "
           "invalid_len=%lu result_errors=%lu queue_errors=%lu rmt_rearm_errors=%lu buffered_frames=%u "
           "max_buffered=%u rx_overwritten=%lu tx_overwritten=%lu dropped=%lu task_running=%s",
           static_cast<unsigned long>(boundaries), static_cast<unsigned long>(completed_transactions),
           static_cast<unsigned long>(completed_tx_frames), static_cast<unsigned long>(tx_failures),
           static_cast<unsigned long>(frames_20), static_cast<unsigned long>(frames_33),
           static_cast<unsigned long>(invalid_length_transactions),
           static_cast<unsigned long>(transaction_result_errors), static_cast<unsigned long>(transaction_queue_errors),
           static_cast<unsigned long>(rearm_errors), static_cast<unsigned int>(queued_frames),
           static_cast<unsigned int>(max_buffered_frames), static_cast<unsigned long>(overwritten_rx),
           static_cast<unsigned long>(overwritten_tx), static_cast<unsigned long>(dropped_frames),
           task_running_.load(std::memory_order_acquire) ? "YES" : "NO");
#endif
}

void MhiRmtCsSpiTransport::shutdown() {
#if MHI_RMT_CS_SPI_SUPPORTED
  this->cleanup_();
#else
  ready_ = false;
#endif
}

std::size_t MhiRmtCsSpiTransport::read(uint8_t* dst, std::size_t max_len) {
#if !MHI_RMT_CS_SPI_SUPPORTED
  (void)dst;
  (void)max_len;
  return 0U;
#else
  if (!ready_.load(std::memory_order_acquire) || dst == nullptr || max_len == 0U) {
    return 0U;
  }

  MhiCapturedFrame frame{};

  portENTER_CRITICAL(&mux_);
  const bool available = completed_frames_.pop(frame);
  portEXIT_CRITICAL(&mux_);

  if (!available) {
    return 0U;
  }

  if (frame.len > max_len) {
    portENTER_CRITICAL(&mux_);
    dropped_frames_++;
    portEXIT_CRITICAL(&mux_);
    ESP_LOGW(TAG, "RX destination too small: frame=%u destination=%u; frame dropped",
             static_cast<unsigned int>(frame.len), static_cast<unsigned int>(max_len));
    return 0U;
  }

  std::memcpy(dst, frame.data.data(), frame.len);
  return frame.len;
#endif
}

bool MhiRmtCsSpiTransport::send(const MhiTxEnvelope& envelope) {
#if !MHI_RMT_CS_SPI_SUPPORTED
  (void)envelope;
  return false;
#else
  if (!ready_.load(std::memory_order_acquire) || !envelope.valid() || envelope.len != config_.frame_size_hint) {
    return false;
  }

  portENTER_CRITICAL(&mux_);
  const bool staged = tx_mailbox_.stage(envelope);
  portEXIT_CRITICAL(&mux_);
  return staged;
#endif
}

bool MhiRmtCsSpiTransport::take_tx_completion(MhiTxCompletion& completion) {
#if !MHI_RMT_CS_SPI_SUPPORTED
  (void)completion;
  return false;
#else
  portENTER_CRITICAL(&mux_);
  const bool available = tx_completions_.pop(completion);
  portEXIT_CRITICAL(&mux_);
  return available;
#endif
}

uint32_t MhiRmtCsSpiTransport::completed_tx_frames() const {
#if !MHI_RMT_CS_SPI_SUPPORTED
  return 0U;
#else
  uint32_t value = 0U;
  portENTER_CRITICAL(const_cast<portMUX_TYPE*>(&mux_));
  value = completed_tx_frames_;
  portEXIT_CRITICAL(const_cast<portMUX_TYPE*>(&mux_));
  return value;
#endif
}

uint32_t MhiRmtCsSpiTransport::tx_failures() const {
#if !MHI_RMT_CS_SPI_SUPPORTED
  return 0U;
#else
  uint32_t value = 0U;
  portENTER_CRITICAL(const_cast<portMUX_TYPE*>(&mux_));
  value = tx_failures_;
  portEXIT_CRITICAL(const_cast<portMUX_TYPE*>(&mux_));
  return value;
#endif
}

#if MHI_RMT_CS_SPI_SUPPORTED
bool MhiRmtCsSpiTransport::validate_config_() const {
  if (pins_.sck < 0 || pins_.mosi < 0 || pins_.miso < 0) {
    ESP_LOGE(TAG, "RMT-CS SPI setup failed: invalid pins SCK=%d MOSI=%d MISO=%d", pins_.sck, pins_.mosi, pins_.miso);
    return false;
  }

  if (pins_.sck == pins_.mosi || pins_.sck == pins_.miso || pins_.mosi == pins_.miso) {
    ESP_LOGE(TAG, "RMT-CS SPI setup failed: SCK, MOSI, and MISO must use different GPIOs");
    return false;
  }

  if (config_.frame_size_hint != kMhiFrame20Bytes && config_.frame_size_hint != kMhiFrame33Bytes) {
    ESP_LOGE(TAG, "RMT-CS SPI setup failed: unsupported frame size hint=%u",
             static_cast<unsigned int>(config_.frame_size_hint));
    return false;
  }

  if (config_.frame_gap_us < 500U || config_.frame_gap_us > 5000U) {
    ESP_LOGE(TAG, "RMT-CS SPI setup failed: frame gap must be 500..5000us, got %luus",
             static_cast<unsigned long>(config_.frame_gap_us));
    return false;
  }

  if (config_.task_stack_size < 4096U || config_.task_priority == 0U || config_.task_core_id < -1 ||
      config_.task_core_id > 1) {
    ESP_LOGE(TAG, "RMT-CS SPI setup failed: invalid task config stack=%lu priority=%lu core=%d",
             static_cast<unsigned long>(config_.task_stack_size), static_cast<unsigned long>(config_.task_priority),
             config_.task_core_id);
    return false;
  }

  return true;
}

bool IRAM_ATTR MhiRmtCsSpiTransport::rmt_receive_done_callback_(rmt_channel_handle_t channel,
                                                                const rmt_rx_done_event_data_t* event_data,
                                                                void* user_context) {
  (void)channel;
  if (event_data != nullptr && !event_data->flags.is_last) {
    return false;
  }

  if (user_context == nullptr) {
    return false;
  }

  auto* self = static_cast<MhiRmtCsSpiTransport*>(user_context);
  self->on_frame_boundary_from_isr_();
  return false;
}

void MhiRmtCsSpiTransport::task_entry_(void* arg) {
  auto* self = static_cast<MhiRmtCsSpiTransport*>(arg);
  if (self == nullptr) {
    vTaskDelete(nullptr);
    return;
  }

  self->task_loop_();
  vTaskDelete(nullptr);
}

bool MhiRmtCsSpiTransport::allocate_transaction_buffers_() {
  for (auto& slot : transaction_slots_) {
    slot.rx_buffer = static_cast<uint8_t*>(
        heap_caps_calloc(kDmaTransferBytes, sizeof(uint8_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    slot.tx_buffer = static_cast<uint8_t*>(
        heap_caps_calloc(kDmaTransferBytes, sizeof(uint8_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));

    if (slot.rx_buffer == nullptr || slot.tx_buffer == nullptr) {
      ESP_LOGE(TAG, "Failed to allocate %u-byte DMA transaction buffers", static_cast<unsigned int>(kDmaTransferBytes));
      return false;
    }
  }

  return true;
}

bool MhiRmtCsSpiTransport::setup_spi_() {
  spi_bus_config_t bus_config{};
  bus_config.mosi_io_num = pins_.mosi;
  bus_config.miso_io_num = pins_.miso;
  bus_config.sclk_io_num = pins_.sck;
  bus_config.quadwp_io_num = -1;
  bus_config.quadhd_io_num = -1;
  bus_config.data4_io_num = -1;
  bus_config.data5_io_num = -1;
  bus_config.data6_io_num = -1;
  bus_config.data7_io_num = -1;
  bus_config.max_transfer_sz = static_cast<int>(kDmaTransferBytes);
  bus_config.flags = SPICOMMON_BUSFLAG_GPIO_PINS | SPICOMMON_BUSFLAG_SLAVE;

  spi_slave_interface_config_t slave_config{};
  slave_config.spics_io_num = -1;
  slave_config.flags = SPI_SLAVE_BIT_LSBFIRST;
  slave_config.queue_size = static_cast<int>(kTransactionQueueDepth);
  slave_config.mode = 3;

  const esp_err_t result = spi_slave_initialize(host_, &bus_config, &slave_config, SPI_DMA_CH_AUTO);
  if (result != ESP_OK) {
    ESP_LOGE(TAG, "spi_slave_initialize failed: %s", esp_err_to_name(result));
    return false;
  }

  spi_initialized_ = true;
  this->connect_internal_cs_(true);
  return true;
}

bool MhiRmtCsSpiTransport::setup_rmt_() {
  rmt_rx_channel_config_t channel_config{};
  channel_config.gpio_num = static_cast<gpio_num_t>(pins_.sck);
  channel_config.clk_src = RMT_CLK_SRC_DEFAULT;
  channel_config.resolution_hz = 1000000U;
  channel_config.mem_block_symbols = SOC_RMT_MEM_WORDS_PER_CHANNEL;

  esp_err_t result = rmt_new_rx_channel(&channel_config, &rmt_channel_);
  if (result != ESP_OK) {
    ESP_LOGE(TAG, "rmt_new_rx_channel failed: %s", esp_err_to_name(result));
    return false;
  }

  rmt_rx_event_callbacks_t callbacks{};
  callbacks.on_recv_done = &MhiRmtCsSpiTransport::rmt_receive_done_callback_;

  result = rmt_rx_register_event_callbacks(rmt_channel_, &callbacks, this);
  if (result != ESP_OK) {
    ESP_LOGE(TAG, "rmt_rx_register_event_callbacks failed: %s", esp_err_to_name(result));
    return false;
  }

  result = rmt_enable(rmt_channel_);
  if (result != ESP_OK) {
    ESP_LOGE(TAG, "rmt_enable failed: %s", esp_err_to_name(result));
    return false;
  }
  rmt_enabled_ = true;

  rmt_receive_config_ = {};
  rmt_receive_config_.signal_range_min_ns = 1000U;
  rmt_receive_config_.signal_range_max_ns = config_.frame_gap_us * 1000U;

  this->setup_glitch_filter_();
  return true;
}

bool MhiRmtCsSpiTransport::setup_glitch_filter_() {
#if SOC_GPIO_SUPPORT_PIN_GLITCH_FILTER
  gpio_pin_glitch_filter_config_t filter_config{};
  filter_config.clk_src = GLITCH_FILTER_CLK_SRC_DEFAULT;
  filter_config.gpio_num = static_cast<gpio_num_t>(pins_.sck);

  esp_err_t result = gpio_new_pin_glitch_filter(&filter_config, &glitch_filter_);
  if (result != ESP_OK) {
    ESP_LOGW(TAG, "gpio_new_pin_glitch_filter failed; continuing without filter: %s", esp_err_to_name(result));
    glitch_filter_ = nullptr;
    return false;
  }

  result = gpio_glitch_filter_enable(glitch_filter_);
  if (result != ESP_OK) {
    ESP_LOGW(TAG, "gpio_glitch_filter_enable failed; continuing without filter: %s", esp_err_to_name(result));
    gpio_del_glitch_filter(glitch_filter_);
    glitch_filter_ = nullptr;
    return false;
  }

  return true;
#else
  return false;
#endif
}

bool MhiRmtCsSpiTransport::start_task_() {
  stop_requested_ = false;
  task_running_ = false;
  task_handle_ = nullptr;

  BaseType_t created = pdFALSE;
  if (config_.task_core_id >= 0) {
    created = xTaskCreatePinnedToCore(&MhiRmtCsSpiTransport::task_entry_, "mhi_spi_transport", config_.task_stack_size,
                                      this, static_cast<UBaseType_t>(config_.task_priority), &task_handle_,
                                      static_cast<BaseType_t>(config_.task_core_id));
  } else {
    created = xTaskCreate(&MhiRmtCsSpiTransport::task_entry_, "mhi_spi_transport", config_.task_stack_size, this,
                          static_cast<UBaseType_t>(config_.task_priority), &task_handle_);
  }

  if (created != pdPASS || task_handle_ == nullptr) {
    ESP_LOGE(TAG, "Transport task start failed: stack=%lu priority=%lu core=%d",
             static_cast<unsigned long>(config_.task_stack_size), static_cast<unsigned long>(config_.task_priority),
             config_.task_core_id);
    task_handle_ = nullptr;
    return false;
  }

  return true;
}

bool MhiRmtCsSpiTransport::queue_transaction_(TransactionSlot& slot) {
  if (slot.rx_buffer == nullptr || slot.tx_buffer == nullptr) {
    return false;
  }

  std::memset(slot.rx_buffer, 0, kDmaTransferBytes);
  std::memset(slot.tx_buffer, 0, kDmaTransferBytes);
  slot.tx_envelope = {};

  portENTER_CRITICAL(&mux_);
  tx_mailbox_.take(slot.tx_buffer, kDmaTransferBytes, slot.tx_envelope);
  portEXIT_CRITICAL(&mux_);

  slot.transaction = {};
  slot.transaction.length = kDmaTransferBytes * 8U;
  slot.transaction.rx_buffer = slot.rx_buffer;
  slot.transaction.tx_buffer = slot.tx_buffer;
  slot.transaction.user = &slot;

  const esp_err_t result = spi_slave_queue_trans(host_, &slot.transaction, pdMS_TO_TICKS(20));
  if (result != ESP_OK) {
    MhiTxCompletion completion{};
    if (slot.tx_envelope.is_command()) {
      completion.generation = slot.tx_envelope.generation;
      completion.kind = slot.tx_envelope.kind;
      completion.command_mask = slot.tx_envelope.command_mask;
      completion.intent = slot.tx_envelope.intent;
      completion.success = false;
      completion.completed_at_ms = millis();
    }

    portENTER_CRITICAL(&mux_);
    transaction_queue_errors_++;
    if (slot.tx_envelope.valid()) {
      tx_failures_++;
      if (slot.tx_envelope.is_command()) {
        tx_completions_.push(completion);
      }
    }
    portEXIT_CRITICAL(&mux_);
    ESP_LOGE(TAG, "spi_slave_queue_trans failed: %s", esp_err_to_name(result));
    return false;
  }

  return true;
}

void MhiRmtCsSpiTransport::task_loop_() {
  task_running_ = true;

  while (!stop_requested_.load(std::memory_order_acquire)) {
    spi_slave_transaction_t* completed = nullptr;
    const esp_err_t result = spi_slave_get_trans_result(host_, &completed, pdMS_TO_TICKS(100));

    if (result == ESP_ERR_TIMEOUT) {
      continue;
    }

    if (result != ESP_OK) {
      portENTER_CRITICAL(&mux_);
      transaction_result_errors_++;
      portEXIT_CRITICAL(&mux_);
      if (!stop_requested_.load(std::memory_order_acquire)) {
        ESP_LOGW(TAG, "spi_slave_get_trans_result failed: %s", esp_err_to_name(result));
      }
      continue;
    }

    this->process_completed_transaction_(completed);

    if (stop_requested_.load(std::memory_order_acquire)) {
      break;
    }

    if (completed == nullptr || completed->user == nullptr) {
      ready_ = false;
      break;
    }

    auto* slot = static_cast<TransactionSlot*>(completed->user);
    if (!this->queue_transaction_(*slot)) {
      ready_ = false;
      break;
    }

    // The RMT callback leaves internal CS inactive after completing the prior
    // frame. Activate it only after the next transaction is fully queued.
    this->connect_internal_cs_(false);
  }

  task_running_.store(false, std::memory_order_release);

  // Publish task completion before self-deleting. cleanup_() treats a null
  // handle as the ownership hand-off point and may then release SPI/RMT.
  portENTER_CRITICAL(&mux_);
  task_handle_ = nullptr;
  portEXIT_CRITICAL(&mux_);
}

void MhiRmtCsSpiTransport::process_completed_transaction_(spi_slave_transaction_t* completed) {
  if (completed == nullptr || completed->user == nullptr) {
    portENTER_CRITICAL(&mux_);
    transaction_result_errors_++;
    portEXIT_CRITICAL(&mux_);
    ESP_LOGW(TAG, "Completed SPI transaction did not identify its transaction slot");
    return;
  }

  auto* slot = static_cast<TransactionSlot*>(completed->user);
  const std::size_t received_bits = completed->trans_len;
  const bool whole_bytes = (received_bits % 8U) == 0U;
  const std::size_t received_bytes = whole_bytes ? received_bits / 8U : 0U;
  const bool valid_frame_len =
      whole_bytes && (received_bytes == kMhiFrame20Bytes || received_bytes == kMhiFrame33Bytes);

  uint32_t sequence = 0U;
  uint32_t frame_end_us = 0U;
  const bool command_completion = slot->tx_envelope.is_command();
  MhiTxCompletion completion{};

  if (command_completion) {
    completion.generation = slot->tx_envelope.generation;
    completion.kind = slot->tx_envelope.kind;
    completion.command_mask = slot->tx_envelope.command_mask;
    completion.intent = slot->tx_envelope.intent;
    completion.success = valid_frame_len && received_bytes == slot->tx_envelope.len;
    completion.completed_at_ms = millis();
  }

  portENTER_CRITICAL(&mux_);
  sequence = marker_sequence_;
  frame_end_us = marker_frame_end_us_;

  if (valid_frame_len) {
    const uint32_t overwritten_before = completed_frames_.overwritten_frames();
    completed_frames_.push(slot->rx_buffer, received_bytes, sequence, frame_end_us);
    const uint32_t overwritten_after = completed_frames_.overwritten_frames();
    dropped_frames_ += overwritten_after - overwritten_before;

    completed_transactions_++;
    if (received_bytes == kMhiFrame20Bytes) {
      frames_20_++;
    } else {
      frames_33_++;
    }
  } else {
    invalid_length_transactions_++;
  }

  if (slot->tx_envelope.valid()) {
    const bool tx_success = valid_frame_len && received_bytes == slot->tx_envelope.len;
    if (tx_success) {
      completed_tx_frames_++;
    } else {
      tx_failures_++;
    }

    if (command_completion) {
      tx_completions_.push(completion);
    }
  }
  portEXIT_CRITICAL(&mux_);

  if (!valid_frame_len) {
    ESP_LOGV(TAG, "Discarded SPI transaction length=%u bits", static_cast<unsigned int>(received_bits));
  }
}

void MhiRmtCsSpiTransport::cleanup_() {
  ready_ = false;
  stop_requested_ = true;

  if (spi_initialized_) {
    this->connect_internal_cs_(true);
  }

#if SOC_GPIO_SUPPORT_PIN_GLITCH_FILTER
  if (glitch_filter_ != nullptr) {
    gpio_glitch_filter_disable(glitch_filter_);
    gpio_del_glitch_filter(glitch_filter_);
    glitch_filter_ = nullptr;
  }
#endif

  if (rmt_channel_ != nullptr) {
    if (rmt_enabled_) {
      rmt_disable(rmt_channel_);
      rmt_enabled_ = false;
    }
    rmt_del_channel(rmt_channel_);
    rmt_channel_ = nullptr;
  }

  TaskHandle_t task_to_delete = nullptr;
  for (uint8_t attempt = 0U; attempt < 25U; attempt++) {
    portENTER_CRITICAL(&mux_);
    const bool task_finished = task_handle_ == nullptr;
    portEXIT_CRITICAL(&mux_);

    if (task_finished) {
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  portENTER_CRITICAL(&mux_);
  task_to_delete = task_handle_;
  task_handle_ = nullptr;
  portEXIT_CRITICAL(&mux_);

  if (task_to_delete != nullptr) {
    // The task did not leave spi_slave_get_trans_result() within the bounded
    // shutdown window. Force deletion before freeing the peripheral.
    vTaskDelete(task_to_delete);
  }
  task_running_.store(false, std::memory_order_release);

  if (spi_initialized_) {
    spi_slave_free(host_);
    spi_initialized_ = false;
  }

  for (auto& slot : transaction_slots_) {
    if (slot.rx_buffer != nullptr) {
      heap_caps_free(slot.rx_buffer);
      slot.rx_buffer = nullptr;
    }
    if (slot.tx_buffer != nullptr) {
      heap_caps_free(slot.tx_buffer);
      slot.tx_buffer = nullptr;
    }
    slot.transaction = {};
    slot.tx_envelope = {};
  }

  portENTER_CRITICAL(&mux_);
  completed_frames_.clear();
  tx_mailbox_.clear();
  tx_completions_.reset();
  marker_sequence_ = 0U;
  marker_frame_end_us_ = 0U;
  portEXIT_CRITICAL(&mux_);

  stop_requested_ = false;
}

void MhiRmtCsSpiTransport::connect_internal_cs_(bool inactive_high) {
  const uint32_t signal = spi_periph_signal[host_].spics_in;
  const uint32_t source = inactive_high ? GPIO_MATRIX_CONST_ONE_INPUT : GPIO_MATRIX_CONST_ZERO_INPUT;
  esp_rom_gpio_connect_in_signal(source, signal, false);
}

void IRAM_ATTR MhiRmtCsSpiTransport::on_frame_boundary_from_isr_() {
  // End the current full-duplex SPI transaction. The owner task queues the
  // next descriptor during the frame gap and then asserts internal CS again.
  const uint32_t signal = spi_periph_signal[host_].spics_in;
  esp_rom_gpio_connect_in_signal(GPIO_MATRIX_CONST_ONE_INPUT, signal, false);

  portENTER_CRITICAL_ISR(&mux_);
  marker_sequence_++;
  marker_frame_end_us_ = static_cast<uint32_t>(esp_timer_get_time());
  rmt_boundaries_++;
  portEXIT_CRITICAL_ISR(&mux_);

  const esp_err_t result = rmt_receive(rmt_channel_, rmt_symbols_.data(), sizeof(rmt_symbols_), &rmt_receive_config_);
  if (result != ESP_OK) {
    portENTER_CRITICAL_ISR(&mux_);
    rmt_rearm_errors_++;
    portEXIT_CRITICAL_ISR(&mux_);
  }
}
#endif

}  // namespace mhi_ac_ctrl
}  // namespace esphome
