#pragma once

#ifdef ARDUINO_ARCH_ESP32

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <cstdint>
#include <cstring>

#include "loradriver/lora_error.hpp"
#include "loradriver/lora_transceiver.hpp"

namespace loradriver::platform::esp32 {

class RadioPumpTask {
public:
    struct Metrics {
        std::uint32_t polls = 0;
        std::uint32_t max_poll_us = 0;
        std::uint64_t total_poll_us = 0;
        std::uint32_t tx_enqueued = 0;
        std::uint32_t tx_errors = 0;
    };

    RadioPumpTask() = default;
    ~RadioPumpTask() {
        stop();
        if (tx_queue_) {
            vQueueDelete(tx_queue_);
            tx_queue_ = nullptr;
        }
    }

    RadioPumpTask(const RadioPumpTask&) = delete;
    RadioPumpTask& operator=(const RadioPumpTask&) = delete;

    bool start(LoRaTransceiver& trx, std::uint32_t period_ms = 2, UBaseType_t priority = 2,
               std::uint32_t stack_words = 2048, BaseType_t core_id = 1,
               std::uint8_t tx_queue_depth = 4, std::uint32_t stop_timeout_ms = 1000) {
        stop();
        trx_ = &trx;
        period_ms_ = (period_ms == 0u) ? 1u : period_ms;
        stop_timeout_ms_ = stop_timeout_ms;

        if (!tx_queue_) {
            tx_queue_ = xQueueCreate(tx_queue_depth, sizeof(TxItem));
            if (!tx_queue_)
                return false;
        }

        const BaseType_t rc =
            xTaskCreatePinnedToCore(&RadioPumpTask::task_entry, "lora_pump", stack_words, this,
                                    priority, const_cast<TaskHandle_t*>(&task_), core_id);
        return rc == pdPASS;
    }

    void stop() {
        const TaskHandle_t t = task_;
        if (t == nullptr)
            return;
        // Signal the task to exit; it will vTaskDelete(nullptr) itself.
        stop_requested_ = true;
        // Nudge in case task is blocked in ulTaskNotifyTake. Non-ISR context
        // here — xTaskNotifyGive is the correct API (not the FromISR variant).
        xTaskNotifyGive(t);
        // Wait up to stop_timeout_ms_ for the task to drain its cycle.
        const std::uint32_t step_ms = 10;
        const std::uint32_t iters = (stop_timeout_ms_ + step_ms - 1) / step_ms;
        for (std::uint32_t i = 0; i < iters && task_ != nullptr; ++i) {
            vTaskDelay(pdMS_TO_TICKS(step_ms));
        }
        if (task_ != nullptr) {
            // Last-resort kill if the task didn't honour the request.
            vTaskDelete(task_);
            task_ = nullptr;
        }
        stop_requested_ = false;
        tx_pending_ = false;
    }

    bool running() const noexcept { return task_ != nullptr; }

    bool enqueue_packet(const std::uint8_t* data, std::uint8_t len) {
        if (!tx_queue_ || !task_ || data == nullptr || len == 0u)
            return false;
        TxItem item{};
        item.len = len;
        std::memcpy(item.data, data, len);
        if (xQueueSend(tx_queue_, &item, 0) != pdTRUE)
            return false;
        portENTER_CRITICAL(&mux_);
        ++metrics_.tx_enqueued;
        portEXIT_CRITICAL(&mux_);
        return true;
    }

    void IRAM_ATTR notify_from_isr() {
        const TaskHandle_t t = task_;
        if (t == nullptr)
            return;
        BaseType_t woken = pdFALSE;
        vTaskNotifyGiveFromISR(t, &woken);
        portYIELD_FROM_ISR(woken);
    }

    Metrics metrics() const {
        Metrics out{};
        portENTER_CRITICAL(&mux_);
        out = metrics_;
        portEXIT_CRITICAL(&mux_);
        return out;
    }

    void reset_metrics() {
        portENTER_CRITICAL(&mux_);
        metrics_ = Metrics{};
        portEXIT_CRITICAL(&mux_);
    }

private:
    struct TxItem {
        std::uint8_t data[255];
        std::uint8_t len;
    };

    static void task_entry(void* arg) {
        auto* self = static_cast<RadioPumpTask*>(arg);
        const TickType_t period_ticks = pdMS_TO_TICKS(self->period_ms_);

        while (self->task_ != nullptr && !self->stop_requested_) {
            if (!self->tx_pending_ && self->tx_queue_) {
                TxItem item;
                if (xQueueReceive(self->tx_queue_, &item, 0) == pdTRUE) {
                    self->tx_pending_ = true;
                    const LoRaError err = self->trx_->send(item.data, item.len, 500);
                    if (err != LoRaError::OK) {
                        self->tx_pending_ = false;
                        portENTER_CRITICAL(&self->mux_);
                        ++self->metrics_.tx_errors;
                        portEXIT_CRITICAL(&self->mux_);
                    }
                }
            }

            ulTaskNotifyTake(pdTRUE, period_ticks);
            if (self->trx_ == nullptr)
                continue;

            const std::uint32_t start_us = static_cast<std::uint32_t>(micros());
            self->trx_->poll();
            const std::uint32_t elapsed_us = static_cast<std::uint32_t>(micros() - start_us);

            if (self->tx_pending_) {
                const auto st = self->trx_->state();
                if (st != LoRaTransceiver::State::Tx) {
                    self->tx_pending_ = false;
                    if (st == LoRaTransceiver::State::Standby) {
                        (void)self->trx_->start_receive(true);
                    }
                }
            }

            portENTER_CRITICAL(&self->mux_);
            ++self->metrics_.polls;
            self->metrics_.total_poll_us += elapsed_us;
            if (elapsed_us > self->metrics_.max_poll_us) {
                self->metrics_.max_poll_us = elapsed_us;
            }
            portEXIT_CRITICAL(&self->mux_);
        }
        self->task_ = nullptr;
        vTaskDelete(nullptr);
    }

    volatile TaskHandle_t task_ = nullptr;
    LoRaTransceiver* trx_ = nullptr;
    volatile bool stop_requested_ = false;
    std::uint32_t period_ms_ = 2;
    std::uint32_t stop_timeout_ms_ = 1000; // total budget for cooperative stop
    QueueHandle_t tx_queue_ = nullptr;
    volatile bool tx_pending_ = false;
    mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
    Metrics metrics_{};
};

} // namespace loradriver::platform::esp32

#endif // ARDUINO_ARCH_ESP32
