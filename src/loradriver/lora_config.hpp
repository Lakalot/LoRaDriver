#pragma once

#include <cstdint>

#include "loradriver/lora_error.hpp"

namespace loradriver {

/// @brief Chip family variant. Affects frequency validation and (for SX1277) SF cap.
enum class ChipModel : std::uint8_t { SX1276, SX1277, SX1278, SX1279 };

/// @brief PA output pin selection.
enum class PaOutput : std::uint8_t { PaBoost, Rfo };

/// @brief Complete radio configuration passed to LoRaTransceiver::begin().
///
/// All defaults are sane for SX1276 868 MHz LoRa P2P. Override per
/// member as needed. Call validate() before passing to begin().
struct LoRaConfig {
    // RF
    std::uint32_t frequency_hz = 868'000'000u;
    std::uint8_t spreading_factor = 9;
    std::uint32_t bandwidth_hz = 125'000u;
    std::uint8_t coding_rate = 5;
    std::uint16_t preamble_length = 8;
    std::uint16_t symbol_timeout = 100;
    std::uint16_t sync_word = 0x12;
    bool crc_enabled = true;
    bool invert_iq = false;
    bool implicit_header = false;

    // Power
    std::int8_t tx_power_dbm = 14;
    PaOutput pa_output = PaOutput::PaBoost;
    std::uint8_t ocp_ma = 100;

    // Optimisations
    bool ldro_auto = true;
    bool agc_auto = true;
    bool lna_boost_rx = false;
    bool isr_snapshot = false;
    bool tcxo_enabled = false;               // external 32 MHz TCXO clock
                                             // (TTGO LoRa32, Heltec WiFi LoRa).
    bool skip_image_calibration = false;     // Skip the 1ms RX image
                                             // calibration at init. Use
                                             // when re-initing on a chip
                                             // already calibrated.
    bool auto_reset = true;                  // Driver pulses pin_reset before init
    std::uint16_t reset_low_ms = 2;          // RST low duration
    std::uint16_t reset_settle_ms = 10;      // Wait after RST high before SPI
    bool polling_mode = false;               // process_events() reads RegIrqFlags
                                             // every call regardless of ring buffer.
                                             // Use when no DIO0 ISR is attached.
    std::uint32_t rx_silence_timeout_ms = 0; // 0 = disabled. >0 = emit
                                             // RxTimeout if no RxDone seen
                                             // in this window during
                                             // RX_CONTINUOUS.

    // Chip + pinout
    ChipModel chip = ChipModel::SX1276;
    std::uint32_t spi_frequency_hz = 8'000'000u;
    std::int8_t pin_ss = -1;
    std::int8_t pin_reset = -1;
    std::int8_t pin_dio0 = -1;
    std::int8_t pin_dio1 = -1;

    /// @brief Optional SPI bus pin override (ESP32 boards with non-default
    /// MOSI/MISO/SCK like TTGO MOSI=27 or SYNC-SIGNAL-LORA MOSI=22).
    /// All -1 (default) → LoRa::begin() calls SPI.begin() with no arguments.
    /// Any pin >= 0 → LoRa::begin() calls SPI.begin(sck, miso, mosi).
    struct SpiPins {
        std::int8_t sck = -1;
        std::int8_t miso = -1;
        std::int8_t mosi = -1;
    };
    SpiPins spi_pins;

    /// @brief FreeRTOS pump task tuning (ESP32 only). Defaults match the
    /// values previously hardcoded in pump_.start(trx, 2, 2, 2048, 1, 4, 1000).
    struct PumpConfig {
        std::uint32_t period_ms = 2;
        std::uint8_t priority = 2;
        std::uint32_t stack_words = 2048;
        std::int8_t core_id = 1;
        std::uint8_t tx_queue_depth = 4;
        std::uint32_t stop_timeout_ms = 1000;
    };
    PumpConfig pump;

    /// @brief LoRa::begin() automatically enters start_receive(true) after
    /// init. Set false for sender-only sketches.
    bool facade_auto_start_receive = true;

    /// @brief LoRa::begin() automatically starts RadioPumpTask after init
    /// (ESP32 only). Set false for polling-only sketches.
    bool facade_auto_pump = true;

