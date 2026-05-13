#pragma once

#include <cstdint>
#include <type_traits>

namespace loradriver {

struct LoRaPacket {
    std::int16_t rssi_dbm = 0;
    std::int16_t snr_q4 = 0;
    std::int32_t frequency_error_hz = 0;
    std::uint8_t length = 0;
    bool crc_valid = false;

    [[nodiscard]] float snr_db() const noexcept { return static_cast<float>(snr_q4) / 4.0f; }
};

static_assert(sizeof(LoRaPacket) <= 16, "LoRaPacket must remain cheap to copy");
static_assert(std::is_trivially_copyable<LoRaPacket>::value, "LoRaPacket trivially copyable");

} // namespace loradriver
