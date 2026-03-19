#include "loradriver/release_monitoring.hpp"
#include "loradriver/artifact_governance.hpp"
#include "loradriver/versioning.hpp"

#include <cstdio>
#include <cstring>

namespace loradriver {

// ─────────────────────────────────────────────────────────────────────────────
// Static storage
// ─────────────────────────────────────────────────────────────────────────────
ReleaseMonitoringEngine::WindowState ReleaseMonitoringEngine::windows_[kMaxWindows] = {};
size_t ReleaseMonitoringEngine::window_count_ = 0;

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace {

bool IsSupportedRadioFamily(const char* value) noexcept {
  return value != nullptr &&
         (std::strcmp(value, "SX1276") == 0 || std::strcmp(value, "SX1278") == 0);
}

bool IsSupportedBand(const char* value) noexcept {
  return value != nullptr &&
         (std::strcmp(value, "433") == 0 || std::strcmp(value, "868") == 0);
}

bool PopulateBlockArtifact(const MonitoringWindow& context,
                           const char*             window_id,
                           BlockDecisionRationale& out_rationale,
                           uint32_t                timestamp) noexcept {
  ArtifactMetadata block_meta{};
  block_meta.type              = ArtifactType::kGateReport;
  block_meta.created_timestamp = timestamp != 0 ? timestamp : context.window_start_utc;
  block_meta.retention_days    = 90;
  std::strncpy(block_meta.linked_version, context.release_version,
               sizeof(block_meta.linked_version) - 1);
  block_meta.linked_version[sizeof(block_meta.linked_version) - 1] = '\0';
  std::snprintf(block_meta.source_module, sizeof(block_meta.source_module),
                "RelMonBlk:%s:%s:%s",
                context.radio_family,
                context.active_band,
                context.profile_id);

  const char* block_art_id = ArtifactRegistry::registerArtifact(block_meta);
  if (block_art_id == nullptr) {
    return false;
  }

  std::strncpy(out_rationale.artifact_id, block_art_id,
               sizeof(out_rationale.artifact_id) - 1);
  out_rationale.artifact_id[sizeof(out_rationale.artifact_id) - 1] = '\0';

  const LoRaError link_err = ArtifactRegistry::linkArtifacts(window_id, block_art_id);
  if (link_err != LoRaError::kOk) {
    return false;
  }

  char block_incident_id[TraceabilityLink::kMaxIdLength] = {};
  std::snprintf(block_incident_id, sizeof(block_incident_id),
                "BLK-%s-%u",
                context.profile_id,
                block_meta.created_timestamp);

  return TraceabilityEngine::linkIncidentToArtifact(block_incident_id, block_art_id)
      == LoRaError::kOk;
}

void AppendFailedRule(BlockDecisionRationale& out,
                      const char*             rule_id,
                      float                   metric_value,
                      float                   threshold) noexcept {
  if (out.failed_rule_count >= BlockDecisionRationale::kMaxFailedRules) {
    return;
  }
  const uint8_t idx = out.failed_rule_count;
  std::strncpy(out.failed_rule_ids[idx], rule_id,
               BlockDecisionRationale::kMaxRuleIdLength - 1);
  out.failed_rule_ids[idx][BlockDecisionRationale::kMaxRuleIdLength - 1] = '\0';
  out.metric_values[idx] = metric_value;
  out.thresholds[idx]    = threshold;
  out.failed_rule_count++;
}

}  // namespace

// ─────────────────────────────────────────────────────────────────────────────
// ReleaseMonitoringEngine::findWindow  (private)
// ─────────────────────────────────────────────────────────────────────────────
ReleaseMonitoringEngine::WindowState* ReleaseMonitoringEngine::findWindow(
    const char* window_id) noexcept {
  if (window_id == nullptr || window_id[0] == '\0') {
    return nullptr;
  }
  for (size_t i = 0; i < window_count_; ++i) {
    if (std::strcmp(windows_[i].window_id, window_id) == 0) {
      return &windows_[i];
    }
  }
  return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// ReleaseMonitoringEngine::validateWindowContext
// ─────────────────────────────────────────────────────────────────────────────
bool ReleaseMonitoringEngine::validateWindowContext(const MonitoringWindow& window,
                                                    char*                   out_reason,
                                                    size_t                  reason_size) noexcept {
  if (out_reason == nullptr || reason_size == 0) {
    return false;
  }

  if (window.release_version[0] == '\0') {
    std::strncpy(out_reason, "missing release_version", reason_size - 1);
    out_reason[reason_size - 1] = '\0';
    return false;
  }

  if (window.radio_family[0] == '\0') {
    std::strncpy(out_reason, "missing radio_family", reason_size - 1);
    out_reason[reason_size - 1] = '\0';
    return false;
  }
  if (!IsSupportedRadioFamily(window.radio_family)) {
    std::snprintf(out_reason, reason_size,
                  "unsupported radio_family '%s' (V1: SX1276/SX1278)",
                  window.radio_family);
    return false;
  }

  if (window.active_band[0] == '\0') {
    std::strncpy(out_reason, "missing active_band", reason_size - 1);
    out_reason[reason_size - 1] = '\0';
    return false;
  }
  if (!IsSupportedBand(window.active_band)) {
    std::snprintf(out_reason, reason_size,
                  "unsupported active_band '%s' (V1: 433/868)",
                  window.active_band);
    return false;
  }

  if (window.window_start_utc == 0) {
    std::strncpy(out_reason, "missing window_start_utc (zero is invalid)", reason_size - 1);
    out_reason[reason_size - 1] = '\0';
    return false;
  }

  if (window.window_end_utc != 0 && window.window_end_utc <= window.window_start_utc) {
    std::strncpy(out_reason,
                 "window_end_utc must be after window_start_utc",
                 reason_size - 1);
    out_reason[reason_size - 1] = '\0';
    return false;
  }

  if (window.profile_id[0] == '\0') {
    std::strncpy(out_reason, "missing profile_id", reason_size - 1);
    out_reason[reason_size - 1] = '\0';
    return false;
  }

  out_reason[0] = '\0';
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// ReleaseMonitoringEngine::openWindow
// ─────────────────────────────────────────────────────────────────────────────
LoRaError ReleaseMonitoringEngine::openWindow(const MonitoringWindow& window,
                                               char*                   out_window_id,
                                               size_t                  window_id_size) noexcept {
  if (out_window_id == nullptr || window_id_size == 0) {
    return LoRaError::kInvalidConfig;
  }

  // Validate V1 profile/band context
  char reason[128] = {};
  if (!validateWindowContext(window, reason, sizeof(reason))) {
    return LoRaError::kInvalidConfig;
  }

  // Check window capacity
  if (window_count_ >= kMaxWindows) {
    return LoRaError::kRegistryFull;
  }

  // Register kIncidentEvidence artifact to anchor this window in governance
  ArtifactMetadata meta{};
  meta.type              = ArtifactType::kIncidentEvidence;
  meta.created_timestamp = window.window_start_utc;
  meta.retention_days    = 180;  // kIncidentEvidence: 90–180 days; use maximum for monitoring windows
  std::strncpy(meta.linked_version, window.release_version,
               sizeof(meta.linked_version) - 1);
  meta.linked_version[sizeof(meta.linked_version) - 1] = '\0';
  std::snprintf(meta.source_module, sizeof(meta.source_module),
                "RelMon:%s:%s:%s", window.radio_family, window.active_band, window.profile_id);

  const char* artifact_id = ArtifactRegistry::registerArtifact(meta);
  if (artifact_id == nullptr) {
    return LoRaError::kArtifactRegistrationFailed;
  }

  // Initialise window state
  WindowState& state   = windows_[window_count_++];
  state                = {};
  state.context        = window;
  state.incident_count = 0;
  state.is_open        = true;

  std::strncpy(state.window_id, artifact_id, sizeof(state.window_id) - 1);
  state.window_id[sizeof(state.window_id) - 1] = '\0';

  // Return the artifact ID as the caller's window identifier
  std::strncpy(out_window_id, artifact_id, window_id_size - 1);
  out_window_id[window_id_size - 1] = '\0';

  return LoRaError::kOk;
}

// ─────────────────────────────────────────────────────────────────────────────
// ReleaseMonitoringEngine::recordIncident
// ─────────────────────────────────────────────────────────────────────────────
LoRaError ReleaseMonitoringEngine::recordIncident(const char*           window_id,
                                                   const IncidentRecord& incident) noexcept {
  if (window_id == nullptr || window_id[0] == '\0') {
    return LoRaError::kInvalidConfig;
  }

  if (incident.incident_id[0] == '\0' || incident.first_seen_utc == 0 || incident.count == 0) {
    return LoRaError::kInvalidConfig;
  }

  WindowState* state = findWindow(window_id);
  if (state == nullptr || !state->is_open) {
    return LoRaError::kInvalidConfig;
  }

  if (state->incident_count >= kMaxIncidents) {
    return LoRaError::kRegistryFull;
  }

  // Store incident record
  state->incidents[state->incident_count++] = incident;

  // Update bounded trend ledger
  switch (incident.severity) {
    case IncidentSeverity::kCritical: state->trend.critical_count += incident.count;  break;
    case IncidentSeverity::kHigh:     state->trend.high_count += incident.count;      break;
    case IncidentSeverity::kMedium:   state->trend.medium_count += incident.count;    break;
    case IncidentSeverity::kLow:      state->trend.low_count += incident.count;       break;
  }
  state->trend.total_incident_count += incident.count;
  state->trend.snapshot_timestamp = incident.first_seen_utc;

  // Link incident to window artifact via TraceabilityEngine for governance chain
  if (TraceabilityEngine::linkIncidentToArtifact(incident.incident_id, window_id)
      != LoRaError::kOk) {
    return LoRaError::kLinkFailed;
  }

  return LoRaError::kOk;
}

// ─────────────────────────────────────────────────────────────────────────────
// ReleaseMonitoringEngine::getTrendSnapshot
// ─────────────────────────────────────────────────────────────────────────────
bool ReleaseMonitoringEngine::getTrendSnapshot(const char*            window_id,
                                                MonitoringTrendLedger& out_ledger) noexcept {
  if (window_id == nullptr || window_id[0] == '\0') {
    return false;
  }

  const WindowState* state = findWindow(window_id);
  if (state == nullptr) {
    return false;
  }

  out_ledger = state->trend;
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// ReleaseMonitoringEngine::evaluateBlockPolicy
// ─────────────────────────────────────────────────────────────────────────────
RolloutBlockOutcome ReleaseMonitoringEngine::evaluateBlockPolicy(
    const char*               window_id,
    const OtaTelemetryInput*  telemetry,
    BlockDecisionSource       source,
    BlockDecisionRationale&   out_rationale) noexcept {
  // Zero-initialise rationale
  out_rationale               = {};
  out_rationale.decision_source = source;

  // ── Step 1: Context check — missing or closed window → kHold ─────────────
  const WindowState* state = findWindow(window_id);
  if (state == nullptr || !state->is_open) {
    out_rationale.stale_context = true;
    out_rationale.outcome       = RolloutBlockOutcome::kHold;
    std::strncpy(out_rationale.reason,
                 "stale or missing monitoring context; manual escalation required",
                 sizeof(out_rationale.reason) - 1);
    out_rationale.reason[sizeof(out_rationale.reason) - 1] = '\0';
    return RolloutBlockOutcome::kHold;
  }

  // ── Step 2: Incident trend check (RM-TREND-001: any critical incident blocks) ──
  if (state->trend.critical_count > kCriticalIncidentBlockThreshold) {
    out_rationale.trend_breach = true;
    AppendFailedRule(out_rationale,
                     "RM-TREND-001",
                     static_cast<float>(state->trend.critical_count),
                     static_cast<float>(kCriticalIncidentBlockThreshold));
    out_rationale.outcome = RolloutBlockOutcome::kBlock;
    std::snprintf(out_rationale.reason, sizeof(out_rationale.reason),
                  "critical incident trend exceeded: %u critical incidents (threshold: %u)",
                  state->trend.critical_count,
                  static_cast<unsigned>(kCriticalIncidentBlockThreshold));

    if (!PopulateBlockArtifact(state->context,
                               state->window_id,
                               out_rationale,
                               state->trend.snapshot_timestamp)) {
      out_rationale.outcome = RolloutBlockOutcome::kHold;
      std::strncpy(out_rationale.reason,
                   "critical trend breach detected but governance evidence could not be persisted",
                   sizeof(out_rationale.reason) - 1);
      out_rationale.reason[sizeof(out_rationale.reason) - 1] = '\0';
      return RolloutBlockOutcome::kHold;
    }

    return RolloutBlockOutcome::kBlock;
  }

  // ── Step 3: OTA KPI gate check (reuses OtaGateEngine, existing gate IDs) ──
  if (telemetry != nullptr) {
    OtaDecisionRationale ota_rationale{};
    const OtaRolloutDecision ota_decision =
        OtaGateEngine::evaluate(*telemetry, ota_rationale);

    if (ota_decision == OtaRolloutDecision::kBlock) {
      out_rationale.kpi_breach = true;
      out_rationale.outcome    = RolloutBlockOutcome::kBlock;

      // Copy OTA gate IDs into block rationale (reuses existing CI gate IDs)
      for (uint8_t i = 0; i < ota_rationale.failed_gate_count; ++i) {
        AppendFailedRule(out_rationale,
                         ota_rationale.failed_gate_ids[i],
                         ota_rationale.failed_gate_actuals[i],
                         ota_rationale.failed_gate_thresholds[i]);
      }
      std::strncpy(out_rationale.reason, ota_rationale.reason,
                   sizeof(out_rationale.reason) - 1);
      out_rationale.reason[sizeof(out_rationale.reason) - 1] = '\0';

      if (!PopulateBlockArtifact(state->context,
                                 state->window_id,
                                 out_rationale,
                                 telemetry->sample_timestamp_utc)) {
        out_rationale.outcome = RolloutBlockOutcome::kHold;
        std::strncpy(out_rationale.reason,
                     "KPI breach detected but governance evidence could not be persisted",
                     sizeof(out_rationale.reason) - 1);
        out_rationale.reason[sizeof(out_rationale.reason) - 1] = '\0';
        return RolloutBlockOutcome::kHold;
      }

      return RolloutBlockOutcome::kBlock;
    }

    if (ota_decision == OtaRolloutDecision::kHold) {
      out_rationale.stale_context =
          ota_rationale.quality_issue || ota_rationale.stale_data;
      out_rationale.outcome = RolloutBlockOutcome::kHold;
      std::strncpy(out_rationale.reason, ota_rationale.reason,
                   sizeof(out_rationale.reason) - 1);
      out_rationale.reason[sizeof(out_rationale.reason) - 1] = '\0';
      return RolloutBlockOutcome::kHold;
    }
  }

  // ── Step 4: All checks passed — rollout may continue ────────────────────
  out_rationale.outcome = RolloutBlockOutcome::kContinue;
  std::strncpy(out_rationale.reason,
               "all incident trend and KPI checks passed; progressive rollout may continue",
               sizeof(out_rationale.reason) - 1);
  out_rationale.reason[sizeof(out_rationale.reason) - 1] = '\0';
  return RolloutBlockOutcome::kContinue;
}

// ─────────────────────────────────────────────────────────────────────────────
// ReleaseMonitoringEngine::closeWindow
// ─────────────────────────────────────────────────────────────────────────────
LoRaError ReleaseMonitoringEngine::closeWindow(const char*         window_id,
                                                uint32_t            closure_timestamp,
                                                WindowReviewOutput& out_review) noexcept {
  out_review = {};

  if (window_id == nullptr || window_id[0] == '\0') {
    return LoRaError::kInvalidConfig;
  }

  WindowState* state = findWindow(window_id);
  if (state == nullptr || !state->is_open) {
    return LoRaError::kInvalidConfig;
  }

  // ── Objective comparisons — target vs actual per tracked metric ──────────
  uint8_t cmp_idx = 0;

  // Objective 1: critical_incidents — target: 0
  {
    ObjectiveComparison& cmp = out_review.comparisons[cmp_idx];
    std::strncpy(cmp.metric_name, "critical_incidents",
                 sizeof(cmp.metric_name) - 1);
    cmp.metric_name[sizeof(cmp.metric_name) - 1] = '\0';
    cmp.target = 0.0f;
    cmp.actual = static_cast<float>(state->trend.critical_count);
    cmp.delta  = cmp.actual - cmp.target;
    cmp.met    = (state->trend.critical_count == 0);
    ++cmp_idx;
  }

  // Objective 2: high_incidents — target: 0
  {
    ObjectiveComparison& cmp = out_review.comparisons[cmp_idx];
    std::strncpy(cmp.metric_name, "high_incidents",
                 sizeof(cmp.metric_name) - 1);
    cmp.metric_name[sizeof(cmp.metric_name) - 1] = '\0';
    cmp.target = 0.0f;
    cmp.actual = static_cast<float>(state->trend.high_count);
    cmp.delta  = cmp.actual - cmp.target;
    cmp.met    = (state->trend.high_count == 0);
    ++cmp_idx;
  }

  // Objective 3: total_incidents — target: 0
  {
    ObjectiveComparison& cmp = out_review.comparisons[cmp_idx];
    std::strncpy(cmp.metric_name, "total_incidents",
                 sizeof(cmp.metric_name) - 1);
    cmp.metric_name[sizeof(cmp.metric_name) - 1] = '\0';
    cmp.target = 0.0f;
    cmp.actual = static_cast<float>(state->trend.total_incident_count);
    cmp.delta  = cmp.actual - cmp.target;
    cmp.met    = (state->trend.total_incident_count == 0);
    ++cmp_idx;
  }

  out_review.comparison_count = cmp_idx;

  // ── Determine overall objectives_met ──────────────────────────────────────
  bool all_met = true;
  for (uint8_t i = 0; i < out_review.comparison_count; ++i) {
    if (!out_review.comparisons[i].met) {
      all_met = false;
      break;
    }
  }
  out_review.objectives_met = all_met;

  // ── Calibration notes — machine-readable, evidence-referenced ────────────
  uint8_t note_idx = 0;

  if (state->trend.critical_count > 0) {
    std::snprintf(out_review.calibration_notes[note_idx],
                  WindowReviewOutput::kMaxNoteLength,
                  "critical_incidents=%u exceeded target=0; "
                  "tighten OTA gate thresholds or reduce rollout velocity",
                  state->trend.critical_count);
    ++note_idx;
  }

  if (state->trend.total_incident_count > 5 &&
      note_idx < WindowReviewOutput::kMaxCalibrationNotes) {
    std::snprintf(out_review.calibration_notes[note_idx],
                  WindowReviewOutput::kMaxNoteLength,
                  "total_incidents=%u elevated; review incident trend "
                  "detection sensitivity and gate thresholds",
                  state->trend.total_incident_count);
    ++note_idx;
  }

  if (note_idx == 0) {
    // No issues found — record that explicitly for calibration continuity
    std::strncpy(out_review.calibration_notes[note_idx],
                 "all incident metrics within target; no calibration adjustment required",
                 WindowReviewOutput::kMaxNoteLength - 1);
    out_review.calibration_notes[note_idx][WindowReviewOutput::kMaxNoteLength - 1] = '\0';
    ++note_idx;
  }

  out_review.calibration_note_count = note_idx;

  // ── Final trend and closure timestamp ────────────────────────────────────
  out_review.final_trend       = state->trend;
  out_review.closure_timestamp = closure_timestamp;

  // ── Register kValidationReport artifact for window review evidence ────────
  ArtifactMetadata review_meta{};
  review_meta.type              = ArtifactType::kValidationReport;
  review_meta.created_timestamp = closure_timestamp;
  review_meta.retention_days    = 90;  // kValidationReport: 90–180 days
  std::strncpy(review_meta.linked_version, state->context.release_version,
               sizeof(review_meta.linked_version) - 1);
  review_meta.linked_version[sizeof(review_meta.linked_version) - 1] = '\0';
  std::snprintf(review_meta.source_module, sizeof(review_meta.source_module),
                "RelMonReview:%s:%s:%s",
                state->context.radio_family,
                state->context.active_band,
                state->context.profile_id);

  const char* review_art_id = ArtifactRegistry::registerArtifact(review_meta);
  if (review_art_id == nullptr) {
    return LoRaError::kArtifactRegistrationFailed;
  }

  std::strncpy(out_review.window_artifact_id, review_art_id,
               sizeof(out_review.window_artifact_id) - 1);
  out_review.window_artifact_id[sizeof(out_review.window_artifact_id) - 1] = '\0';

  // ── Link window artifact (from openWindow) to review artifact ────────────
  // Structural link in ArtifactRegistry: openWindow artifact → review artifact
  if (ArtifactRegistry::linkArtifacts(state->window_id, review_art_id) != LoRaError::kOk) {
    return LoRaError::kLinkFailed;
  }

  // Traceability link: window incident ID → review artifact
  char win_incident_id[TraceabilityLink::kMaxIdLength] = {};
  std::snprintf(win_incident_id, sizeof(win_incident_id),
                "WIN-%s-%u",
                state->context.radio_family,
                state->context.window_start_utc);
  if (TraceabilityEngine::linkIncidentToArtifact(win_incident_id, review_art_id)
      != LoRaError::kOk) {
    return LoRaError::kLinkFailed;
  }

  // ── Linked artifact chain (original window artifact for traceability) ─────
  uint8_t link_idx = 0;
  std::strncpy(out_review.linked_artifact_ids[link_idx], state->window_id,
               WindowReviewOutput::kMaxArtifactIdLength - 1);
  out_review.linked_artifact_ids[link_idx][WindowReviewOutput::kMaxArtifactIdLength - 1] = '\0';
  ++link_idx;

  out_review.linked_artifact_count = link_idx;

  // ── Mark window as closed ────────────────────────────────────────────────
  state->is_open = false;

  return LoRaError::kOk;
}

// ─────────────────────────────────────────────────────────────────────────────
// ReleaseMonitoringEngine::clear
// ─────────────────────────────────────────────────────────────────────────────
void ReleaseMonitoringEngine::clear() noexcept {
  window_count_ = 0;
  for (size_t i = 0; i < kMaxWindows; ++i) {
    windows_[i] = {};
  }
}

}  // namespace loradriver
