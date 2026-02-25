#pragma once

namespace loradriver {

enum class LoRaError {
  kOk = 0,
  kInvalidConfig,
  kUnsupportedProfile,
  kHardwareInitFailure,
  kTransitionGuardFailure,
  kAlreadyInitialized,
  kNotInitialized,
  kNotImplemented
};

}  // namespace loradriver
