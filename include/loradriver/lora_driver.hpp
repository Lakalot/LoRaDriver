#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

#include "loradriver/incident_snapshot.hpp"
#include "loradriver/lora_error.hpp"
#include "loradriver/radio_config.hpp"
#include "loradriver/radio_counters.hpp"
#include "loradriver/radio_event.hpp"
#include "loradriver/version.hpp"

namespace loradriver {

struct OtaTelemetryInput;

class LoRaDriver {
 public:
  using RadioEventCallback = std::function<void(RadioEvent, int)>;

  /// Injectable timestamp source. Returns current time in milliseconds.
  /// If not set, timestamp_ms defaults to 0 in diagnostics.
  using TimestampSource = std::function<std::uint32_t()>;

  struct DiagnosticContext {
    std::uint8_t version_major = LORADRIVER_VERSION_MAJOR;
    std::uint8_t version_minor = LORADRIVER_VERSION_MINOR;
    std::uint8_t version_patch = LORADRIVER_VERSION_PATCH;
    LoRaError error = LoRaError::kOk;
    int detail_code = 0;
    RadioConfig::Chip chip = RadioConfig::Chip{};
    RadioConfig::Band band = RadioConfig::Band{};
    RadioConfig::DioRouting dio_routing = RadioConfig::DioRouting{};
    std::uint32_t sequence = 0;
    /// Timestamp in milliseconds. Populated from TimestampSource if set,
    /// otherwise 0. Host firmware should inject a platform-specific source.
    std::uint32_t timestamp_ms = 0;
    std::uint32_t spi_frequency_hz = 8000000;
    // Active LoRa modulation parameters
    std::uint8_t spreading_factor = 9;
    std::uint32_t bandwidth_khz = 125;
    std::uint8_t coding_rate_denominator = 5;
    std::uint8_t sync_word = 0x12;
    std::int8_t tx_power_dbm = 14;
    bool crc_enabled = true;
    std::uint16_t preamble_length = 8;
  };

  [[nodiscard]] LoRaError begin(const RadioConfig& config) noexcept;
  [[nodiscard]] LoRaError initialize(const RadioConfig& config) noexcept;
  [[nodiscard]] LoRaError setEventCallback(RadioEventCallback callback) noexcept;
  [[nodiscard]] LoRaError setTimestampSource(TimestampSource source) noexcept;
  [[nodiscard]] LoRaError shutdown() noexcept;
  [[nodiscard]] LoRaError send(const std::uint8_t* payload, std::size_t size) noexcept;
  [[nodiscard]] LoRaError startReceive() noexcept;
  [[nodiscard]] LoRaError sleep() noexcept;
  [[nodiscard]] LoRaError standby() noexcept;
  [[nodiscard]] LoRaError recoverFromTimeout() noexcept;

  [[nodiscard]] bool isInitialized() const noexcept;
  [[nodiscard]] const RadioConfig& currentConfig() const noexcept;
  [[nodiscard]] LoRaError lastError() const noexcept;
  [[nodiscard]] int lastDiagnosticCode() const noexcept;
  [[nodiscard]] DiagnosticContext lastDiagnosticContext() const noexcept;
  [[nodiscard]] IncidentSnapshot captureIncidentSnapshot() const noexcept;
  [[nodiscard]] std::uint32_t currentSequence() const noexcept;
  [[nodiscard]] RadioCounters getCounters() const noexcept;
  [[nodiscard]] OtaTelemetryInput getOtaTelemetryInput(const char* firmware_version) const noexcept;
  [[nodiscard]] LoRaError handleIrqOverflow() noexcept;

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
    kTimeoutRecovering,
  };

  [[nodiscard]] bool transitionTo(DriverState next) noexcept;
  [[nodiscard]] bool isTxEntryState(DriverState state) const noexcept;
  [[nodiscard]] bool isRxEntryState(DriverState state) const noexcept;
  [[nodiscard]] bool emitEvent(RadioEvent event, int detail_code) noexcept;
  [[nodiscard]] LoRaError fail(LoRaError error, int detail_code, const RadioConfig& context) noexcept;
  [[nodiscard]] LoRaError initFail(LoRaError error, int detail_code, const RadioConfig& context) noexcept;
  void advanceSequence() noexcept;
  [[nodiscard]] std::uint32_t currentTimestamp() const noexcept;
  void updateDiagnosticContext(LoRaError error, int detail_code) noexcept;

  RadioConfig config_{};
  RadioEventCallback callback_{};
  TimestampSource timestamp_source_{};
  bool initialized_ = false;
  DriverState state_ = DriverState::kIdle;
  LoRaError last_error_ = LoRaError::kOk;
  int last_diagnostic_code_ = 0;
  DiagnosticContext last_diagnostic_context_{};
  std::uint32_t sequence_ = 0;
  RadioCounters counters_{};
};

}  // namespace loradriver
