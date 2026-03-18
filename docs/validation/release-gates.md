# Release Gates and Qualification Requirements

This document defines the quality gates and qualification requirements for LoRaDriver releases.

## Release Gate Overview

Release gates ensure that only qualified profiles are promoted to production. The gates are enforced through:

1. **Profile Qualification Matrix** - Defines which profiles are validated and blocking
2. **CI Quality Gates** - Automated tests that must pass before release
3. **Governance Workflow** - Explicit approval for status changes

## Blocking Release Gates

### Gate 1: Profile Validation Gate

**Condition**: All **validated** profiles must pass their required tests.

```cpp
// Enforced by ProfileQualificationMatrix::isReleaseBlocking()
// Returns true only for kValidated profiles
```

| Status | Blocks Release | Required Action |
|--------|----------------|-----------------|
| Validated | **YES** | Must pass 100% of required tests |
| Secondary | No | Warning only |
| Deferred | No | Explicitly excluded |
| Experimental | No | Not for production |

### Gate 2: Test Pass Rate Gate

**Condition**: Each validated profile must achieve the configured pass rate threshold.

| Profile Type | Minimum Pass Rate |
|--------------|-------------------|
| Validated | 100% |
| Secondary | 80% |
| Experimental | N/A |

### Gate 3: Matrix Completeness Gate

**Condition**: All V1 profile combinations must be present in the qualification matrix.

**V1 Required Combinations**: 8 (SX1276/SX1278 x 433/868 x DIO0/DIO0+DIO1)

---

## CI Quality Gates (Story 3.2)

CI Quality Gates provide automated, blocking enforcement of quality criteria for all pull requests and releases.

### Gate Severity Taxonomy

| Severity | Behavior | Code |
|----------|----------|------|
| `kBlocking` | Merge/release denied on failure | 0 |
| `kWarning` | Logged, does not block | 1 |
| `kAdvisory` | Informational only | 2 |

### V1 Blocking Quality Gates

The following 11 gates are **blocking** for all V1 releases:

| Gate ID | Category | Metric | Threshold | Description |
|---------|----------|--------|-----------|-------------|
| INIT-001 | Init | init_success_rate | >= 99% | Radio init success rate |
| INIT-002 | Init | init_time_p99 | <= 500ms | Init latency P99 |
| TXRX-001 | TxRx | tx_success_rate | >= 99% | TX success rate |
| TXRX-002 | TxRx | rx_success_rate | >= 98% | RX success rate |
| IRQ-001 | Irq | irq_handled_rate | >= 99.9% | IRQ handling rate |
| IRQ-002 | Irq | irq_overflow_count | == 0 | No IRQ overflow allowed |
| TIMEOUT-001 | Timeout | recovery_success_rate | >= 99% | Timeout recovery rate |
| RECOVERY-001 | Recovery | sleep_wakeup_success | >= 99% | Sleep/wakeup recovery |
| RECOVERY-002 | Recovery | timeout_recovery_success_rate | >= 100% | All profiles have recovery evidence |
| INTEGRATION-001 | Integration | fsm_deadlock_count | == 0 | No FSM deadlock allowed |
| NONREG-001 | Integration | suite_pass_rate | >= 100% | All non-regression cases pass |

### Gate Evaluation API

```cpp
#include <loradriver/ci_gates.hpp>

// Evaluate a single gate
loradriver::GateResult result = CiGateEngine::evaluateGate("INIT-001", 99.5f);

// Evaluate all gates with test results
float values[CiGateEngine::kGateRulesCount] = { /* ... */ };
GateReport report;
CiGateEngine::evaluateAllGates(values, CiGateEngine::kGateRulesCount, report);

// Check if release is blocked
if (CiGateEngine::isReleaseBlocked(report)) {
    // Merge denied
}

// Profile-aware evaluation (Story 3.1 integration)
HardwareProfile profile{Chip::kSx1276, Band::k868, DioRouting::kDio0Only};
GateResult result = CiGateEngine::evaluateForProfile(profile, values, count, report);
```

### Channel Policy

Both **regular** and **hotfix** release channels use the **same 11 blocking gates**. Urgency does not bypass quality checks.

| Channel | Blocking Gates | Waiver Approvers |
|---------|----------------|------------------|
| Regular | 11 (all) | release-owner, tech-lead |
| Hotfix | 11 (all) | release-owner, tech-lead, incident-commander |

### Waiver Workflow

