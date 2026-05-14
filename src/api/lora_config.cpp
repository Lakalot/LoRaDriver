#include "loradriver/lora_config.hpp"

namespace loradriver {

namespace {

constexpr std::uint32_t kAllowedBw[] = {
    7'800u, 10'400u, 15'600u, 20'800u, 31'250u, 41'700u, 62'500u, 125'000u, 250'000u, 500'000u,
};

constexpr bool bw_allowed(std::uint32_t hz) noexcept {
    for (auto v : kAllowedBw) {
        if (v == hz)
            return true;
    }
    return false;
}

constexpr bool freq_in_range_sx1278(std::uint32_t hz) noexcept {
    return hz >= 137'000'000u && hz <= 525'000'000u;
}

constexpr bool freq_in_range_sx1276(std::uint32_t hz) noexcept {
    return hz >= 137'000'000u && hz <= 1'020'000'000u;
}

constexpr bool is_high_band(std::uint32_t hz) noexcept {
    return hz >= 525'000'000u;
}

} // namespace

LoRaError LoRaConfig::validate() const noexcept {
    // Frequency vs chip
    switch (chip) {
    case ChipModel::SX1278:
        if (!freq_in_range_sx1278(frequency_hz))
            return LoRaError::InvalidConfig;
        break;
    case ChipModel::SX1277:
        // SX1277: 137-1020 MHz, same as SX1276 except SF max=9
        if (!freq_in_range_sx1276(frequency_hz))
            return LoRaError::InvalidConfig;
        if (spreading_factor > 9)
            return LoRaError::InvalidConfig;
        break;
    case ChipModel::SX1279:
        // SX1279: 137-960 MHz
        if (frequency_hz < 137'000'000u || frequency_hz > 960'000'000u) {
            return LoRaError::InvalidConfig;
        }
        break;
    case ChipModel::SX1276:
    default:
        if (!freq_in_range_sx1276(frequency_hz))
            return LoRaError::InvalidConfig;
        break;
    }

    // Bandwidth (membership)
    if (!bw_allowed(bandwidth_hz))
        return LoRaError::InvalidConfig;

    // Errata: BW 500 kHz only allowed in high-band; on SX1278 (low-band) → reject
    if (bandwidth_hz == 500'000u && !is_high_band(frequency_hz)) {
        return LoRaError::InvalidConfig;
    }

    // SF
    if (spreading_factor < 6 || spreading_factor > 12) {
        return LoRaError::InvalidConfig;
    }
    if (spreading_factor == 6 && !implicit_header) {
        return LoRaError::InvalidConfig;
    }

    // Coding rate
    if (coding_rate < 5 || coding_rate > 8)
        return LoRaError::InvalidConfig;

    // Preamble
    if (preamble_length < 6)
        return LoRaError::InvalidConfig;

    // OCP
    if (ocp_ma < 45 || ocp_ma > 240)
        return LoRaError::InvalidConfig;

    // TX power vs PA output
    if (pa_output == PaOutput::Rfo) {
        if (tx_power_dbm < 0 || tx_power_dbm > 14)
            return LoRaError::InvalidConfig;
    } else {
        if (tx_power_dbm < 2 || tx_power_dbm > 20)
            return LoRaError::InvalidConfig;
    }

    // Pins
    if (pin_ss < 0 || pin_reset < 0 || pin_dio0 < 0)
        return LoRaError::InvalidConfig;

    // spi_pins: either all -1 (use board default SPI pins) or all set.
    {
        const int set_count = (spi_pins.sck >= 0 ? 1 : 0) + (spi_pins.miso >= 0 ? 1 : 0) +
                              (spi_pins.mosi >= 0 ? 1 : 0);
        if (set_count != 0 && set_count != 3) {
            return LoRaError::InvalidConfig;
        }
    }

    return LoRaError::OK;
}

bool LoRaConfig::ldro_required() const noexcept {
    // Symbol duration ms = (2^SF / BW_hz) * 1000.
    // LDRO recommended when symbol duration > 16 ms (Semtech AN1200.24).
    const std::uint32_t bw = bandwidth_hz == 0u ? 1u : bandwidth_hz;
    const std::uint64_t sym_us = (1ull << spreading_factor) * 1'000'000ull / bw;
    return sym_us > 16'000ull;
}

} // namespace loradriver
