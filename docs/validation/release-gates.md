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
# Generate qualification report
./build/tests/host/loradriver_test_profile_qualification

# Run CI gate tests
./build/tests/host/loradriver_test_ci_gates

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
