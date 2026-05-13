#pragma once

#include <cstdint>

namespace loradriver {

/// @brief Low-level radio events emitted from process_events().
enum class RadioEvent : std::uint8_t {
    None = 0,
    TxDone,
    TxTimeout,
    RxDone,
    RxTimeout,
    RxCrcError,
    CadDone,
    CadDetected,
    ValidHeader,
    IrqOverflow,
};

} // namespace loradriver
