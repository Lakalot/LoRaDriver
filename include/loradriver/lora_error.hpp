#pragma once

#include <cstdint>

namespace loradriver {

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

const char* to_string(LoRaError e) noexcept;

} // namespace loradriver
