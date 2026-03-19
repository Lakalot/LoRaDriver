#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "loradriver/release_monitoring.hpp"
#include "loradriver/ota_gate.hpp"
#include "loradriver/artifact_governance.hpp"
#include "loradriver/versioning.hpp"

namespace {

using loradriver::ArtifactRegistry;
using loradriver::ArtifactType;
using loradriver::BlockDecisionRationale;
using loradriver::BlockDecisionSource;
using loradriver::IncidentCategory;
using loradriver::IncidentRecord;
using loradriver::IncidentSeverity;
using loradriver::LoRaError;
using loradriver::MonitoringTrendLedger;
using loradriver::MonitoringWindow;
using loradriver::ObjectiveComparison;
using loradriver::OtaTelemetryInput;
using loradriver::ReleaseMonitoringEngine;
using loradriver::RolloutBlockOutcome;
using loradriver::TraceabilityEngine;
using loradriver::WindowReviewOutput;

// ─────────────────────────────────────────────────────────────────────────────
// Test helpers
// ─────────────────────────────────────────────────────────────────────────────

static void ClearAll() {
  ArtifactRegistry::clear();
  TraceabilityEngine::clear();
  ReleaseMonitoringEngine::clear();
}

static MonitoringWindow MakeValidWindow() {
  MonitoringWindow w{};
  std::strncpy(w.release_version, "1.2.0",   sizeof(w.release_version) - 1);
  std::strncpy(w.radio_family,    "SX1276",  sizeof(w.radio_family) - 1);
  std::strncpy(w.active_band,     "868",     sizeof(w.active_band) - 1);
  std::strncpy(w.profile_id,      "P1-868",  sizeof(w.profile_id) - 1);
  w.window_start_utc = 10000;
  w.window_end_utc   = 20000;
  return w;
}

static IncidentRecord MakeCriticalIncident(const char* id, uint32_t ts = 11000) {
  IncidentRecord rec{};
  std::strncpy(rec.incident_id, id, sizeof(rec.incident_id) - 1);
  rec.category      = IncidentCategory::kRadioInit;
  rec.severity      = IncidentSeverity::kCritical;
  rec.first_seen_utc = ts;
  rec.count         = 1;
  return rec;
}

static IncidentRecord MakeHighIncident(const char* id, uint32_t ts = 11000) {
  IncidentRecord rec{};
  std::strncpy(rec.incident_id, id, sizeof(rec.incident_id) - 1);
  rec.category       = IncidentCategory::kTxFailure;
  rec.severity       = IncidentSeverity::kHigh;
  rec.first_seen_utc = ts;
  rec.count          = 1;
  return rec;
}

static IncidentRecord MakeLowIncident(const char* id, uint32_t ts = 11000) {
  IncidentRecord rec{};
  std::strncpy(rec.incident_id, id, sizeof(rec.incident_id) - 1);
  rec.category       = IncidentCategory::kTimeout;
  rec.severity       = IncidentSeverity::kLow;
  rec.first_seen_utc = ts;
  rec.count          = 1;
  return rec;
}

// ─────────────────────────────────────────────────────────────────────────────
// Regression: enum values are stable (serialisation contract)
// ─────────────────────────────────────────────────────────────────────────────

bool TestIncidentCategoryEnumValuesAreStable() {
  if (static_cast<uint8_t>(IncidentCategory::kRadioInit)   != 0) return false;
  if (static_cast<uint8_t>(IncidentCategory::kTxFailure)   != 1) return false;
  if (static_cast<uint8_t>(IncidentCategory::kRxFailure)   != 2) return false;
  if (static_cast<uint8_t>(IncidentCategory::kIrqOverflow) != 3) return false;
  if (static_cast<uint8_t>(IncidentCategory::kTimeout)     != 4) return false;
  if (static_cast<uint8_t>(IncidentCategory::kOther)       != 5) return false;
  return true;
}

bool TestIncidentSeverityEnumValuesAreStable() {
  if (static_cast<uint8_t>(IncidentSeverity::kCritical) != 0) return false;
  if (static_cast<uint8_t>(IncidentSeverity::kHigh)     != 1) return false;
  if (static_cast<uint8_t>(IncidentSeverity::kMedium)   != 2) return false;
  if (static_cast<uint8_t>(IncidentSeverity::kLow)      != 3) return false;
  return true;
}

bool TestBlockDecisionSourceEnumValuesAreStable() {
  if (static_cast<uint8_t>(BlockDecisionSource::kAutoPolicy)     != 0) return false;
  if (static_cast<uint8_t>(BlockDecisionSource::kOperatorPolicy) != 1) return false;
  return true;
}

bool TestRolloutBlockOutcomeEnumValuesAreStable() {
  if (static_cast<uint8_t>(RolloutBlockOutcome::kContinue) != 0) return false;
  if (static_cast<uint8_t>(RolloutBlockOutcome::kHold)     != 1) return false;
  if (static_cast<uint8_t>(RolloutBlockOutcome::kBlock)    != 2) return false;
  return true;
}

bool TestMonitoringWindowIsTriviallyCopyable() {
  static_assert(std::is_trivially_copyable<MonitoringWindow>::value,
                "MonitoringWindow must be trivially copyable");
  return true;
}

bool TestIncidentRecordIsTriviallyCopyable() {
  static_assert(std::is_trivially_copyable<IncidentRecord>::value,
                "IncidentRecord must be trivially copyable");
  return true;
}

bool TestMonitoringTrendLedgerIsTriviallyCopyable() {
  static_assert(std::is_trivially_copyable<MonitoringTrendLedger>::value,
                "MonitoringTrendLedger must be trivially copyable");
  return true;
}

bool TestBlockDecisionRationaleIsTriviallyCopyable() {
  static_assert(std::is_trivially_copyable<BlockDecisionRationale>::value,
                "BlockDecisionRationale must be trivially copyable");
  return true;
}

bool TestWindowReviewOutputIsTriviallyCopyable() {
  static_assert(std::is_trivially_copyable<WindowReviewOutput>::value,
                "WindowReviewOutput must be trivially copyable");
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 1: validateWindowContext — V1 profile/band compatibility
// ─────────────────────────────────────────────────────────────────────────────

bool TestValidateWindowContextPassesForValidWindow() {
  MonitoringWindow w = MakeValidWindow();
  char reason[128] = {};
  return ReleaseMonitoringEngine::validateWindowContext(w, reason, sizeof(reason));
}

bool TestValidateWindowContextFailsForMissingVersion() {
  MonitoringWindow w = MakeValidWindow();
  w.release_version[0] = '\0';
  char reason[128] = {};
  bool ok = ReleaseMonitoringEngine::validateWindowContext(w, reason, sizeof(reason));
  if (ok) return false;
  return reason[0] != '\0';
}

bool TestValidateWindowContextFailsForMissingRadioFamily() {
  MonitoringWindow w = MakeValidWindow();
  w.radio_family[0] = '\0';
  char reason[128] = {};
  bool ok = ReleaseMonitoringEngine::validateWindowContext(w, reason, sizeof(reason));
  if (ok) return false;
  return reason[0] != '\0';
}

bool TestValidateWindowContextFailsForInvalidRadioFamily() {
  MonitoringWindow w = MakeValidWindow();
  std::strncpy(w.radio_family, "SX1262", sizeof(w.radio_family) - 1);
  char reason[128] = {};
  bool ok = ReleaseMonitoringEngine::validateWindowContext(w, reason, sizeof(reason));
  if (ok) return false;
  return reason[0] != '\0';
}

bool TestValidateWindowContextAcceptsSX1278() {
  MonitoringWindow w = MakeValidWindow();
  std::strncpy(w.radio_family, "SX1278", sizeof(w.radio_family) - 1);
  char reason[128] = {};
  return ReleaseMonitoringEngine::validateWindowContext(w, reason, sizeof(reason));
}

bool TestValidateWindowContextFailsForMissingActiveBand() {
  MonitoringWindow w = MakeValidWindow();
  w.active_band[0] = '\0';
  char reason[128] = {};
  bool ok = ReleaseMonitoringEngine::validateWindowContext(w, reason, sizeof(reason));
  if (ok) return false;
  return reason[0] != '\0';
}

bool TestValidateWindowContextFailsForMissingProfileId() {
  MonitoringWindow w = MakeValidWindow();
  w.profile_id[0] = '\0';
  char reason[128] = {};
  bool ok = ReleaseMonitoringEngine::validateWindowContext(w, reason, sizeof(reason));
  if (ok) return false;
  return reason[0] != '\0';
}

bool TestValidateWindowContextFailsForInvalidActiveBand() {
  MonitoringWindow w = MakeValidWindow();
  std::strncpy(w.active_band, "915", sizeof(w.active_band) - 1);
  char reason[128] = {};
  bool ok = ReleaseMonitoringEngine::validateWindowContext(w, reason, sizeof(reason));
  if (ok) return false;
  return reason[0] != '\0';
}

bool TestValidateWindowContextAccepts433Band() {
  MonitoringWindow w = MakeValidWindow();
  std::strncpy(w.active_band, "433", sizeof(w.active_band) - 1);
  char reason[128] = {};
  return ReleaseMonitoringEngine::validateWindowContext(w, reason, sizeof(reason));
}

bool TestValidateWindowContextFailsForZeroStartTimestamp() {
  MonitoringWindow w = MakeValidWindow();
  w.window_start_utc = 0;
  char reason[128] = {};
  bool ok = ReleaseMonitoringEngine::validateWindowContext(w, reason, sizeof(reason));
  if (ok) return false;
  return reason[0] != '\0';
}

bool TestValidateWindowContextFailsForEndBeforeStart() {
  MonitoringWindow w = MakeValidWindow();
  w.window_end_utc = w.window_start_utc - 1;  // end before start
  char reason[128] = {};
  bool ok = ReleaseMonitoringEngine::validateWindowContext(w, reason, sizeof(reason));
  if (ok) return false;
  return reason[0] != '\0';
}

bool TestValidateWindowContextAcceptsOpenEndedWindow() {
  MonitoringWindow w = MakeValidWindow();
  w.window_end_utc = 0;  // open-ended
  char reason[128] = {};
  return ReleaseMonitoringEngine::validateWindowContext(w, reason, sizeof(reason));
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 1: openWindow — deterministic data model for monitoring windows
// ─────────────────────────────────────────────────────────────────────────────

bool TestOpenWindowReturnsOkForValidContext() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  return ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id)) == LoRaError::kOk;
}

