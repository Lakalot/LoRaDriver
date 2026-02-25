#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

#include "loradriver/lora_error.hpp"
#include "loradriver/radio_config.hpp"
#include "loradriver/radio_event.hpp"

namespace loradriver {

class LoRaDriver {
 public:
  using RadioEventCallback = std::function<void(RadioEvent, int)>;

  struct DiagnosticContext {
    LoRaError error = LoRaError::kOk;
    int detail_code = 0;
    RadioConfig::Chip chip = RadioConfig::Chip{};
    RadioConfig::Band band = RadioConfig::Band{};
    RadioConfig::DioRouting dio_routing = RadioConfig::DioRouting{};
  };

  [[nodiscard]] LoRaError begin(const RadioConfig& config) noexcept;
  [[nodiscard]] LoRaError initialize(const RadioConfig& config) noexcept;
  [[nodiscard]] LoRaError setEventCallback(RadioEventCallback callback) noexcept;
  [[nodiscard]] LoRaError shutdown() noexcept;
  [[nodiscard]] LoRaError send(const std::uint8_t* payload, std::size_t size) noexcept;
  [[nodiscard]] LoRaError startReceive() noexcept;
  [[nodiscard]] LoRaError sleep() noexcept;
  [[nodiscard]] LoRaError standby() noexcept;

  [[nodiscard]] bool isInitialized() const noexcept;
  [[nodiscard]] const RadioConfig& currentConfig() const noexcept;
  [[nodiscard]] LoRaError lastError() const noexcept;
  [[nodiscard]] int lastDiagnosticCode() const noexcept;
  [[nodiscard]] DiagnosticContext lastDiagnosticContext() const noexcept;

 private:
  enum class DriverState {
    kIdle = 0,
    kValidating,
    kBindingAdapters,
    kHardwareBringUp,
    kReady,
    kTxPreparing,
    kTxInProgress,
    kTxCompleted,
    kTxFailed,
    kListening,
    kRxInProgress,
  };

  [[nodiscard]] bool transitionTo(DriverState next) noexcept;
  [[nodiscard]] bool isTxEntryState(DriverState state) const noexcept;
  [[nodiscard]] bool isRxEntryState(DriverState state) const noexcept;
  [[nodiscard]] bool emitEvent(RadioEvent event, int detail_code) noexcept;
  [[nodiscard]] LoRaError fail(LoRaError error, int detail_code, const RadioConfig& context) noexcept;

  RadioConfig config_{};
  RadioEventCallback callback_{};
  bool initialized_ = false;
  DriverState state_ = DriverState::kIdle;
  LoRaError last_error_ = LoRaError::kOk;
  int last_diagnostic_code_ = 0;
  DiagnosticContext last_diagnostic_context_{};
};

}  // namespace loradriver
