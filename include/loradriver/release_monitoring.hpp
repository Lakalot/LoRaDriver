#pragma once

#include <array>
#include <cstdint>
#include <type_traits>

#include <loradriver/artifact_governance.hpp>
#include <loradriver/lora_error.hpp>
#include <loradriver/ota_gate.hpp>

namespace loradriver {

// ─────────────────────────────────────────────────────────────────────────────
// Incident classification — machine-readable, serialization-stable
// ─────────────────────────────────────────────────────────────────────────────
enum class IncidentCategory : uint8_t {
  kRadioInit   = 0,  ///< Radio initialization failure
  kTxFailure   = 1,  ///< TX failure event
  kRxFailure   = 2,  ///< RX failure event
  kIrqOverflow = 3,  ///< IRQ overflow
  kTimeout     = 4,  ///< Timeout event
  kOther       = 5   ///< Uncategorized
};

enum class IncidentSeverity : uint8_t {
  kCritical = 0,  ///< Blocking-level incident; triggers rollout block policy
  kHigh     = 1,  ///< Significant impact; monitored but not auto-blocking
  kMedium   = 2,  ///< Moderate impact
  kLow      = 3   ///< Minor impact
};

// ─────────────────────────────────────────────────────────────────────────────
// Block decision — source and outcome values (AC: 2)
// ─────────────────────────────────────────────────────────────────────────────
enum class BlockDecisionSource : uint8_t {
  kAutoPolicy     = 0,  ///< Triggered by automated policy engine
  kOperatorPolicy = 1   ///< Operator-confirmed explicit block decision
};

enum class RolloutBlockOutcome : uint8_t {
  kContinue = 0,  ///< All checks clear; rollout may expand
  kHold     = 1,  ///< Data quality or missing context issue; manual escalation
  kBlock    = 2   ///< Incident trend or KPI threshold exceeded; expansion denied
};

// ─────────────────────────────────────────────────────────────────────────────
// Monitoring window — immutable release/profile context + UTC bounds (AC: 1, 3)
// Fixed-size, no heap allocation
// ─────────────────────────────────────────────────────────────────────────────
struct MonitoringWindow {
  static constexpr size_t kMaxVersionLength   = 16;  ///< SemVer string capacity
  static constexpr size_t kMaxFamilyLength    = 16;  ///< e.g. "SX1276"
  static constexpr size_t kMaxBandLength      = 8;   ///< e.g. "433" or "868"
  static constexpr size_t kMaxProfileIdLength = 16;  ///< Profile identifier

  char     release_version[kMaxVersionLength];  ///< Release version under monitoring
  uint32_t window_start_utc;                    ///< Window open time (UTC epoch; 0 = invalid)
  uint32_t window_end_utc;                      ///< Window close time (0 = open-ended)
  char     radio_family[kMaxFamilyLength];       ///< "SX1276" or "SX1278" (V1 only)
  char     active_band[kMaxBandLength];          ///< "433" or "868" (V1 only)
  char     profile_id[kMaxProfileIdLength];      ///< Profile identifier for this window
};

static_assert(std::is_trivially_copyable<MonitoringWindow>::value,
              "MonitoringWindow must be trivially copyable (no heap)");

// ─────────────────────────────────────────────────────────────────────────────
// Normalized critical incident record (AC: 1)
// Stable for trend aggregation and governance reporting
// ─────────────────────────────────────────────────────────────────────────────
struct IncidentRecord {
  static constexpr size_t kMaxIdLength         = 32;
  static constexpr size_t kMaxArtifactIdLength = ArtifactMetadata::kMaxIdLength;

  char             incident_id[kMaxIdLength];            ///< Unique incident identifier
  IncidentCategory category;                             ///< Incident category
  IncidentSeverity severity;                             ///< Incident severity
  uint32_t         first_seen_utc;                       ///< First occurrence UTC epoch
  uint32_t         count;                                ///< Occurrence count for this record
  char             linked_artifact_id[kMaxArtifactIdLength];  ///< Linked artifact (optional)
};

static_assert(std::is_trivially_copyable<IncidentRecord>::value,
              "IncidentRecord must be trivially copyable (no heap)");

// ─────────────────────────────────────────────────────────────────────────────
// Bounded aggregate counters for rolling trend snapshots (AC: 1, 3)
// Reset/closure semantics: cleared when window opens; snapshot on each update
// ─────────────────────────────────────────────────────────────────────────────
struct MonitoringTrendLedger {
  uint32_t critical_count;        ///< Count of kCritical incidents recorded
  uint32_t high_count;            ///< Count of kHigh incidents recorded
  uint32_t medium_count;          ///< Count of kMedium incidents recorded
  uint32_t low_count;             ///< Count of kLow incidents recorded
  uint32_t total_incident_count;  ///< Sum of all severity counts
  uint32_t snapshot_timestamp;    ///< UTC timestamp of last update (0 = no updates)

