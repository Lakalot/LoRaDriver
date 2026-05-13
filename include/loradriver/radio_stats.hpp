#pragma once

#include <cstdint>
#include <type_traits>

namespace loradriver {

struct RadioStats {
    std::uint32_t tx_done = 0;
    std::uint32_t tx_timeout = 0;
    std::uint32_t rx_done = 0;
    std::uint32_t rx_timeout = 0;
    std::uint32_t rx_crc_errors = 0;
    std::uint32_t irq_events_processed = 0;
    std::uint32_t irq_overflows = 0;
    std::uint32_t callback_exceptions = 0;
    std::uint8_t max_irq_backlog = 0;
    std::int16_t last_rssi_dbm = 0;
    std::int16_t last_snr_q4 = 0;
    std::int32_t last_freq_error_hz = 0;
};

static_assert(std::is_trivially_copyable<RadioStats>::value,
              "RadioStats must be trivially copyable for snapshot reads");

} // namespace loradriver
