# Non-Regression Test Suites

Version: 1.0.0
Last Updated: 2026-02-26

## Overview

Non-regression suites ensure that previously stabilized critical radio flows do not silently degrade across releases. Each suite contains ordered test cases with baseline results that must match deterministically.

## Suite Definitions

### V1-CRITICAL Suite

The primary non-regression suite for V1 validated profiles.

| Case ID | Category | Description | Baseline Latency | Profile Scope |
|---------|----------|-------------|------------------|---------------|
| NR-TIMEOUT-001 | kTimeout | TX timeout recovery | 100ms | All validated |
| NR-TIMEOUT-002 | kTimeout | RX timeout recovery | 100ms | All validated |
| NR-TIMEOUT-003 | kTimeout | Consecutive timeout recovery | 150ms | All validated |
| NR-SLEEP-001 | kSleepWakeup | Sleep->active transition | 50ms | All validated |
| NR-SLEEP-002 | kSleepWakeup | Sleep->RX resume | 75ms | All validated |
| NR-SLEEP-003 | kSleepWakeup | Sleep->TX resume | 75ms | All validated |
| NR-IRQ-001 | kIrqRace | DIO0/DIO1 simultaneous IRQ | 10ms | DIO0+DIO1 only |
| NR-IRQ-002 | kIrqRace | IRQ during state transition | 20ms | All validated |
| NR-FSM-001 | kFsmTransition | Illegal transition rejection | 5ms | All validated |
| NR-FSM-002 | kFsmTransition | Recovery state re-entry | 30ms | All validated |

## Category Definitions

### kTimeout
Timeout recovery scenarios that validate the driver's ability to recover from TX/RX timeouts and restore normal operation.

### kSleepWakeup
Sleep/wakeup recovery scenarios that validate power management transitions and state restoration.

### kIrqRace
IRQ race condition scenarios that validate correct handling of simultaneous or overlapping interrupt conditions.

### kFsmTransition
FSM state transition scenarios that validate state machine integrity and illegal transition rejection.

### kConfigValidation
Configuration validation scenarios that ensure radio configuration integrity across operations.

## Execution Model

### Suite Execution
1. Load suite definition with ordered test cases
2. For each case matching the target profile:
   - Execute test with deterministic inputs
   - Compare actual results with baseline
   - Record pass/fail with delta diagnostics
3. Generate suite execution report

### Baseline Comparison
- **Deterministic**: Actual latency must match baseline exactly
- **Reproducible**: Same inputs produce same outputs across runs
- **Delta Detection**: Any deviation from baseline is flagged

### Profile-Aware Selection
- Cases with `applies_to_all_profiles=true` run on all validated profiles
- Cases with specific `profile_constraint` only run on matching profiles
- Non-matching cases are skipped with appropriate status

## Integration with CI Gates

### NONREG-001 Gate
- **Metric**: `suite_pass_rate`
- **Threshold**: 100%
- **Severity**: Blocking
- **Description**: All non-regression cases must pass

### Failure Handling
When a non-regression case fails:
1. CI pipeline is blocked
2. Delta diagnostics are captured in report
3. Incident pattern mapping identifies related incident categories
4. New test case may be created from incident for future protection

## Evidence Artifacts

### Suite Execution Report
Generated in `artifacts/test-reports/non-regression/`:
```
Suite: V1-CRITICAL
Total: 10, Passed: 10, Failed: 0, Mismatch: 0
  [PASS] NR-TIMEOUT-001 (latency: 100 ms)
  [PASS] NR-TIMEOUT-002 (latency: 100 ms)
  ...
```

### Recovery Evidence
Collected in `artifacts/test-reports/recovery-evidence/`:
- Timeout recovery success status and latency per profile
- Sleep/wakeup recovery success status and latency per profile
- Firmware version and timestamp

## Extending Suites

### Adding New Cases
1. Define case with unique ID (NR-CATEGORY-NNN)
2. Set category and baseline result
3. Link to incident pattern ID if applicable
4. Set profile constraints
5. Enable for release blocking

### From Incident to Test Case
Use `IncidentPatternMapper::addCaseFromIncident()` to create new regression tests from observed incidents:
```cpp
IncidentSnapshot incident = captureIncident();
NonRegressionCase new_case = IncidentPatternMapper::addCaseFromIncident(
    incident, "NR-TIMEOUT-004");
```

## References

- ADR-0006: Non-Regression Governance
- Story 3.3: Add Non-Regression Suites and Recovery Evidence Requirements
- FR22: QA engineers can run non-regression validation
- FR23: Product teams can require evidence of successful recovery behavior
