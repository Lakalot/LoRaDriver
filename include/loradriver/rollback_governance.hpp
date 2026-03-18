#pragma once

#include <cstdint>
#include <type_traits>

#include <loradriver/lora_error.hpp>
#include <loradriver/ota_gate.hpp>

namespace loradriver {

// ─────────────────────────────────────────────────────────────────────────────
// Rollback trigger reason (why rollback was initiated)
// ─────────────────────────────────────────────────────────────────────────────
enum class RollbackTriggerReason : uint8_t {
  kOperatorInitiated = 0,  ///< Manual operator decision to revert
  kPolicyInitiated   = 1,  ///< Triggered by kBlock OTA gate decision
};

// ─────────────────────────────────────────────────────────────────────────────
// Rollback execution states — deterministic FSM, no ambiguous branching (AC: 2)
// Transitions: requested → precheck → execute → verify → complete/failed
// ─────────────────────────────────────────────────────────────────────────────
enum class RollbackState : uint8_t {
  kRequested = 0,  ///< Rollback requested; not yet started
  kPrecheck  = 1,  ///< Validating LKG candidate and V1 profile/band compatibility
  kExecute   = 2,  ///< Executing rollback to LKG baseline
  kVerify    = 3,  ///< Running postchecks to confirm rollback outcome
  kComplete  = 4,  ///< Rollback completed successfully
  kFailed    = 5,  ///< Rollback failed at a specific step (see failed_step_id)
};

// ─────────────────────────────────────────────────────────────────────────────
// Rollback request — fixed-size, no heap allocation (AC: 2)
// All required fields must be populated; missing fields cause precheck failure.
// ─────────────────────────────────────────────────────────────────────────────
struct RollbackRequest {
  static constexpr size_t kMaxVersionLength = 16;  ///< SemVer string capacity
  static constexpr size_t kMaxFamilyLength  = 16;  ///< e.g. "SX1276"
  static constexpr size_t kMaxBandLength    = 8;   ///< e.g. "433" or "868"

  char                  target_version[kMaxVersionLength];  ///< LKG baseline firmware version
  char                  radio_family[kMaxFamilyLength];     ///< "SX1276" or "SX1278" (V1 only)
  char                  active_band[kMaxBandLength];        ///< "433" or "868" (V1 only)
  RollbackTriggerReason trigger_reason;                     ///< Why rollback was initiated
  uint32_t              trigger_timestamp_utc;              ///< UTC epoch (0 = invalid)
};

static_assert(std::is_trivially_copyable<RollbackRequest>::value,
              "RollbackRequest must be trivially copyable (no heap)");

// ─────────────────────────────────────────────────────────────────────────────
// Rollback result — machine-readable outcome with evidence chain (AC: 1, 3)
// ─────────────────────────────────────────────────────────────────────────────
struct RollbackResult {
  static constexpr size_t kMaxArtifactIdLength = 32;
  static constexpr size_t kMaxStepIdLength     = 16;
  static constexpr size_t kMaxReasonLength     = 128;

  RollbackState final_state;                         ///< kComplete or kFailed
  char          failed_step_id[kMaxStepIdLength];    ///< Step that failed, or empty on success
  char          reason[kMaxReasonLength];            ///< Human-readable outcome summary
  char          artifact_id[kMaxArtifactIdLength];   ///< Registered ArtifactRegistry ID

  // Timeline checkpoints for post-incident review (UTC epoch; 0 = not reached)
  uint32_t triggered_timestamp;   ///< When rollback was triggered
  uint32_t started_timestamp;     ///< When execution phase began
  uint32_t completed_timestamp;   ///< When execution phase ended (success or failure)
};

static_assert(std::is_trivially_copyable<RollbackResult>::value,
              "RollbackResult must be trivially copyable (no heap)");

// ─────────────────────────────────────────────────────────────────────────────
// Rollback governance engine — wraps OtaGateEngine evidence for rollback policy
// All methods are allocation-free and noexcept (AC: 1, 2, 3)
// ─────────────────────────────────────────────────────────────────────────────
class RollbackGovernance {
 public:
  /// Evaluate whether OTA gate output warrants a policy-initiated rollback.
  /// Data-quality holds (quality_issue == true) do NOT trigger rollback — they
  /// represent potential false positives and require manual escalation.
  ///
  /// @param decision    OTA gate engine rollout decision
  /// @param rationale   Machine-readable decision evidence from OtaGateEngine
  /// @returns true only if kBlock decision (failing gate threshold) detected
  [[nodiscard]] static bool evaluateTriggerPolicy(
      OtaRolloutDecision          decision,
      const OtaDecisionRationale& rationale) noexcept;

  /// Execute the rollback state machine end-to-end:
  ///   requested → precheck → execute → verify → complete/failed
  ///
  /// Registers a kRecoveryProof artifact and links traceability chain.
  /// Traceability linkage failure is explicit and fails the rollback result.
  ///
  /// @param request      Populated rollback request with LKG candidate context
  /// @param out_result   Fully populated result with state, reason, evidence
  /// @returns kOk on success; kInvalidConfig on precheck/postcheck failure;
  ///          kArtifactRegistrationFailed if registry is full;
  ///          kLinkFailed if incident-to-artifact linkage cannot be recorded
  [[nodiscard]] static LoRaError execute(
      const RollbackRequest& request,
      RollbackResult&        out_result) noexcept;

  /// Run prechecks only — testable independent of full execute() flow.
  /// Validates: LKG candidate presence, V1 profile/band compatibility,
  /// and artifact integrity prerequisites (non-zero trigger timestamp).
  ///
  /// @returns true if all prechecks pass; false + reason on first failure
  [[nodiscard]] static bool runPrechecks(
      const RollbackRequest& request,
      char*                  out_reason,
      size_t                 reason_size) noexcept;

  /// Run postchecks only — testable independent of full execute() flow.
  /// Validates: execution state captured, rollback artifact registered and
  /// retrievable, and V1 profile/band still valid post-execution.
  ///
  /// @param partial_result  Result struct populated through execute phase
  /// @returns true if all postchecks pass; false + reason on first failure
  [[nodiscard]] static bool runPostchecks(
      const RollbackRequest& request,
      const RollbackResult&  partial_result,
      char*                  out_reason,
      size_t                 reason_size) noexcept;
};

}  // namespace loradriver
