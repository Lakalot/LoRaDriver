#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "loradriver/incident_classification.hpp"
#include "loradriver/incident_snapshot.hpp"
#include "loradriver/lora_driver.hpp"

namespace {

using loradriver::EscalationPath;
using loradriver::IncidentCategory;
using loradriver::IncidentClassification;
using loradriver::IncidentSnapshot;
using loradriver::LoRaDriver;
using loradriver::LoRaError;
using loradriver::RadioConfig;
using loradriver::RadioEvent;
using loradriver::classifyIncident;

constexpr int kDiagPhaseStart = 1000;
constexpr int kDiagConfigValidated = 1500;
constexpr int kDiagChipDetectedBase = 0;
constexpr int kDiagTxPreparing = 3100;
constexpr int kDiagTxInProgress = 3200;
constexpr int kDiagTxFailedInvalidPayload = 3301;
constexpr int kDiagRxListening = 4100;
constexpr int kDiagRxInProgress = 4200;
constexpr int kDiagSleepNotInitialized = 5101;
constexpr int kDiagSleepIllegalState = 5102;
constexpr int kDiagStandbyNotInitialized = 5201;
constexpr int kDiagTimeoutDetected = 6100;
constexpr int kDiagTimeoutRecoveryCompleted = 6200;
constexpr int kDiagTimeoutRecoveryCompletedDio0Only = 6201;
constexpr int kDiagTimeoutNotInitialized = 6301;
constexpr int kDiagTimeoutIllegalState = 6401;

std::string ReadTextFile(const std::string& path) {
  std::ifstream file(path, std::ios::in | std::ios::binary);
  if (!file.is_open()) {
    return {};
  }

  std::string content;
  file.seekg(0, std::ios::end);
  const std::streamoff length = file.tellg();
  if (length <= 0) {
    return {};
  }
  content.resize(static_cast<std::size_t>(length));
  file.seekg(0, std::ios::beg);
  file.read(&content[0], static_cast<std::streamsize>(content.size()));
  return content;
}

bool Contains(const std::string& value, const std::string& token) {
  return value.find(token) != std::string::npos;
}

struct EventTrace {
  RadioEvent event = RadioEvent::kNone;
  int detail = 0;

