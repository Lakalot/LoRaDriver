#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "loradriver/lora_driver.hpp"

namespace {

using loradriver::LoRaDriver;
using loradriver::LoRaError;
using loradriver::RadioConfig;
using loradriver::RadioEvent;

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
      {RadioEvent::kTxPreparing, 3100},
      {RadioEvent::kTxInProgress, 3200},
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
      {RadioEvent::kTxPreparing, 3100},
      {RadioEvent::kTxFailed, 3301},
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
      {RadioEvent::kRxListening, 4100},
      {RadioEvent::kRxInProgress, 4200},
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
  return context.error == LoRaError::kNotInitialized && context.detail_code == 5201;
}

bool TestSleepBeforeBeginReturnsTypedError() {
  LoRaDriver driver;
  if (driver.sleep() != LoRaError::kNotInitialized) {
    return false;
  }

  const auto context = driver.lastDiagnosticContext();
  return context.error == LoRaError::kNotInitialized && context.detail_code == 5101;
}

bool TestSleepDuringRxInProgressReturnsTransitionGuardFailure() {
  LoRaDriver driver;
  std::vector<EventTrace> events;

  if (!BeginAndClearInitEvents(driver, events)) {
    return false;
  }

  if (driver.startReceive() != LoRaError::kOk) {
    return false;
  }

  events.clear();

  const LoRaError result = driver.sleep();
  if (result != LoRaError::kTransitionGuardFailure) {
    return false;
  }

  return driver.lastDiagnosticCode() == 5102;
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
  const std::array<std::string, 4> public_headers = {
      root + "/include/loradriver/lora_driver.hpp",
      root + "/include/loradriver/lora_error.hpp",
      root + "/include/loradriver/radio_config.hpp",
      root + "/include/loradriver/radio_event.hpp",
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
  if (!TestSleepDuringRxInProgressReturnsTransitionGuardFailure()) {
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
  return EXIT_SUCCESS;
}

}  // namespace

int main() {
  return RunSmoke();
}
