#pragma once

#include <cstddef>
#include <cstdint>

#include "loradriver/chips/sx127x_driver.hpp"
#include "loradriver/lora_config.hpp"
#include "loradriver/lora_error.hpp"
#include "loradriver/lora_packet.hpp"
#include "loradriver/lora_transceiver.hpp"
#include "loradriver/radio_event.hpp"
#include "loradriver/radio_stats.hpp"

#ifdef ARDUINO
#include "loradriver/hal/arduino_spi_device.hpp"
#endif
#ifdef ARDUINO_ARCH_ESP32
#include "loradriver/hal/esp32_spi_device.hpp"
#include "loradriver/platform/esp32/radio_pump_task.hpp"
#endif

namespace loradriver {

/// @brief High-level facade that wraps SPI HAL + chip driver + transceiver
/// + (ESP32) FreeRTOS pump task in a single object.
///
/// Reduces the minimal ESP32 setup from ~40 lines to ~12. Use the global
/// instance @ref lora for the common case; drop down to LoRaTransceiver
/// for multi-instance, host tests with FakeSpiDevice, or custom HAL.
///
/// @note Facade is single-instance per binary. Multi-instance is supported
///       through the direct DI API (see examples/MultiInstance).
class LoRa {
public:
    LoRa() noexcept = default;
    ~LoRa();
    LoRa(const LoRa&) = delete;
    LoRa& operator=(const LoRa&) = delete;

    /// Initialise SPI bus, attach DIO0 ISR, start the pump task (ESP32),
    /// and enter RX continuous mode. See LoRaConfig::facade_auto_* flags to
    /// opt out of the last two steps.
    [[nodiscard]] LoRaError begin(const LoRaConfig& cfg) noexcept;

    /// Tear down. Idempotent. Detaches DIO0, stops the pump task,
    /// puts the chip to sleep.
    void end() noexcept;

    [[nodiscard]] bool is_running() const noexcept { return running_; }

    // === Send ===
    [[nodiscard]] LoRaError send(const std::uint8_t* data, std::size_t len,
                                 std::uint32_t timeout_ms = 2000) noexcept;
#ifdef ARDUINO_ARCH_ESP32
    /// Non-blocking enqueue into the pump task's TX queue. Returns false
    /// if the queue is full or the pump is not running.
    [[nodiscard]] bool send_async(const std::uint8_t* data, std::uint8_t len) noexcept;
#endif

    // === Receive control ===
    [[nodiscard]] LoRaError start_receive(bool continuous = true) noexcept;
    [[nodiscard]] LoRaError set_standby() noexcept;
    [[nodiscard]] LoRaError set_sleep() noexcept;
    [[nodiscard]] LoRaError start_cad(bool auto_rx = false) noexcept;

    // === Callbacks (forwarded to inner LoRaTransceiver) ===
    void on_receive(LoRaTransceiver::PacketCallback cb) noexcept;
    void on_event(LoRaTransceiver::EventCallback cb) noexcept;
    void on_tx_done(LoRaTransceiver::TxDoneCallback cb) noexcept;
    void on_header(LoRaTransceiver::HeaderCallback cb) noexcept;

    // === Metrics ===
    [[nodiscard]] std::int16_t rssi() const noexcept;
    [[nodiscard]] float        snr()  const noexcept;
    [[nodiscard]] std::int32_t frequency_error_hz() const noexcept;
    [[nodiscard]] RadioStats   stats() const noexcept;
    [[nodiscard]] LoRaError    check_alive() noexcept;

#ifdef ARDUINO_ARCH_ESP32
    [[nodiscard]] platform::esp32::RadioPumpTask::Metrics
        pump_metrics() const noexcept;
#endif

    // === Escape hatches to the direct DI API ===
#ifdef ARDUINO
    [[nodiscard]] LoRaTransceiver&     transceiver() noexcept { return trx_; }
    [[nodiscard]] chips::SX127xDriver& driver() noexcept { return drv_; }
#endif
#ifdef ARDUINO_ARCH_ESP32
    [[nodiscard]] platform::esp32::RadioPumpTask& pump() noexcept { return pump_; }
#endif

private:
#ifdef ARDUINO
    // ISR trampoline is a friend so it can reach the private inner objects.
    friend void loradriver_isr_dio0_trampoline();
#endif

#ifdef ARDUINO_ARCH_ESP32
    hal::Esp32SpiDevice            spi_;
    chips::SX127xDriver            drv_{spi_};
    LoRaTransceiver                trx_{drv_};
    platform::esp32::RadioPumpTask pump_;
#elif defined(ARDUINO)
    hal::ArduinoSpiDevice          spi_;
    chips::SX127xDriver            drv_{spi_};
    LoRaTransceiver                trx_{drv_};
#else
    // Host build path: no Arduino runtime. The facade still has a usable
    // transceiver()/driver() pair via the test-only constructor (see
    // LORADRIVER_FACADE_HOST_TEST below, introduced in Task 6).
#endif

    bool        running_ = false;
    std::int8_t attached_dio0_ = -1;

    static LoRa* instance_;
};

#if defined(ARDUINO) || defined(ARDUINO_ARCH_ESP32)
/// Global facade instance. Lives in .bss. Defined in src/api/lora_facade.cpp.
extern LoRa lora;
#endif

} // namespace loradriver