Waivers allow temporary bypass of blocking gates with documented approval:

1. **Request**: `requestWaiver(gate_id, justification)`
2. **Approve**: `approveWaiver(waiver_id, approver, approver_id)`
3. **Validate**: `isWaiverValid(waiver_id)` - checks approval and expiry

```cpp
uint32_t waiver_id = CiGateEngine::requestWaiver("INIT-001", "Emergency fix for production");
CiGateEngine::approveWaiver(waiver_id, "tech-lead", 42);
if (CiGateEngine::isWaiverValid(waiver_id)) {
    // Waiver is approved and not expired
}
```

### Gate Report Serialization

Gate reports can be serialized for CI artifacts:

```cpp
GateReport report;
CiGateEngine::generateGateReport(report, 1, 0, 0);

char buffer[8192];
size_t len = GateReportSerializer::serializeTo(report, buffer, sizeof(buffer));
// Write buffer to CI artifact file
```

**Output format:**
```
GATE_REPORT v=1.0.0
blocking_passed=11 blocking_failed=0 release_blocked=false
GATE: INIT-001 result=0 actual=99.00 threshold=99.00 met=true
GATE: INIT-002 result=0 actual=450.00 threshold=500.00 met=true
...
```

---

## Qualification Report

The qualification report provides release evidence:

```cpp
#include <loradriver/profile_qualification.hpp>

QualificationReport report;
ProfileQualificationMatrix::generateQualificationReport(report, 1, 0, 0);

// Report contains:
// - Version information
// - Per-profile status and pass rates
// - Summary counts by status
```

### Report Format

```
QUALIFICATION_REPORT:v=1.0.0:total=9:V=8:S=0:D=1:E=0
PROFILE:c=0:b=0:i=0:s=V:r=100
PROFILE:c=0:b=0:i=1:s=V:r=100
...
```

## Release Decision Matrix

| Scenario | Validated Tests | Secondary Tests | Deferred | Decision |
|----------|-----------------|-----------------|----------|----------|
| All pass | PASS | PASS | N/A | **APPROVE** |
| Some fail | FAIL | - | N/A | **BLOCK** |
| All pass | PASS | FAIL | N/A | **WARN** (allow with acknowledgment) |
| N/A | N/A | N/A | N/A | **BLOCK** (no profiles) |

## Governance Workflow

### Proposing Status Changes

Status changes require explicit approval:

1. **Propose**: Create a change proposal with justification
2. **Review**: Technical review of impact
3. **Approve/Reject**: Explicit decision recorded in audit log
4. **Update**: Matrix updated (requires new release)

```cpp
// Propose a change
uint32_t id = ProfileGovernance::proposeStatusChange(
    profile,
    ProfileStatus::kSecondary,
    "Field incident #1234 - demoting",
    1, 0, 0
);

// Approve (recorded in audit log)
ProfileGovernance::approveStatusChange(id, approver_id);
```

### Audit Trail

All status changes are logged with:
- Change ID
- Profile affected
- Old and new status
- Justification
- Approver ID
- Version at time of change

## CI Integration

### Pre-Release Checks

```bash
# Configure + build with presets
cmake --preset default
cmake --build --preset default

# Run full host suite
ctest --preset default --output-on-failure

# Run gate-focused checks only (used by CI quality-gate step)
ctest --preset default --tests-regex "^(ci_gates|ota_gate)$" --output-on-failure

# Check all validated profiles pass
# Returns 0 if all gates pass, non-zero otherwise
```

### Artifact Generation

```cpp
// Generate CI artifact
QualificationReport report;
ProfileQualificationMatrix::generateQualificationReport(report, version);

char buffer[4096];
QualificationReportSerializer::serializeTo(report, buffer, sizeof(buffer));
// Write buffer to artifact file for CI pipeline
```

## References

- Profile Test Matrix: [test-matrix.md](./test-matrix.md)
- ADR-0004: Profile Qualification Governance Model
- ADR-0005: Go/No-Go Governance Model
- API: `include/loradriver/profile_qualification.hpp`
- API: `include/loradriver/ci_gates.hpp`
- Configuration: `tools/ci/gate_rules.yaml`

---

## Traceability Requirements (Story 3.4)

All release artifacts must be registered and linked for audit and RCA support.

### Artifact Registration

