#pragma once

#include <functional>

#include "loradriver/lora_error.hpp"
#include "loradriver/radio_config.hpp"
#include "loradriver/radio_event.hpp"

namespace loradriver {

class LoRaDriver {
 public:
  using RadioEventCallback = std::function<void(RadioEvent, int)>;

  [[nodiscard]] LoRaError initialize(const RadioConfig& config) noexcept;
  [[nodiscard]] LoRaError setEventCallback(RadioEventCallback callback) noexcept;
  [[nodiscard]] LoRaError shutdown() noexcept;

  [[nodiscard]] bool isInitialized() const noexcept;
  [[nodiscard]] const RadioConfig& currentConfig() const noexcept;

 private:
  RadioConfig config_{};
  RadioEventCallback callback_{};
  bool initialized_ = false;
};

}  // namespace loradriver
