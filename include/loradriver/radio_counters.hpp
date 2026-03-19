#pragma once

#include <cstdint>
#include <type_traits>

namespace loradriver {

/// Cumulative runtime radio event counters.
///
/// Counts start at zero for a new LoRaDriver instance and accumulate over the
/// entire driver lifetime.  All fields are uint32_t; wrap-around on overflow is
/// accepted for V1 (documented behaviour, not an error).
///
/// Obtained via LoRaDriver::getCounters(). Reads are non-blocking and
/// allocation-free. The returned struct is a value snapshot – modifying it
/// does not affect the driver's internal state.
struct RadioCounters {
  std::uint32_t init_attempts      = 0;  ///< Times begin() was called
  std::uint32_t init_failures      = 0;  ///< Times begin() returned a non-kOk error (excl. kAlreadyInitialized)
  std::uint32_t tx_success         = 0;  ///< Completed TX operations (kOk)
  std::uint32_t tx_fail            = 0;  ///< Rejected or failed TX operations
  std::uint32_t rx_success         = 0;  ///< Completed RX operations (kOk)
  std::uint32_t rx_fail            = 0;  ///< Rejected or failed RX operations
  std::uint32_t timeout_events     = 0;  ///< Times recoverFromTimeout() entered the recovery path
  std::uint32_t irq_overflow_events = 0; ///< Times handleIrqOverflow() was called successfully
};

static_assert(std::is_trivially_copyable<RadioCounters>::value,
              "RadioCounters must be trivially copyable (allocation-free reads)");

}  // namespace loradriver