  /// Get incident count for a specific severity level.
  [[nodiscard]] constexpr uint32_t getCountBySeverity(IncidentSeverity s) const noexcept {
    switch (s) {
      case IncidentSeverity::kCritical: return critical_count;
      case IncidentSeverity::kHigh:     return high_count;
      case IncidentSeverity::kMedium:   return medium_count;
      case IncidentSeverity::kLow:      return low_count;
    }
    return 0;
  }
};

static_assert(std::is_trivially_copyable<MonitoringTrendLedger>::value,
              "MonitoringTrendLedger must be trivially copyable (no heap)");

// ─────────────────────────────────────────────────────────────────────────────
// Block decision rationale with evidence fields (AC: 2)
// Identical schema for auto-policy and operator-confirmed decisions
// ─────────────────────────────────────────────────────────────────────────────
struct BlockDecisionRationale {
  static constexpr size_t kMaxFailedRules      = 8;
  static constexpr size_t kMaxRuleIdLength     = 16;
  static constexpr size_t kMaxReasonLength     = 128;
  static constexpr size_t kMaxArtifactIdLength = ArtifactMetadata::kMaxIdLength;

  // Failed rule evidence (rule IDs + measured/threshold pairs)
  char    failed_rule_ids[kMaxFailedRules][kMaxRuleIdLength];
  float   metric_values[kMaxFailedRules];    ///< Actual metric values at time of decision
  float   thresholds[kMaxFailedRules];       ///< Threshold values that were violated
  uint8_t failed_rule_count;

  BlockDecisionSource decision_source;  ///< Who/what made this decision
  RolloutBlockOutcome outcome;          ///< Decision outcome

  // Artifact ID registered with ArtifactRegistry for audit trail (populated on kBlock)
  char artifact_id[kMaxArtifactIdLength];

  // Human-readable decision summary
  char reason[kMaxReasonLength];

  // Decision flags — context for escalation
  bool stale_context;  ///< kHold caused by missing/invalid window context
  bool trend_breach;   ///< kBlock caused by incident trend threshold exceeded
  bool kpi_breach;     ///< kBlock contributed by OTA KPI gate failure
};

static_assert(std::is_trivially_copyable<BlockDecisionRationale>::value,
              "BlockDecisionRationale must be trivially copyable (no heap)");

// ─────────────────────────────────────────────────────────────────────────────
// Objective comparison for post-window review (AC: 3)
// Machine-readable: target vs actual with delta and met status
// ─────────────────────────────────────────────────────────────────────────────
struct ObjectiveComparison {
  static constexpr size_t kMaxMetricNameLength = 32;

  char  metric_name[kMaxMetricNameLength];  ///< Metric being compared
  float target;                             ///< Target objective value
  float actual;                             ///< Observed value at window closure
  float delta;                              ///< actual - target (positive = exceeded target)
  bool  met;                                ///< true if actual satisfies target
};

static_assert(std::is_trivially_copyable<ObjectiveComparison>::value,
              "ObjectiveComparison must be trivially copyable (no heap)");

// ─────────────────────────────────────────────────────────────────────────────
// Window review output — produced at window closure (AC: 3)
// Traceable to stored artifacts; compatible with retention policy
// ─────────────────────────────────────────────────────────────────────────────
struct WindowReviewOutput {
  static constexpr size_t kMaxComparisons      = 8;
  static constexpr size_t kMaxCalibrationNotes = 4;
  static constexpr size_t kMaxNoteLength       = 128;
  static constexpr size_t kMaxArtifactIdLength = ArtifactMetadata::kMaxIdLength;
  static constexpr size_t kMaxLinkedArtifacts  = 8;

  // Objective comparisons (target vs actual, one per tracked metric)
  ObjectiveComparison comparisons[kMaxComparisons];
  uint8_t             comparison_count;

  // Machine-readable calibration recommendations (versioned, evidence-referenced)
  char    calibration_notes[kMaxCalibrationNotes][kMaxNoteLength];
  uint8_t calibration_note_count;

  // Traceability — registered artifact IDs for audit trail
  char    window_artifact_id[kMaxArtifactIdLength];                          ///< Window evidence artifact
  char    linked_artifact_ids[kMaxLinkedArtifacts][kMaxArtifactIdLength];    ///< Linked artifact chain
  uint8_t linked_artifact_count;

  // Final trend at closure and closure timestamp
  MonitoringTrendLedger final_trend;
  uint32_t              closure_timestamp;