bool TestOpenWindowPopulatesWindowId() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));
  return window_id[0] != '\0';
}

bool TestOpenWindowRegistersArtifactInRegistry() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));
  const auto* artifact = ArtifactRegistry::getArtifact(window_id);
  if (artifact == nullptr) return false;
  return artifact->type == ArtifactType::kIncidentEvidence;
}

bool TestOpenWindowArtifactLinkedVersionMatchesReleaseVersion() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));
  const auto* artifact = ArtifactRegistry::getArtifact(window_id);
  if (artifact == nullptr) return false;
  return std::strcmp(artifact->linked_version, w.release_version) == 0;
}

bool TestOpenWindowFailsForInvalidContext() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  w.radio_family[0] = '\0';  // missing radio family
  char window_id[32] = {};
  return ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id))
         == LoRaError::kInvalidConfig;
}

bool TestOpenWindowFailsForNullOutputBuffer() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  return ReleaseMonitoringEngine::openWindow(w, nullptr, 32) == LoRaError::kInvalidConfig;
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 1: recordIncident — normalized incident model and trend ledger
// ─────────────────────────────────────────────────────────────────────────────

bool TestRecordIncidentReturnsOkForValidWindow() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  IncidentRecord inc = MakeCriticalIncident("INC-001");
  return ReleaseMonitoringEngine::recordIncident(window_id, inc) == LoRaError::kOk;
}

