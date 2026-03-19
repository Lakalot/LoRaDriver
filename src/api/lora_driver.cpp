#include "loradriver/lora_driver.hpp"

#include <cstring>
#include <utility>

#include "config_validation.hpp"
#include "loradriver/ota_gate.hpp"

namespace loradriver {

namespace {

constexpr int kDiagPhaseStart = 1000;
constexpr int kDiagPhaseValidate = 1100;
constexpr int kDiagPhaseBindAdapters = 1200;
constexpr int kDiagPhaseHardwareBringUp = 1300;
constexpr int kDiagConfigValidated = 1500;
constexpr int kDiagChipDetected = 1600;
constexpr int kDiagTransitionGuard = 1400;
constexpr int kDiagInvalidSpiFrequency = 2001;
constexpr int kDiagReasonUnsupportedChip = 21;
constexpr int kDiagReasonUnsupportedBand = 22;
constexpr int kDiagReasonUnsupportedIrqRouting = 23;
constexpr int kDiagShutdownNotInitialized = 3001;

constexpr int kDiagTxPreparing = 3100;
constexpr int kDiagTxInProgress = 3200;
constexpr int kDiagTxFailedInvalidPayload = 3301;
constexpr int kDiagTxNotInitialized = 3401;
constexpr int kDiagTxIllegalState = 3402;

constexpr int kDiagRxListening = 4100;
constexpr int kDiagRxInProgress = 4200;
constexpr int kDiagRxNotInitialized = 4401;
constexpr int kDiagRxIllegalState = 4402;
constexpr int kDiagSleepNotInitialized = 5101;
constexpr int kDiagSleepIllegalState = 5102;
constexpr int kDiagSleepTransition = 5103;
constexpr int kDiagStandbyNotInitialized = 5201;
constexpr int kDiagStandbyIllegalState = 5202;
constexpr int kDiagStandbyTransition = 5203;
constexpr int kDiagTimeoutDetected = 6100;
constexpr int kDiagTimeoutRecoveryCompleted = 6200;
constexpr int kDiagTimeoutNotInitialized = 6301;
constexpr int kDiagTimeoutIllegalState = 6401;
constexpr int kDiagTimeoutTransition = 6402;

constexpr int kDiagIrqOverflow = 7100;

constexpr std::size_t kMaxPayloadBytes = 255;

float ComputePercentRate(std::uint32_t numerator, std::uint32_t denominator) noexcept {
  const std::uint32_t safe_denominator = (denominator == 0U) ? 1U : denominator;
  return (static_cast<float>(numerator) * 100.0f) / static_cast<float>(safe_denominator);
}

const char* ToRadioFamily(RadioConfig::Chip chip) noexcept {
  switch (chip) {
    case RadioConfig::Chip::kSx1276:
      return "SX1276";
    case RadioConfig::Chip::kSx1278:
      return "SX1278";
    case RadioConfig::Chip::kSx126xStub:
      return "SX126X";
    default:
      return "UNKNOWN";
  }
}

const char* ToBandString(RadioConfig::Band band) noexcept {
  switch (band) {
    case RadioConfig::Band::k433:
      return "433";
    case RadioConfig::Band::k868:
      return "868";
    default:
      return "UNK";
  }
}

int EncodeProfileDiagnostic(const RadioConfig& config, int reason) noexcept {
  const int chip = static_cast<int>(config.chip);
  const int band = static_cast<int>(config.band);
  const int dio = static_cast<int>(config.dio_routing);
  return (reason * 100) + (chip * 10) + (band * 3) + dio;
}

bool IsSupportedChip(RadioConfig::Chip chip) noexcept {
  return chip == RadioConfig::Chip::kSx1276 || chip == RadioConfig::Chip::kSx1278;
}

bool IsSupportedBand(RadioConfig::Band band) noexcept {
  return band == RadioConfig::Band::k433 || band == RadioConfig::Band::k868;
}

bool IsSupportedDioRouting(RadioConfig::DioRouting routing) noexcept {
  return routing == RadioConfig::DioRouting::kDio0Only || routing == RadioConfig::DioRouting::kDio0Dio1;
}

}  // namespace

LoRaError LoRaDriver::begin(const RadioConfig& config) noexcept {
  ++counters_.init_attempts;

  updateDiagnosticContext(LoRaError::kOk, 0);
  last_diagnostic_context_.chip = config.chip;
  last_diagnostic_context_.band = config.band;
  last_diagnostic_context_.dio_routing = config.dio_routing;
  last_diagnostic_context_.spi_frequency_hz = config.spi_frequency_hz;
  last_diagnostic_context_.spreading_factor = config.spreading_factor;
  last_diagnostic_context_.bandwidth_khz = config.bandwidth_khz;
  last_diagnostic_context_.coding_rate_denominator = config.coding_rate_denominator;
  last_diagnostic_context_.sync_word = config.sync_word;
  last_diagnostic_context_.tx_power_dbm = config.tx_power_dbm;
  last_diagnostic_context_.crc_enabled = config.crc_enabled;
  last_diagnostic_context_.preamble_length = config.preamble_length;

  if (initialized_) {
    updateDiagnosticContext(LoRaError::kAlreadyInitialized, kDiagTransitionGuard);
    last_diagnostic_context_.chip = config_.chip;
    last_diagnostic_context_.band = config_.band;
    last_diagnostic_context_.dio_routing = config_.dio_routing;
    (void)emitEvent(RadioEvent::kError, kDiagTransitionGuard);
    return LoRaError::kAlreadyInitialized;
  }

  state_ = DriverState::kIdle;
  advanceSequence();
  if (!emitEvent(RadioEvent::kInitPhaseStart, kDiagPhaseStart)) {
    return initFail(LoRaError::kTransitionGuardFailure, kDiagTransitionGuard + 9, config);
  }

  if (!transitionTo(DriverState::kValidating)) {
    return initFail(LoRaError::kTransitionGuardFailure, kDiagTransitionGuard + 10, config);
  }
  if (!emitEvent(RadioEvent::kInitValidate, kDiagPhaseValidate)) {
    return initFail(LoRaError::kTransitionGuardFailure, kDiagTransitionGuard + 1, config);
  }

  if (!IsSupportedChip(config.chip)) {
    return initFail(LoRaError::kUnsupportedProfile, EncodeProfileDiagnostic(config, kDiagReasonUnsupportedChip), config);
  }

  advanceSequence();
  if (!emitEvent(RadioEvent::kChipDetected, static_cast<int>(config.chip))) {
    return initFail(LoRaError::kTransitionGuardFailure, kDiagTransitionGuard + 14, config);
  }

  if (!IsSupportedBand(config.band)) {
    return initFail(LoRaError::kUnsupportedProfile, EncodeProfileDiagnostic(config, kDiagReasonUnsupportedBand), config);
  }

  if (!IsSupportedDioRouting(config.dio_routing)) {
    return initFail(LoRaError::kUnsupportedProfile, EncodeProfileDiagnostic(config, kDiagReasonUnsupportedIrqRouting), config);
  }

  if (!config.isSpiFrequencyInRange()) {
    return initFail(LoRaError::kInvalidConfig, kDiagInvalidSpiFrequency, config);
  }

  const int lora_param_error = ValidateLoRaParams(config);
  if (lora_param_error != 0) {
    return initFail(LoRaError::kInvalidConfig, lora_param_error, config);
  }

  advanceSequence();
  if (!emitEvent(RadioEvent::kConfigValidated, kDiagConfigValidated)) {
    return initFail(LoRaError::kTransitionGuardFailure, kDiagTransitionGuard + 15, config);
  }

  if (!transitionTo(DriverState::kBindingAdapters)) {
    return initFail(LoRaError::kTransitionGuardFailure, kDiagTransitionGuard + 11, config);
  }
  if (!emitEvent(RadioEvent::kInitBindAdapters, kDiagPhaseBindAdapters)) {
    return initFail(LoRaError::kTransitionGuardFailure, kDiagTransitionGuard + 2, config);
  }

  if (!transitionTo(DriverState::kHardwareBringUp)) {
    return initFail(LoRaError::kTransitionGuardFailure, kDiagTransitionGuard + 12, config);
  }
  if (!emitEvent(RadioEvent::kInitHardwareBringUp, kDiagPhaseHardwareBringUp)) {
    return initFail(LoRaError::kHardwareInitFailure, kDiagPhaseHardwareBringUp + 1, config);
  }

  config_ = config;
  initialized_ = true;
  if (!transitionTo(DriverState::kReady)) {
    return initFail(LoRaError::kTransitionGuardFailure, kDiagTransitionGuard + 13, config);
  }

  if (!emitEvent(RadioEvent::kInitialized, 0)) {
    return initFail(LoRaError::kTransitionGuardFailure, kDiagTransitionGuard + 4, config);
  }

  updateDiagnosticContext(LoRaError::kOk, 0);
  return LoRaError::kOk;
}

LoRaError LoRaDriver::initialize(const RadioConfig& config) noexcept {
  return begin(config);
}

LoRaError LoRaDriver::setEventCallback(RadioEventCallback callback) noexcept {
  callback_ = std::move(callback);
  return LoRaError::kOk;
}

LoRaError LoRaDriver::setTimestampSource(TimestampSource source) noexcept {
  timestamp_source_ = std::move(source);
  return LoRaError::kOk;
}

LoRaError LoRaDriver::shutdown() noexcept {
  if (!initialized_) {
    updateDiagnosticContext(LoRaError::kNotInitialized, kDiagShutdownNotInitialized);
    return LoRaError::kNotInitialized;
  }

  initialized_ = false;
  (void)transitionTo(DriverState::kIdle);
  updateDiagnosticContext(LoRaError::kOk, 0);

  return LoRaError::kOk;
}

LoRaError LoRaDriver::send(const std::uint8_t* payload, std::size_t size) noexcept {
  if (!initialized_) {
    ++counters_.tx_fail;
    return fail(LoRaError::kNotInitialized, kDiagTxNotInitialized, config_);
  }

  if (!isTxEntryState(state_)) {
    ++counters_.tx_fail;
    return fail(LoRaError::kTransitionGuardFailure, kDiagTxIllegalState, config_);
  }

  advanceSequence();

  if (!transitionTo(DriverState::kTxPreparing)) {
    ++counters_.tx_fail;
    return fail(LoRaError::kTransitionGuardFailure, kDiagTxIllegalState + 1, config_);
  }
  if (!emitEvent(RadioEvent::kTxPreparing, kDiagTxPreparing)) {
    ++counters_.tx_fail;
    return fail(LoRaError::kTransitionGuardFailure, kDiagTxIllegalState + 2, config_);
  }

  if (payload == nullptr || size == 0 || size > kMaxPayloadBytes) {
    if (!transitionTo(DriverState::kTxFailed)) {
      ++counters_.tx_fail;
      return fail(LoRaError::kTransitionGuardFailure, kDiagTxIllegalState + 3, config_);
    }
    if (!emitEvent(RadioEvent::kTxFailed, kDiagTxFailedInvalidPayload)) {
      ++counters_.tx_fail;
      return fail(LoRaError::kTransitionGuardFailure, kDiagTxIllegalState + 4, config_);
    }
    if (!transitionTo(DriverState::kReady)) {
      ++counters_.tx_fail;
      return fail(LoRaError::kTransitionGuardFailure, kDiagTxIllegalState + 5, config_);
    }

    ++counters_.tx_fail;
    updateDiagnosticContext(LoRaError::kInvalidConfig, kDiagTxFailedInvalidPayload);
    return LoRaError::kInvalidConfig;
  }

  if (!transitionTo(DriverState::kTxInProgress)) {
    ++counters_.tx_fail;
    return fail(LoRaError::kTransitionGuardFailure, kDiagTxIllegalState + 6, config_);
  }
  if (!emitEvent(RadioEvent::kTxInProgress, kDiagTxInProgress)) {
    ++counters_.tx_fail;
    return fail(LoRaError::kTransitionGuardFailure, kDiagTxIllegalState + 7, config_);
  }

  if (!transitionTo(DriverState::kTxCompleted)) {
    ++counters_.tx_fail;
    return fail(LoRaError::kTransitionGuardFailure, kDiagTxIllegalState + 8, config_);
  }
  const int tx_detail = (config_.dio_routing == RadioConfig::DioRouting::kDio0Only) ? 1 : 0;
  if (!emitEvent(RadioEvent::kTxCompleted, tx_detail)) {
    ++counters_.tx_fail;
    return fail(LoRaError::kTransitionGuardFailure, kDiagTxIllegalState + 9, config_);
  }

  if (!transitionTo(DriverState::kReady)) {
    ++counters_.tx_fail;
    return fail(LoRaError::kTransitionGuardFailure, kDiagTxIllegalState + 10, config_);
  }

  ++counters_.tx_success;
  updateDiagnosticContext(LoRaError::kOk, 0);
  return LoRaError::kOk;
}

LoRaError LoRaDriver::startReceive() noexcept {
  if (!initialized_) {
    ++counters_.rx_fail;
    return fail(LoRaError::kNotInitialized, kDiagRxNotInitialized, config_);
  }

  if (!isRxEntryState(state_)) {
    ++counters_.rx_fail;
    return fail(LoRaError::kTransitionGuardFailure, kDiagRxIllegalState, config_);
  }

  advanceSequence();

  if (!transitionTo(DriverState::kListening)) {
    ++counters_.rx_fail;
    return fail(LoRaError::kTransitionGuardFailure, kDiagRxIllegalState + 1, config_);
  }
  if (!emitEvent(RadioEvent::kRxListening, kDiagRxListening)) {
    ++counters_.rx_fail;
    return fail(LoRaError::kTransitionGuardFailure, kDiagRxIllegalState + 2, config_);
  }

  if (!transitionTo(DriverState::kRxInProgress)) {
    ++counters_.rx_fail;
    return fail(LoRaError::kTransitionGuardFailure, kDiagRxIllegalState + 3, config_);
  }
  if (!emitEvent(RadioEvent::kRxInProgress, kDiagRxInProgress)) {
    ++counters_.rx_fail;
    return fail(LoRaError::kTransitionGuardFailure, kDiagRxIllegalState + 4, config_);
  }

  const int rx_detail = (config_.dio_routing == RadioConfig::DioRouting::kDio0Only) ? 1 : 0;
  if (!emitEvent(RadioEvent::kRxDone, rx_detail)) {
    ++counters_.rx_fail;
    return fail(LoRaError::kTransitionGuardFailure, kDiagRxIllegalState + 5, config_);
  }

  if (!transitionTo(DriverState::kListening)) {
    ++counters_.rx_fail;
    return fail(LoRaError::kTransitionGuardFailure, kDiagRxIllegalState + 6, config_);
  }

  ++counters_.rx_success;
  updateDiagnosticContext(LoRaError::kOk, 0);
  return LoRaError::kOk;
}

LoRaError LoRaDriver::sleep() noexcept {
  if (!initialized_) {
    return fail(LoRaError::kNotInitialized, kDiagSleepNotInitialized, config_);
  }

  if (state_ != DriverState::kReady && state_ != DriverState::kListening) {
    return fail(LoRaError::kTransitionGuardFailure, kDiagSleepIllegalState, config_);
  }

  if (!transitionTo(DriverState::kIdle)) {
    return fail(LoRaError::kTransitionGuardFailure, kDiagSleepTransition, config_);
  }

  if (!emitEvent(RadioEvent::kSleep, 0)) {
    return fail(LoRaError::kTransitionGuardFailure, kDiagSleepTransition + 1, config_);
  }

  updateDiagnosticContext(LoRaError::kOk, 0);
  return LoRaError::kOk;
}

LoRaError LoRaDriver::standby() noexcept {
  if (!initialized_) {
    return fail(LoRaError::kNotInitialized, kDiagStandbyNotInitialized, config_);
  }

  if (state_ != DriverState::kIdle && state_ != DriverState::kListening && state_ != DriverState::kReady) {
    return fail(LoRaError::kTransitionGuardFailure, kDiagStandbyIllegalState, config_);
  }

  if (state_ != DriverState::kReady && !transitionTo(DriverState::kReady)) {
    return fail(LoRaError::kTransitionGuardFailure, kDiagStandbyTransition, config_);
  }

  if (!emitEvent(RadioEvent::kStandby, 0)) {
    return fail(LoRaError::kTransitionGuardFailure, kDiagStandbyTransition + 1, config_);
  }

  updateDiagnosticContext(LoRaError::kOk, 0);
  return LoRaError::kOk;
}

LoRaError LoRaDriver::recoverFromTimeout() noexcept {
  if (!initialized_) {
    return fail(LoRaError::kNotInitialized, kDiagTimeoutNotInitialized, config_);
  }

  if (state_ != DriverState::kTxInProgress && state_ != DriverState::kRxInProgress &&
      state_ != DriverState::kListening) {
    return fail(LoRaError::kTransitionGuardFailure, kDiagTimeoutIllegalState, config_);
  }

  ++counters_.timeout_events;
  advanceSequence();

  if (!transitionTo(DriverState::kTimeoutRecovering)) {
    return fail(LoRaError::kTimeoutRecoveryFailure, kDiagTimeoutTransition, config_);
  }

  if (!emitEvent(RadioEvent::kTimeout, kDiagTimeoutDetected)) {
    return fail(LoRaError::kTimeoutRecoveryFailure, kDiagTimeoutTransition + 1, config_);
  }

  if (!transitionTo(DriverState::kReady)) {
    return fail(LoRaError::kTimeoutRecoveryFailure, kDiagTimeoutTransition + 2, config_);
  }

  const int recovery_detail =
      (config_.dio_routing == RadioConfig::DioRouting::kDio0Only) ? 1 : 0;
  if (!emitEvent(RadioEvent::kRecoveryCompleted, kDiagTimeoutRecoveryCompleted + recovery_detail)) {
    return fail(LoRaError::kTimeoutRecoveryFailure, kDiagTimeoutTransition + 3, config_);
  }

  updateDiagnosticContext(LoRaError::kTimeoutRecovered, 0);
  return LoRaError::kTimeoutRecovered;
}

bool LoRaDriver::isInitialized() const noexcept {
  return initialized_;
}

const RadioConfig& LoRaDriver::currentConfig() const noexcept {
  return config_;
}

LoRaError LoRaDriver::lastError() const noexcept {
  return last_error_;
}

int LoRaDriver::lastDiagnosticCode() const noexcept {
  return last_diagnostic_code_;
}

LoRaDriver::DiagnosticContext LoRaDriver::lastDiagnosticContext() const noexcept {
  return last_diagnostic_context_;
}

bool LoRaDriver::transitionTo(DriverState next) noexcept {
  const DriverState current = state_;

  bool allowed = false;
  switch (current) {
    case DriverState::kIdle:
      allowed = (next == DriverState::kValidating || next == DriverState::kReady);
      break;
    case DriverState::kValidating:
      allowed = (next == DriverState::kBindingAdapters || next == DriverState::kIdle);
      break;
    case DriverState::kBindingAdapters:
      allowed = (next == DriverState::kHardwareBringUp || next == DriverState::kIdle);
      break;
    case DriverState::kHardwareBringUp:
      allowed = (next == DriverState::kReady || next == DriverState::kIdle);
      break;
    case DriverState::kReady:
      allowed = (next == DriverState::kTxPreparing || next == DriverState::kListening ||
                 next == DriverState::kIdle);
      break;
    case DriverState::kTxPreparing:
      allowed = (next == DriverState::kTxInProgress || next == DriverState::kTxFailed || next == DriverState::kIdle);
      break;
    case DriverState::kTxInProgress:
      allowed = (next == DriverState::kTxCompleted || next == DriverState::kTxFailed ||
                 next == DriverState::kTimeoutRecovering || next == DriverState::kIdle);
      break;
    case DriverState::kTxCompleted:
      allowed = (next == DriverState::kReady || next == DriverState::kIdle);
      break;
    case DriverState::kTxFailed:
      allowed = (next == DriverState::kReady || next == DriverState::kIdle);
      break;
    case DriverState::kListening:
      allowed = (next == DriverState::kTxPreparing || next == DriverState::kRxInProgress ||
                 next == DriverState::kTimeoutRecovering || next == DriverState::kIdle ||
                 next == DriverState::kReady);
      break;
    case DriverState::kRxInProgress:
      allowed = (next == DriverState::kListening || next == DriverState::kTimeoutRecovering ||
                 next == DriverState::kIdle);
      break;
    case DriverState::kTimeoutRecovering:
      allowed = (next == DriverState::kReady || next == DriverState::kIdle);
      break;
  }

  if (!allowed) {
    return false;
  }

  state_ = next;
  return true;
}

bool LoRaDriver::isTxEntryState(DriverState state) const noexcept {
  return state == DriverState::kReady || state == DriverState::kListening;
}

bool LoRaDriver::isRxEntryState(DriverState state) const noexcept {
  return state == DriverState::kReady || state == DriverState::kListening;
}

// NOTE: emitEvent() and fail() use try/catch as a DEFENSIVE SAFETY NET against
// user-provided callbacks that throw. The library itself does not throw exceptions
// in runtime paths (per project policy). This catch is intentional to prevent
// undefined behavior if a callback violates the no-throw contract documented in
// contracts.md. It is NOT a supported usage pattern for callbacks to throw.
bool LoRaDriver::emitEvent(RadioEvent event, int detail_code) noexcept {
  if (!callback_) {
    return true;
  }

  try {
    callback_(event, detail_code);
    return true;
  } catch (...) {
    return false;
  }
}

LoRaError LoRaDriver::fail(LoRaError error, int detail_code, const RadioConfig& context) noexcept {
  initialized_ = false;
  state_ = DriverState::kIdle;
  updateDiagnosticContext(error, detail_code);
  last_diagnostic_context_.chip = context.chip;
  last_diagnostic_context_.band = context.band;
  last_diagnostic_context_.dio_routing = context.dio_routing;
  last_diagnostic_context_.spi_frequency_hz = context.spi_frequency_hz;
  last_diagnostic_context_.spreading_factor = context.spreading_factor;
  last_diagnostic_context_.bandwidth_khz = context.bandwidth_khz;
  last_diagnostic_context_.coding_rate_denominator = context.coding_rate_denominator;
  last_diagnostic_context_.sync_word = context.sync_word;
  last_diagnostic_context_.tx_power_dbm = context.tx_power_dbm;
  last_diagnostic_context_.crc_enabled = context.crc_enabled;
  last_diagnostic_context_.preamble_length = context.preamble_length;

  if (callback_) {
    try {
      callback_(RadioEvent::kError, detail_code);
    } catch (...) {
    }
  }

  return error;
}

IncidentSnapshot LoRaDriver::captureIncidentSnapshot() const noexcept {
  IncidentSnapshot snapshot{};
  snapshot.version_major = LORADRIVER_VERSION_MAJOR;
  snapshot.version_minor = LORADRIVER_VERSION_MINOR;
  snapshot.version_patch = LORADRIVER_VERSION_PATCH;
  snapshot.error = last_error_;
  snapshot.detail_code = last_diagnostic_code_;
  snapshot.chip = last_diagnostic_context_.chip;
  snapshot.band = last_diagnostic_context_.band;
  snapshot.dio_routing = last_diagnostic_context_.dio_routing;
  snapshot.sequence = last_diagnostic_context_.sequence;
  snapshot.timestamp_ms = currentTimestamp();
  snapshot.spi_frequency_hz = last_diagnostic_context_.spi_frequency_hz;
  snapshot.spreading_factor = last_diagnostic_context_.spreading_factor;
  snapshot.bandwidth_khz = last_diagnostic_context_.bandwidth_khz;
  snapshot.coding_rate_denominator = last_diagnostic_context_.coding_rate_denominator;
  snapshot.sync_word = last_diagnostic_context_.sync_word;
  snapshot.tx_power_dbm = last_diagnostic_context_.tx_power_dbm;
  snapshot.crc_enabled = last_diagnostic_context_.crc_enabled;
  snapshot.preamble_length = last_diagnostic_context_.preamble_length;
  return snapshot;
}

std::uint32_t LoRaDriver::currentSequence() const noexcept {
  return sequence_;
}

void LoRaDriver::advanceSequence() noexcept {
  ++sequence_;
}

std::uint32_t LoRaDriver::currentTimestamp() const noexcept {
  if (timestamp_source_) {
    try {
      return timestamp_source_();
    } catch (...) {
      return 0;
    }
  }
  return 0;
}

void LoRaDriver::updateDiagnosticContext(LoRaError error, int detail_code) noexcept {
  last_error_ = error;
  last_diagnostic_code_ = detail_code;
  last_diagnostic_context_.version_major = LORADRIVER_VERSION_MAJOR;
  last_diagnostic_context_.version_minor = LORADRIVER_VERSION_MINOR;
  last_diagnostic_context_.version_patch = LORADRIVER_VERSION_PATCH;
  last_diagnostic_context_.error = error;
  last_diagnostic_context_.detail_code = detail_code;
  last_diagnostic_context_.chip = config_.chip;
  last_diagnostic_context_.band = config_.band;
  last_diagnostic_context_.dio_routing = config_.dio_routing;
  last_diagnostic_context_.sequence = sequence_;
  last_diagnostic_context_.timestamp_ms = currentTimestamp();
  last_diagnostic_context_.spi_frequency_hz = config_.spi_frequency_hz;
  last_diagnostic_context_.spreading_factor = config_.spreading_factor;
  last_diagnostic_context_.bandwidth_khz = config_.bandwidth_khz;
  last_diagnostic_context_.coding_rate_denominator = config_.coding_rate_denominator;
  last_diagnostic_context_.sync_word = config_.sync_word;
  last_diagnostic_context_.tx_power_dbm = config_.tx_power_dbm;
  last_diagnostic_context_.crc_enabled = config_.crc_enabled;
  last_diagnostic_context_.preamble_length = config_.preamble_length;
}

RadioCounters LoRaDriver::getCounters() const noexcept {
  return counters_;
}

OtaTelemetryInput LoRaDriver::getOtaTelemetryInput(const char* firmware_version) const noexcept {
  OtaTelemetryInput telemetry{};

  if (firmware_version != nullptr) {
    std::strncpy(telemetry.firmware_version, firmware_version,
                 OtaTelemetryInput::kMaxVersionLength - 1U);
  }
  std::strncpy(telemetry.radio_family, ToRadioFamily(config_.chip),
               OtaTelemetryInput::kMaxFamilyLength - 1U);
  std::strncpy(telemetry.active_band, ToBandString(config_.band),
               OtaTelemetryInput::kMaxBandLength - 1U);

  telemetry.init_failure_rate =
      ComputePercentRate(counters_.init_failures, counters_.init_attempts);
  telemetry.timeout_events = counters_.timeout_events;
  telemetry.irq_overflow_events = counters_.irq_overflow_events;
  telemetry.tx_success_rate =
      ComputePercentRate(counters_.tx_success, counters_.tx_success + counters_.tx_fail);
  telemetry.rx_success_rate =
      ComputePercentRate(counters_.rx_success, counters_.rx_success + counters_.rx_fail);
  telemetry.sample_timestamp_utc = currentTimestamp();
  telemetry.baseline_tx_success_rate = 0.0f;
  telemetry.baseline_rx_success_rate = 0.0f;
  telemetry.baseline_init_failure_rate = 0.0f;

  return telemetry;
}

LoRaError LoRaDriver::handleIrqOverflow() noexcept {
  if (!initialized_) {
    updateDiagnosticContext(LoRaError::kNotInitialized, kDiagIrqOverflow + 1);
    return LoRaError::kNotInitialized;
  }

  ++counters_.irq_overflow_events;
  if (!emitEvent(RadioEvent::kIrqOverflow, kDiagIrqOverflow)) {
    updateDiagnosticContext(LoRaError::kTransitionGuardFailure, kDiagIrqOverflow + 2);
    return LoRaError::kTransitionGuardFailure;
  }
  return LoRaError::kOk;
}

LoRaError LoRaDriver::initFail(LoRaError error, int detail_code, const RadioConfig& context) noexcept {
  ++counters_.init_failures;
  return fail(error, detail_code, context);
}

}  // namespace loradriver