  // Overall objective outcome
  bool objectives_met;  ///< true if ALL comparisons met their targets
};

static_assert(std::is_trivially_copyable<WindowReviewOutput>::value,
              "WindowReviewOutput must be trivially copyable (no heap)");

// ─────────────────────────────────────────────────────────────────────────────
// Release monitoring engine (AC: 1, 2, 3)
// All methods are allocation-free and noexcept
// Executes in non-ISR control flow only
// ─────────────────────────────────────────────────────────────────────────────
class ReleaseMonitoringEngine {
 public:
  static constexpr size_t   kMaxWindows                    = 4;
  static constexpr size_t   kMaxIncidents                  = 64;
  /// Any critical incident in a monitoring window triggers kBlock (AC: 2)
  static constexpr uint32_t kCriticalIncidentBlockThreshold = 0;

  /// Open a monitoring window and register a kIncidentEvidence artifact.
  /// Validates V1 profile/band compatibility before opening.
  ///
  /// @param window        Immutable window context (version, radio, band, profile, UTC bounds)
  /// @param out_window_id Buffer to receive the window identifier (= registered artifact ID)
  /// @param window_id_size  Size of out_window_id buffer (>= ArtifactMetadata::kMaxIdLength)
  /// @returns kOk on success; kInvalidConfig if context invalid;
  ///          kRegistryFull if window capacity reached;
  ///          kArtifactRegistrationFailed if artifact registry is full
  [[nodiscard]] static LoRaError openWindow(
      const MonitoringWindow& window,
      char*                   out_window_id,
      size_t                  window_id_size) noexcept;

  /// Record a critical incident in an open monitoring window.
  /// Updates bounded trend ledger counters; links incident to window artifact
  /// via TraceabilityEngine.
  ///
  /// @returns kOk on success; kInvalidConfig if window not found or closed;
  ///          kRegistryFull if window incident capacity reached
  [[nodiscard]] static LoRaError recordIncident(
      const char*           window_id,
      const IncidentRecord& incident) noexcept;

  /// Evaluate rollout block policy for a monitoring window.
  ///
  /// Decision precedence (deterministic):
  ///   1. Stale/missing context → kHold
  ///   2. Critical incident trend threshold exceeded → kBlock (RM-TREND-001)
  ///   3. OTA KPI gate failure (if telemetry provided) → kBlock (reuses CI gate IDs)
  ///   4. All clear → kContinue
  ///
  /// @param window_id    Open monitoring window to evaluate
  /// @param telemetry    Optional OTA telemetry snapshot; nullptr = incident-trend-only
  /// @param source       Decision source (auto-policy or operator-confirmed)
  /// @param out_rationale  Fully populated rationale with evidence fields
  [[nodiscard]] static RolloutBlockOutcome evaluateBlockPolicy(
      const char*               window_id,
      const OtaTelemetryInput*  telemetry,
      BlockDecisionSource       source,
      BlockDecisionRationale&   out_rationale) noexcept;

  /// Get current trend snapshot for a monitoring window.
  /// @returns true if window found and snapshot populated; false otherwise
  [[nodiscard]] static bool getTrendSnapshot(
      const char*            window_id,
      MonitoringTrendLedger& out_ledger) noexcept;

  /// Close a monitoring window and produce objective comparison + calibration output.
  /// Registers a kValidationReport artifact and links evidence via TraceabilityEngine
  /// and ArtifactRegistry.
  ///
  /// @returns kOk on success; kInvalidConfig if window not found or already closed;
  ///          kArtifactRegistrationFailed if review artifact cannot be registered
  [[nodiscard]] static LoRaError closeWindow(
      const char*         window_id,
      uint32_t            closure_timestamp,
      WindowReviewOutput& out_review) noexcept;

  /// Validate monitoring window context (V1 profile/band compatibility).
  /// Called implicitly by openWindow; also available for pre-validation.
  [[nodiscard]] static bool validateWindowContext(
      const MonitoringWindow& window,
      char*                   out_reason,
      size_t                  reason_size) noexcept;

  static void clear() noexcept;

 private:
  // Internal per-window state — fixed-size, no heap
  struct WindowState {
    char                  window_id[ArtifactMetadata::kMaxIdLength];
    MonitoringWindow      context;
    MonitoringTrendLedger trend;
    IncidentRecord        incidents[kMaxIncidents];
    uint32_t              incident_count;
    bool                  is_open;
  };

  static WindowState windows_[kMaxWindows];
  static size_t      window_count_;

  [[nodiscard]] static WindowState* findWindow(const char* window_id) noexcept;
};

}  // namespace loradriver
