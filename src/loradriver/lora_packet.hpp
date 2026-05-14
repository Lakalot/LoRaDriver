#pragma once

#include <cstdint>
#include <type_traits>

namespace loradriver {

/// @brief Per-packet metadata delivered alongside the payload to on_receive.
struct LoRaPacket {
    /// @brief RSSI in dBm (negative).
    std::int16_t rssi_dbm = 0;
    /// @brief SNR in quarter-dB units (signed).
    std::int16_t snr_q4 = 0;
    /// @brief Frequency offset of TX vs RX (Hz).
    std::int32_t frequency_error_hz = 0;
    /// @brief Payload length in bytes.
    std::uint8_t length = 0;
    /// @brief true if the packet CRC matched.
    bool crc_valid = false;

    /// @brief Return SNR as floating-point dB (snr_q4 / 4.0).
    [[nodiscard]] float snr_db() const noexcept { return static_cast<float>(snr_q4) / 4.0f; }
};

static_assert(sizeof(LoRaPacket) <= 16, "LoRaPacket must remain cheap to copy");
static_assert(std::is_trivially_copyable<LoRaPacket>::value, "LoRaPacket trivially copyable");

} // namespace loradriver
