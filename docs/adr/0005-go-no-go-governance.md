# ADR-0005: Go/No-Go Governance Model

## Status

Accepted

## Context

Story 3.2 introduces blocking CI quality gates for critical radio scenarios. We need a governance model that:

1. Ensures unstable radio behavior never reaches production by default
2. Provides explicit go/no-go thresholds based on measurable criteria
3. Allows waiver paths with documented approval when necessary
4. Maintains consistent policy across regular and hotfix release channels

## Decision

### 1. Gate Severity Taxonomy

We adopt a three-tier severity model:

| Severity | Behavior | Numeric Code |
|----------|----------|--------------|
| `kBlocking` | Merge/release denied | 0 |
| `kWarning` | Logged, no block | 1 |
| `kAdvisory` | Informational | 2 |

Numeric codes ensure stable serialization and backward compatibility.

### 2. Threshold Operators

We support five comparison operators for go/no-go thresholds:

| Operator | Code | Use Case |
|----------|------|----------|
| `>=` | 0 | Success rates (must be at least X) |
| `<=` | 1 | Latency (must be at most X) |
| `==` | 2 | Zero-tolerance (must be exactly X) |
| `<` | 3 | Strict upper bound |
| `>` | 4 | Strict lower bound |

### 3. Waiver Governance

Waivers require:
- **Justification**: Free-text explanation (max 256 chars)
- **Approver Authorization**: Must be in channel's approver list
- **Expiry**: Time-bounded validity (default 72 hours)
- **Audit Trail**: Full record of request, approval, timestamp

### 4. Channel Policy Consistency

Both regular and hotfix channels:
- **Same blocking gates** - no shortcuts for urgency
- **Same thresholds** - quality bar is immutable
- **Waivers allowed** - with appropriate approvers

Hotfix-only difference:
- Additional approver role: `incident-commander`
- Faster approval path for urgent situations

### 5. Fixed-Size Data Structures

All gate types use fixed-size members to avoid heap allocation:

```cpp
struct GateRule {
  char id[16];           // Gate ID (e.g., "INIT-001")
  char description[128]; // Human-readable description
  // ... other fields
};
```

## Consequences

### Positive

- **Deterministic behavior**: Gate evaluation is reproducible and predictable
- **No heap allocation**: Suitable for embedded CI runners
- **Clear accountability**: Waiver audit trail provides traceability
- **Quality bar enforcement**: Urgency cannot bypass critical checks

### Negative

- **Fixed string limits**: Long descriptions may be truncated
- **Manual waiver process**: Requires human approval (intentional friction)

### Mitigations

- Documentation limits clearly stated
- Serialization handles truncation gracefully

## Implementation

- Header: `include/loradriver/ci_gates.hpp`
- Implementation: `src/validation/ci_gates.cpp`
- Configuration: `tools/ci/gate_rules.yaml`
- Tests: `tests/host/test_ci_gates.cpp`

## References

- FR20: Release owners can enforce blocking quality gates
- FR21: Product teams can determine release readiness using explicit go/no-go criteria
- NFR16: CI shall enforce blocking gates on critical radio scenarios
- Story 3.2: Implement Blocking CI Quality Gates and Explicit Go/No-Go Rules