bool TestRecordIncidentUpdatesCriticalCount() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  ReleaseMonitoringEngine::recordIncident(window_id, MakeCriticalIncident("INC-001"));
  ReleaseMonitoringEngine::recordIncident(window_id, MakeCriticalIncident("INC-002"));

  MonitoringTrendLedger ledger{};
  ReleaseMonitoringEngine::getTrendSnapshot(window_id, ledger);
  return ledger.critical_count == 2;
}

bool TestRecordIncidentUpdatesHighCount() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  ReleaseMonitoringEngine::recordIncident(window_id, MakeHighIncident("INC-H-001"));

  MonitoringTrendLedger ledger{};
  ReleaseMonitoringEngine::getTrendSnapshot(window_id, ledger);
  return ledger.high_count == 1;
}

bool TestRecordIncidentUpdatesTotalCount() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  ReleaseMonitoringEngine::recordIncident(window_id, MakeCriticalIncident("INC-001"));
  ReleaseMonitoringEngine::recordIncident(window_id, MakeHighIncident("INC-002"));
  ReleaseMonitoringEngine::recordIncident(window_id, MakeLowIncident("INC-003"));

  MonitoringTrendLedger ledger{};
  ReleaseMonitoringEngine::getTrendSnapshot(window_id, ledger);
  return ledger.total_incident_count == 3;
}

bool TestRecordIncidentAggregatesByIncidentCount() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  IncidentRecord batched = MakeCriticalIncident("INC-BATCH");
  batched.count = 3;
  ReleaseMonitoringEngine::recordIncident(window_id, batched);

  MonitoringTrendLedger ledger{};
  ReleaseMonitoringEngine::getTrendSnapshot(window_id, ledger);
  return ledger.critical_count == 3 && ledger.total_incident_count == 3;
}

bool TestRecordIncidentFailsForZeroCount() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  IncidentRecord inc = MakeCriticalIncident("INC-000");
  inc.count = 0;
  return ReleaseMonitoringEngine::recordIncident(window_id, inc) == LoRaError::kInvalidConfig;
}

bool TestRecordIncidentFailsForUnknownWindowId() {
  ClearAll();
  IncidentRecord inc = MakeCriticalIncident("INC-001");
  return ReleaseMonitoringEngine::recordIncident("NONEXISTENT-WIN", inc)
         == LoRaError::kInvalidConfig;
}

bool TestRecordIncidentLinksToWindowArtifactViaTraceability() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  IncidentRecord inc = MakeCriticalIncident("INC-TRACE-001");
  ReleaseMonitoringEngine::recordIncident(window_id, inc);

  std::array<loradriver::TraceabilityLink, 16> chain{};
  size_t count = TraceabilityEngine::getFullTraceChain("INC-TRACE-001", chain);
  if (count == 0) return false;
  return std::strcmp(chain[0].target_artifact, window_id) == 0;
}

bool TestRecordIncidentUpdatesSnapshotTimestamp() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  IncidentRecord inc = MakeCriticalIncident("INC-TS-001", 15000);
  ReleaseMonitoringEngine::recordIncident(window_id, inc);

  MonitoringTrendLedger ledger{};
  ReleaseMonitoringEngine::getTrendSnapshot(window_id, ledger);
  return ledger.snapshot_timestamp == 15000;
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 1: getTrendSnapshot
// ─────────────────────────────────────────────────────────────────────────────

bool TestGetTrendSnapshotReturnsFalseForUnknownWindow() {
  ClearAll();
  MonitoringTrendLedger ledger{};
  return !ReleaseMonitoringEngine::getTrendSnapshot("NONEXISTENT", ledger);
}

bool TestGetTrendSnapshotReturnsZeroCountsForFreshWindow() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  MonitoringTrendLedger ledger{};
  bool ok = ReleaseMonitoringEngine::getTrendSnapshot(window_id, ledger);
  if (!ok) return false;
  return ledger.critical_count == 0 && ledger.total_incident_count == 0;
}

bool TestTrendLedgerGetCountBySeverityMatchesCriticalCount() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  ReleaseMonitoringEngine::recordIncident(window_id, MakeCriticalIncident("INC-C1"));
  ReleaseMonitoringEngine::recordIncident(window_id, MakeCriticalIncident("INC-C2"));

  MonitoringTrendLedger ledger{};
  ReleaseMonitoringEngine::getTrendSnapshot(window_id, ledger);
  return ledger.getCountBySeverity(IncidentSeverity::kCritical) == 2;
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 2: evaluateBlockPolicy — happy path (low incident trend)
// AC: 2 — progressive rollout continues when KPIs and incidents are nominal
// ─────────────────────────────────────────────────────────────────────────────

bool TestEvaluateBlockPolicyContinuesWithNoIncidentsAndNoTelemetry() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  BlockDecisionRationale rationale{};
  RolloutBlockOutcome outcome = ReleaseMonitoringEngine::evaluateBlockPolicy(
      window_id, nullptr, BlockDecisionSource::kAutoPolicy, rationale);

  return outcome == RolloutBlockOutcome::kContinue;
}

bool TestEvaluateBlockPolicyContinueReasonFieldIsNonEmpty() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  BlockDecisionRationale rationale{};
  ReleaseMonitoringEngine::evaluateBlockPolicy(
      window_id, nullptr, BlockDecisionSource::kAutoPolicy, rationale);

  return rationale.reason[0] != '\0';
}

