#pragma once

namespace loradriver {

enum class LoRaError {
  kOk = 0,
  kInvalidArgument,
  kUnsupportedProfile,
  kNotInitialized,
  kNotImplemented
};

}  // namespace loradriver
