#pragma once

#include <array>
#include <cstdint>
#include <type_traits>

#include <loradriver/artifact_governance.hpp>
#include <loradriver/ci_gates.hpp>

namespace loradriver {

// ─────────────────────────────────────────────────────────────────────────────
// OTA rollout decision values (machine-readable, serialization-stable)
// ─────────────────────────────────────────────────────────────────────────────
enum class OtaRolloutDecision : uint8_t {
  kAllow = 0,  ///< Metrics satisfy thresholds; no negative trend signal
  kHold  = 1,  ///< Data quality issue or risk trend; requires manual escalation
  kBlock = 2   ///< Blocking threshold violated; rollout expansion denied
};

// ─────────────────────────────────────────────────────────────────────────────
// Minimum telemetry input – fixed-size, no heap allocation (AC: 2)
// All fields must be populated; missing fields cause kHold.
// ─────────────────────────────────────────────────────────────────────────────
struct OtaTelemetryInput {
  static constexpr size_t kMaxVersionLength = 16;  ///< SemVer string capacity
  static constexpr size_t kMaxFamilyLength  = 16;  ///< e.g. "SX1276"
  static constexpr size_t kMaxBandLength    = 8;   ///< e.g. "433" or "868"

  /// Trend detection: if current metric drops from baseline by more than this
  /// many percentage points, kHold is triggered even if the gate still passes.
  static constexpr float kTrendDegradationThreshold = 2.0f;

  // Required string fields (empty → kHold)
  char firmware_version[kMaxVersionLength];  ///< SemVer-compatible firmware version
  char radio_family[kMaxFamilyLength];       ///< "SX1276" or "SX1278"
  char active_band[kMaxBandLength];          ///< "433" or "868"

  // Required numeric KPI fields
  float    init_failure_rate;       ///< Radio init failure rate [0.0 – 100.0] %
  uint32_t timeout_events;          ///< Timeout event count since last window
  uint32_t irq_overflow_events;     ///< IRQ overflow count (must be 0 to pass)
  float    tx_success_rate;         ///< TX success rate [0.0 – 100.0] %
  float    rx_success_rate;         ///< RX success rate [0.0 – 100.0] %
  uint32_t sample_timestamp_utc;    ///< UTC epoch timestamp (0 → kHold)

  // Optional baseline values for trend analysis (0 = no baseline, skip check)
  float baseline_tx_success_rate;    ///< Previous-wave TX success rate baseline
  float baseline_rx_success_rate;    ///< Previous-wave RX success rate baseline
  float baseline_init_failure_rate;  ///< Previous-wave init failure rate baseline
};

static_assert(std::is_trivially_copyable<OtaTelemetryInput>::value,
              "OtaTelemetryInput must be trivially copyable (no heap)");

// ─────────────────────────────────────────────────────────────────────────────
// Machine-readable decision rationale with evidence for escalation (AC: 3)
// ─────────────────────────────────────────────────────────────────────────────
struct OtaDecisionRationale {
  static constexpr size_t kMaxFailedGates      = 11;  ///< Max concurrent failures
  static constexpr size_t kMaxArtifactIdLength = ArtifactMetadata::kMaxIdLength;
  static constexpr size_t kMaxReasonLength     = 128;

  // Failed gate evidence (gate IDs + measured/threshold pairs)
  char    failed_gate_ids[kMaxFailedGates][GateRule::kMaxIdLength];
  float   failed_gate_actuals[kMaxFailedGates];     ///< Actual metric values
  float   failed_gate_thresholds[kMaxFailedGates];  ///< Threshold values violated
  uint8_t failed_gate_count;

  // Artifact ID registered with ArtifactRegistry for audit trail
  char artifact_id[kMaxArtifactIdLength];

  // Human-readable summary for escalation notification
  char reason[kMaxReasonLength];

  // Decision flags (mutually informative, not mutually exclusive)
  bool quality_issue;  ///< kHold caused by missing/invalid telemetry field
  bool trend_risk;     ///< kHold caused by KPI trend degradation signal
  bool stale_data;     ///< kHold caused by zero/missing timestamp
};

static_assert(std::is_trivially_copyable<OtaDecisionRationale>::value,
              "OtaDecisionRationale must be trivially copyable (no heap)");

// ─────────────────────────────────────────────────────────────────────────────
// OTA rollout gate engine – wraps CiGateEngine for field-telemetry decisions
// ─────────────────────────────────────────────────────────────────────────────
class OtaGateEngine {
 public:
  /// Evaluate full OTA rollout gate:
  ///   1. Data quality check (missing/invalid → kHold)
  ///   2. CI gate evaluation against radio KPI thresholds (failure → kBlock)
  ///   3. KPI trend check against baselines (degradation → kHold)
  ///   4. Register telemetry artifact for governance audit trail
  ///
  /// @param telemetry   Populated telemetry snapshot from deployment wave
  /// @param out_rationale  Machine-readable decision evidence (always filled)
  /// @returns           kAllow, kHold, or kBlock (deterministic)
  [[nodiscard]] static OtaRolloutDecision evaluate(
      const OtaTelemetryInput& telemetry,
      OtaDecisionRationale&    out_rationale) noexcept;

  /// Validate telemetry data quality (required-field presence, numeric ranges).
  /// @returns true if all quality checks pass; false + populated reason if not.
  [[nodiscard]] static bool validateDataQuality(
      const OtaTelemetryInput& telemetry,
      char*                    out_reason,
      size_t                   reason_size) noexcept;

  /// Check for KPI trend degradation relative to baselines.
  /// Returns false (no risk) when all baseline fields are 0.
  /// @returns true if trend degradation risk detected; reason explains which KPI.
  [[nodiscard]] static bool detectTrendRisk(
      const OtaTelemetryInput& telemetry,
      char*                    out_reason,
      size_t                   reason_size) noexcept;

 private:
  /// Register telemetry gate artifact and populate rationale.artifact_id.
  static void registerTelemetryArtifact(
      const OtaTelemetryInput& telemetry,
      OtaDecisionRationale&    out_rationale) noexcept;

  /// Evaluate a single CI gate metric and append failure to rationale.
  static void evaluateCiGate(
      const char*           gate_id,
      float                 actual_value,
      OtaDecisionRationale& out_rationale) noexcept;
};

}  // namespace loradriver
