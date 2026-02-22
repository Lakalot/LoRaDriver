#include "loradriver/lora_driver.hpp"

namespace loradriver {

LoRaError LoRaDriver::initialize(const RadioConfig& config) noexcept {
  if (!config.isV1Supported()) {
    return LoRaError::kUnsupportedProfile;
  }

  config_ = config;
  initialized_ = true;
  if (callback_) {
    try {
      callback_(RadioEvent::kInitialized, 0);
    } catch (...) {
      initialized_ = false;
      return LoRaError::kInvalidArgument;
    }
  }
  return LoRaError::kOk;
}

LoRaError LoRaDriver::setEventCallback(RadioEventCallback callback) noexcept {
  callback_ = static_cast<RadioEventCallback&&>(callback);
  return LoRaError::kOk;
}

LoRaError LoRaDriver::shutdown() noexcept {
  if (!initialized_) {
    return LoRaError::kNotInitialized;
  }
  initialized_ = false;
  return LoRaError::kOk;
}

bool LoRaDriver::isInitialized() const noexcept {
  return initialized_;
}

const RadioConfig& LoRaDriver::currentConfig() const noexcept {
  return config_;
}

}  // namespace loradriver