bool TestEvaluateBlockPolicyContinueHasCorrectSource() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  BlockDecisionRationale rationale{};
  ReleaseMonitoringEngine::evaluateBlockPolicy(
      window_id, nullptr, BlockDecisionSource::kOperatorPolicy, rationale);

  return rationale.decision_source == BlockDecisionSource::kOperatorPolicy;
}

bool TestEvaluateBlockPolicyContinueWithOnlyLowIncidents() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  // Low-severity incidents should not block
  ReleaseMonitoringEngine::recordIncident(window_id, MakeLowIncident("INC-L1"));
  ReleaseMonitoringEngine::recordIncident(window_id, MakeLowIncident("INC-L2"));

  BlockDecisionRationale rationale{};
  RolloutBlockOutcome outcome = ReleaseMonitoringEngine::evaluateBlockPolicy(
      window_id, nullptr, BlockDecisionSource::kAutoPolicy, rationale);

  return outcome == RolloutBlockOutcome::kContinue;
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 2: evaluateBlockPolicy — block path (incident trend threshold exceeded)
// ─────────────────────────────────────────────────────────────────────────────

bool TestEvaluateBlockPolicyBlocksOnCriticalIncident() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  ReleaseMonitoringEngine::recordIncident(window_id, MakeCriticalIncident("INC-C1"));

  BlockDecisionRationale rationale{};
  RolloutBlockOutcome outcome = ReleaseMonitoringEngine::evaluateBlockPolicy(
      window_id, nullptr, BlockDecisionSource::kAutoPolicy, rationale);

  return outcome == RolloutBlockOutcome::kBlock;
}

bool TestEvaluateBlockPolicyBlockPopulatesTrendBreach() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));
  ReleaseMonitoringEngine::recordIncident(window_id, MakeCriticalIncident("INC-C1"));

  BlockDecisionRationale rationale{};
  ReleaseMonitoringEngine::evaluateBlockPolicy(
      window_id, nullptr, BlockDecisionSource::kAutoPolicy, rationale);

  return rationale.trend_breach;
}

bool TestEvaluateBlockPolicyBlockPopulatesFailedRuleId() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));
  ReleaseMonitoringEngine::recordIncident(window_id, MakeCriticalIncident("INC-C1"));

  BlockDecisionRationale rationale{};
  ReleaseMonitoringEngine::evaluateBlockPolicy(
      window_id, nullptr, BlockDecisionSource::kAutoPolicy, rationale);

  if (rationale.failed_rule_count == 0) return false;
  return std::strcmp(rationale.failed_rule_ids[0], "RM-TREND-001") == 0;
}

bool TestEvaluateBlockPolicyBlockMetricAndThresholdArePopulated() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));
  ReleaseMonitoringEngine::recordIncident(window_id, MakeCriticalIncident("INC-C1"));
  ReleaseMonitoringEngine::recordIncident(window_id, MakeCriticalIncident("INC-C2"));

  BlockDecisionRationale rationale{};
  ReleaseMonitoringEngine::evaluateBlockPolicy(
      window_id, nullptr, BlockDecisionSource::kAutoPolicy, rationale);

  // metric_value = actual critical count; threshold = kCriticalIncidentBlockThreshold
  if (rationale.failed_rule_count == 0) return false;
  return rationale.metric_values[0] == 2.0f &&
         rationale.thresholds[0] == static_cast<float>(
             ReleaseMonitoringEngine::kCriticalIncidentBlockThreshold);
}

bool TestEvaluateBlockPolicyBlockRegistersGateReportArtifact() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));
  ReleaseMonitoringEngine::recordIncident(window_id, MakeCriticalIncident("INC-C1"));

  BlockDecisionRationale rationale{};
  ReleaseMonitoringEngine::evaluateBlockPolicy(
      window_id, nullptr, BlockDecisionSource::kAutoPolicy, rationale);

  if (rationale.artifact_id[0] == '\0') return false;
  const auto* artifact = ArtifactRegistry::getArtifact(rationale.artifact_id);
  if (artifact == nullptr) return false;
  return artifact->type == ArtifactType::kGateReport;
}

bool TestEvaluateBlockPolicyBlockPopulatesNonEmptyReason() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));
  ReleaseMonitoringEngine::recordIncident(window_id, MakeCriticalIncident("INC-C1"));

  BlockDecisionRationale rationale{};
  ReleaseMonitoringEngine::evaluateBlockPolicy(
      window_id, nullptr, BlockDecisionSource::kAutoPolicy, rationale);

  return rationale.reason[0] != '\0';
}

bool TestEvaluateBlockPolicyOperatorPolicyBlocksOnCritical() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));
  ReleaseMonitoringEngine::recordIncident(window_id, MakeCriticalIncident("INC-C1"));

  BlockDecisionRationale rationale{};
  RolloutBlockOutcome outcome = ReleaseMonitoringEngine::evaluateBlockPolicy(
      window_id, nullptr, BlockDecisionSource::kOperatorPolicy, rationale);

  return outcome == RolloutBlockOutcome::kBlock &&
         rationale.decision_source == BlockDecisionSource::kOperatorPolicy;
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 2: evaluateBlockPolicy — block path (KPI breach via OTA telemetry)
// ─────────────────────────────────────────────────────────────────────────────

static OtaTelemetryInput MakeHealthyTelemetry() {
  OtaTelemetryInput t{};
  std::strncpy(t.firmware_version, "1.2.0",  sizeof(t.firmware_version) - 1);
  std::strncpy(t.radio_family,     "SX1276", sizeof(t.radio_family) - 1);
  std::strncpy(t.active_band,      "868",    sizeof(t.active_band) - 1);
  t.init_failure_rate    = 0.1f;   // 99.9% success — passes INIT-001
  t.tx_success_rate      = 99.5f;  // passes TXRX-001
  t.rx_success_rate      = 98.5f;  // passes TXRX-002
  t.irq_overflow_events  = 0;
  t.timeout_events       = 0;
  t.sample_timestamp_utc = 15000;
  return t;
}

