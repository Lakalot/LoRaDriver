# ADR-0006: Non-Regression Governance

## Status

Accepted

## Context

Previously stabilized critical radio flows need protection against silent degradation across releases. Without systematic non-regression testing:

1. Bug fixes in one area may inadvertently break previously working flows
2. Timeout and sleep/wakeup recovery paths may degrade without detection
3. Incident patterns are not systematically converted to protective test cases
4. Release evidence requirements are not enforced at the CI gate level

FR22 requires QA engineers to run non-regression validation on previously stabilized critical radio flows. FR23 requires product teams to have evidence of successful recovery behavior before go-live.

## Decision

Implement a comprehensive non-regression governance framework with:

### 1. Non-Regression Suite Data Structures

```cpp
enum class NonRegressionCategory : uint8_t {
  kTimeout = 0,
  kSleepWakeup = 1,
  kIrqRace = 2,
  kFsmTransition = 3,
  kConfigValidation = 4
};
```

Fixed-size structs for embedded compatibility:
- `NonRegressionCase`: Individual test case with baseline result
- `NonRegressionSuite`: Ordered collection of test cases
- `RecoveryEvidence`: Proof of successful recovery behavior

### 2. Non-Regression Test Executor

`NonRegressionExecutor` provides:
- Suite and case retrieval by ID
- Deterministic baseline comparison
- Profile-aware test selection
- Delta diagnostic generation

### 3. Recovery Evidence Collection

`RecoveryEvidenceCollector` provides:
- Timeout recovery evidence collection per profile
- Sleep/wakeup evidence collection per profile
- Evidence validation for go/no-go decisions
- Serialization for release artifacts

### 4. Incident Pattern Mapping

`IncidentPatternMapper` provides:
- Category mapping from incidents to regression tests
- Incident-to-case linking for traceability
- New case creation from observed incidents

### 5. CI Gate Integration

New blocking gates:
- **RECOVERY-002**: Timeout recovery evidence required (100% threshold)
- **NONREG-001**: Non-regression suite pass rate (100% threshold)

Go-live is blocked when required recovery evidence is missing.

## Consequences

### Positive

1. **Regression Protection**: Previously stabilized flows are automatically revalidated on each release
2. **Evidence Enforcement**: Recovery evidence is mandatory, not optional
3. **Incident Learning**: New bugfixes automatically create protective test cases
4. **Deterministic Validation**: Baseline comparison is exact and reproducible
5. **Profile Awareness**: Tests run only on applicable hardware profiles

### Negative

1. **Maintenance Overhead**: Baseline results must be updated when behavior intentionally changes
2. **CI Time**: Additional test execution time in CI pipeline
3. **Rigidity**: 100% threshold may require waiver process for known acceptable deviations

### Neutral

1. Requires integration with existing `ProfileQualificationMatrix` and `CiGateEngine`
2. Evidence artifacts add to release artifact storage

## Implementation

### File Structure
```
include/loradriver/
  non_regression.hpp              (public types)

src/validation/
  non_regression.cpp              (implementation)
  non_regression.hpp              (internal helpers)

tests/host/non_regression/
  test_timeout_recovery.cpp
  test_sleep_wakeup_recovery.cpp
  test_irq_race_scenarios.cpp
  test_fsm_regression.cpp
```

### V1 Baseline Cases
10 mandatory non-regression cases covering:
- Timeout recovery (TX, RX, consecutive)
- Sleep/wakeup transitions (active, RX resume, TX resume)
- IRQ race conditions (simultaneous, during transition)
- FSM integrity (illegal rejection, recovery re-entry)

## References

- FR22: QA engineers can run non-regression validation
- FR23: Product teams can require evidence of successful recovery behavior
- NFR8: Deterministic recovery latency behavior preservation
- Story 3.1: ProfileQualificationMatrix (consumed)
- Story 3.2: CiGateEngine (consumed)
- Story 2.3: IncidentClassification (consumed)
