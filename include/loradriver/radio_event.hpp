#pragma once

namespace loradriver {

enum class RadioEvent {
  kNone = 0,
  kInitialized,
  kTxDone,
  kRxDone,
  kTimeout,
  kError
};

}  // namespace loradriver