```cpp
#include <loradriver/artifact_governance.hpp>

// Register a validation report artifact
ArtifactMetadata metadata = {
    .type = ArtifactType::kValidationReport,
    .created_timestamp = getCurrentTimestamp(),
    .expires_timestamp = getCurrentTimestamp() + (90 * 86400),
    .retention_days = 90
};
std::strncpy(metadata.linked_version, "1.0.0", sizeof(metadata.linked_version) - 1);
std::strncpy(metadata.source_module, "ProfileQualificationMatrix", sizeof(metadata.source_module) - 1);

const char* artifact_id = ArtifactRegistry::registerArtifact(metadata);
```

### Artifact Types

| Type | Description | Retention | Auto-Delete |
|------|-------------|-----------|-------------|
| kValidationReport | Profile qualification reports | 90-180 days | Yes |
| kIncidentEvidence | Field incident snapshots | 90-180 days | Yes |
| kRecoveryProof | Recovery evidence | 90-180 days | Yes |
| kTestMatrix | Non-regression results | 90-180 days | Yes |
| kGateReport | CI gate evaluations | 90-180 days | Yes |
| kTelemetryBaseline | Radio KPI baselines | 180-365 days | No |
| kBuildLog | Build artifacts | 30-90 days | Yes |
| kReleaseManifest | Release metadata | 365-730 days | No |

### Trace Chain Requirements

All releases must have complete trace chains:

```cpp
#include <loradriver/versioning.hpp>

// Link build → test → release
TraceabilityEngine::linkBuildToTest(build_id, test_report_id);
TraceabilityEngine::linkTestToRelease(test_report_id, release_id);

// Validate chain integrity
bool valid = TraceabilityEngine::validateTraceIntegrity(version);

// Get full chain for RCA
std::array<TraceabilityLink, 16> chain;
size_t count = TraceabilityEngine::getFullTraceChain(artifact_id, chain);
```

### Changelog Requirements

All releases must have valid changelog entries:

```cpp
#include <loradriver/versioning.hpp>

ChangelogEntry entry = {
    .version = {1, 0, 0, {}, {}},
    .date = getCurrentTimestamp(),
    .category = ChangeCategory::kFeature,
    .description = "Initial V1 release with governance"
};

LoRaError result = ChangelogManager::addEntry(entry);
if (result != LoRaError::kOk) {
    // Validation failed
}

// Validate changelog before release
bool valid = ChangelogManager::validateChangelog();
```

### Release Artifact Checklist

- [ ] All gate reports registered as `kGateReport` artifacts
- [ ] Profile qualification reports registered as `kValidationReport` artifacts
- [ ] Non-regression results registered as `kTestMatrix` artifacts
- [ ] Recovery evidence registered as `kRecoveryProof` artifacts
- [ ] Build → Test → Release trace chain complete
- [ ] Changelog validated with all required fields
- [ ] Breaking changes include `breaking_notes` and `migration_guide`
- [ ] Security fixes include `issue_refs` (CVE or internal)

### Retention Enforcement

```cpp
// Get expired artifacts for cleanup
std::array<ArtifactMetadata, 32> expired;
size_t count = ArtifactRegistry::getExpiredArtifacts(expired, current_timestamp);

// Purge expired artifacts
bool purged = ArtifactRegistry::purgeExpired(current_timestamp);
```

---

## OTA Rollout Health Gating (Story 4.1)

OTA rollout decisions are gated by radio health KPIs from early deployment wave telemetry.
The `OtaGateEngine` produces deterministic, machine-readable decisions with attached evidence.

### Decision Values

| Decision | Meaning |
|----------|---------|
| `kAllow` | All KPI gates pass; no negative trend signal; rollout may expand |
| `kHold`  | Data quality issue or trend risk detected; manual escalation required |
| `kBlock` | Blocking KPI threshold violated; rollout expansion denied |

### Minimum Required Telemetry Fields

All fields must be present and valid; missing/invalid fields trigger `kHold`:

| Field | Type | Validation |
|-------|------|-----------|
| `firmware_version` | char[16] | Non-empty string |
| `radio_family` | char[16] | Non-empty string (e.g. "SX1276") |
| `active_band` | char[8] | Non-empty string (e.g. "433", "868") |
| `init_failure_rate` | float % | In range [0.0, 100.0] |
| `timeout_events` | uint32 count | Any value |
| `irq_overflow_events` | uint32 count | Any value |
| `tx_success_rate` | float % | In range [0.0, 100.0] |
| `rx_success_rate` | float % | In range [0.0, 100.0] |
| `sample_timestamp_utc` | uint32 epoch | Non-zero |

