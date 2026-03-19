#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "loradriver/lora_driver.hpp"
#include "loradriver/ota_gate.hpp"
#include "loradriver/radio_counters.hpp"

namespace {

using loradriver::LoRaDriver;
using loradriver::LoRaError;
using loradriver::RadioConfig;
using loradriver::RadioCounters;
using loradriver::RadioEvent;
using loradriver::OtaTelemetryInput;

RadioConfig MakeV1Config() {
  RadioConfig config;
  config.chip = RadioConfig::Chip::kSx1276;
  config.band = RadioConfig::Band::k868;
  config.dio_routing = RadioConfig::DioRouting::kDio0Only;
  config.spi_frequency_hz = 8000000;
  config.spreading_factor = 9;
  config.bandwidth_khz = 125;
  config.coding_rate_denominator = 5;
  config.sync_word = 0x12;
  config.tx_power_dbm = 14;
  config.crc_enabled = true;
  config.preamble_length = 8;
  return config;
}

RadioConfig MakeDio0Dio1Config() {
  RadioConfig config = MakeV1Config();
  config.dio_routing = RadioConfig::DioRouting::kDio0Dio1;
  return config;
}

// ─────────────────────────────────────────────────────────────────────────────
// Zero-init and static assertions
// ─────────────────────────────────────────────────────────────────────────────

bool TestCountersZeroInitialized() {
  LoRaDriver driver;
  const RadioCounters c = driver.getCounters();
  if (c.init_attempts != 0) return false;
  if (c.init_failures != 0) return false;
  if (c.tx_success != 0) return false;
  if (c.tx_fail != 0) return false;
  if (c.rx_success != 0) return false;
  if (c.rx_fail != 0) return false;
  if (c.timeout_events != 0) return false;
  if (c.irq_overflow_events != 0) return false;
  return true;
}

bool TestRadioCountersTrivialCopyable() {
  static_assert(std::is_trivially_copyable<RadioCounters>::value,
                "RadioCounters must be trivially copyable");
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// getCounters() snapshot semantics
// ─────────────────────────────────────────────────────────────────────────────

bool TestGetCountersReturnsSnapshot() {
  LoRaDriver driver;
  (void)driver.begin(MakeV1Config());
  RadioCounters c1 = driver.getCounters();
  (void)driver.begin(MakeV1Config());  // kAlreadyInitialized – still an attempt
  RadioCounters c2 = driver.getCounters();
  // c1 was a value copy – modifying driver after c1 captured must not change c1
  if (c1.init_attempts != 1) return false;
  if (c2.init_attempts != 2) return false;
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// init_attempts / init_failures
// ─────────────────────────────────────────────────────────────────────────────

bool TestInitAttemptsIncrementOnBegin() {
  LoRaDriver driver;
  if (driver.getCounters().init_attempts != 0) return false;
  (void)driver.begin(MakeV1Config());
  if (driver.getCounters().init_attempts != 1) return false;
  return true;
}

bool TestInitAttemptsCountsAlreadyInitialized() {
  LoRaDriver driver;
  (void)driver.begin(MakeV1Config());
  (void)driver.begin(MakeV1Config());  // kAlreadyInitialized
  if (driver.getCounters().init_attempts != 2) return false;
  return true;
}

bool TestInitFailuresOnUnsupportedProfile() {
  LoRaDriver driver;
  RadioConfig bad = MakeV1Config();
  bad.chip = RadioConfig::Chip::kSx126xStub;
  (void)driver.begin(bad);
  const RadioCounters c = driver.getCounters();
  if (c.init_attempts != 1) return false;
  if (c.init_failures != 1) return false;
  return true;
}

bool TestInitFailuresOnInvalidConfig() {
  LoRaDriver driver;
  RadioConfig bad = MakeV1Config();
  bad.spi_frequency_hz = 1000000;  // out of [4MHz, 8MHz]
  (void)driver.begin(bad);
  const RadioCounters c = driver.getCounters();
  if (c.init_attempts != 1) return false;
  if (c.init_failures != 1) return false;
  return true;
}

bool TestInitSuccessNoFailureCount() {
  LoRaDriver driver;
  (void)driver.begin(MakeV1Config());
  const RadioCounters c = driver.getCounters();
  if (c.init_attempts != 1) return false;
  if (c.init_failures != 0) return false;
  return true;
}

bool TestInitFailuresMultipleAttempts() {
  LoRaDriver driver;
  RadioConfig bad = MakeV1Config();
  bad.chip = RadioConfig::Chip::kSx126xStub;
  (void)driver.begin(bad);
  (void)driver.begin(bad);
  (void)driver.begin(bad);
  const RadioCounters c = driver.getCounters();
  if (c.init_attempts != 3) return false;
  if (c.init_failures != 3) return false;
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// tx_success / tx_fail
// ─────────────────────────────────────────────────────────────────────────────

bool TestTxSuccessIncrementOnSend() {
  LoRaDriver driver;
  (void)driver.begin(MakeV1Config());
  const std::uint8_t payload[] = {0x01, 0x02};
  (void)driver.send(payload, sizeof(payload));
  if (driver.getCounters().tx_success != 1) return false;
  if (driver.getCounters().tx_fail != 0) return false;
  return true;
}

bool TestTxFailIncrementOnInvalidPayload() {
  LoRaDriver driver;
  (void)driver.begin(MakeV1Config());
  (void)driver.send(nullptr, 0);
  const RadioCounters c = driver.getCounters();
  if (c.tx_success != 0) return false;
  if (c.tx_fail != 1) return false;
  return true;
}

bool TestTxSuccessMultiple() {
  LoRaDriver driver;
  (void)driver.begin(MakeV1Config());
  const std::uint8_t payload[] = {0xAB};
  (void)driver.send(payload, 1);
  (void)driver.send(payload, 1);
  (void)driver.send(payload, 1);
  if (driver.getCounters().tx_success != 3) return false;
  return true;
}

bool TestTxSuccessAfterRxCycle() {
  LoRaDriver driver;
  (void)driver.begin(MakeV1Config());
  (void)driver.startReceive();  // goes to Listening after done
  const std::uint8_t payload[] = {0x01};
  (void)driver.send(payload, 1);
  if (driver.getCounters().tx_success != 1) return false;
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// rx_success / rx_fail
// ─────────────────────────────────────────────────────────────────────────────

bool TestRxSuccessIncrementOnStartReceive() {
  LoRaDriver driver;
  (void)driver.begin(MakeV1Config());
  (void)driver.startReceive();
  const RadioCounters c = driver.getCounters();
  if (c.rx_success != 1) return false;
  if (c.rx_fail != 0) return false;
  return true;
}

bool TestRxSuccessMultiple() {
  LoRaDriver driver;
  (void)driver.begin(MakeV1Config());
  (void)driver.startReceive();   // Listening → rx_success=1
  (void)driver.standby();        // Listening → Ready (required to restart RX)
  (void)driver.startReceive();   // Ready → Listening → rx_success=2
  if (driver.getCounters().rx_success != 2) return false;
  return true;
}

bool TestRxFailIncrementsOnNotInitialized() {
  LoRaDriver driver;
  (void)driver.startReceive();
  if (driver.getCounters().rx_fail != 1) return false;
  return true;
}

bool TestRxFailIncrementsOnIllegalState() {
  LoRaDriver driver;
  (void)driver.begin(MakeV1Config());
  const std::uint8_t payload[] = {0x01};
  (void)driver.send(payload, 1);   // Ready
  (void)driver.sleep();            // Idle
  (void)driver.startReceive();     // illegal from Idle
  if (driver.getCounters().rx_fail != 1) return false;
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// timeout_events
// ─────────────────────────────────────────────────────────────────────────────

bool TestTimeoutEventsIncrementOnRecovery() {
  LoRaDriver driver;
  (void)driver.begin(MakeV1Config());
  (void)driver.startReceive();  // state → Listening
  (void)driver.recoverFromTimeout();
  const RadioCounters c = driver.getCounters();
  if (c.timeout_events != 1) return false;
  return true;
}

bool TestTimeoutCounterAccumulates() {
  LoRaDriver driver;
  (void)driver.begin(MakeV1Config());
  for (int i = 0; i < 3; ++i) {
    (void)driver.startReceive();       // → Listening
    (void)driver.recoverFromTimeout();  // → Ready, timeout_events++
  }
  if (driver.getCounters().timeout_events != 3) return false;
  return true;
}

bool TestTimeoutNotIncrementedOnGuardReject() {
  LoRaDriver driver;
  (void)driver.begin(MakeV1Config());
  // From Ready state: recoverFromTimeout is invalid (illegal state)
  (void)driver.recoverFromTimeout();
  if (driver.getCounters().timeout_events != 0) return false;
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// irq_overflow_events + kIrqOverflow event
// ─────────────────────────────────────────────────────────────────────────────

bool TestIrqOverflowCounterIncrements() {
  LoRaDriver driver;
  (void)driver.begin(MakeV1Config());
  (void)driver.handleIrqOverflow();
  if (driver.getCounters().irq_overflow_events != 1) return false;
  return true;
}

bool TestIrqOverflowEmitsEvent() {
  LoRaDriver driver;
  (void)driver.begin(MakeV1Config());

  std::vector<RadioEvent> events;
  (void)driver.setEventCallback([&events](RadioEvent e, int) {
    events.push_back(e);
  });

  (void)driver.handleIrqOverflow();

  bool found_overflow = false;
  for (const auto& e : events) {
    if (e == RadioEvent::kIrqOverflow) {
      found_overflow = true;
      break;
    }
  }
  return found_overflow;
}

bool TestIrqOverflowNotInitializedReturnsError() {
  LoRaDriver driver;
  const LoRaError result = driver.handleIrqOverflow();
  if (result != LoRaError::kNotInitialized) return false;
  if (driver.getCounters().irq_overflow_events != 0) return false;
  return true;
}

bool TestIrqOverflowCallbackThrowReturnsGuardFailure() {
  LoRaDriver driver;
  (void)driver.begin(MakeV1Config());
  (void)driver.setEventCallback([](RadioEvent e, int) {
    if (e == RadioEvent::kIrqOverflow) {
      throw 42;
    }
  });

  const LoRaError result = driver.handleIrqOverflow();
  if (result != LoRaError::kTransitionGuardFailure) return false;
  if (driver.lastError() != LoRaError::kTransitionGuardFailure) return false;
  if (driver.lastDiagnosticCode() != 7102) return false;
  return true;
}

bool TestIrqOverflowWorksForDio0Only() {
  LoRaDriver driver;
  (void)driver.begin(MakeV1Config());  // kDio0Only
  const LoRaError result = driver.handleIrqOverflow();
  if (result != LoRaError::kOk) return false;
  if (driver.getCounters().irq_overflow_events != 1) return false;
  return true;
}

bool TestIrqOverflowWorksForDio0Dio1() {
  LoRaDriver driver;
  (void)driver.begin(MakeDio0Dio1Config());  // kDio0Dio1
  const LoRaError result = driver.handleIrqOverflow();
  if (result != LoRaError::kOk) return false;
  if (driver.getCounters().irq_overflow_events != 1) return false;
  return true;
}

bool TestIrqOverflowAccumulates() {
  LoRaDriver driver;
  (void)driver.begin(MakeV1Config());
  (void)driver.handleIrqOverflow();
  (void)driver.handleIrqOverflow();
  (void)driver.handleIrqOverflow();
  if (driver.getCounters().irq_overflow_events != 3) return false;
  return true;
}

bool TestIrqOverflowEventHasDeterministicDetailCode() {
  LoRaDriver driver;
  (void)driver.begin(MakeV1Config());

  int captured_detail = -1;
  (void)driver.setEventCallback([&captured_detail](RadioEvent e, int detail) {
    if (e == RadioEvent::kIrqOverflow) {
      captured_detail = detail;
    }
  });

  (void)driver.handleIrqOverflow();
  (void)driver.handleIrqOverflow();

  // Detail code must be the same both times (deterministic)
  int second_detail = -2;
  (void)driver.setEventCallback([&second_detail](RadioEvent e, int detail) {
    if (e == RadioEvent::kIrqOverflow) {
      second_detail = detail;
    }
  });
  (void)driver.handleIrqOverflow();

  return (captured_detail >= 0) && (captured_detail == second_detail);
}

// ─────────────────────────────────────────────────────────────────────────────
// No regression: FSM behavior, diagnostics, snapshots
// ─────────────────────────────────────────────────────────────────────────────

bool TestCountersDoNotAffectFsmBehavior() {
  LoRaDriver driver;
  if (driver.begin(MakeV1Config()) != LoRaError::kOk) return false;
  if (!driver.isInitialized()) return false;

  const std::uint8_t payload[] = {0x01};
  if (driver.send(payload, 1) != LoRaError::kOk) return false;
  if (driver.startReceive() != LoRaError::kOk) return false;
  if (driver.recoverFromTimeout() != LoRaError::kTimeoutRecovered) return false;
  if (!driver.isInitialized()) return false;
  return true;
}

bool TestLastErrorUnchangedByCounters() {
  LoRaDriver driver;
  (void)driver.begin(MakeV1Config());
  if (driver.lastError() != LoRaError::kOk) return false;

  (void)driver.handleIrqOverflow();
  // handleIrqOverflow should not pollute lastError on success
  if (driver.lastError() != LoRaError::kOk) return false;
  return true;
}

bool TestDiagnosticContextUnchangedByCounters() {
  LoRaDriver driver;
  (void)driver.begin(MakeV1Config());
  const auto ctx_before = driver.lastDiagnosticContext();
  (void)driver.handleIrqOverflow();
  const auto ctx_after = driver.lastDiagnosticContext();
  // handleIrqOverflow should not alter lastDiagnosticContext
  if (ctx_before.error != ctx_after.error) return false;
  if (ctx_before.detail_code != ctx_after.detail_code) return false;
  return true;
}

bool TestCaptureIncidentSnapshotUnchangedByCounters() {
  LoRaDriver driver;
  (void)driver.begin(MakeV1Config());
  const auto snap1 = driver.captureIncidentSnapshot();
  (void)driver.handleIrqOverflow();
  const auto snap2 = driver.captureIncidentSnapshot();
  // snapshot fields should be unaffected by overflow
  if (snap1.error != snap2.error) return false;
  if (snap1.detail_code != snap2.detail_code) return false;
  return true;
}

bool TestShutdownPreservesCounters() {
  LoRaDriver driver;
  (void)driver.begin(MakeV1Config());
  (void)driver.handleIrqOverflow();
  (void)driver.shutdown();
  // Counters should survive shutdown (they are cumulative runtime metrics)
  if (driver.getCounters().init_attempts != 1) return false;
  if (driver.getCounters().irq_overflow_events != 1) return false;
  return true;
}

bool TestGetOtaTelemetryInputSuppliesRequiredFields() {
  LoRaDriver driver;
  (void)driver.begin(MakeV1Config());
  (void)driver.begin(MakeV1Config());     // init_attempts++ only
  (void)driver.send(nullptr, 0);          // tx_fail++
  (void)driver.startReceive();            // rx_success++
  (void)driver.recoverFromTimeout();      // timeout_events++
  (void)driver.handleIrqOverflow();       // irq_overflow_events++

  const OtaTelemetryInput telemetry = driver.getOtaTelemetryInput("2.4.0");
  if (std::string(telemetry.firmware_version) != "2.4.0") return false;
  if (std::string(telemetry.radio_family) != "SX1276") return false;
  if (std::string(telemetry.active_band) != "868") return false;
  if (telemetry.init_failure_rate != 0.0f) return false;      // 0 / 2
  if (telemetry.tx_success_rate != 0.0f) return false;        // 0 / 1
  if (telemetry.rx_success_rate != 100.0f) return false;      // 1 / 1
  if (telemetry.timeout_events != 1) return false;
  if (telemetry.irq_overflow_events != 1) return false;
  return true;
}

bool TestGetOtaTelemetryInputPreservesFractionalRates() {
  LoRaDriver driver;
  (void)driver.begin(MakeV1Config());

  const std::uint8_t payload[] = {0x01};
  (void)driver.send(payload, sizeof(payload));  // success #1
  (void)driver.send(payload, sizeof(payload));  // success #2
  (void)driver.send(nullptr, 0);                // fail #1

  const OtaTelemetryInput telemetry = driver.getOtaTelemetryInput("2.4.0");
  const float expected = (2.0f * 100.0f) / 3.0f;
  if (std::fabs(telemetry.tx_success_rate - expected) > 0.001f) return false;
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Runner
// ─────────────────────────────────────────────────────────────────────────────

#define RUN_TEST(fn) \
  if (!(fn)()) { \
    std::fprintf(stderr, "FAIL: %s\n", #fn); \
    return EXIT_FAILURE; \
  }

int RunRadioCounterTests() {
  // Zero-init and type contract
  RUN_TEST(TestCountersZeroInitialized)
  RUN_TEST(TestRadioCountersTrivialCopyable)
  RUN_TEST(TestGetCountersReturnsSnapshot)
  // init_attempts / init_failures
  RUN_TEST(TestInitAttemptsIncrementOnBegin)
  RUN_TEST(TestInitAttemptsCountsAlreadyInitialized)
  RUN_TEST(TestInitFailuresOnUnsupportedProfile)
  RUN_TEST(TestInitFailuresOnInvalidConfig)
  RUN_TEST(TestInitSuccessNoFailureCount)
  RUN_TEST(TestInitFailuresMultipleAttempts)
  // tx_success / tx_fail
  RUN_TEST(TestTxSuccessIncrementOnSend)
  RUN_TEST(TestTxFailIncrementOnInvalidPayload)
  RUN_TEST(TestTxSuccessMultiple)
  RUN_TEST(TestTxSuccessAfterRxCycle)
  // rx_success / rx_fail
  RUN_TEST(TestRxSuccessIncrementOnStartReceive)
  RUN_TEST(TestRxSuccessMultiple)
  RUN_TEST(TestRxFailIncrementsOnNotInitialized)
  RUN_TEST(TestRxFailIncrementsOnIllegalState)
  // timeout_events
  RUN_TEST(TestTimeoutEventsIncrementOnRecovery)
  RUN_TEST(TestTimeoutCounterAccumulates)
  RUN_TEST(TestTimeoutNotIncrementedOnGuardReject)
  // irq_overflow_events + kIrqOverflow event
  RUN_TEST(TestIrqOverflowCounterIncrements)
  RUN_TEST(TestIrqOverflowEmitsEvent)
  RUN_TEST(TestIrqOverflowNotInitializedReturnsError)
  RUN_TEST(TestIrqOverflowCallbackThrowReturnsGuardFailure)
  RUN_TEST(TestIrqOverflowWorksForDio0Only)
  RUN_TEST(TestIrqOverflowWorksForDio0Dio1)
  RUN_TEST(TestIrqOverflowAccumulates)
  RUN_TEST(TestIrqOverflowEventHasDeterministicDetailCode)
  // No regression: FSM, diagnostics, snapshots
  RUN_TEST(TestCountersDoNotAffectFsmBehavior)
  RUN_TEST(TestLastErrorUnchangedByCounters)
  RUN_TEST(TestDiagnosticContextUnchangedByCounters)
  RUN_TEST(TestCaptureIncidentSnapshotUnchangedByCounters)
  RUN_TEST(TestShutdownPreservesCounters)
  RUN_TEST(TestGetOtaTelemetryInputSuppliesRequiredFields)
  RUN_TEST(TestGetOtaTelemetryInputPreservesFractionalRates)
  return EXIT_SUCCESS;
}

}  // namespace

int main() {
  return RunRadioCounterTests();
}
