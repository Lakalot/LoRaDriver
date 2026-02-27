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
  kNotImplemented,
  kMissingRecoveryEvidence,
  kNonRegressionFailed,
  kGateFailed,
  kWaiverExpired,
  kUnknownGateId
};

}  // namespace loradriver