### OTA Gate Evaluation — CI Threshold Reuse

`OtaGateEngine` reuses existing `CiGateEngine` thresholds without duplication:

| CI Gate ID | OTA Metric Mapping | Decision on Failure |
|------------|-------------------|---------------------|
| INIT-001 | `init_success_rate = 100 - init_failure_rate` ≥ 99% | `kBlock` |
| TXRX-001 | `tx_success_rate` ≥ 99% | `kBlock` |
| TXRX-002 | `rx_success_rate` ≥ 98% | `kBlock` |
| IRQ-002 | `irq_overflow_events` == 0 | `kBlock` |

### KPI Trend Detection

If baseline values are provided (non-zero), degradation beyond the threshold triggers `kHold`:

- **Threshold**: 2.0 percentage-point drop from baseline
- **Applicable KPIs**: `tx_success_rate`, `rx_success_rate`, `init_failure_rate`
- **Basis**: Optional baseline fields in `OtaTelemetryInput`; zero = no baseline, skip check

### OTA Gate API

```cpp
#include <loradriver/ota_gate.hpp>

// Populate telemetry from deployment wave data
loradriver::OtaTelemetryInput telemetry{};
std::strncpy(telemetry.firmware_version, "1.2.3", sizeof(telemetry.firmware_version) - 1);
std::strncpy(telemetry.radio_family, "SX1276", sizeof(telemetry.radio_family) - 1);
std::strncpy(telemetry.active_band, "868", sizeof(telemetry.active_band) - 1);
telemetry.init_failure_rate   = 0.3f;   // 99.7% success
telemetry.tx_success_rate     = 99.5f;
telemetry.rx_success_rate     = 98.5f;
telemetry.irq_overflow_events = 0;
telemetry.timeout_events      = 0;
telemetry.sample_timestamp_utc = getCurrentTimestampUtc();

// Optional: set baselines for trend analysis
telemetry.baseline_tx_success_rate = 99.8f;
telemetry.baseline_rx_success_rate = 98.8f;

// Evaluate rollout gate
loradriver::OtaDecisionRationale rationale{};
loradriver::OtaRolloutDecision decision = loradriver::OtaGateEngine::evaluate(telemetry, rationale);

switch (decision) {
  case loradriver::OtaRolloutDecision::kAllow:
    // Proceed with rollout expansion
    break;
  case loradriver::OtaRolloutDecision::kHold:
    // Escalate with rationale.reason; check rationale.quality_issue / trend_risk
    break;
  case loradriver::OtaRolloutDecision::kBlock:
    // Deny expansion; rationale.failed_gate_ids contains evidence
    break;
}

// Artifact ID available for audit trail
// rationale.artifact_id links to ArtifactRegistry entry
```

### Governance Integration

- Telemetry decision artifacts registered as `kTelemetryBaseline` with 180-day retention
- Blocked decisions include gate IDs, actual values, and threshold values in rationale
- Artifact IDs in `OtaDecisionRationale` link to `ArtifactRegistry` for incident review/audit
- Both regular and hotfix release channels apply identical OTA rollout gate policy

### Escalation on Hold/Block

When `kHold` or `kBlock` is returned:
1. `rationale.reason` — human-readable explanation for escalation notification
2. `rationale.failed_gate_ids[]` — machine-readable gate IDs that failed
3. `rationale.failed_gate_actuals[]` / `rationale.failed_gate_thresholds[]` — metric evidence
4. `rationale.artifact_id` — registered artifact ID for traceability chain
5. `rationale.quality_issue` / `rationale.trend_risk` / `rationale.stale_data` — decision flags

---

## References

- Profile Test Matrix: [test-matrix.md](./test-matrix.md)
- Artifact Retention Policy: [../governance/artifact-retention.md](../governance/artifact-retention.md)
- Versioning Policy: [../governance/versioning-policy.md](../governance/versioning-policy.md)
- ADR-0004: Profile Qualification Governance Model
- ADR-0005: Go/No-Go Governance Model
- ADR-0006: Non-Regression Suite Design
- ADR-0007: Artifact Traceability
- API: `include/loradriver/profile_qualification.hpp`
- API: `include/loradriver/ci_gates.hpp`
- API: `include/loradriver/ota_gate.hpp`
- API: `include/loradriver/artifact_governance.hpp`
- API: `include/loradriver/versioning.hpp`
- Configuration: `tools/ci/gate_rules.yaml`
