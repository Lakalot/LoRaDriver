#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <utility>
#include <vector>

#include "loradriver/lora_driver.hpp"

namespace {

using loradriver::LoRaDriver;
using loradriver::LoRaError;
using loradriver::RadioConfig;
using loradriver::RadioEvent;

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
  return EXIT_SUCCESS;
}

}  // namespace

int main() {
  return RunSmoke();
}