static OtaTelemetryInput MakeBlockingTelemetry() {
  OtaTelemetryInput t = MakeHealthyTelemetry();
  t.tx_success_rate = 80.0f;  // fails TXRX-001 (threshold 99%)
  return t;
}

bool TestEvaluateBlockPolicyContinuesWithHealthyTelemetry() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  OtaTelemetryInput tel = MakeHealthyTelemetry();
  BlockDecisionRationale rationale{};
  RolloutBlockOutcome outcome = ReleaseMonitoringEngine::evaluateBlockPolicy(
      window_id, &tel, BlockDecisionSource::kAutoPolicy, rationale);

  return outcome == RolloutBlockOutcome::kContinue;
}

bool TestEvaluateBlockPolicyBlocksOnKpiBreach() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  OtaTelemetryInput tel = MakeBlockingTelemetry();
  BlockDecisionRationale rationale{};
  RolloutBlockOutcome outcome = ReleaseMonitoringEngine::evaluateBlockPolicy(
      window_id, &tel, BlockDecisionSource::kAutoPolicy, rationale);

  return outcome == RolloutBlockOutcome::kBlock;
}

bool TestEvaluateBlockPolicyKpiBreachPopulatesKpiBreach() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  OtaTelemetryInput tel = MakeBlockingTelemetry();
  BlockDecisionRationale rationale{};
  ReleaseMonitoringEngine::evaluateBlockPolicy(
      window_id, &tel, BlockDecisionSource::kAutoPolicy, rationale);

  return rationale.kpi_breach;
}

bool TestEvaluateBlockPolicyKpiBreachPopulatesOtaGateIdInFailedRules() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  OtaTelemetryInput tel = MakeBlockingTelemetry();
  BlockDecisionRationale rationale{};
  ReleaseMonitoringEngine::evaluateBlockPolicy(
      window_id, &tel, BlockDecisionSource::kAutoPolicy, rationale);

  // Should contain the OTA gate ID (TXRX-001) that failed
  if (rationale.failed_rule_count == 0) return false;
  // At least one failed rule ID should be populated
  return rationale.failed_rule_ids[0][0] != '\0';
}

bool TestEvaluateBlockPolicyKpiBreachRegistersGateReportArtifact() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  OtaTelemetryInput tel = MakeBlockingTelemetry();
  BlockDecisionRationale rationale{};
  ReleaseMonitoringEngine::evaluateBlockPolicy(
      window_id, &tel, BlockDecisionSource::kAutoPolicy, rationale);

  if (rationale.artifact_id[0] == '\0') return false;
  const auto* artifact = ArtifactRegistry::getArtifact(rationale.artifact_id);
  if (artifact == nullptr) return false;
  return artifact->type == ArtifactType::kGateReport;
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 2: evaluateBlockPolicy — hold path (stale/missing context)
// ─────────────────────────────────────────────────────────────────────────────

bool TestEvaluateBlockPolicyHoldsForUnknownWindowId() {
  ClearAll();
  BlockDecisionRationale rationale{};
  RolloutBlockOutcome outcome = ReleaseMonitoringEngine::evaluateBlockPolicy(
      "NONEXISTENT-WIN", nullptr, BlockDecisionSource::kAutoPolicy, rationale);

  return outcome == RolloutBlockOutcome::kHold;
}

bool TestEvaluateBlockPolicyHoldPopulatesStaleContext() {
  ClearAll();
  BlockDecisionRationale rationale{};
  ReleaseMonitoringEngine::evaluateBlockPolicy(
      "NONEXISTENT-WIN", nullptr, BlockDecisionSource::kAutoPolicy, rationale);
  return rationale.stale_context;
}

bool TestEvaluateBlockPolicyHoldOnStaleTelemetry() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  // Telemetry with missing timestamp → kHold from OtaGateEngine
  OtaTelemetryInput stale = MakeHealthyTelemetry();
  stale.sample_timestamp_utc = 0;  // stale/missing timestamp

  BlockDecisionRationale rationale{};
  RolloutBlockOutcome outcome = ReleaseMonitoringEngine::evaluateBlockPolicy(
      window_id, &stale, BlockDecisionSource::kAutoPolicy, rationale);

  return outcome == RolloutBlockOutcome::kHold;
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 2: decision precedence — data quality before continuation
// ─────────────────────────────────────────────────────────────────────────────

bool TestBlockPrecedenceIncidentTrendBeforeKpi() {
  // Critical incident should block even if no telemetry provided
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));
  ReleaseMonitoringEngine::recordIncident(window_id, MakeCriticalIncident("INC-C1"));

  BlockDecisionRationale rationale{};
  RolloutBlockOutcome outcome = ReleaseMonitoringEngine::evaluateBlockPolicy(
      window_id, nullptr, BlockDecisionSource::kAutoPolicy, rationale);

  // Must block and attribute to trend_breach (not kpi_breach)
  if (outcome != RolloutBlockOutcome::kBlock) return false;
  return rationale.trend_breach && !rationale.kpi_breach;
}

bool TestBlockOutcomeFieldMatchesReturn() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  BlockDecisionRationale rationale{};
  RolloutBlockOutcome ret = ReleaseMonitoringEngine::evaluateBlockPolicy(
      window_id, nullptr, BlockDecisionSource::kAutoPolicy, rationale);

  return rationale.outcome == ret;
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 3: closeWindow — objective comparison + calibration output (AC: 3)
// ─────────────────────────────────────────────────────────────────────────────

bool TestCloseWindowReturnsOkForOpenWindow() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  WindowReviewOutput review{};
  return ReleaseMonitoringEngine::closeWindow(window_id, 20000, review) == LoRaError::kOk;
}

