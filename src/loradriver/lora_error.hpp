#pragma once

#include <cstdint>

namespace loradriver {

/// @brief Result code returned by every fallible driver/transceiver method.
///
/// Cast to int gives the underlying value; use to_string() for log output.
enum class LoRaError : std::uint8_t {
    OK = 0,
    InvalidConfig,
    UnsupportedChip,
    SpiFailure,
    SpiVerifyMismatch,
    InvalidState,
    TxTimeout,
    TxBufferTooLarge,
    RxTimeout,
    RxCrcError,
    AlreadyInitialized,
    NotInitialized,
    QueueFull,
    NullArgument,
};

/// @brief Human-readable name of an error code. Stable across versions.
const char* to_string(LoRaError e) noexcept;

} // namespace loradriver