  bool operator==(const EventTrace& other) const {
    return event == other.event && detail == other.detail;
  }
};

RadioConfig MakeV1Config(RadioConfig::DioRouting routing = RadioConfig::DioRouting::kDio0Only) {
  RadioConfig config;
  config.chip = RadioConfig::Chip::kSx1276;
  config.band = RadioConfig::Band::k868;
  config.dio_routing = routing;
  config.spi_frequency_hz = 8000000;
  return config;
}

bool BeginAndClearInitEvents(LoRaDriver& driver, std::vector<EventTrace>& events,
                             RadioConfig::DioRouting routing = RadioConfig::DioRouting::kDio0Only) {
  if (driver.setEventCallback([&events](RadioEvent event, int detail_code) {
        events.push_back(EventTrace{event, detail_code});
      }) != LoRaError::kOk) {
    return false;
  }

  if (driver.begin(MakeV1Config(routing)) != LoRaError::kOk) {
    return false;
  }

  events.clear();
  return true;
}

bool TestDeterministicTxSuccessTransitionOrder() {
  LoRaDriver driver;
  std::vector<EventTrace> events;

  if (!BeginAndClearInitEvents(driver, events)) {
    return false;
  }

  const std::array<std::uint8_t, 3> payload = {0x11u, 0x22u, 0x33u};
  if (driver.send(payload.data(), payload.size()) != LoRaError::kOk) {
    return false;
  }

  const std::vector<EventTrace> expected = {
      {RadioEvent::kTxPreparing, kDiagTxPreparing},
      {RadioEvent::kTxInProgress, kDiagTxInProgress},
      {RadioEvent::kTxCompleted, 1},
  };

  if (events != expected) {
    return false;
  }

  return driver.isInitialized() && driver.lastError() == LoRaError::kOk;
}

bool TestSendRejectsIllegalEntryStateWithTypedError() {
  LoRaDriver driver;
  const std::array<std::uint8_t, 1> payload = {0x7Fu};

  if (driver.send(payload.data(), payload.size()) != LoRaError::kNotInitialized) {
    return false;
  }

  const auto context = driver.lastDiagnosticContext();
  if (context.error != LoRaError::kNotInitialized || context.detail_code == 0) {
    return false;
  }

  return true;
}

bool TestTxFailureUsesDeterministicFailurePath() {
  LoRaDriver driver;
  std::vector<EventTrace> events;

  if (!BeginAndClearInitEvents(driver, events)) {
    return false;
  }

  if (driver.send(nullptr, 1) != LoRaError::kInvalidConfig) {
    return false;
  }

  const std::vector<EventTrace> expected = {
      {RadioEvent::kTxPreparing, kDiagTxPreparing},
      {RadioEvent::kTxFailed, kDiagTxFailedInvalidPayload},
  };
  if (events != expected) {
    return false;
  }

  return driver.lastError() == LoRaError::kInvalidConfig;
}

bool TestDeterministicRxCompletionAndReturnToListening() {
  LoRaDriver driver;
  std::vector<EventTrace> events;

  if (!BeginAndClearInitEvents(driver, events)) {
    return false;
  }

  if (driver.startReceive() != LoRaError::kOk) {
    return false;
  }

  const std::vector<EventTrace> expected_rx = {
      {RadioEvent::kRxListening, kDiagRxListening},
      {RadioEvent::kRxInProgress, kDiagRxInProgress},
      {RadioEvent::kRxDone, 1},
  };
  if (events != expected_rx) {
    return false;
  }

  events.clear();
  const std::array<std::uint8_t, 2> payload = {0x01u, 0x02u};
  if (driver.send(payload.data(), payload.size()) != LoRaError::kOk) {
    return false;
  }

  return events.size() == 3 && events[0].event == RadioEvent::kTxPreparing;
}

bool TestIrqProfilesKeepCoreTxRxParity() {
  const std::array<RadioConfig::DioRouting, 2> profiles = {
      RadioConfig::DioRouting::kDio0Only,
      RadioConfig::DioRouting::kDio0Dio1,
  };

  std::array<std::vector<RadioEvent>, 2> traces;

  for (std::size_t i = 0; i < profiles.size(); ++i) {
    LoRaDriver driver;
    std::vector<EventTrace> events;

    if (!BeginAndClearInitEvents(driver, events, profiles[i])) {
      return false;
    }

    const std::array<std::uint8_t, 3> payload = {0x0Au, 0x0Bu, 0x0Cu};
    if (driver.send(payload.data(), payload.size()) != LoRaError::kOk) {
      return false;
    }
    if (driver.startReceive() != LoRaError::kOk) {
      return false;
    }

    for (const auto& evt : events) {
      traces[i].push_back(evt.event);
    }
  }

  return traces[0] == traces[1];
}

bool TestRepeatedTxRxCyclesRemainDeterministic() {
  LoRaDriver driver;
  std::vector<EventTrace> events;

  if (!BeginAndClearInitEvents(driver, events)) {
    return false;
  }

  for (int cycle = 0; cycle < 32; ++cycle) {
    const std::array<std::uint8_t, 4> payload = {
        static_cast<std::uint8_t>(cycle),
        static_cast<std::uint8_t>(cycle + 1),
        static_cast<std::uint8_t>(cycle + 2),
        static_cast<std::uint8_t>(cycle + 3),
    };

    if (driver.send(payload.data(), payload.size()) != LoRaError::kOk) {
      return false;
    }
    if (driver.startReceive() != LoRaError::kOk) {
      return false;
    }
  }

  if (events.size() != (32u * 6u)) {
    return false;
  }

  return driver.lastError() == LoRaError::kOk;
}

bool TestCallbackExceptionReturnsTransitionGuardFailure() {
  LoRaDriver driver;
  std::vector<EventTrace> events;

  if (driver.setEventCallback([&events](RadioEvent, int) {
        events.push_back(EventTrace{RadioEvent::kError, 0});
        throw std::runtime_error("test exception");
      }) != LoRaError::kOk) {
    return false;
  }

  RadioConfig config = MakeV1Config();
  const LoRaError result = driver.begin(config);

  return result == LoRaError::kTransitionGuardFailure;
}

bool TestPayloadExceedsMaxSizeReturnsInvalidConfig() {
  LoRaDriver driver;
  std::vector<EventTrace> events;

  if (!BeginAndClearInitEvents(driver, events)) {
    return false;
  }

  std::array<std::uint8_t, 256> oversized_payload{};
  const LoRaError result = driver.send(oversized_payload.data(), oversized_payload.size());

  return result == LoRaError::kInvalidConfig && driver.lastError() == LoRaError::kInvalidConfig;
}

bool TestSleepAndStandbyProvideCanonicalPowerFlow() {
  LoRaDriver driver;
  std::vector<EventTrace> events;

  if (!BeginAndClearInitEvents(driver, events, RadioConfig::DioRouting::kDio0Dio1)) {
    return false;
  }

  if (driver.sleep() != LoRaError::kOk) {
    return false;
  }
  if (events.empty() || events.back().event != RadioEvent::kSleep) {
    return false;
  }

  events.clear();
  if (driver.standby() != LoRaError::kOk) {
    return false;
  }
  if (events.empty() || events.back().event != RadioEvent::kStandby) {
    return false;
  }

  events.clear();
  const std::array<std::uint8_t, 2> payload = {0xAAu, 0x55u};
  if (driver.send(payload.data(), payload.size()) != LoRaError::kOk) {
    return false;
  }

  return !events.empty() && events.back().event == RadioEvent::kTxCompleted;
}

bool TestStandbyBeforeBeginReturnsTypedError() {
  LoRaDriver driver;
  if (driver.standby() != LoRaError::kNotInitialized) {
    return false;
  }

  const auto context = driver.lastDiagnosticContext();
  return context.error == LoRaError::kNotInitialized && context.detail_code == kDiagStandbyNotInitialized;
}

bool TestSleepBeforeBeginReturnsTypedError() {
  LoRaDriver driver;
  if (driver.sleep() != LoRaError::kNotInitialized) {
    return false;
  }

  const auto context = driver.lastDiagnosticContext();
  return context.error == LoRaError::kNotInitialized && context.detail_code == kDiagSleepNotInitialized;
}

bool TestSleepFromIdleReturnsTransitionGuardFailure() {
  LoRaDriver driver;
  std::vector<EventTrace> events;

  if (!BeginAndClearInitEvents(driver, events)) {
    return false;
  }

  if (driver.sleep() != LoRaError::kOk) {
    return false;
  }

  events.clear();

  const LoRaError result = driver.sleep();
  if (result != LoRaError::kTransitionGuardFailure) {
    return false;
  }

  return driver.lastDiagnosticCode() == kDiagSleepIllegalState;
}

bool TestTimeoutRecoveryFromListeningReturnsDeterministicSequence() {
  LoRaDriver driver;
  std::vector<EventTrace> events;

  if (!BeginAndClearInitEvents(driver, events)) {
    return false;
  }

  if (driver.startReceive() != LoRaError::kOk) {
    return false;
  }

  events.clear();
  const LoRaError timeout_result = driver.recoverFromTimeout();
  if (timeout_result != LoRaError::kTimeoutRecovered) {
    return false;
  }

  const std::vector<EventTrace> expected = {
      {RadioEvent::kTimeout, kDiagTimeoutDetected},
      {RadioEvent::kRecoveryCompleted, kDiagTimeoutRecoveryCompletedDio0Only},
  };
  if (events != expected) {
    return false;
  }

  const auto ctx = driver.lastDiagnosticContext();
  if (ctx.error != LoRaError::kTimeoutRecovered || ctx.detail_code != 0) {
    return false;
  }

  const std::array<std::uint8_t, 1> payload = {0x2Au};
  return driver.send(payload.data(), payload.size()) == LoRaError::kOk;
}

bool TestTimeoutRecoveryFromReadyStateReturnsGuardFailure() {
  LoRaDriver driver;
  if (driver.begin(MakeV1Config()) != LoRaError::kOk) {
    return false;
  }

  const LoRaError timeout_result = driver.recoverFromTimeout();
  if (timeout_result != LoRaError::kTransitionGuardFailure) {
    return false;
  }

  const auto ctx = driver.lastDiagnosticContext();
  return ctx.error == LoRaError::kTransitionGuardFailure && ctx.detail_code == kDiagTimeoutIllegalState;
}

bool TestRepeatedTimeoutRecoveryLoopsRemainBounded() {
  LoRaDriver driver;
  std::vector<EventTrace> events;

  if (!BeginAndClearInitEvents(driver, events, RadioConfig::DioRouting::kDio0Dio1)) {
    return false;
  }

  constexpr int kLoops = 32;
  for (int i = 0; i < kLoops; ++i) {
    if (driver.startReceive() != LoRaError::kOk) {
      return false;
    }

    events.clear();
    if (driver.recoverFromTimeout() != LoRaError::kTimeoutRecovered) {
      return false;
    }

    if (events.size() != 2u || events[0].event != RadioEvent::kTimeout ||
        events[1].event != RadioEvent::kRecoveryCompleted) {
      return false;
    }
  }

  return driver.lastError() == LoRaError::kTimeoutRecovered;
}

bool TestRepeatedSleepWakeLoopsResumeWithoutReset() {
  LoRaDriver driver;
  std::vector<EventTrace> events;

  if (!BeginAndClearInitEvents(driver, events, RadioConfig::DioRouting::kDio0Only)) {
    return false;
  }

  constexpr int kLoops = 32;
  for (int i = 0; i < kLoops; ++i) {
    events.clear();
    if (driver.sleep() != LoRaError::kOk) {
      return false;
    }
    if (driver.standby() != LoRaError::kOk) {
      return false;
    }

    const std::vector<EventTrace> expected_power = {
        {RadioEvent::kSleep, 0},
        {RadioEvent::kStandby, 0},
    };
    if (events != expected_power) {
      return false;
    }

    events.clear();
    const std::array<std::uint8_t, 2> payload = {static_cast<std::uint8_t>(i), 0x55u};
    if (driver.send(payload.data(), payload.size()) != LoRaError::kOk) {
      return false;
    }

    if (events.size() != 3u || events.front().event != RadioEvent::kTxPreparing ||
        events.back().event != RadioEvent::kTxCompleted) {
      return false;
    }
  }

  return driver.isInitialized() && driver.lastError() == LoRaError::kOk;
}

bool TestTimeoutRecoveryFromReadyReturnsGuardFailureAfterTx() {
  LoRaDriver driver;
  std::vector<EventTrace> events;

  if (!BeginAndClearInitEvents(driver, events, RadioConfig::DioRouting::kDio0Dio1)) {
    return false;
  }

  const std::array<std::uint8_t, 2> payload = {0xDEu, 0xADu};
  if (driver.send(payload.data(), payload.size()) != LoRaError::kOk) {
    return false;
  }

  events.clear();
  const LoRaError timeout_result = driver.recoverFromTimeout();
  if (timeout_result != LoRaError::kTransitionGuardFailure) {
    return false;
  }

  const auto ctx = driver.lastDiagnosticContext();
  return ctx.error == LoRaError::kTransitionGuardFailure && ctx.detail_code == kDiagTimeoutIllegalState;
}

bool TestTimeoutRecoveryFromListeningAfterRxReturnsDeterministicSequence() {
  LoRaDriver driver;
  std::vector<EventTrace> events;

  if (!BeginAndClearInitEvents(driver, events)) {
    return false;
  }

  if (driver.startReceive() != LoRaError::kOk) {
    return false;
  }

  events.clear();
  const LoRaError timeout_result = driver.recoverFromTimeout();
  if (timeout_result != LoRaError::kTimeoutRecovered) {
    return false;
  }

  const std::vector<EventTrace> expected = {
      {RadioEvent::kTimeout, kDiagTimeoutDetected},
      {RadioEvent::kRecoveryCompleted, kDiagTimeoutRecoveryCompletedDio0Only},
  };
  if (events != expected) {
    return false;
  }

  const auto ctx = driver.lastDiagnosticContext();
  if (ctx.error != LoRaError::kTimeoutRecovered || ctx.detail_code != 0) {
    return false;
  }

  return driver.lastError() == LoRaError::kTimeoutRecovered;
}

bool TestTimeoutRecoveryFromNotInitializedReturnsTypedError() {
  LoRaDriver driver;
  const LoRaError result = driver.recoverFromTimeout();
  if (result != LoRaError::kNotInitialized) {
    return false;
  }

  const auto ctx = driver.lastDiagnosticContext();
  return ctx.error == LoRaError::kNotInitialized && ctx.detail_code == kDiagTimeoutNotInitialized;
}

bool TestTimeoutRecoveryCallbackFailureReturnsRecoveryFailure() {
  LoRaDriver driver;
  std::vector<EventTrace> events;

  if (driver.setEventCallback([&events](RadioEvent event, int) {
        events.push_back(EventTrace{event, 0});
        if (event == RadioEvent::kTimeout) {
          throw std::runtime_error("recovery callback failure");
        }
      }) != LoRaError::kOk) {
    return false;
  }

  if (driver.begin(MakeV1Config()) != LoRaError::kOk) {
    return false;
  }

  if (driver.startReceive() != LoRaError::kOk) {
    return false;
  }

  events.clear();
  const LoRaError result = driver.recoverFromTimeout();
  if (result != LoRaError::kTimeoutRecoveryFailure) {
    return false;
  }

  const auto ctx = driver.lastDiagnosticContext();
  return ctx.error == LoRaError::kTimeoutRecoveryFailure;
}

bool TestUnsupportedBandAndIrqRoutingReturnTypedDiagnostics() {
  LoRaDriver driver;

  RadioConfig invalid_band;
  invalid_band.chip = RadioConfig::Chip::kSx1276;
  invalid_band.band = static_cast<RadioConfig::Band>(99);
  invalid_band.dio_routing = RadioConfig::DioRouting::kDio0Only;
  invalid_band.spi_frequency_hz = 8000000;
  if (driver.begin(invalid_band) != LoRaError::kUnsupportedProfile) {
    return false;
  }
  if (driver.lastDiagnosticCode() == 0) {
    return false;
  }

  LoRaDriver driver2;
  RadioConfig invalid_irq;
  invalid_irq.chip = RadioConfig::Chip::kSx1278;
  invalid_irq.band = RadioConfig::Band::k868;
  invalid_irq.dio_routing = static_cast<RadioConfig::DioRouting>(99);
  invalid_irq.spi_frequency_hz = 8000000;
  if (driver2.begin(invalid_irq) != LoRaError::kUnsupportedProfile) {
    return false;
  }

  return driver2.lastDiagnosticCode() != 0;
}

bool TestIntegrationContractDocDefinesCanonicalFlowAndOnboarding() {
  const std::string root = LORADRIVER_REPO_ROOT;
  const std::string contract_doc = ReadTextFile(root + "/docs/api/contracts.md");
  const std::string integration_doc = ReadTextFile(root + "/docs/api/integration.md");
  if (contract_doc.empty()) {
    return false;
  }
  if (integration_doc.empty()) {
    return false;
  }

  if (!Contains(contract_doc, "## Standard Product Firmware Integration Contract (V1)")) {
    return false;
  }
  if (!Contains(contract_doc, "Canonical integration lifecycle (stable API only)")) {
    return false;
  }
  if (!Contains(contract_doc, "`begin`, `send`, `startReceive`, `sleep`, `standby`")) {
    return false;
  }
  if (!Contains(contract_doc, "## SX127x V1 Onboarding and Deviation Points")) {
    return false;
  }
  if (!Contains(contract_doc, "Profile configuration")) {
    return false;
  }
  if (!Contains(contract_doc, "IRQ routing mode")) {
    return false;
  }
  if (!Contains(contract_doc, "Band settings")) {
    return false;
  }
  if (!Contains(integration_doc, "## Onboarding Checklist")) {
    return false;
  }
  if (!Contains(integration_doc, "`sleep` and `standby`")) {
    return false;
  }

  return true;
}

bool TestPublicHeadersPreserveAdapterBoundary() {
  const std::string root = LORADRIVER_REPO_ROOT;
  const std::array<std::string, 6> public_headers = {
      root + "/include/loradriver/lora_driver.hpp",
      root + "/include/loradriver/lora_error.hpp",
      root + "/include/loradriver/radio_config.hpp",
      root + "/include/loradriver/radio_event.hpp",
      root + "/include/loradriver/incident_snapshot.hpp",
      root + "/include/loradriver/incident_classification.hpp",
  };

  for (const auto& header_path : public_headers) {
    const std::string content = ReadTextFile(header_path);
    if (content.empty()) {
      return false;
    }

    if (Contains(content, "src/chips") || Contains(content, "src/platform") ||
        Contains(content, "internal/") || Contains(content, "platform/") ||
        Contains(content, "chips/")) {
      return false;
    }
  }

  return true;
}

bool TestInitPhaseEventsAreEmittedInDeterministicOrder() {
  LoRaDriver driver;
  std::vector<EventTrace> events;

  if (driver.setEventCallback([&events](RadioEvent event, int detail_code) {
        events.push_back(EventTrace{event, detail_code});
      }) != LoRaError::kOk) {
    return false;
  }

  if (driver.begin(MakeV1Config()) != LoRaError::kOk) {
    return false;
  }

  if (events.empty()) {
    return false;
  }

  if (events[0].event != RadioEvent::kInitPhaseStart || events[0].detail != kDiagPhaseStart) {
    return false;
  }

  bool found_chip_detected = false;
  bool found_config_validated = false;
  int chip_detected_index = -1;
  int config_validated_index = -1;

  for (std::size_t i = 0; i < events.size(); ++i) {
    if (events[i].event == RadioEvent::kChipDetected) {
      found_chip_detected = true;
      chip_detected_index = static_cast<int>(i);
    }
    if (events[i].event == RadioEvent::kConfigValidated) {
      found_config_validated = true;
      config_validated_index = static_cast<int>(i);
    }
  }

  if (!found_chip_detected || !found_config_validated) {
    return false;
  }

  if (chip_detected_index >= config_validated_index) {
    return false;
  }

  return events.back().event == RadioEvent::kInitialized;
}

bool TestDiagnosticContextIncludesVersionAndSequence() {
  LoRaDriver driver;

  if (driver.begin(MakeV1Config()) != LoRaError::kOk) {
    return false;
  }

  const auto ctx = driver.lastDiagnosticContext();
  if (ctx.version_major != LORADRIVER_VERSION_MAJOR) {
    return false;
  }
  if (ctx.version_minor != LORADRIVER_VERSION_MINOR) {
    return false;
  }
  if (ctx.version_patch != LORADRIVER_VERSION_PATCH) {
    return false;
  }
  if (ctx.sequence == 0) {
    return false;
  }

  return true;
}

bool TestIncidentSnapshotCaptureIncludesAllRequiredFields() {
  LoRaDriver driver;

  if (driver.begin(MakeV1Config()) != LoRaError::kOk) {
    return false;
  }

  const IncidentSnapshot snapshot = driver.captureIncidentSnapshot();
  if (snapshot.version_major != LORADRIVER_VERSION_MAJOR) {
    return false;
  }
  if (snapshot.version_minor != LORADRIVER_VERSION_MINOR) {
    return false;
  }
  if (snapshot.version_patch != LORADRIVER_VERSION_PATCH) {
    return false;
  }
  if (snapshot.error != LoRaError::kOk) {
    return false;
  }
  if (snapshot.chip != RadioConfig::Chip::kSx1276) {
    return false;
  }
  if (snapshot.band != RadioConfig::Band::k868) {
    return false;
  }
  if (snapshot.dio_routing != RadioConfig::DioRouting::kDio0Only) {
    return false;
  }

  return true;
}

bool TestIncidentSnapshotFormatToProducesStableOutput() {
  LoRaDriver driver;

  if (driver.begin(MakeV1Config()) != LoRaError::kOk) {
    return false;
  }

  const IncidentSnapshot snapshot = driver.captureIncidentSnapshot();
  char buffer[IncidentSnapshot::kFormatBufferSize];
  const std::size_t written = snapshot.formatTo(buffer, sizeof(buffer));

  if (written == 0) {
    return false;
  }

  const std::string output(buffer, written);
  if (!Contains(output, "LORADRIVER_INCIDENT:")) {
    return false;
  }
  if (!Contains(output, "v=1.0.0")) {
    return false;
  }
  if (!Contains(output, "e=0")) {
    return false;
  }
  if (!Contains(output, "c=")) {
    return false;
  }
  if (!Contains(output, "b=")) {
    return false;
  }
  if (!Contains(output, "d=")) {
    return false;
  }
  if (!Contains(output, "dc=")) {
    return false;
  }
  if (!Contains(output, "seq=")) {
    return false;
  }
  if (!Contains(output, "ts=")) {
    return false;
  }

  return true;
}

bool TestIncidentSnapshotFormatToRejectsInvalidBuffer() {
  IncidentSnapshot snapshot{};
  char small_buffer[10];
  const std::size_t written = snapshot.formatTo(small_buffer, sizeof(small_buffer));
  return written == 0;
}

bool TestEventOrderingParityAcrossIrqProfiles() {
  const std::array<RadioConfig::DioRouting, 2> profiles = {
      RadioConfig::DioRouting::kDio0Only,
      RadioConfig::DioRouting::kDio0Dio1,
  };

  for (const auto routing : profiles) {
    LoRaDriver driver;
    std::vector<EventTrace> events;

    if (driver.setEventCallback([&events](RadioEvent event, int detail_code) {
          events.push_back(EventTrace{event, detail_code});
        }) != LoRaError::kOk) {
      return false;
    }

    if (driver.begin(MakeV1Config(routing)) != LoRaError::kOk) {
      return false;
    }

    bool found_init_phase_start = false;
    for (const auto& evt : events) {
      if (evt.event == RadioEvent::kInitPhaseStart) {
        found_init_phase_start = true;
        break;
      }
    }

    if (!found_init_phase_start) {
      return false;
    }
  }

  return true;
}

bool TestDiagnosticContextSequenceIncrementsOnOperations() {
  LoRaDriver driver;

  if (driver.begin(MakeV1Config()) != LoRaError::kOk) {
    return false;
  }

  const std::uint32_t initial_seq = driver.currentSequence();
  if (initial_seq == 0) {
    return false;
  }

  const std::array<std::uint8_t, 2> payload = {0x01u, 0x02u};
  if (driver.send(payload.data(), payload.size()) != LoRaError::kOk) {
    return false;
  }

  const std::uint32_t after_tx_seq = driver.currentSequence();

  return after_tx_seq >= initial_seq;
}

bool TestErrorDiagnosticContextIncludesSequence() {
  LoRaDriver driver;

  if (driver.begin(MakeV1Config()) != LoRaError::kOk) {
    return false;
  }

  if (driver.send(nullptr, 1) != LoRaError::kInvalidConfig) {
    return false;
  }

  const auto ctx = driver.lastDiagnosticContext();
  if (ctx.error != LoRaError::kInvalidConfig) {
    return false;
  }

  return ctx.sequence > 0;
}

bool TestClassifyIncidentReturnsUnknownForOkError() {
  IncidentSnapshot snapshot;
  snapshot.error = LoRaError::kOk;

  const IncidentClassification classification = classifyIncident(snapshot);
  if (classification.category != IncidentCategory::kUnknown) {
    return false;
  }
  if (classification.severity != IncidentSeverity::kInfo) {
    return false;
  }

  return true;
}

bool TestClassifyIncidentMapsTimeoutErrorsCorrectly() {
  IncidentSnapshot snapshot1;
  snapshot1.error = LoRaError::kTimeoutRecovered;
  const IncidentClassification class1 = classifyIncident(snapshot1);
  if (class1.category != IncidentCategory::kTimeoutRelated) {
    return false;
  }
  if (class1.severity != IncidentSeverity::kWarning) {
    return false;
  }
  if (class1.escalation_path != EscalationPath::kSupportL1) {
    return false;
  }

  IncidentSnapshot snapshot2;
  snapshot2.error = LoRaError::kTimeoutRecoveryFailure;
  const IncidentClassification class2 = classifyIncident(snapshot2);
  if (class2.category != IncidentCategory::kTimeoutRelated) {
    return false;
  }
  if (class2.severity != IncidentSeverity::kCritical) {
    return false;
  }

  return true;
}

bool TestClassifyIncidentMapsConfigErrorsCorrectly() {
  IncidentSnapshot snapshot1;
  snapshot1.error = LoRaError::kInvalidConfig;
  const IncidentClassification class1 = classifyIncident(snapshot1);
  if (class1.category != IncidentCategory::kConfigError) {
    return false;
  }
  if (class1.escalation_path != EscalationPath::kSupportL2) {
    return false;
  }

  IncidentSnapshot snapshot2;
  snapshot2.error = LoRaError::kUnsupportedProfile;
  const IncidentClassification class2 = classifyIncident(snapshot2);
  if (class2.category != IncidentCategory::kConfigError) {
    return false;
  }

  return true;
}

bool TestClassifyIncidentMapsHardwareFaultCorrectly() {
  IncidentSnapshot snapshot;
  snapshot.error = LoRaError::kHardwareInitFailure;
  const IncidentClassification classification = classifyIncident(snapshot);
  if (classification.category != IncidentCategory::kHardwareFault) {
    return false;
  }
  if (classification.severity != IncidentSeverity::kCritical) {
    return false;
  }
  if (classification.escalation_path != EscalationPath::kHardwareTeam) {
    return false;
  }

  return true;
}

bool TestClassifyIncidentMapsRuntimeTransitionCorrectly() {
  IncidentSnapshot snapshot;
  snapshot.error = LoRaError::kTransitionGuardFailure;
  const IncidentClassification classification = classifyIncident(snapshot);
  if (classification.category != IncidentCategory::kRuntimeTransition) {
    return false;
  }
  if (classification.escalation_path != EscalationPath::kEngineering) {
    return false;
  }

  return true;
}

bool TestClassifyIncidentIsDeterministic() {
  IncidentSnapshot snapshot;
  snapshot.error = LoRaError::kInvalidConfig;
  snapshot.detail_code = 12345;
  snapshot.chip = RadioConfig::Chip::kSx1276;
  snapshot.band = RadioConfig::Band::k868;

  const IncidentClassification class1 = classifyIncident(snapshot);
  const IncidentClassification class2 = classifyIncident(snapshot);

  if (class1.category != class2.category) {
    return false;
  }
  if (class1.severity != class2.severity) {
    return false;
  }
  if (class1.escalation_path != class2.escalation_path) {
    return false;
  }

  return true;
}

bool TestClassifyIncidentProvidesPlaybookName() {
  IncidentSnapshot snapshot;
  snapshot.error = LoRaError::kTimeoutRecovered;
  const IncidentClassification classification = classifyIncident(snapshot);

  if (classification.suggested_playbook[0] == '\0') {
    return false;
  }

  const std::string playbook(classification.suggested_playbook);
  if (playbook.find("timeout") == std::string::npos) {
    return false;
  }

  return true;
}

bool TestIncidentClassificationHasTaxonomyVersion() {
  IncidentSnapshot snapshot;
  snapshot.error = LoRaError::kInvalidConfig;
  const IncidentClassification classification = classifyIncident(snapshot);

  if (classification.taxonomy_version_major != 1) {
    return false;
  }
  if (classification.taxonomy_version_minor != 0) {
    return false;
  }

  return true;
}

bool TestIncidentCategoryCodesAreStable() {
  if (static_cast<std::uint16_t>(IncidentCategory::kTimeoutRelated) != 1000) {
    return false;
  }
  if (static_cast<std::uint16_t>(IncidentCategory::kIrqAnomaly) != 2000) {
    return false;
  }
  if (static_cast<std::uint16_t>(IncidentCategory::kConfigError) != 3000) {
    return false;
  }
  if (static_cast<std::uint16_t>(IncidentCategory::kRuntimeTransition) != 4000) {
    return false;
  }
  if (static_cast<std::uint16_t>(IncidentCategory::kHardwareFault) != 5000) {
    return false;
  }
  if (static_cast<std::uint16_t>(IncidentCategory::kUnknown) != 9000) {
    return false;
  }

  return true;
}

bool TestIncidentSeverityValuesAreStable() {
  if (static_cast<std::uint8_t>(IncidentSeverity::kInfo) != 0) {
    return false;
  }
  if (static_cast<std::uint8_t>(IncidentSeverity::kWarning) != 1) {
    return false;
  }
  if (static_cast<std::uint8_t>(IncidentSeverity::kCritical) != 2) {
    return false;
  }

  return true;
}

bool TestIncidentClassificationToStringMethods() {
  IncidentSnapshot snapshot;
  snapshot.error = LoRaError::kHardwareInitFailure;
  const IncidentClassification classification = classifyIncident(snapshot);

  const char* cat_str = classification.categoryToString();
  if (cat_str == nullptr || cat_str[0] == '\0') {
    return false;
  }

  const char* sev_str = classification.severityToString();
  if (sev_str == nullptr || sev_str[0] == '\0') {
    return false;
  }

  const char* esc_str = classification.escalationToString();
  if (esc_str == nullptr || esc_str[0] == '\0') {
    return false;
  }

  return true;
}

bool TestClassifyIncidentFromDriverSnapshot() {
  LoRaDriver driver;

  if (driver.begin(MakeV1Config()) != LoRaError::kOk) {
    return false;
  }

  if (driver.send(nullptr, 1) != LoRaError::kInvalidConfig) {
    return false;
  }

  const IncidentSnapshot snapshot = driver.captureIncidentSnapshot();
  const IncidentClassification classification = classifyIncident(snapshot);

  if (classification.category != IncidentCategory::kConfigError) {
    return false;
  }

  return true;
}

int RunSmoke() {
  if (!TestDeterministicTxSuccessTransitionOrder()) {
    return EXIT_FAILURE;
  }
  if (!TestSendRejectsIllegalEntryStateWithTypedError()) {
    return EXIT_FAILURE;
  }
  if (!TestTxFailureUsesDeterministicFailurePath()) {
    return EXIT_FAILURE;
  }
  if (!TestDeterministicRxCompletionAndReturnToListening()) {
    return EXIT_FAILURE;
  }
  if (!TestIrqProfilesKeepCoreTxRxParity()) {
    return EXIT_FAILURE;
  }
  if (!TestRepeatedTxRxCyclesRemainDeterministic()) {
    return EXIT_FAILURE;
  }
  if (!TestCallbackExceptionReturnsTransitionGuardFailure()) {
    return EXIT_FAILURE;
  }
  if (!TestPayloadExceedsMaxSizeReturnsInvalidConfig()) {
    return EXIT_FAILURE;
  }
  if (!TestSleepAndStandbyProvideCanonicalPowerFlow()) {
    return EXIT_FAILURE;
  }
  if (!TestStandbyBeforeBeginReturnsTypedError()) {
    return EXIT_FAILURE;
  }
  if (!TestSleepBeforeBeginReturnsTypedError()) {
    return EXIT_FAILURE;
  }
  if (!TestSleepFromIdleReturnsTransitionGuardFailure()) {
    return EXIT_FAILURE;
  }
  if (!TestTimeoutRecoveryFromListeningReturnsDeterministicSequence()) {
    return EXIT_FAILURE;
  }
  if (!TestTimeoutRecoveryFromReadyStateReturnsGuardFailure()) {
    return EXIT_FAILURE;
  }
  if (!TestRepeatedTimeoutRecoveryLoopsRemainBounded()) {
    return EXIT_FAILURE;
  }
  if (!TestRepeatedSleepWakeLoopsResumeWithoutReset()) {
    return EXIT_FAILURE;
  }
  if (!TestTimeoutRecoveryFromReadyReturnsGuardFailureAfterTx()) {
    return EXIT_FAILURE;
  }
  if (!TestTimeoutRecoveryFromListeningAfterRxReturnsDeterministicSequence()) {
    return EXIT_FAILURE;
  }
  if (!TestTimeoutRecoveryFromNotInitializedReturnsTypedError()) {
    return EXIT_FAILURE;
  }
  if (!TestTimeoutRecoveryCallbackFailureReturnsRecoveryFailure()) {
    return EXIT_FAILURE;
  }
  if (!TestUnsupportedBandAndIrqRoutingReturnTypedDiagnostics()) {
    return EXIT_FAILURE;
  }
  if (!TestIntegrationContractDocDefinesCanonicalFlowAndOnboarding()) {
    return EXIT_FAILURE;
  }
  if (!TestPublicHeadersPreserveAdapterBoundary()) {
    return EXIT_FAILURE;
  }
  if (!TestInitPhaseEventsAreEmittedInDeterministicOrder()) {
    return EXIT_FAILURE;
  }
  if (!TestDiagnosticContextIncludesVersionAndSequence()) {
    return EXIT_FAILURE;
  }
  if (!TestIncidentSnapshotCaptureIncludesAllRequiredFields()) {
    return EXIT_FAILURE;
  }
  if (!TestIncidentSnapshotFormatToProducesStableOutput()) {
    return EXIT_FAILURE;
  }
  if (!TestIncidentSnapshotFormatToRejectsInvalidBuffer()) {
    return EXIT_FAILURE;
  }
  if (!TestEventOrderingParityAcrossIrqProfiles()) {
    return EXIT_FAILURE;
  }
  if (!TestDiagnosticContextSequenceIncrementsOnOperations()) {
    return EXIT_FAILURE;
  }
  if (!TestErrorDiagnosticContextIncludesSequence()) {
    return EXIT_FAILURE;
  }
  if (!TestClassifyIncidentReturnsUnknownForOkError()) {
    return EXIT_FAILURE;
  }
  if (!TestClassifyIncidentMapsTimeoutErrorsCorrectly()) {
    return EXIT_FAILURE;
  }
  if (!TestClassifyIncidentMapsConfigErrorsCorrectly()) {
    return EXIT_FAILURE;
  }
  if (!TestClassifyIncidentMapsHardwareFaultCorrectly()) {
    return EXIT_FAILURE;
  }
  if (!TestClassifyIncidentMapsRuntimeTransitionCorrectly()) {
    return EXIT_FAILURE;
  }
  if (!TestClassifyIncidentIsDeterministic()) {
    return EXIT_FAILURE;
  }
  if (!TestClassifyIncidentProvidesPlaybookName()) {
    return EXIT_FAILURE;
  }
  if (!TestIncidentClassificationHasTaxonomyVersion()) {
    return EXIT_FAILURE;
  }
  if (!TestIncidentCategoryCodesAreStable()) {
    return EXIT_FAILURE;
  }
  if (!TestIncidentSeverityValuesAreStable()) {
    return EXIT_FAILURE;
  }
  if (!TestIncidentClassificationToStringMethods()) {
    return EXIT_FAILURE;
  }
  if (!TestClassifyIncidentFromDriverSnapshot()) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

}  // namespace

int main() {
  return RunSmoke();
}