bool TestCloseWindowFailsForUnknownWindowId() {
  ClearAll();
  WindowReviewOutput review{};
  return ReleaseMonitoringEngine::closeWindow("NONEXISTENT-WIN", 20000, review)
         == LoRaError::kInvalidConfig;
}

bool TestCloseWindowProducesObjectiveComparisons() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  WindowReviewOutput review{};
  ReleaseMonitoringEngine::closeWindow(window_id, 20000, review);
  return review.comparison_count > 0;
}

bool TestCloseWindowObjectivesMet_WhenNoIncidents() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  WindowReviewOutput review{};
  ReleaseMonitoringEngine::closeWindow(window_id, 20000, review);
  return review.objectives_met;
}

bool TestCloseWindowObjectivesNotMet_WhenCriticalIncident() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));
  ReleaseMonitoringEngine::recordIncident(window_id, MakeCriticalIncident("INC-C1"));

  WindowReviewOutput review{};
  ReleaseMonitoringEngine::closeWindow(window_id, 20000, review);
  return !review.objectives_met;
}

bool TestCloseWindowFinalTrendMatchesRecordedIncidents() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));
  ReleaseMonitoringEngine::recordIncident(window_id, MakeCriticalIncident("INC-C1"));
  ReleaseMonitoringEngine::recordIncident(window_id, MakeHighIncident("INC-H1"));

  WindowReviewOutput review{};
  ReleaseMonitoringEngine::closeWindow(window_id, 20000, review);
  return review.final_trend.critical_count == 1 && review.final_trend.high_count == 1;
}

bool TestCloseWindowClosureTimestampIsSet() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  WindowReviewOutput review{};
  ReleaseMonitoringEngine::closeWindow(window_id, 22222, review);
  return review.closure_timestamp == 22222;
}

bool TestCloseWindowRegistersReviewArtifact() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  WindowReviewOutput review{};
  ReleaseMonitoringEngine::closeWindow(window_id, 20000, review);

  if (review.window_artifact_id[0] == '\0') return false;
  const auto* artifact = ArtifactRegistry::getArtifact(review.window_artifact_id);
  if (artifact == nullptr) return false;
  return artifact->type == ArtifactType::kValidationReport;
}

bool TestCloseWindowProducesCalibrationNotes() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  WindowReviewOutput review{};
  ReleaseMonitoringEngine::closeWindow(window_id, 20000, review);
  return review.calibration_note_count > 0;
}

bool TestCloseWindowCalibrationNoteForCriticalIncident() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));
  ReleaseMonitoringEngine::recordIncident(window_id, MakeCriticalIncident("INC-C1"));

  WindowReviewOutput review{};
  ReleaseMonitoringEngine::closeWindow(window_id, 20000, review);

  // Should have at least one calibration note
  if (review.calibration_note_count == 0) return false;
  // First calibration note should be non-empty
  return review.calibration_notes[0][0] != '\0';
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 3: closeWindow — traceability linkage continuity (AC: 1, 2, 3)
// ─────────────────────────────────────────────────────────────────────────────

bool TestCloseWindowLinksIncidentToWindowViaTraceability() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));
  ReleaseMonitoringEngine::recordIncident(window_id, MakeCriticalIncident("INC-LINK-001"));

  WindowReviewOutput review{};
  ReleaseMonitoringEngine::closeWindow(window_id, 20000, review);

  // Verify TraceabilityEngine has incident → window_id link
  std::array<loradriver::TraceabilityLink, 16> chain{};
  size_t count = TraceabilityEngine::getFullTraceChain("INC-LINK-001", chain);
  if (count == 0) return false;
  return std::strcmp(chain[0].target_artifact, window_id) == 0;
}

bool TestCloseWindowWindowArtifactIdIsRetrievable() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  WindowReviewOutput review{};
  ReleaseMonitoringEngine::closeWindow(window_id, 20000, review);

  return ArtifactRegistry::getArtifact(review.window_artifact_id) != nullptr;
}

bool TestCloseWindowLinkedArtifactIdsIncludesWindowArtifact() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  WindowReviewOutput review{};
  ReleaseMonitoringEngine::closeWindow(window_id, 20000, review);

  // linked_artifact_ids should contain the original window artifact (openWindow artifact)
  if (review.linked_artifact_count == 0) return false;
  return std::strcmp(review.linked_artifact_ids[0], window_id) == 0;
}

bool TestCloseWindowProducesObjectiveWithCorrectFields() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  WindowReviewOutput review{};
  ReleaseMonitoringEngine::closeWindow(window_id, 20000, review);

  // Each comparison must have a non-empty metric name
  for (uint8_t i = 0; i < review.comparison_count; ++i) {
    if (review.comparisons[i].metric_name[0] == '\0') return false;
  }
  return true;
}

bool TestCloseWindowObjectiveDeltaIsComputedCorrectly() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));
  ReleaseMonitoringEngine::recordIncident(window_id, MakeCriticalIncident("INC-C1"));

  WindowReviewOutput review{};
  ReleaseMonitoringEngine::closeWindow(window_id, 20000, review);

  // Find critical_incidents comparison
  for (uint8_t i = 0; i < review.comparison_count; ++i) {
    if (std::strcmp(review.comparisons[i].metric_name, "critical_incidents") == 0) {
      // actual=1.0, target=0.0, delta=1.0
      return review.comparisons[i].actual == 1.0f &&
             review.comparisons[i].target == 0.0f &&
             review.comparisons[i].delta  == 1.0f &&
             !review.comparisons[i].met;
    }
  }
  return false;  // critical_incidents comparison not found
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 3: closeWindow — second close attempt fails (closed window guard)
// ─────────────────────────────────────────────────────────────────────────────

