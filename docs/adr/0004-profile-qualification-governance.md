# ADR-0004: Profile Qualification Governance Model

## Status

Accepted

## Context

LoRaDriver needs to support multiple hardware profile combinations (chip, band, IRQ mode). Not all profiles have equal validation coverage or production readiness. We need:

1. A way to classify profile support status (validated, secondary, deferred, experimental)
2. Clear criteria for what makes a profile "validated"
3. A governance process for changing profile status
4. CI integration to enforce quality gates based on profile status

This enables:
- Product teams to understand which profiles are production-ready
- QA engineers to know which profiles require testing
- Release managers to make informed go/no-go decisions
- Field teams to classify and respond to profile-specific incidents

## Decision

### Profile Status Taxonomy

We define four stable status codes with explicit meanings:

| Code | Value | Meaning | Release Impact |
|------|-------|---------|----------------|
| `kValidated` | 0 | Primary V1 support, fully tested | **Blocking** - must pass |
| `kSecondary` | 1 | Supported but lower priority | Non-blocking |
| `kDeferred` | 2 | Out of current scope | Excluded |
| `kExperimental` | 3 | Under evaluation | Not for production |

### Immutable Matrix Principle

The qualification matrix is **immutable at runtime**:
- Loaded from compile-time constants
- Changes require code modification and new release
- Status changes are tracked via audit log, not in-place mutation

This ensures:
- Reproducible qualification results
- Clear version-to-version traceability
- No runtime configuration drift

### Governance Workflow

Status changes follow an explicit approval process:

1. **Proposal**: Any stakeholder proposes a change with justification
2. **Review**: Technical review of impact and evidence
3. **Decision**: Explicit approve/reject recorded with approver ID
4. **Audit**: All decisions logged for compliance

```cpp
uint32_t id = ProfileGovernance::proposeStatusChange(
    profile, new_status, justification, version);
ProfileGovernance::approveStatusChange(id, approver_id);
```

### Release Gate Integration

Only `kValidated` profiles block releases:
- `isReleaseBlocking()` returns true only for validated profiles
- CI pipelines query this to determine required test coverage
- Deferred profiles are explicitly excluded, not implicitly forgotten

## Consequences

### Positive

- **Clarity**: Product teams know exactly which profiles are production-ready
- **Traceability**: All status changes have audit trail
- **Automation**: CI can programmatically determine required tests
- **Governance**: No implicit promotion of unsupported profiles

### Negative

- **Rigidity**: Status changes require new release (but this is intentional)
- **Overhead**: Governance process adds step to status changes

### Risks Mitigated

- **Implicit promotion**: Profiles cannot accidentally become "supported"
- **Scope creep**: Deferred profiles are explicitly tracked
- **Untraceable decisions**: All changes have justification and approver

## Implementation

### Files

- `include/loradriver/profile_qualification.hpp` - Public types and API
- `src/validation/profile_qualification.cpp` - Matrix implementation
- `docs/validation/test-matrix.md` - V1 profile definitions
- `docs/validation/release-gates.md` - Quality gate requirements

### Key Types

```cpp
enum class ProfileStatus : uint8_t { kValidated = 0, kSecondary = 1, kDeferred = 2, kExperimental = 3 };

struct HardwareProfile { RadioConfig::Chip chip; RadioConfig::Band band; RadioConfig::DioRouting irq; };

class ProfileQualificationMatrix { /* lookup methods */ };
class ProfileGovernance { /* proposal/approval workflow */ };
```

## References

- FR11: Product teams can classify hardware support status by profile
- FR19: QA engineers can execute repeatable validation matrix
- Story 3.1: Define Hardware Profile Qualification Matrix and Support Status Governance
