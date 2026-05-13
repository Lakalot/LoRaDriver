#include "loradriver/lora_error.hpp"

namespace loradriver {

const char* to_string(LoRaError e) noexcept {
    switch (e) {
        case LoRaError::OK:                  return "OK";
        case LoRaError::InvalidConfig:       return "InvalidConfig";
        case LoRaError::UnsupportedChip:     return "UnsupportedChip";
        case LoRaError::SpiFailure:          return "SpiFailure";
        case LoRaError::SpiVerifyMismatch:   return "SpiVerifyMismatch";
        case LoRaError::InvalidState:        return "InvalidState";
        case LoRaError::TxTimeout:           return "TxTimeout";
        case LoRaError::TxBufferTooLarge:    return "TxBufferTooLarge";
        case LoRaError::RxTimeout:           return "RxTimeout";
        case LoRaError::RxCrcError:          return "RxCrcError";
        case LoRaError::AlreadyInitialized:  return "AlreadyInitialized";
        case LoRaError::NotInitialized:      return "NotInitialized";
        case LoRaError::QueueFull:           return "QueueFull";
        case LoRaError::NullArgument:        return "NullArgument";
    }
    return "Unknown";
}

}  // namespace loradriver