bool TestCloseWindowFailsWhenAlreadyClosed() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  WindowReviewOutput review{};
  ReleaseMonitoringEngine::closeWindow(window_id, 20000, review);

  // Second close should fail
  WindowReviewOutput review2{};
  return ReleaseMonitoringEngine::closeWindow(window_id, 21000, review2)
         == LoRaError::kInvalidConfig;
}

bool TestRecordIncidentFailsForClosedWindow() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  WindowReviewOutput review{};
  ReleaseMonitoringEngine::closeWindow(window_id, 20000, review);

  // Recording to closed window should fail
  IncidentRecord inc = MakeCriticalIncident("INC-AFTER-CLOSE");
  return ReleaseMonitoringEngine::recordIncident(window_id, inc)
         == LoRaError::kInvalidConfig;
}

bool TestRecordIncidentFailsWhenTraceabilityIsFull() {
  ClearAll();

  for (size_t i = 0; i < TraceabilityEngine::kMaxLinks; ++i) {
    char incident_id[32] = {};
    std::snprintf(incident_id, sizeof(incident_id), "PRELINK-%zu", i);
    if (TraceabilityEngine::linkIncidentToArtifact(incident_id, "ART-SEED") != LoRaError::kOk) {
      return false;
    }
  }

  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  IncidentRecord inc = MakeCriticalIncident("INC-LINK-FULL");
  return ReleaseMonitoringEngine::recordIncident(window_id, inc) == LoRaError::kLinkFailed;
}

bool TestCloseWindowFailsWhenTraceabilityIsFull() {
  ClearAll();

  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));

  for (size_t i = 0; i < TraceabilityEngine::kMaxLinks; ++i) {
    char incident_id[32] = {};
    std::snprintf(incident_id, sizeof(incident_id), "CLOSELINK-%zu", i);
    if (TraceabilityEngine::linkIncidentToArtifact(incident_id, "ART-SEED") != LoRaError::kOk) {
      return false;
    }
  }

  WindowReviewOutput review{};
  return ReleaseMonitoringEngine::closeWindow(window_id, 20000, review) == LoRaError::kLinkFailed;
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 5: policy language — rollout gate vs post-release governance distinction
// ─────────────────────────────────────────────────────────────────────────────

bool TestBlockDecisionSourceAutoAndOperatorProduceSameEvidenceSchema() {
  ClearAll();
  MonitoringWindow w = MakeValidWindow();
  char window_id[32] = {};
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));
  ReleaseMonitoringEngine::recordIncident(window_id, MakeCriticalIncident("INC-C1"));

  BlockDecisionRationale auto_rationale{};
  ReleaseMonitoringEngine::evaluateBlockPolicy(
      window_id, nullptr, BlockDecisionSource::kAutoPolicy, auto_rationale);

  ClearAll();
  ReleaseMonitoringEngine::openWindow(w, window_id, sizeof(window_id));
  ReleaseMonitoringEngine::recordIncident(window_id, MakeCriticalIncident("INC-C1"));

  BlockDecisionRationale op_rationale{};
  ReleaseMonitoringEngine::evaluateBlockPolicy(
      window_id, nullptr, BlockDecisionSource::kOperatorPolicy, op_rationale);

  // Both should produce identical evidence schema (same fields populated)
  if (auto_rationale.outcome != op_rationale.outcome) return false;
  if (auto_rationale.failed_rule_count != op_rationale.failed_rule_count) return false;
  if (!auto_rationale.trend_breach || !op_rationale.trend_breach) return false;
  // Only source differs
  if (auto_rationale.decision_source == op_rationale.decision_source) return false;
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test runner
// ─────────────────────────────────────────────────────────────────────────────

