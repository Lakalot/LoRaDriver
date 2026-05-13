#pragma once

#include <cstdint>
#include <type_traits>

namespace loradriver {

/// @brief Per-packet metadata delivered alongside the payload to on_receive.
struct LoRaPacket {
    std::int16_t rssi_dbm = 0;             ///< RSSI in dBm (negative).
    std::int16_t snr_q4 = 0;               ///< SNR in quarter-dB units (signed).
    std::int32_t frequency_error_hz = 0;   ///< Frequency offset of TX vs RX (Hz).
    std::uint8_t length = 0;               ///< Payload length in bytes.
    bool crc_valid = false;                ///< true if the packet CRC matched.

    /// @brief Return SNR as floating-point dB (snr_q4 / 4.0).
    [[nodiscard]] float snr_db() const noexcept { return static_cast<float>(snr_q4) / 4.0f; }
};

static_assert(sizeof(LoRaPacket) <= 16, "LoRaPacket must remain cheap to copy");
static_assert(std::is_trivially_copyable<LoRaPacket>::value, "LoRaPacket trivially copyable");

} // namespace loradriver
