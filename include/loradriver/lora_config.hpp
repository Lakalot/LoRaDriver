#pragma once

#include <cstdint>

#include "loradriver/lora_error.hpp"

namespace loradriver {

enum class ChipModel : std::uint8_t { SX1276, SX1277, SX1278, SX1279 };
enum class PaOutput : std::uint8_t { PaBoost, Rfo };

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

    [[nodiscard]] LoRaError validate() const noexcept;
    [[nodiscard]] bool ldro_required() const noexcept;
};

} // namespace loradriver
