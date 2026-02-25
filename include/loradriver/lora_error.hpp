#pragma once

namespace loradriver {

enum class LoRaError {
  kOk = 0,
  kInvalidConfig,
  kUnsupportedProfile,
  kHardwareInitFailure,
  kTransitionGuardFailure,
  kTimeoutRecovered,
  kTimeoutRecoveryFailure,
  kAlreadyInitialized,
  kNotInitialized,
  kNotImplemented
};

}  // namespace loradriver
