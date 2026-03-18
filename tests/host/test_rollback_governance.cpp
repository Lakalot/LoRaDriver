#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "loradriver/rollback_governance.hpp"
#include "loradriver/ota_gate.hpp"
#include "loradriver/artifact_governance.hpp"
#include "loradriver/versioning.hpp"

namespace {

using loradriver::ArtifactRegistry;
using loradriver::OtaDecisionRationale;
using loradriver::OtaRolloutDecision;
using loradriver::RollbackGovernance;
using loradriver::RollbackRequest;
using loradriver::RollbackResult;
using loradriver::RollbackState;
using loradriver::RollbackTriggerReason;
using loradriver::TraceabilityEngine;
using loradriver::LoRaError;

// ─────────────────────────────────────────────────────────────────────────────
// Helper: build a valid rollback request for SX1276/868
// ─────────────────────────────────────────────────────────────────────────────
RollbackRequest MakeValidRequest() {
  RollbackRequest req{};
  std::strncpy(req.target_version, "1.1.0", sizeof(req.target_version) - 1);
  std::strncpy(req.radio_family,   "SX1276", sizeof(req.radio_family) - 1);
  std::strncpy(req.active_band,    "868", sizeof(req.active_band) - 1);
  req.trigger_reason         = RollbackTriggerReason::kPolicyInitiated;
  req.trigger_timestamp_utc  = 2000;
  return req;
}

// Helper: build a kBlock OTA rationale (at least one failed gate)
OtaDecisionRationale MakeBlockRationale() {
  OtaDecisionRationale r{};
  r.failed_gate_count = 1;
  std::strncpy(r.failed_gate_ids[0], "TXRX-001", loradriver::GateRule::kMaxIdLength - 1);
  r.failed_gate_actuals[0]    = 80.0f;
  r.failed_gate_thresholds[0] = 99.0f;
  std::strncpy(r.reason, "blocking gate TXRX-001 failed", sizeof(r.reason) - 1);
  return r;
}

// ─────────────────────────────────────────────────────────────────────────────
// Regression: enum values are stable (serialisation contract)
// ─────────────────────────────────────────────────────────────────────────────
bool TestRollbackTriggerReasonEnumValuesAreStable() {
  if (static_cast<uint8_t>(RollbackTriggerReason::kOperatorInitiated) != 0) return false;
  if (static_cast<uint8_t>(RollbackTriggerReason::kPolicyInitiated)   != 1) return false;
  return true;
}

bool TestRollbackStateEnumValuesAreStable() {
  if (static_cast<uint8_t>(RollbackState::kRequested) != 0) return false;
  if (static_cast<uint8_t>(RollbackState::kPrecheck)  != 1) return false;
  if (static_cast<uint8_t>(RollbackState::kExecute)   != 2) return false;
  if (static_cast<uint8_t>(RollbackState::kVerify)    != 3) return false;
  if (static_cast<uint8_t>(RollbackState::kComplete)  != 4) return false;
  if (static_cast<uint8_t>(RollbackState::kFailed)    != 5) return false;
  return true;
}

bool TestRollbackRequestIsTriviallyCopyable() {
  static_assert(std::is_trivially_copyable<RollbackRequest>::value,
                "RollbackRequest must be trivially copyable (no heap)");
  return true;
}

bool TestRollbackResultIsTriviallyCopyable() {
  static_assert(std::is_trivially_copyable<RollbackResult>::value,
                "RollbackResult must be trivially copyable (no heap)");
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 2: evaluateTriggerPolicy – OTA gate output integration
// ─────────────────────────────────────────────────────────────────────────────
bool TestBlockDecisionTriggersPolicyRollback() {
  OtaDecisionRationale r = MakeBlockRationale();
  return RollbackGovernance::evaluateTriggerPolicy(OtaRolloutDecision::kBlock, r);
}

bool TestHoldWithQualityIssueDoesNotTriggerRollback() {
  OtaDecisionRationale r{};
  r.quality_issue = true;
  std::strncpy(r.reason, "missing firmware_version", sizeof(r.reason) - 1);
  return !RollbackGovernance::evaluateTriggerPolicy(OtaRolloutDecision::kHold, r);
}

bool TestHoldWithTrendRiskDoesNotTriggerPolicyRollback() {
  OtaDecisionRationale r{};
  r.trend_risk = true;
  std::strncpy(r.reason, "tx_success_rate degraded", sizeof(r.reason) - 1);
  return !RollbackGovernance::evaluateTriggerPolicy(OtaRolloutDecision::kHold, r);
}

bool TestAllowDecisionDoesNotTriggerRollback() {
  OtaDecisionRationale r{};
  std::strncpy(r.reason, "all gates passed", sizeof(r.reason) - 1);
  return !RollbackGovernance::evaluateTriggerPolicy(OtaRolloutDecision::kAllow, r);
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 3: runPrechecks – standalone API tests
// ─────────────────────────────────────────────────────────────────────────────
bool TestRunPrechecksPassesForValidRequest() {
  RollbackRequest req = MakeValidRequest();
  char reason[128] = {};
  return RollbackGovernance::runPrechecks(req, reason, sizeof(reason));
}

bool TestRunPrechecksFailsForMissingTargetVersion() {
  RollbackRequest req = MakeValidRequest();
  req.target_version[0] = '\0';
  char reason[128] = {};
  bool ok = RollbackGovernance::runPrechecks(req, reason, sizeof(reason));
  if (ok) return false;
  return reason[0] != '\0';  // must provide reason
}

bool TestRunPrechecksFailsForEmptyRadioFamily() {
  RollbackRequest req = MakeValidRequest();
  req.radio_family[0] = '\0';
  char reason[128] = {};
  bool ok = RollbackGovernance::runPrechecks(req, reason, sizeof(reason));
  if (ok) return false;
  return reason[0] != '\0';
}

bool TestRunPrechecksFailsForInvalidRadioFamily() {
  RollbackRequest req = MakeValidRequest();
  std::strncpy(req.radio_family, "SX1262", sizeof(req.radio_family) - 1);
  char reason[128] = {};
  bool ok = RollbackGovernance::runPrechecks(req, reason, sizeof(reason));
  if (ok) return false;
  return reason[0] != '\0';
}

bool TestRunPrechecksFailsForEmptyActiveBand() {
  RollbackRequest req = MakeValidRequest();
  req.active_band[0] = '\0';
  char reason[128] = {};
  bool ok = RollbackGovernance::runPrechecks(req, reason, sizeof(reason));
  if (ok) return false;
  return reason[0] != '\0';
}

bool TestRunPrechecksFailsForInvalidActiveBand() {
  RollbackRequest req = MakeValidRequest();
  std::strncpy(req.active_band, "915", sizeof(req.active_band) - 1);
  char reason[128] = {};
  bool ok = RollbackGovernance::runPrechecks(req, reason, sizeof(reason));
  if (ok) return false;
  return reason[0] != '\0';
}

bool TestRunPrechecksFailsForZeroTimestamp() {
  RollbackRequest req = MakeValidRequest();
  req.trigger_timestamp_utc = 0;
  char reason[128] = {};
  bool ok = RollbackGovernance::runPrechecks(req, reason, sizeof(reason));
  if (ok) return false;
  return reason[0] != '\0';
}

bool TestRunPrechecksAcceptsSX1278And433Band() {
  RollbackRequest req = MakeValidRequest();
  std::strncpy(req.radio_family, "SX1278", sizeof(req.radio_family) - 1);
  std::strncpy(req.active_band,  "433",    sizeof(req.active_band)   - 1);
  char reason[128] = {};
  return RollbackGovernance::runPrechecks(req, reason, sizeof(reason));
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 3: runPostchecks – standalone API tests
// ─────────────────────────────────────────────────────────────────────────────
bool TestRunPostchecksPassesForValidState() {
  ArtifactRegistry::clear();

  RollbackRequest req = MakeValidRequest();
  RollbackResult  partial{};
  partial.started_timestamp = req.trigger_timestamp_utc;

  // Register a real artifact so getArtifact finds it
  loradriver::ArtifactMetadata meta{};
  meta.type              = loradriver::ArtifactType::kRecoveryProof;
  meta.created_timestamp = req.trigger_timestamp_utc;
  meta.retention_days    = 180;
  std::strncpy(meta.linked_version, req.target_version, sizeof(meta.linked_version) - 1);
  std::strncpy(meta.source_module, "TestPostcheck", sizeof(meta.source_module) - 1);
  const char* id = ArtifactRegistry::registerArtifact(meta);
  if (!id) return false;
  std::strncpy(partial.artifact_id, id, sizeof(partial.artifact_id) - 1);

  char reason[128] = {};
  return RollbackGovernance::runPostchecks(req, partial, reason, sizeof(reason));
}

bool TestRunPostchecksFailsForZeroStartedTimestamp() {
  ArtifactRegistry::clear();

  RollbackRequest req = MakeValidRequest();
  RollbackResult  partial{};
  partial.started_timestamp = 0;  // not set
  std::strncpy(partial.artifact_id, "ARTIFACT-00000001", sizeof(partial.artifact_id) - 1);

  char reason[128] = {};
  bool ok = RollbackGovernance::runPostchecks(req, partial, reason, sizeof(reason));
  if (ok) return false;
  return reason[0] != '\0';
}

bool TestRunPostchecksFailsForEmptyArtifactId() {
  RollbackRequest req = MakeValidRequest();
  RollbackResult  partial{};
  partial.started_timestamp = req.trigger_timestamp_utc;
  partial.artifact_id[0]    = '\0';  // empty

  char reason[128] = {};
  bool ok = RollbackGovernance::runPostchecks(req, partial, reason, sizeof(reason));
  if (ok) return false;
  return reason[0] != '\0';
}

bool TestRunPostchecksFailsWhenArtifactNotInRegistry() {
  ArtifactRegistry::clear();

  RollbackRequest req = MakeValidRequest();
  RollbackResult  partial{};
  partial.started_timestamp = req.trigger_timestamp_utc;
  std::strncpy(partial.artifact_id, "NONEXISTENT-42", sizeof(partial.artifact_id) - 1);

  char reason[128] = {};
  bool ok = RollbackGovernance::runPostchecks(req, partial, reason, sizeof(reason));
  if (ok) return false;
  return reason[0] != '\0';
}

bool TestRunPostchecksFailsForInvalidRadioFamilyPostExecution() {
  ArtifactRegistry::clear();

  RollbackRequest req = MakeValidRequest();
  std::strncpy(req.radio_family, "SX1299", sizeof(req.radio_family) - 1);  // invalid

  RollbackResult partial{};
  partial.started_timestamp = req.trigger_timestamp_utc;

  // Register a real artifact
  loradriver::ArtifactMetadata meta{};
  meta.type              = loradriver::ArtifactType::kRecoveryProof;
  meta.created_timestamp = req.trigger_timestamp_utc;
  meta.retention_days    = 180;
  std::strncpy(meta.linked_version, "1.1.0", sizeof(meta.linked_version) - 1);
  std::strncpy(meta.source_module, "TestPostcheckBadFamily", sizeof(meta.source_module) - 1);
  const char* id = ArtifactRegistry::registerArtifact(meta);
  if (!id) return false;
  std::strncpy(partial.artifact_id, id, sizeof(partial.artifact_id) - 1);

  char reason[128] = {};
  bool ok = RollbackGovernance::runPostchecks(req, partial, reason, sizeof(reason));
  if (ok) return false;
  return reason[0] != '\0';
}

bool TestRunPostchecksFailsForInvalidActiveBandPostExecution() {
  ArtifactRegistry::clear();

  RollbackRequest req = MakeValidRequest();
  std::strncpy(req.active_band, "915", sizeof(req.active_band) - 1);  // invalid

  RollbackResult partial{};
  partial.started_timestamp = req.trigger_timestamp_utc;

  loradriver::ArtifactMetadata meta{};
  meta.type              = loradriver::ArtifactType::kRecoveryProof;
  meta.created_timestamp = req.trigger_timestamp_utc;
  meta.retention_days    = 180;
  std::strncpy(meta.linked_version, "1.1.0", sizeof(meta.linked_version) - 1);
  std::strncpy(meta.source_module, "TestPostcheckBadBand", sizeof(meta.source_module) - 1);
  const char* id = ArtifactRegistry::registerArtifact(meta);
  if (!id) return false;
  std::strncpy(partial.artifact_id, id, sizeof(partial.artifact_id) - 1);

  char reason[128] = {};
  bool ok = RollbackGovernance::runPostchecks(req, partial, reason, sizeof(reason));
  if (ok) return false;
  return reason[0] != '\0';
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 1/3: execute – happy path (allow path)
// ─────────────────────────────────────────────────────────────────────────────
bool TestExecuteReturnsOkForValidRequest() {
  ArtifactRegistry::clear();
  TraceabilityEngine::clear();

  RollbackRequest req = MakeValidRequest();
  RollbackResult  result{};
  LoRaError err = RollbackGovernance::execute(req, result);
  if (err != LoRaError::kOk) return false;
  if (result.final_state != RollbackState::kComplete) return false;
  return true;
}

bool TestExecutePopulatesArtifactId() {
  ArtifactRegistry::clear();
  TraceabilityEngine::clear();

  RollbackRequest req = MakeValidRequest();
  RollbackResult  result{};
  RollbackGovernance::execute(req, result);
  return result.artifact_id[0] != '\0';
}

bool TestExecutePopulatesTimelineCheckpoints() {
  ArtifactRegistry::clear();
  TraceabilityEngine::clear();

  RollbackRequest req = MakeValidRequest();
  RollbackResult  result{};
  RollbackGovernance::execute(req, result);

  if (result.triggered_timestamp == 0) return false;
  if (result.started_timestamp   == 0) return false;
  if (result.completed_timestamp == 0) return false;
  return true;
}

bool TestExecutePopulatesNonEmptyReason() {
  ArtifactRegistry::clear();
  TraceabilityEngine::clear();

  RollbackRequest req = MakeValidRequest();
  RollbackResult  result{};
  RollbackGovernance::execute(req, result);
  return result.reason[0] != '\0';
}

bool TestExecuteTriggeredTimestampMatchesRequest() {
  ArtifactRegistry::clear();
  TraceabilityEngine::clear();

  RollbackRequest req = MakeValidRequest();
  RollbackResult  result{};
  RollbackGovernance::execute(req, result);
  return result.triggered_timestamp == req.trigger_timestamp_utc;
}

bool TestExecuteRegistersArtifactInRegistry() {
  ArtifactRegistry::clear();
  TraceabilityEngine::clear();

  RollbackRequest req = MakeValidRequest();
  RollbackResult  result{};
  RollbackGovernance::execute(req, result);

  if (result.artifact_id[0] == '\0') return false;
  const loradriver::ArtifactMetadata* meta = ArtifactRegistry::getArtifact(result.artifact_id);
  if (meta == nullptr) return false;
  if (meta->type != loradriver::ArtifactType::kRecoveryProof) return false;
  return true;
}

bool TestExecuteLinksTraceabilityIncident() {
  ArtifactRegistry::clear();
  TraceabilityEngine::clear();

  RollbackRequest req = MakeValidRequest();
  RollbackResult  result{};
  LoRaError err = RollbackGovernance::execute(req, result);
  if (err != LoRaError::kOk) return false;

  // Reconstruct the incident ID used by execute()
  char incident_id[loradriver::TraceabilityLink::kMaxIdLength] = {};
  std::snprintf(incident_id, sizeof(incident_id), "RB-%s-%s-%u",
                req.radio_family, req.active_band, req.trigger_timestamp_utc);

  std::array<loradriver::TraceabilityLink, 16> chain{};
  size_t count = TraceabilityEngine::getFullTraceChain(incident_id, chain);
  if (count == 0) return false;
  return std::strcmp(chain[0].target_artifact, result.artifact_id) == 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 1/3: execute – failure paths
// ─────────────────────────────────────────────────────────────────────────────
bool TestExecuteFailsForMissingLkgCandidate() {
  ArtifactRegistry::clear();
  TraceabilityEngine::clear();

  RollbackRequest req = MakeValidRequest();
  req.target_version[0] = '\0';  // no LKG candidate
  RollbackResult result{};
  LoRaError err = RollbackGovernance::execute(req, result);
  if (err == LoRaError::kOk) return false;
  if (result.final_state != RollbackState::kFailed) return false;
  if (std::strcmp(result.failed_step_id, "precheck") != 0) return false;
  return result.reason[0] != '\0';
}

bool TestExecuteFailsForIncompatibleRadioFamily() {
  ArtifactRegistry::clear();
  TraceabilityEngine::clear();

  RollbackRequest req = MakeValidRequest();
  std::strncpy(req.radio_family, "SX1262", sizeof(req.radio_family) - 1);
  RollbackResult result{};
  LoRaError err = RollbackGovernance::execute(req, result);
  if (err == LoRaError::kOk) return false;
  if (result.final_state != RollbackState::kFailed) return false;
  if (std::strcmp(result.failed_step_id, "precheck") != 0) return false;
  return result.reason[0] != '\0';
}

bool TestExecuteFailsForIncompatibleBand() {
  ArtifactRegistry::clear();
  TraceabilityEngine::clear();

  RollbackRequest req = MakeValidRequest();
  std::strncpy(req.active_band, "915", sizeof(req.active_band) - 1);
  RollbackResult result{};
  LoRaError err = RollbackGovernance::execute(req, result);
  if (err == LoRaError::kOk) return false;
  if (result.final_state != RollbackState::kFailed) return false;
  if (std::strcmp(result.failed_step_id, "precheck") != 0) return false;
  return result.reason[0] != '\0';
}

bool TestExecuteFailsForZeroTriggerTimestamp() {
  ArtifactRegistry::clear();
  TraceabilityEngine::clear();

  RollbackRequest req = MakeValidRequest();
  req.trigger_timestamp_utc = 0;
  RollbackResult result{};
  LoRaError err = RollbackGovernance::execute(req, result);
  if (err == LoRaError::kOk) return false;
  if (result.final_state != RollbackState::kFailed) return false;
  return true;
}

bool TestExecuteReasonFieldIsStableAfterSuccess() {
  ArtifactRegistry::clear();
  TraceabilityEngine::clear();

  RollbackRequest req = MakeValidRequest();
  RollbackResult  result{};
  RollbackGovernance::execute(req, result);
  // reason must be deterministic and contain meaningful text
  if (result.reason[0] == '\0') return false;
  return true;
}

bool TestExecuteReasonFieldIsStableAfterPrecheckFailure() {
  ArtifactRegistry::clear();
  TraceabilityEngine::clear();

  RollbackRequest req = MakeValidRequest();
  req.target_version[0] = '\0';
  RollbackResult result{};
  RollbackGovernance::execute(req, result);
  if (result.reason[0] == '\0') return false;
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 4: execute – traceability registration failure resilience
// ─────────────────────────────────────────────────────────────────────────────
bool TestExecuteFailsWhenTraceabilityIsFull() {
  ArtifactRegistry::clear();
  TraceabilityEngine::clear();

  // Fill TraceabilityEngine to capacity
  for (size_t i = 0; i < loradriver::TraceabilityEngine::kMaxLinks; ++i) {
    char src[loradriver::TraceabilityLink::kMaxIdLength];
    char tgt[loradriver::TraceabilityLink::kMaxIdLength];
    std::snprintf(src, sizeof(src), "FILL-SRC-%03zu", i);
    std::snprintf(tgt, sizeof(tgt), "FILL-TGT-%03zu", i);
    (void)TraceabilityEngine::linkIncidentToArtifact(src, tgt);
  }

  RollbackRequest req = MakeValidRequest();
  RollbackResult  result{};
  LoRaError err = RollbackGovernance::execute(req, result);
  if (err != LoRaError::kLinkFailed) return false;
  if (result.final_state != RollbackState::kFailed) return false;
  if (std::strcmp(result.failed_step_id, "verify-traceability") != 0) return false;
  if (result.completed_timestamp == 0) return false;
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 2: policy precedence – data quality hold does not trigger rollback
// ─────────────────────────────────────────────────────────────────────────────
bool TestDataQualityHoldPrecedesBlockTriggerPolicy() {
  // Hold with quality_issue must never auto-trigger rollback.
  // Block requires failed gate evidence to trigger policy rollback.
  OtaDecisionRationale r = MakeBlockRationale();
  r.quality_issue = true;
  OtaDecisionRationale hold_quality{};
  hold_quality.quality_issue = true;
  if (RollbackGovernance::evaluateTriggerPolicy(OtaRolloutDecision::kHold, hold_quality)) return false;
  if (RollbackGovernance::evaluateTriggerPolicy(OtaRolloutDecision::kBlock, r)) return false;

  OtaDecisionRationale block_with_evidence = MakeBlockRationale();
  if (!RollbackGovernance::evaluateTriggerPolicy(OtaRolloutDecision::kBlock, block_with_evidence)) return false;
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 1: operator-initiated rollback – trigger reason field
// ─────────────────────────────────────────────────────────────────────────────
bool TestOperatorInitiatedRollbackSucceeds() {
  ArtifactRegistry::clear();
  TraceabilityEngine::clear();

  RollbackRequest req = MakeValidRequest();
  req.trigger_reason = RollbackTriggerReason::kOperatorInitiated;

  RollbackResult result{};
  LoRaError err = RollbackGovernance::execute(req, result);
  if (err != LoRaError::kOk) return false;
  if (result.final_state != RollbackState::kComplete) return false;
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Regression: artifact linked_version matches request target_version
// ─────────────────────────────────────────────────────────────────────────────
bool TestRollbackArtifactLinkedVersionMatchesTargetVersion() {
  ArtifactRegistry::clear();
  TraceabilityEngine::clear();

  RollbackRequest req = MakeValidRequest();
  RollbackResult  result{};
  RollbackGovernance::execute(req, result);

  if (result.artifact_id[0] == '\0') return false;
  const loradriver::ArtifactMetadata* meta = ArtifactRegistry::getArtifact(result.artifact_id);
  if (meta == nullptr) return false;
  return std::strcmp(meta->linked_version, req.target_version) == 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test runner
// ─────────────────────────────────────────────────────────────────────────────
#define RUN_TEST(fn)                                          \
  if (!(fn)()) {                                             \
    std::fprintf(stderr, "FAIL: %s\n", #fn);                \
    return EXIT_FAILURE;                                     \
  }

int RunRollbackGovernanceTests() {
  // Regression: enum/struct stability
  RUN_TEST(TestRollbackTriggerReasonEnumValuesAreStable)
  RUN_TEST(TestRollbackStateEnumValuesAreStable)
  RUN_TEST(TestRollbackRequestIsTriviallyCopyable)
  RUN_TEST(TestRollbackResultIsTriviallyCopyable)

  // Task 2: trigger policy
  RUN_TEST(TestBlockDecisionTriggersPolicyRollback)
  RUN_TEST(TestHoldWithQualityIssueDoesNotTriggerRollback)
  RUN_TEST(TestHoldWithTrendRiskDoesNotTriggerPolicyRollback)
  RUN_TEST(TestAllowDecisionDoesNotTriggerRollback)
  RUN_TEST(TestDataQualityHoldPrecedesBlockTriggerPolicy)

  // Task 3: prechecks standalone API
  RUN_TEST(TestRunPrechecksPassesForValidRequest)
  RUN_TEST(TestRunPrechecksFailsForMissingTargetVersion)
  RUN_TEST(TestRunPrechecksFailsForEmptyRadioFamily)
  RUN_TEST(TestRunPrechecksFailsForInvalidRadioFamily)
  RUN_TEST(TestRunPrechecksFailsForEmptyActiveBand)
  RUN_TEST(TestRunPrechecksFailsForInvalidActiveBand)
  RUN_TEST(TestRunPrechecksFailsForZeroTimestamp)
  RUN_TEST(TestRunPrechecksAcceptsSX1278And433Band)

  // Task 3: postchecks standalone API
  RUN_TEST(TestRunPostchecksPassesForValidState)
  RUN_TEST(TestRunPostchecksFailsForZeroStartedTimestamp)
  RUN_TEST(TestRunPostchecksFailsForEmptyArtifactId)
  RUN_TEST(TestRunPostchecksFailsWhenArtifactNotInRegistry)
  RUN_TEST(TestRunPostchecksFailsForInvalidRadioFamilyPostExecution)
  RUN_TEST(TestRunPostchecksFailsForInvalidActiveBandPostExecution)

  // Task 1/3: execute – happy path
  RUN_TEST(TestExecuteReturnsOkForValidRequest)
  RUN_TEST(TestExecutePopulatesArtifactId)
  RUN_TEST(TestExecutePopulatesTimelineCheckpoints)
  RUN_TEST(TestExecutePopulatesNonEmptyReason)
  RUN_TEST(TestExecuteTriggeredTimestampMatchesRequest)

  // Task 1: operator-initiated rollback
  RUN_TEST(TestOperatorInitiatedRollbackSucceeds)

  // Task 4: governance/artifact evidence
  RUN_TEST(TestExecuteRegistersArtifactInRegistry)
  RUN_TEST(TestExecuteLinksTraceabilityIncident)
  RUN_TEST(TestRollbackArtifactLinkedVersionMatchesTargetVersion)

  // Task 1/3: execute – failure paths
  RUN_TEST(TestExecuteFailsForMissingLkgCandidate)
  RUN_TEST(TestExecuteFailsForIncompatibleRadioFamily)
  RUN_TEST(TestExecuteFailsForIncompatibleBand)
  RUN_TEST(TestExecuteFailsForZeroTriggerTimestamp)

  // Reason field stability
  RUN_TEST(TestExecuteReasonFieldIsStableAfterSuccess)
  RUN_TEST(TestExecuteReasonFieldIsStableAfterPrecheckFailure)

  // Task 4: traceability registration failure path
  RUN_TEST(TestExecuteFailsWhenTraceabilityIsFull)

  return EXIT_SUCCESS;
}

}  // namespace

int main() {
  return RunRollbackGovernanceTests();
}
