#include "loradriver/ota_gate.hpp"
#include "loradriver/artifact_governance.hpp"
#include "loradriver/ci_gates.hpp"
#include "loradriver/versioning.hpp"

#include <cstdio>
#include <cstring>

namespace loradriver {

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace {

bool IsRateInRange(float value) noexcept {
  return value >= 0.0f && value <= 100.0f;
}

bool IsSupportedRadioFamily(const char* value) noexcept {
  return value != nullptr &&
         (std::strcmp(value, "SX1276") == 0 || std::strcmp(value, "SX1278") == 0);
}

bool IsSupportedBand(const char* value) noexcept {
  return value != nullptr &&
         (std::strcmp(value, "433") == 0 || std::strcmp(value, "868") == 0);
}

}  // namespace

// ─────────────────────────────────────────────────────────────────────────────
// OtaGateEngine::validateDataQuality
// ─────────────────────────────────────────────────────────────────────────────
bool OtaGateEngine::validateDataQuality(const OtaTelemetryInput& telemetry,
                                        char*  out_reason,
                                        size_t reason_size) noexcept {
  if (out_reason == nullptr || reason_size == 0) {
    return false;
  }

  if (telemetry.firmware_version[0] == '\0') {
    std::strncpy(out_reason, "missing firmware_version", reason_size - 1);
    out_reason[reason_size - 1] = '\0';
    return false;
  }
  if (telemetry.radio_family[0] == '\0') {
    std::strncpy(out_reason, "missing radio_family", reason_size - 1);
    out_reason[reason_size - 1] = '\0';
    return false;
  }
  if (!IsSupportedRadioFamily(telemetry.radio_family)) {
    std::snprintf(out_reason, reason_size,
                  "invalid radio_family '%s' (expected SX1276/SX1278)",
                  telemetry.radio_family);
    return false;
  }
  if (telemetry.active_band[0] == '\0') {
    std::strncpy(out_reason, "missing active_band", reason_size - 1);
    out_reason[reason_size - 1] = '\0';
    return false;
  }
  if (!IsSupportedBand(telemetry.active_band)) {
    std::snprintf(out_reason, reason_size,
                  "invalid active_band '%s' (expected 433/868)",
                  telemetry.active_band);
    return false;
  }
  if (telemetry.sample_timestamp_utc == 0) {
    std::strncpy(out_reason, "missing sample_timestamp_utc (stale or uninitialised)", reason_size - 1);
    out_reason[reason_size - 1] = '\0';
    return false;
  }
  if (!IsRateInRange(telemetry.init_failure_rate)) {
    std::snprintf(out_reason, reason_size,
                  "init_failure_rate %.2f out of range [0,100]", telemetry.init_failure_rate);
    return false;
  }
  if (!IsRateInRange(telemetry.tx_success_rate)) {
    std::snprintf(out_reason, reason_size,
                  "tx_success_rate %.2f out of range [0,100]", telemetry.tx_success_rate);
    return false;
  }
  if (!IsRateInRange(telemetry.rx_success_rate)) {
    std::snprintf(out_reason, reason_size,
                  "rx_success_rate %.2f out of range [0,100]", telemetry.rx_success_rate);
    return false;
  }

  out_reason[0] = '\0';
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// OtaGateEngine::detectTrendRisk
// ─────────────────────────────────────────────────────────────────────────────
bool OtaGateEngine::detectTrendRisk(const OtaTelemetryInput& telemetry,
                                    char*  out_reason,
                                    size_t reason_size) noexcept {
  if (out_reason == nullptr || reason_size == 0) {
    return false;
  }
  out_reason[0] = '\0';

  const float threshold = OtaTelemetryInput::kTrendDegradationThreshold;

  // TX success rate trend
  if (telemetry.baseline_tx_success_rate > 0.0f) {
    const float drop = telemetry.baseline_tx_success_rate - telemetry.tx_success_rate;
    if (drop > threshold) {
      std::snprintf(out_reason, reason_size,
                    "tx_success_rate degraded %.2fpp from baseline %.2f to %.2f (threshold %.2f)",
                    drop, telemetry.baseline_tx_success_rate, telemetry.tx_success_rate, threshold);
      return true;
    }
  }

  // RX success rate trend
  if (telemetry.baseline_rx_success_rate > 0.0f) {
    const float drop = telemetry.baseline_rx_success_rate - telemetry.rx_success_rate;
    if (drop > threshold) {
      std::snprintf(out_reason, reason_size,
                    "rx_success_rate degraded %.2fpp from baseline %.2f to %.2f (threshold %.2f)",
                    drop, telemetry.baseline_rx_success_rate, telemetry.rx_success_rate, threshold);
      return true;
    }
  }

  // Init failure rate trend (increase in failure rate = degradation)
  if (telemetry.baseline_init_failure_rate > 0.0f) {
    const float increase = telemetry.init_failure_rate - telemetry.baseline_init_failure_rate;
    if (increase > threshold) {
      std::snprintf(out_reason, reason_size,
                    "init_failure_rate increased %.2fpp from baseline %.2f to %.2f (threshold %.2f)",
                    increase, telemetry.baseline_init_failure_rate,
                    telemetry.init_failure_rate, threshold);
      return true;
    }
  }

  return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// OtaGateEngine::evaluateCiGate  (private helper)
// ─────────────────────────────────────────────────────────────────────────────
void OtaGateEngine::evaluateCiGate(const char*           gate_id,
                                   float                  actual_value,
                                   OtaDecisionRationale&  out_rationale) noexcept {
  const GateRule* rule = CiGateEngine::getGateRule(gate_id);
  if (rule == nullptr) {
    return;
  }

  const GateResult result = CiGateEngine::evaluateGate(*rule, actual_value);
  if (result == GateResult::kFail && rule->isBlocking()) {
    const uint8_t idx = out_rationale.failed_gate_count;
    if (idx < OtaDecisionRationale::kMaxFailedGates) {
      std::strncpy(out_rationale.failed_gate_ids[idx], gate_id, GateRule::kMaxIdLength - 1);
      out_rationale.failed_gate_ids[idx][GateRule::kMaxIdLength - 1] = '\0';
      out_rationale.failed_gate_actuals[idx]    = actual_value;
      out_rationale.failed_gate_thresholds[idx] = rule->threshold.value;
      out_rationale.failed_gate_count++;
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// OtaGateEngine::registerTelemetryArtifact  (private helper)
// ─────────────────────────────────────────────────────────────────────────────
void OtaGateEngine::registerTelemetryArtifact(const OtaTelemetryInput& telemetry,
                                                OtaDecisionRationale&    out_rationale) noexcept {
  ArtifactMetadata metadata{};
  metadata.type              = ArtifactType::kTelemetryBaseline;
  metadata.created_timestamp = telemetry.sample_timestamp_utc;
  metadata.retention_days    = 180;  // telemetry baselines: 180-day retention
  std::strncpy(metadata.linked_version, telemetry.firmware_version,
               sizeof(metadata.linked_version) - 1);
  metadata.linked_version[sizeof(metadata.linked_version) - 1] = '\0';
  std::snprintf(metadata.source_module, sizeof(metadata.source_module),
                "OtaGate:%s:%s", telemetry.radio_family, telemetry.active_band);

  const char* registered_id = ArtifactRegistry::registerArtifact(metadata);
  if (registered_id != nullptr) {
    std::strncpy(out_rationale.artifact_id, registered_id,
                 OtaDecisionRationale::kMaxArtifactIdLength - 1);
    out_rationale.artifact_id[OtaDecisionRationale::kMaxArtifactIdLength - 1] = '\0';

    char incident_id[TraceabilityLink::kMaxIdLength] = {};
    std::snprintf(incident_id, sizeof(incident_id), "OTA-%s-%s-%u",
                  telemetry.radio_family,
                  telemetry.active_band,
                  telemetry.sample_timestamp_utc);
    (void)TraceabilityEngine::linkIncidentToArtifact(incident_id, registered_id);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// OtaGateEngine::evaluate  (main entry point)
// ─────────────────────────────────────────────────────────────────────────────
OtaRolloutDecision OtaGateEngine::evaluate(const OtaTelemetryInput& telemetry,
                                            OtaDecisionRationale&    out_rationale) noexcept {
  // Zero-initialise rationale
  out_rationale = {};

  // ── Step 1: Data quality check ───────────────────────────────────────────
  char quality_reason[OtaDecisionRationale::kMaxReasonLength] = {};
  if (!validateDataQuality(telemetry, quality_reason, sizeof(quality_reason))) {
    out_rationale.quality_issue = true;
    out_rationale.stale_data    = (telemetry.sample_timestamp_utc == 0);
    std::strncpy(out_rationale.reason, quality_reason, sizeof(out_rationale.reason) - 1);
    out_rationale.reason[sizeof(out_rationale.reason) - 1] = '\0';
    registerTelemetryArtifact(telemetry, out_rationale);
    return OtaRolloutDecision::kHold;
  }

  // ── Step 2: CI gate evaluation (maps OTA telemetry → CI gate metrics) ───
  // INIT-001: init_success_rate >= 99%  (init_success = 100 - init_failure)
  const float init_success_rate = 100.0f - telemetry.init_failure_rate;
  evaluateCiGate("INIT-001", init_success_rate, out_rationale);

  // TXRX-001: tx_success_rate >= 99%
  evaluateCiGate("TXRX-001", telemetry.tx_success_rate, out_rationale);

  // TXRX-002: rx_success_rate >= 98%
  evaluateCiGate("TXRX-002", telemetry.rx_success_rate, out_rationale);

  // IRQ-002: irq_overflow_count == 0
  evaluateCiGate("IRQ-002", static_cast<float>(telemetry.irq_overflow_events), out_rationale);

  if (out_rationale.failed_gate_count > 0) {
    // Build human-readable reason from first failed gate
    std::snprintf(out_rationale.reason, sizeof(out_rationale.reason),
                  "blocking gate %s failed: actual=%.2f threshold=%.2f",
                  out_rationale.failed_gate_ids[0],
                  out_rationale.failed_gate_actuals[0],
                  out_rationale.failed_gate_thresholds[0]);
    registerTelemetryArtifact(telemetry, out_rationale);
    return OtaRolloutDecision::kBlock;
  }

  // ── Step 3: KPI trend check ──────────────────────────────────────────────
  char trend_reason[OtaDecisionRationale::kMaxReasonLength] = {};
  if (detectTrendRisk(telemetry, trend_reason, sizeof(trend_reason))) {
    out_rationale.trend_risk = true;
    std::strncpy(out_rationale.reason, trend_reason, sizeof(out_rationale.reason) - 1);
    out_rationale.reason[sizeof(out_rationale.reason) - 1] = '\0';
    registerTelemetryArtifact(telemetry, out_rationale);
    return OtaRolloutDecision::kHold;
  }

  // ── Step 4: All checks passed ────────────────────────────────────────────
  std::strncpy(out_rationale.reason, "all gates passed; no trend risk",
               sizeof(out_rationale.reason) - 1);
  out_rationale.reason[sizeof(out_rationale.reason) - 1] = '\0';
  registerTelemetryArtifact(telemetry, out_rationale);
  return OtaRolloutDecision::kAllow;
}

}  // namespace loradriver