#define RUN_TEST(fn)                                      \
  if (!(fn)()) {                                         \
    std::fprintf(stderr, "FAIL: %s\n", #fn);            \
    return EXIT_FAILURE;                                 \
  }

int RunReleaseMonitoringTests() {
  // Regression: enum/struct stability
  RUN_TEST(TestIncidentCategoryEnumValuesAreStable)
  RUN_TEST(TestIncidentSeverityEnumValuesAreStable)
  RUN_TEST(TestBlockDecisionSourceEnumValuesAreStable)
  RUN_TEST(TestRolloutBlockOutcomeEnumValuesAreStable)
  RUN_TEST(TestMonitoringWindowIsTriviallyCopyable)
  RUN_TEST(TestIncidentRecordIsTriviallyCopyable)
  RUN_TEST(TestMonitoringTrendLedgerIsTriviallyCopyable)
  RUN_TEST(TestBlockDecisionRationaleIsTriviallyCopyable)
  RUN_TEST(TestWindowReviewOutputIsTriviallyCopyable)

  // Task 1: validateWindowContext
  RUN_TEST(TestValidateWindowContextPassesForValidWindow)
  RUN_TEST(TestValidateWindowContextFailsForMissingVersion)
  RUN_TEST(TestValidateWindowContextFailsForMissingRadioFamily)
  RUN_TEST(TestValidateWindowContextFailsForInvalidRadioFamily)
  RUN_TEST(TestValidateWindowContextAcceptsSX1278)
  RUN_TEST(TestValidateWindowContextFailsForMissingActiveBand)
  RUN_TEST(TestValidateWindowContextFailsForMissingProfileId)
  RUN_TEST(TestValidateWindowContextFailsForInvalidActiveBand)
  RUN_TEST(TestValidateWindowContextAccepts433Band)
  RUN_TEST(TestValidateWindowContextFailsForZeroStartTimestamp)
  RUN_TEST(TestValidateWindowContextFailsForEndBeforeStart)
  RUN_TEST(TestValidateWindowContextAcceptsOpenEndedWindow)

  // Task 1: openWindow
  RUN_TEST(TestOpenWindowReturnsOkForValidContext)
  RUN_TEST(TestOpenWindowPopulatesWindowId)
  RUN_TEST(TestOpenWindowRegistersArtifactInRegistry)
  RUN_TEST(TestOpenWindowArtifactLinkedVersionMatchesReleaseVersion)
  RUN_TEST(TestOpenWindowFailsForInvalidContext)
  RUN_TEST(TestOpenWindowFailsForNullOutputBuffer)

  // Task 1: recordIncident and trend ledger
  RUN_TEST(TestRecordIncidentReturnsOkForValidWindow)
  RUN_TEST(TestRecordIncidentUpdatesCriticalCount)
  RUN_TEST(TestRecordIncidentUpdatesHighCount)
  RUN_TEST(TestRecordIncidentUpdatesTotalCount)
  RUN_TEST(TestRecordIncidentAggregatesByIncidentCount)
  RUN_TEST(TestRecordIncidentFailsForZeroCount)
  RUN_TEST(TestRecordIncidentFailsForUnknownWindowId)
  RUN_TEST(TestRecordIncidentLinksToWindowArtifactViaTraceability)
  RUN_TEST(TestRecordIncidentUpdatesSnapshotTimestamp)

  // Task 1: getTrendSnapshot
  RUN_TEST(TestGetTrendSnapshotReturnsFalseForUnknownWindow)
  RUN_TEST(TestGetTrendSnapshotReturnsZeroCountsForFreshWindow)
  RUN_TEST(TestTrendLedgerGetCountBySeverityMatchesCriticalCount)

  // Task 2: evaluateBlockPolicy — happy path
  RUN_TEST(TestEvaluateBlockPolicyContinuesWithNoIncidentsAndNoTelemetry)
  RUN_TEST(TestEvaluateBlockPolicyContinueReasonFieldIsNonEmpty)
  RUN_TEST(TestEvaluateBlockPolicyContinueHasCorrectSource)
  RUN_TEST(TestEvaluateBlockPolicyContinueWithOnlyLowIncidents)

  // Task 2: block path — incident trend
  RUN_TEST(TestEvaluateBlockPolicyBlocksOnCriticalIncident)
  RUN_TEST(TestEvaluateBlockPolicyBlockPopulatesTrendBreach)
  RUN_TEST(TestEvaluateBlockPolicyBlockPopulatesFailedRuleId)
  RUN_TEST(TestEvaluateBlockPolicyBlockMetricAndThresholdArePopulated)
  RUN_TEST(TestEvaluateBlockPolicyBlockRegistersGateReportArtifact)
  RUN_TEST(TestEvaluateBlockPolicyBlockPopulatesNonEmptyReason)
  RUN_TEST(TestEvaluateBlockPolicyOperatorPolicyBlocksOnCritical)

  // Task 2: block path — KPI breach
  RUN_TEST(TestEvaluateBlockPolicyContinuesWithHealthyTelemetry)
  RUN_TEST(TestEvaluateBlockPolicyBlocksOnKpiBreach)
  RUN_TEST(TestEvaluateBlockPolicyKpiBreachPopulatesKpiBreach)
  RUN_TEST(TestEvaluateBlockPolicyKpiBreachPopulatesOtaGateIdInFailedRules)
  RUN_TEST(TestEvaluateBlockPolicyKpiBreachRegistersGateReportArtifact)

  // Task 2: hold path — stale/missing context
  RUN_TEST(TestEvaluateBlockPolicyHoldsForUnknownWindowId)
  RUN_TEST(TestEvaluateBlockPolicyHoldPopulatesStaleContext)
  RUN_TEST(TestEvaluateBlockPolicyHoldOnStaleTelemetry)

  // Task 2: decision precedence
  RUN_TEST(TestBlockPrecedenceIncidentTrendBeforeKpi)
  RUN_TEST(TestBlockOutcomeFieldMatchesReturn)

  // Task 3: closeWindow — objective comparison
  RUN_TEST(TestCloseWindowReturnsOkForOpenWindow)
  RUN_TEST(TestCloseWindowFailsForUnknownWindowId)
  RUN_TEST(TestCloseWindowProducesObjectiveComparisons)
  RUN_TEST(TestCloseWindowObjectivesMet_WhenNoIncidents)
  RUN_TEST(TestCloseWindowObjectivesNotMet_WhenCriticalIncident)
  RUN_TEST(TestCloseWindowFinalTrendMatchesRecordedIncidents)
  RUN_TEST(TestCloseWindowClosureTimestampIsSet)
  RUN_TEST(TestCloseWindowRegistersReviewArtifact)
  RUN_TEST(TestCloseWindowProducesCalibrationNotes)
  RUN_TEST(TestCloseWindowCalibrationNoteForCriticalIncident)

  // Task 3: traceability linkage continuity
  RUN_TEST(TestCloseWindowLinksIncidentToWindowViaTraceability)
  RUN_TEST(TestCloseWindowWindowArtifactIdIsRetrievable)
  RUN_TEST(TestCloseWindowLinkedArtifactIdsIncludesWindowArtifact)
  RUN_TEST(TestCloseWindowProducesObjectiveWithCorrectFields)
  RUN_TEST(TestCloseWindowObjectiveDeltaIsComputedCorrectly)

  // Task 3: closed window guards
  RUN_TEST(TestCloseWindowFailsWhenAlreadyClosed)
  RUN_TEST(TestRecordIncidentFailsForClosedWindow)
  RUN_TEST(TestRecordIncidentFailsWhenTraceabilityIsFull)
  RUN_TEST(TestCloseWindowFailsWhenTraceabilityIsFull)

  // Task 5: policy language distinction
  RUN_TEST(TestBlockDecisionSourceAutoAndOperatorProduceSameEvidenceSchema)

  return EXIT_SUCCESS;
}

}  // namespace

int main() {
  return RunReleaseMonitoringTests();
}
