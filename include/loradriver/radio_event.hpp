#pragma once

namespace loradriver {

enum class RadioEvent {
  kNone = 0,
  kInitPhaseStart,
  kInitValidate,
  kConfigValidated,
  kChipDetected,
  kInitBindAdapters,
  kInitHardwareBringUp,
  kInitialized,
  kTxPreparing,
  kTxInProgress,
  kTxCompleted,
  kTxFailed,
  kRxListening,
  kRxInProgress,
  kRxDone,
  kSleep,
  kStandby,
  kTimeout,
  kRecoveryCompleted,
  kError
};

}  // namespace loradriver