    /// @brief Reject configurations that the chip cannot honour.
    /// @return OK if every field is in range and mutually consistent.
    [[nodiscard]] LoRaError validate() const noexcept;

    /// @brief Whether Low Data Rate Optimise must be enabled for this SF/BW.
    /// @return true if symbol duration > 16 ms (Semtech AN1200.24).
    [[nodiscard]] bool ldro_required() const noexcept;

    // ===== Named presets (constexpr, zero runtime cost) =====

    /// @brief ESP32 + SX1276 868 MHz Europe. SF9 / BW 125k / CR 4/5,
    /// sync 0x12 (private LoRa P2P; use 0x34 for LoRaWAN public),
    /// PA_BOOST 14 dBm. Override fields after construction.
    [[nodiscard]] static constexpr LoRaConfig esp32_sx1276_868mhz(
        std::int8_t cs, std::int8_t rst, std::int8_t dio0) noexcept {
        LoRaConfig c{};
        c.chip = ChipModel::SX1276;
        c.frequency_hz = 868'000'000u;
        c.spreading_factor = 9;
        c.bandwidth_hz = 125'000u;
        c.coding_rate = 5;
        c.sync_word = 0x12;
        c.tx_power_dbm = 14;
        c.pa_output = PaOutput::PaBoost;
        c.pin_ss = cs;
        c.pin_reset = rst;
        c.pin_dio0 = dio0;
        return c;
    }

    /// @brief ESP32 + SX1278 433 MHz. SF9 / BW 125k / CR 4/5,
    /// sync 0x12 (private LoRa P2P; use 0x34 for LoRaWAN public),
    /// PA_BOOST 14 dBm.
    [[nodiscard]] static constexpr LoRaConfig esp32_sx1278_433mhz(
        std::int8_t cs, std::int8_t rst, std::int8_t dio0) noexcept {
        LoRaConfig c{};
        c.chip = ChipModel::SX1278;
        c.frequency_hz = 433'920'000u;
        c.spreading_factor = 9;
        c.bandwidth_hz = 125'000u;
        c.coding_rate = 5;
        c.sync_word = 0x12;
        c.tx_power_dbm = 14;
        c.pa_output = PaOutput::PaBoost;
        c.pin_ss = cs;
        c.pin_reset = rst;
        c.pin_dio0 = dio0;
        return c;
    }

    /// @brief Generic Arduino + SX1276 868 MHz. SF9 / BW 125k / CR 4/5,
    /// sync 0x12 (private LoRa P2P; use 0x34 for LoRaWAN public),
    /// PA_BOOST 14 dBm.
    [[nodiscard]] static constexpr LoRaConfig arduino_sx1276_868mhz(
        std::int8_t cs, std::int8_t rst, std::int8_t dio0) noexcept {
        LoRaConfig c{};
        c.chip = ChipModel::SX1276;
        c.frequency_hz = 868'000'000u;
        c.spreading_factor = 9;
        c.bandwidth_hz = 125'000u;
        c.coding_rate = 5;
        c.sync_word = 0x12;
        c.tx_power_dbm = 14;
        c.pa_output = PaOutput::PaBoost;
        c.pin_ss = cs;
        c.pin_reset = rst;
        c.pin_dio0 = dio0;
        return c;
    }

    /// @brief Generic Arduino + SX1278 433 MHz. SF9 / BW 125k / CR 4/5,
    /// sync 0x12 (private LoRa P2P; use 0x34 for LoRaWAN public),
    /// PA_BOOST 14 dBm.
    [[nodiscard]] static constexpr LoRaConfig arduino_sx1278_433mhz(
        std::int8_t cs, std::int8_t rst, std::int8_t dio0) noexcept {
        LoRaConfig c{};
        c.chip = ChipModel::SX1278;
        c.frequency_hz = 433'920'000u;
        c.spreading_factor = 9;
        c.bandwidth_hz = 125'000u;
        c.coding_rate = 5;
        c.sync_word = 0x12;
        c.tx_power_dbm = 14;
        c.pa_output = PaOutput::PaBoost;
        c.pin_ss = cs;
        c.pin_reset = rst;
        c.pin_dio0 = dio0;
        return c;
    }
};

} // namespace loradriver
