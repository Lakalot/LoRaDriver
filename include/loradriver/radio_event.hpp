#pragma once

#include <cstdint>

namespace loradriver {

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
