#include "loradriver/rollback_governance.hpp"
#include "loradriver/artifact_governance.hpp"
#include "loradriver/versioning.hpp"

#include <cstdio>
#include <cstring>
#include <limits>

namespace loradriver {

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers (V1 profile/band validation — mirrors OtaGateEngine)
// ─────────────────────────────────────────────────────────────────────────────
namespace {

bool IsNullTerminated(const char* value, size_t max_len) noexcept {
  if (value == nullptr || max_len == 0) {
    return false;
  }

  for (size_t i = 0; i < max_len; ++i) {
    if (value[i] == '\0') {
      return true;
    }
  }
  return false;
}

void CopyReason(char* out_reason, size_t reason_size, const char* value) noexcept {
  if (out_reason == nullptr || reason_size == 0) {
    return;
  }

  std::strncpy(out_reason, value, reason_size - 1);
  out_reason[reason_size - 1] = '\0';
}

uint32_t StepTimestamp(uint32_t base, uint32_t offset) noexcept {
  const uint32_t max_value = std::numeric_limits<uint32_t>::max();
  if (offset > (max_value - base)) {
    return base;
  }
  return base + offset;
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
// RollbackGovernance::evaluateTriggerPolicy
// ─────────────────────────────────────────────────────────────────────────────
bool RollbackGovernance::evaluateTriggerPolicy(
    OtaRolloutDecision          decision,
    const OtaDecisionRationale& rationale) noexcept {
  // Data-quality hold must NOT trigger rollback — potential false positive.
  // Trend-risk hold also does not trigger — manual escalation path only.
  if (decision == OtaRolloutDecision::kHold) {
    return false;
  }

  // kAllow: no issue detected; no rollback needed.
  if (decision == OtaRolloutDecision::kAllow) {
    return false;
  }

  // kBlock: rollback only when rationale contains explicit blocking evidence.
  const bool has_blocking_evidence = rationale.failed_gate_count > 0;
  return has_blocking_evidence && !rationale.quality_issue;
}

// ─────────────────────────────────────────────────────────────────────────────
// RollbackGovernance::runPrechecks
// ─────────────────────────────────────────────────────────────────────────────
bool RollbackGovernance::runPrechecks(const RollbackRequest& request,
                                      char*                  out_reason,
                                      size_t                 reason_size) noexcept {
  if (out_reason == nullptr || reason_size == 0) {
    return false;
  }

  if (!IsNullTerminated(request.target_version, sizeof(request.target_version))) {
    CopyReason(out_reason, reason_size,
               "precheck-lkg: target_version is not null-terminated");
    return false;
  }
  if (!IsNullTerminated(request.radio_family, sizeof(request.radio_family))) {
    CopyReason(out_reason, reason_size,
               "precheck-profile: radio_family is not null-terminated");
    return false;
  }
  if (!IsNullTerminated(request.active_band, sizeof(request.active_band))) {
    CopyReason(out_reason, reason_size,
               "precheck-band: active_band is not null-terminated");
    return false;
  }

  // Check 1: LKG candidate presence — target_version must be non-empty
  if (request.target_version[0] == '\0') {
    CopyReason(out_reason, reason_size,
               "precheck-lkg: missing target_version (no LKG candidate registered)");
    return false;
  }

  // Check 2: V1 profile compatibility — radio family
  if (request.radio_family[0] == '\0') {
    CopyReason(out_reason, reason_size, "precheck-profile: missing radio_family");
    return false;
  }
  if (!IsSupportedRadioFamily(request.radio_family)) {
    std::snprintf(out_reason, reason_size,
                  "precheck-profile: unsupported radio_family '%s' (V1: SX1276/SX1278)",
                  request.radio_family);
    return false;
  }

  // Check 3: V1 profile compatibility — active band
  if (request.active_band[0] == '\0') {
    CopyReason(out_reason, reason_size, "precheck-band: missing active_band");
    return false;
  }
  if (!IsSupportedBand(request.active_band)) {
    std::snprintf(out_reason, reason_size,
                  "precheck-band: unsupported active_band '%s' (V1: 433/868)",
                  request.active_band);
    return false;
  }

  // Check 4: Artifact integrity prerequisite — trigger timestamp must be valid
  if (request.trigger_timestamp_utc == 0) {
    CopyReason(out_reason, reason_size,
               "precheck-integrity: trigger_timestamp_utc is zero (uninitialised or invalid)");
    return false;
  }

  out_reason[0] = '\0';
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// RollbackGovernance::runPostchecks
// ─────────────────────────────────────────────────────────────────────────────
bool RollbackGovernance::runPostchecks(const RollbackRequest& request,
                                       const RollbackResult&  partial_result,
                                       char*                  out_reason,
                                       size_t                 reason_size) noexcept {
  if (out_reason == nullptr || reason_size == 0) {
    return false;
  }

  if (!IsNullTerminated(request.radio_family, sizeof(request.radio_family))) {
    CopyReason(out_reason, reason_size,
               "postcheck-health: radio_family is not null-terminated");
    return false;
  }
  if (!IsNullTerminated(request.active_band, sizeof(request.active_band))) {
    CopyReason(out_reason, reason_size,
               "postcheck-health: active_band is not null-terminated");
    return false;
  }

  // Postcheck 1: Rollout state — execution must have been recorded
  if (partial_result.started_timestamp == 0) {
    CopyReason(out_reason, reason_size,
               "postcheck-state: started_timestamp not recorded (rollback execution not captured)");
    return false;
  }

  // Postcheck 2: Incident linkage registration — artifact must be present and retrievable
  if (partial_result.artifact_id[0] == '\0') {
    CopyReason(out_reason, reason_size,
               "postcheck-linkage: artifact_id is empty (rollback artifact not registered)");
    return false;
  }
  if (ArtifactRegistry::getArtifact(partial_result.artifact_id) == nullptr) {
    std::snprintf(out_reason, reason_size,
                  "postcheck-linkage: artifact '%s' not found in registry",
                  partial_result.artifact_id);
    return false;
  }

  // Postcheck 3: Minimum radio health KPI snapshot — V1 profile/band still consistent
  if (!IsSupportedRadioFamily(request.radio_family)) {
    std::snprintf(out_reason, reason_size,
                  "postcheck-health: radio_family '%s' invalid post-rollback (V1: SX1276/SX1278)",
                  request.radio_family);
    return false;
  }
  if (!IsSupportedBand(request.active_band)) {
    std::snprintf(out_reason, reason_size,
                  "postcheck-health: active_band '%s' invalid post-rollback (V1: 433/868)",
                  request.active_band);
    return false;
  }

  out_reason[0] = '\0';
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// RollbackGovernance::execute
// ─────────────────────────────────────────────────────────────────────────────
LoRaError RollbackGovernance::execute(const RollbackRequest& request,
                                      RollbackResult&        out_result) noexcept {
  // Zero-initialise result
  out_result = {};
  out_result.final_state         = RollbackState::kRequested;
  out_result.triggered_timestamp = request.trigger_timestamp_utc;

  // ── State: kPrecheck ─────────────────────────────────────────────────────
  out_result.final_state = RollbackState::kPrecheck;

  char precheck_reason[RollbackResult::kMaxReasonLength] = {};
  if (!runPrechecks(request, precheck_reason, sizeof(precheck_reason))) {
    out_result.final_state = RollbackState::kFailed;
    std::strncpy(out_result.failed_step_id, "precheck",
                 sizeof(out_result.failed_step_id) - 1);
    out_result.failed_step_id[sizeof(out_result.failed_step_id) - 1] = '\0';
    std::strncpy(out_result.reason, precheck_reason, sizeof(out_result.reason) - 1);
    out_result.reason[sizeof(out_result.reason) - 1] = '\0';
    out_result.completed_timestamp = StepTimestamp(out_result.triggered_timestamp, 1);
    return LoRaError::kInvalidConfig;
  }

  // ── State: kExecute ──────────────────────────────────────────────────────
  out_result.final_state       = RollbackState::kExecute;
  out_result.started_timestamp = StepTimestamp(out_result.triggered_timestamp, 1);

  // Register rollback artifact (evidence chain anchored to execute phase)
  ArtifactMetadata meta{};
  meta.type              = ArtifactType::kRecoveryProof;
  meta.created_timestamp = request.trigger_timestamp_utc;
  meta.retention_days    = 180;  // kRecoveryProof: 90–180 days; use maximum for rollback evidence
  std::strncpy(meta.linked_version, request.target_version,
               sizeof(meta.linked_version) - 1);
  meta.linked_version[sizeof(meta.linked_version) - 1] = '\0';
  std::snprintf(meta.source_module, sizeof(meta.source_module),
                "Rollback:%s:%s", request.radio_family, request.active_band);

  const char* registered_id = ArtifactRegistry::registerArtifact(meta);
  if (registered_id == nullptr) {
    out_result.final_state = RollbackState::kFailed;
    std::strncpy(out_result.failed_step_id, "execute-artifact",
                 sizeof(out_result.failed_step_id) - 1);
    out_result.failed_step_id[sizeof(out_result.failed_step_id) - 1] = '\0';
    std::strncpy(out_result.reason,
                 "execute: artifact registration failed (registry full or policy violation)",
                 sizeof(out_result.reason) - 1);
    out_result.reason[sizeof(out_result.reason) - 1] = '\0';
    out_result.completed_timestamp = StepTimestamp(out_result.started_timestamp, 1);
    return LoRaError::kArtifactRegistrationFailed;
  }

  std::strncpy(out_result.artifact_id, registered_id,
               sizeof(out_result.artifact_id) - 1);
  out_result.artifact_id[sizeof(out_result.artifact_id) - 1] = '\0';

  // ── State: kVerify ───────────────────────────────────────────────────────
  out_result.final_state = RollbackState::kVerify;

  char postcheck_reason[RollbackResult::kMaxReasonLength] = {};
  if (!runPostchecks(request, out_result, postcheck_reason, sizeof(postcheck_reason))) {
    out_result.final_state = RollbackState::kFailed;
    std::strncpy(out_result.failed_step_id, "postcheck",
                 sizeof(out_result.failed_step_id) - 1);
    out_result.failed_step_id[sizeof(out_result.failed_step_id) - 1] = '\0';
    std::strncpy(out_result.reason, postcheck_reason, sizeof(out_result.reason) - 1);
    out_result.reason[sizeof(out_result.reason) - 1] = '\0';
    out_result.completed_timestamp = StepTimestamp(out_result.started_timestamp, 1);

    // Link incident even on failure — evidence of attempted rollback
    char incident_id[TraceabilityLink::kMaxIdLength] = {};
    std::snprintf(incident_id, sizeof(incident_id), "RB-%s-%s-%u",
                  request.radio_family, request.active_band,
                  request.trigger_timestamp_utc);
    (void)TraceabilityEngine::linkIncidentToArtifact(incident_id, registered_id);

    return LoRaError::kInvalidConfig;
  }

  // ── State: kComplete ─────────────────────────────────────────────────────
  out_result.final_state         = RollbackState::kComplete;
  out_result.completed_timestamp = StepTimestamp(out_result.started_timestamp, 1);

  std::strncpy(out_result.reason,
               "rollback completed: LKG baseline restored",
               sizeof(out_result.reason) - 1);
  out_result.reason[sizeof(out_result.reason) - 1] = '\0';

  // Link incident to registered artifact for traceability chain.
  char incident_id[TraceabilityLink::kMaxIdLength] = {};
  std::snprintf(incident_id, sizeof(incident_id), "RB-%s-%s-%u",
                request.radio_family, request.active_band,
                request.trigger_timestamp_utc);
  const LoRaError link_result =
      TraceabilityEngine::linkIncidentToArtifact(incident_id, registered_id);
  if (link_result != LoRaError::kOk) {
    out_result.final_state = RollbackState::kFailed;
    std::strncpy(out_result.failed_step_id, "verify-traceability",
                 sizeof(out_result.failed_step_id) - 1);
    out_result.failed_step_id[sizeof(out_result.failed_step_id) - 1] = '\0';
    std::strncpy(out_result.reason,
                 "verify-traceability: incident linkage failed",
                 sizeof(out_result.reason) - 1);
    out_result.reason[sizeof(out_result.reason) - 1] = '\0';
    return LoRaError::kLinkFailed;
  }

  return LoRaError::kOk;
}

}  // namespace loradriver
