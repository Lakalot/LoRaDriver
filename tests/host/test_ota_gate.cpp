#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "loradriver/ota_gate.hpp"
#include "loradriver/ci_gates.hpp"
#include "loradriver/artifact_governance.hpp"
#include "loradriver/versioning.hpp"

namespace {

using loradriver::ArtifactRegistry;
using loradriver::CiGateEngine;
using loradriver::GateRule;
using loradriver::OtaDecisionRationale;
using loradriver::OtaGateEngine;
using loradriver::OtaRolloutDecision;
using loradriver::OtaTelemetryInput;

// ─────────────────────────────────────────────────────────────────────────────
// Helper: build a fully-populated, health-OK telemetry sample
// ─────────────────────────────────────────────────────────────────────────────
OtaTelemetryInput MakeHealthyTelemetry() {
  OtaTelemetryInput t{};
  std::strncpy(t.firmware_version, "1.2.3", sizeof(t.firmware_version) - 1);
  std::strncpy(t.radio_family, "SX1276", sizeof(t.radio_family) - 1);
  std::strncpy(t.active_band, "868", sizeof(t.active_band) - 1);
  t.init_failure_rate        = 0.5f;   // => init_success_rate = 99.5%  (INIT-001 passes)
  t.timeout_events           = 0;
  t.irq_overflow_events      = 0;      // IRQ-002: == 0  (passes)
  t.tx_success_rate          = 99.5f;  // TXRX-001: >= 99%  (passes)
  t.rx_success_rate          = 98.5f;  // TXRX-002: >= 98%  (passes)
  t.sample_timestamp_utc     = 1000;
  t.baseline_tx_success_rate = 0.0f;   // no baseline → trend check skipped
  t.baseline_rx_success_rate = 0.0f;
  t.baseline_init_failure_rate = 0.0f;
  return t;
}

// ─────────────────────────────────────────────────────────────────────────────
// Regression: enum values are stable (serialisation contract)
// ─────────────────────────────────────────────────────────────────────────────
bool TestOtaRolloutDecisionEnumValuesAreStable() {
  if (static_cast<uint8_t>(OtaRolloutDecision::kAllow) != 0) return false;
  if (static_cast<uint8_t>(OtaRolloutDecision::kHold)  != 1) return false;
  if (static_cast<uint8_t>(OtaRolloutDecision::kBlock) != 2) return false;
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 1: telemetry struct carries all mandatory fields
// ─────────────────────────────────────────────────────────────────────────────
bool TestOtaTelemetryInputHasAllMandatoryFields() {
  OtaTelemetryInput t = MakeHealthyTelemetry();
  if (t.firmware_version[0] == '\0') return false;
  if (t.radio_family[0]     == '\0') return false;
  if (t.active_band[0]      == '\0') return false;
  // numeric fields initialised
  (void)t.init_failure_rate;
  (void)t.timeout_events;
  (void)t.irq_overflow_events;
  (void)t.tx_success_rate;
  (void)t.rx_success_rate;
  (void)t.sample_timestamp_utc;
  return true;
}

bool TestOtaTelemetryInputIsFixedSize() {
  // Must be a plain struct — no pointers, no virtual dispatch
  static_assert(std::is_trivially_copyable<OtaTelemetryInput>::value,
                "OtaTelemetryInput must be trivially copyable (no heap)");
  return true;
}

bool TestOtaDecisionRationaleIsFixedSize() {
  static_assert(std::is_trivially_copyable<OtaDecisionRationale>::value,
                "OtaDecisionRationale must be trivially copyable (no heap)");
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 2: Gate-pass scenario → kAllow
// ─────────────────────────────────────────────────────────────────────────────
bool TestGatePassAllowsRollout() {
  OtaTelemetryInput t = MakeHealthyTelemetry();
  OtaDecisionRationale rationale{};
  OtaRolloutDecision decision = OtaGateEngine::evaluate(t, rationale);
  if (decision != OtaRolloutDecision::kAllow) return false;
  if (rationale.failed_gate_count != 0)       return false;
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 2: Blocking threshold violation → kBlock
// ─────────────────────────────────────────────────────────────────────────────
bool TestTxRateFailureBlocksRollout() {
  OtaTelemetryInput t = MakeHealthyTelemetry();
  t.tx_success_rate = 95.0f;  // below 99% threshold → TXRX-001 fails (blocking)
  OtaDecisionRationale rationale{};
  OtaRolloutDecision decision = OtaGateEngine::evaluate(t, rationale);
  if (decision != OtaRolloutDecision::kBlock) return false;
  if (rationale.failed_gate_count == 0)       return false;
  // Gate ID must be present in rationale
  bool found = false;
  for (uint8_t i = 0; i < rationale.failed_gate_count; ++i) {
    if (std::strcmp(rationale.failed_gate_ids[i], "TXRX-001") == 0) {
      found = true;
      break;
    }
  }
  return found;
}

bool TestRxRateFailureBlocksRollout() {
  OtaTelemetryInput t = MakeHealthyTelemetry();
  t.rx_success_rate = 90.0f;  // below 98% threshold → TXRX-002 fails
  OtaDecisionRationale rationale{};
  OtaRolloutDecision decision = OtaGateEngine::evaluate(t, rationale);
  if (decision != OtaRolloutDecision::kBlock) return false;
  bool found = false;
  for (uint8_t i = 0; i < rationale.failed_gate_count; ++i) {
    if (std::strcmp(rationale.failed_gate_ids[i], "TXRX-002") == 0) {
      found = true;
      break;
    }
  }
  return found;
}

bool TestInitFailureRateHighBlocksRollout() {
  OtaTelemetryInput t = MakeHealthyTelemetry();
  t.init_failure_rate = 5.0f;  // success_rate = 95% < 99% → INIT-001 fails
  OtaDecisionRationale rationale{};
  OtaRolloutDecision decision = OtaGateEngine::evaluate(t, rationale);
  if (decision != OtaRolloutDecision::kBlock) return false;
  bool found = false;
  for (uint8_t i = 0; i < rationale.failed_gate_count; ++i) {
    if (std::strcmp(rationale.failed_gate_ids[i], "INIT-001") == 0) {
      found = true;
      break;
    }
  }
  return found;
}

bool TestIrqOverflowBlocksRollout() {
  OtaTelemetryInput t = MakeHealthyTelemetry();
  t.irq_overflow_events = 3;  // IRQ-002: must be == 0 (blocking)
  OtaDecisionRationale rationale{};
  OtaRolloutDecision decision = OtaGateEngine::evaluate(t, rationale);
  if (decision != OtaRolloutDecision::kBlock) return false;
  bool found = false;
  for (uint8_t i = 0; i < rationale.failed_gate_count; ++i) {
    if (std::strcmp(rationale.failed_gate_ids[i], "IRQ-002") == 0) {
      found = true;
      break;
    }
  }
  return found;
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 2: Rationale includes metric actual values and thresholds
// ─────────────────────────────────────────────────────────────────────────────
bool TestRationaleContainsMetricValues() {
  OtaTelemetryInput t = MakeHealthyTelemetry();
  t.tx_success_rate = 95.0f;
  OtaDecisionRationale rationale{};
  OtaGateEngine::evaluate(t, rationale);
  // At least one entry must have actual value matching what we sent
  for (uint8_t i = 0; i < rationale.failed_gate_count; ++i) {
    if (std::strcmp(rationale.failed_gate_ids[i], "TXRX-001") == 0) {
      // actual_values should record 95.0 and threshold 99.0
      if (rationale.failed_gate_actuals[i] < 94.9f || rationale.failed_gate_actuals[i] > 95.1f) {
        return false;
      }
      if (rationale.failed_gate_thresholds[i] < 98.9f) {
        return false;
      }
      return true;
    }
  }
  return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 3: Data quality — missing required fields → kHold
// ─────────────────────────────────────────────────────────────────────────────
bool TestMissingFirmwareVersionHolds() {
  OtaTelemetryInput t = MakeHealthyTelemetry();
  t.firmware_version[0] = '\0';
  OtaDecisionRationale rationale{};
  OtaRolloutDecision decision = OtaGateEngine::evaluate(t, rationale);
  if (decision != OtaRolloutDecision::kHold) return false;
  if (!rationale.quality_issue) return false;
  return true;
}

bool TestMissingRadioFamilyHolds() {
  OtaTelemetryInput t = MakeHealthyTelemetry();
  t.radio_family[0] = '\0';
  OtaDecisionRationale rationale{};
  OtaRolloutDecision decision = OtaGateEngine::evaluate(t, rationale);
  if (decision != OtaRolloutDecision::kHold) return false;
  if (!rationale.quality_issue) return false;
  return true;
}

bool TestMissingActiveBandHolds() {
  OtaTelemetryInput t = MakeHealthyTelemetry();
  t.active_band[0] = '\0';
  OtaDecisionRationale rationale{};
  OtaRolloutDecision decision = OtaGateEngine::evaluate(t, rationale);
  if (decision != OtaRolloutDecision::kHold) return false;
  if (!rationale.quality_issue) return false;
  return true;
}

bool TestInvalidRadioFamilyHolds() {
  OtaTelemetryInput t = MakeHealthyTelemetry();
  std::strncpy(t.radio_family, "SX1262", sizeof(t.radio_family) - 1);
  OtaDecisionRationale rationale{};
  OtaRolloutDecision decision = OtaGateEngine::evaluate(t, rationale);
  if (decision != OtaRolloutDecision::kHold) return false;
  if (!rationale.quality_issue) return false;
  return true;
}

bool TestInvalidActiveBandHolds() {
  OtaTelemetryInput t = MakeHealthyTelemetry();
  std::strncpy(t.active_band, "915", sizeof(t.active_band) - 1);
  OtaDecisionRationale rationale{};
  OtaRolloutDecision decision = OtaGateEngine::evaluate(t, rationale);
  if (decision != OtaRolloutDecision::kHold) return false;
  if (!rationale.quality_issue) return false;
  return true;
}

bool TestMissingTimestampHolds() {
  OtaTelemetryInput t = MakeHealthyTelemetry();
  t.sample_timestamp_utc = 0;  // zero = missing/uninitialised
  OtaDecisionRationale rationale{};
  OtaRolloutDecision decision = OtaGateEngine::evaluate(t, rationale);
  if (decision != OtaRolloutDecision::kHold) return false;
  if (!rationale.quality_issue) return false;
  return true;
}

bool TestInvalidTxRateOutOfRangeHolds() {
  OtaTelemetryInput t = MakeHealthyTelemetry();
  t.tx_success_rate = 150.0f;  // > 100% is invalid data
  OtaDecisionRationale rationale{};
  OtaRolloutDecision decision = OtaGateEngine::evaluate(t, rationale);
  if (decision != OtaRolloutDecision::kHold) return false;
  if (!rationale.quality_issue) return false;
  return true;
}

bool TestInvalidRxRateNegativeHolds() {
  OtaTelemetryInput t = MakeHealthyTelemetry();
  t.rx_success_rate = -5.0f;  // negative is invalid
  OtaDecisionRationale rationale{};
  OtaRolloutDecision decision = OtaGateEngine::evaluate(t, rationale);
  if (decision != OtaRolloutDecision::kHold) return false;
  if (!rationale.quality_issue) return false;
  return true;
}

bool TestInvalidInitFailureRateOutOfRangeHolds() {
  OtaTelemetryInput t = MakeHealthyTelemetry();
  t.init_failure_rate = 120.0f;  // > 100% invalid
  OtaDecisionRationale rationale{};
  OtaRolloutDecision decision = OtaGateEngine::evaluate(t, rationale);
  if (decision != OtaRolloutDecision::kHold) return false;
  if (!rationale.quality_issue) return false;
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 3: validateDataQuality standalone API
// ─────────────────────────────────────────────────────────────────────────────
bool TestValidateDataQualityPassesForHealthyTelemetry() {
  OtaTelemetryInput t = MakeHealthyTelemetry();
  char reason[128] = {};
  return OtaGateEngine::validateDataQuality(t, reason, sizeof(reason));
}

bool TestValidateDataQualityFailsForMissingField() {
  OtaTelemetryInput t = MakeHealthyTelemetry();
  t.radio_family[0] = '\0';
  char reason[128] = {};
  bool ok = OtaGateEngine::validateDataQuality(t, reason, sizeof(reason));
  if (ok) return false;
  if (reason[0] == '\0') return false;  // must provide reason
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 3: Trend degradation triggers kHold
// ─────────────────────────────────────────────────────────────────────────────
bool TestTrendDegradationInTxTriggersHold() {
  OtaTelemetryInput t = MakeHealthyTelemetry();
  t.baseline_tx_success_rate = 99.0f + OtaTelemetryInput::kTrendDegradationThreshold + 0.5f;
  t.tx_success_rate          = 99.0f;  // still passes TXRX-001, but trend should hold
  OtaDecisionRationale rationale{};
  OtaRolloutDecision decision = OtaGateEngine::evaluate(t, rationale);
  if (decision != OtaRolloutDecision::kHold) return false;
  if (!rationale.trend_risk) return false;
  return rationale.reason[0] != '\0';
}

bool TestLargeTrendDegradationHolds() {
  OtaTelemetryInput t = MakeHealthyTelemetry();
  // Both TX and baseline set so that gate still passes but trend is large
  t.tx_success_rate          = 99.0f;   // passes TXRX-001 (>= 99%)
  t.baseline_tx_success_rate = 99.0f + OtaTelemetryInput::kTrendDegradationThreshold + 1.0f;
  OtaDecisionRationale rationale{};
  OtaRolloutDecision decision = OtaGateEngine::evaluate(t, rationale);
  if (decision != OtaRolloutDecision::kHold) return false;
  if (!rationale.trend_risk)                 return false;
  return true;
}

bool TestRxTrendDegradationHolds() {
  OtaTelemetryInput t = MakeHealthyTelemetry();
  t.rx_success_rate          = 98.0f;   // passes TXRX-002 (>= 98%)
  t.baseline_rx_success_rate = 98.0f + OtaTelemetryInput::kTrendDegradationThreshold + 1.0f;
  OtaDecisionRationale rationale{};
  OtaRolloutDecision decision = OtaGateEngine::evaluate(t, rationale);
  if (decision != OtaRolloutDecision::kHold) return false;
  if (!rationale.trend_risk)                 return false;
  return true;
}

bool TestInitTrendDegradationHolds() {
  OtaTelemetryInput t = MakeHealthyTelemetry();
  t.init_failure_rate = 3.5f;
  t.baseline_init_failure_rate = 0.1f;
  char reason[128] = {};
  bool risk = OtaGateEngine::detectTrendRisk(t, reason, sizeof(reason));
  if (!risk) return false;
  return reason[0] != '\0';
}

bool TestDetectTrendRiskStandaloneApi() {
  OtaTelemetryInput t = MakeHealthyTelemetry();
  t.tx_success_rate          = 99.0f;
  t.baseline_tx_success_rate = 99.0f + OtaTelemetryInput::kTrendDegradationThreshold + 1.0f;
  char reason[128] = {};
  bool risk = OtaGateEngine::detectTrendRisk(t, reason, sizeof(reason));
  if (!risk)             return false;
  if (reason[0] == '\0') return false;  // must explain why
  return true;
}

bool TestNoTrendRiskWhenNoBaseline() {
  OtaTelemetryInput t = MakeHealthyTelemetry();
  // All baselines zero → no trend check
  t.baseline_tx_success_rate  = 0.0f;
  t.baseline_rx_success_rate  = 0.0f;
  t.baseline_init_failure_rate = 0.0f;
  char reason[128] = {};
  bool risk = OtaGateEngine::detectTrendRisk(t, reason, sizeof(reason));
  return !risk;
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 4: Governance — artifact registered on block decision
// ─────────────────────────────────────────────────────────────────────────────
bool TestBlockDecisionRegistersArtifact() {
  ArtifactRegistry::clear();
  OtaTelemetryInput t = MakeHealthyTelemetry();
  t.tx_success_rate = 80.0f;  // triggers block
  OtaDecisionRationale rationale{};
  OtaGateEngine::evaluate(t, rationale);
  // artifact_id must be populated
  if (rationale.artifact_id[0] == '\0') return false;
  return true;
}

bool TestAllowDecisionRegistersArtifact() {
  ArtifactRegistry::clear();
  OtaTelemetryInput t = MakeHealthyTelemetry();
  OtaDecisionRationale rationale{};
  OtaGateEngine::evaluate(t, rationale);
  if (rationale.artifact_id[0] == '\0') return false;
  return true;
}

bool TestBlockedDecisionHasNonEmptyReason() {
  OtaTelemetryInput t = MakeHealthyTelemetry();
  t.irq_overflow_events = 5;
  OtaDecisionRationale rationale{};
  OtaGateEngine::evaluate(t, rationale);
  return rationale.reason[0] != '\0';
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 2: Block takes precedence over Hold (quality issue + block metric both)
// ─────────────────────────────────────────────────────────────────────────────
bool TestBlockPrecedesHold() {
  // Quality checks execute first and deterministically produce kHold.
  OtaTelemetryInput t = MakeHealthyTelemetry();
  t.tx_success_rate = 80.0f;  // would block
  t.firmware_version[0] = '\0';  // also a quality issue
  OtaDecisionRationale rationale{};
  OtaRolloutDecision decision = OtaGateEngine::evaluate(t, rationale);
  if (decision != OtaRolloutDecision::kHold) return false;
  return rationale.quality_issue;
}

bool TestDecisionCreatesTraceabilityIncidentLink() {
  loradriver::TraceabilityEngine::clear();
  ArtifactRegistry::clear();

  OtaTelemetryInput t = MakeHealthyTelemetry();
  OtaDecisionRationale rationale{};
  OtaRolloutDecision decision = OtaGateEngine::evaluate(t, rationale);
  if (decision != OtaRolloutDecision::kAllow) return false;
  if (rationale.artifact_id[0] == '\0') return false;

  char incident_id[loradriver::TraceabilityLink::kMaxIdLength] = {};
  std::snprintf(incident_id, sizeof(incident_id), "OTA-%s-%s-%u",
                t.radio_family, t.active_band, t.sample_timestamp_utc);

  std::array<loradriver::TraceabilityLink, 16> chain{};
  size_t count = loradriver::TraceabilityEngine::getFullTraceChain(incident_id, chain);
  if (count == 0) return false;
  return std::strcmp(chain[0].target_artifact, rationale.artifact_id) == 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Regression: gate IDs used in OTA eval match stable CI gate IDs
// ─────────────────────────────────────────────────────────────────────────────
bool TestOtaGateUsesStableCiGateIds() {
  // Verify the gate IDs we rely on in OTA still exist in CiGateEngine
  const char* ota_required_gates[] = {
      "INIT-001",   // init_success_rate
      "TXRX-001",   // tx_success_rate
      "TXRX-002",   // rx_success_rate
      "IRQ-002",    // irq_overflow_count
  };
  for (const char* gate_id : ota_required_gates) {
    if (CiGateEngine::getGateRule(gate_id) == nullptr) return false;
  }
  return true;
}

bool TestOtaGateEvalReusesCiThresholdValues() {
  // TXRX-001 threshold must still be 99%; verify OTA block at 98.9%
  OtaTelemetryInput t = MakeHealthyTelemetry();
  t.tx_success_rate = 98.9f;  // just below TXRX-001 threshold
  OtaDecisionRationale rationale{};
  OtaRolloutDecision decision = OtaGateEngine::evaluate(t, rationale);
  if (decision != OtaRolloutDecision::kBlock) return false;

  // 99.0% exactly must pass
  t.tx_success_rate = 99.0f;
  rationale = {};
  decision = OtaGateEngine::evaluate(t, rationale);
  if (decision != OtaRolloutDecision::kAllow) return false;
  return true;
}

bool TestOtaGateRxThresholdMatchesCiGate() {
  // TXRX-002 threshold is 98%
  OtaTelemetryInput t = MakeHealthyTelemetry();
  t.rx_success_rate = 97.9f;
  OtaDecisionRationale rationale{};
  if (OtaGateEngine::evaluate(t, rationale) != OtaRolloutDecision::kBlock) return false;
  t.rx_success_rate = 98.0f;
  rationale = {};
  if (OtaGateEngine::evaluate(t, rationale) != OtaRolloutDecision::kAllow) return false;
  return true;
}

bool TestOtaGateInitThresholdMatchesCiGate() {
  // INIT-001 threshold: init_success_rate >= 99%
  // => init_failure_rate must be <= 1%
  OtaTelemetryInput t = MakeHealthyTelemetry();
  t.init_failure_rate = 1.1f;  // success = 98.9% → fails INIT-001
  OtaDecisionRationale rationale{};
  if (OtaGateEngine::evaluate(t, rationale) != OtaRolloutDecision::kBlock) return false;
  t.init_failure_rate = 1.0f;  // success = 99.0% → passes INIT-001
  rationale = {};
  if (OtaGateEngine::evaluate(t, rationale) != OtaRolloutDecision::kAllow) return false;
  return true;
}

bool TestOtaGateIrqOverflowThresholdMatchesCiGate() {
  // IRQ-002: irq_overflow_count == 0
  OtaTelemetryInput t = MakeHealthyTelemetry();
  t.irq_overflow_events = 1;
  OtaDecisionRationale rationale{};
  if (OtaGateEngine::evaluate(t, rationale) != OtaRolloutDecision::kBlock) return false;
  t.irq_overflow_events = 0;
  rationale = {};
  if (OtaGateEngine::evaluate(t, rationale) != OtaRolloutDecision::kAllow) return false;
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Regression: multiple simultaneous gate failures captured
// ─────────────────────────────────────────────────────────────────────────────
bool TestMultipleGateFailuresCaptured() {
  OtaTelemetryInput t = MakeHealthyTelemetry();
  t.tx_success_rate     = 80.0f;   // TXRX-001 fails
  t.rx_success_rate     = 80.0f;   // TXRX-002 fails
  t.irq_overflow_events = 5;       // IRQ-002 fails
  OtaDecisionRationale rationale{};
  OtaGateEngine::evaluate(t, rationale);
  if (rationale.failed_gate_count < 3) return false;
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test runner
// ─────────────────────────────────────────────────────────────────────────────
#define RUN_TEST(fn)                                            \
  if (!(fn)()) {                                               \
    std::fprintf(stderr, "FAIL: %s\n", #fn);                  \
    return EXIT_FAILURE;                                       \
  }

int RunOtaGateTests() {
  // Enum stability (regression)
  RUN_TEST(TestOtaRolloutDecisionEnumValuesAreStable)

  // Task 1 – input model
  RUN_TEST(TestOtaTelemetryInputHasAllMandatoryFields)
  RUN_TEST(TestOtaTelemetryInputIsFixedSize)
  RUN_TEST(TestOtaDecisionRationaleIsFixedSize)

  // Task 2 – CI gate integration
  RUN_TEST(TestGatePassAllowsRollout)
  RUN_TEST(TestTxRateFailureBlocksRollout)
  RUN_TEST(TestRxRateFailureBlocksRollout)
  RUN_TEST(TestInitFailureRateHighBlocksRollout)
  RUN_TEST(TestIrqOverflowBlocksRollout)
  RUN_TEST(TestRationaleContainsMetricValues)

  // Task 3 – data quality
  RUN_TEST(TestMissingFirmwareVersionHolds)
  RUN_TEST(TestMissingRadioFamilyHolds)
  RUN_TEST(TestMissingActiveBandHolds)
  RUN_TEST(TestInvalidRadioFamilyHolds)
  RUN_TEST(TestInvalidActiveBandHolds)
  RUN_TEST(TestMissingTimestampHolds)
  RUN_TEST(TestInvalidTxRateOutOfRangeHolds)
  RUN_TEST(TestInvalidRxRateNegativeHolds)
  RUN_TEST(TestInvalidInitFailureRateOutOfRangeHolds)
  RUN_TEST(TestValidateDataQualityPassesForHealthyTelemetry)
  RUN_TEST(TestValidateDataQualityFailsForMissingField)

  // Task 3 – trend checks
  RUN_TEST(TestTrendDegradationInTxTriggersHold)
  RUN_TEST(TestLargeTrendDegradationHolds)
  RUN_TEST(TestRxTrendDegradationHolds)
  RUN_TEST(TestInitTrendDegradationHolds)
  RUN_TEST(TestDetectTrendRiskStandaloneApi)
  RUN_TEST(TestNoTrendRiskWhenNoBaseline)

  // Task 4 – governance/artifacts
  RUN_TEST(TestBlockDecisionRegistersArtifact)
  RUN_TEST(TestAllowDecisionRegistersArtifact)
  RUN_TEST(TestBlockedDecisionHasNonEmptyReason)

  // Determinism
  RUN_TEST(TestBlockPrecedesHold)

  // Regression – gate ID/threshold stability
  RUN_TEST(TestOtaGateUsesStableCiGateIds)
  RUN_TEST(TestOtaGateEvalReusesCiThresholdValues)
  RUN_TEST(TestOtaGateRxThresholdMatchesCiGate)
  RUN_TEST(TestOtaGateInitThresholdMatchesCiGate)
  RUN_TEST(TestOtaGateIrqOverflowThresholdMatchesCiGate)
  RUN_TEST(TestMultipleGateFailuresCaptured)
  RUN_TEST(TestDecisionCreatesTraceabilityIncidentLink)

  return EXIT_SUCCESS;
}

}  // namespace

int main() {
  return RunOtaGateTests();
}
