# ADR 0007: Artifact Traceability

## Status

Accepted

## Context

Epic 3 establishes governance for LoRaDriver releases. To support root cause analysis (RCA) and audit compliance, we need traceability across build, test, and release artifacts.

Requirements:
- **FR18**: Product teams can retain validation and incident artifacts per defined retention policy
- **NFR20**: Artifacts retained 90-180 days for traceability and regression investigations
- **FR30**: Product teams can manage versioning and changelog for traceable release evolution

## Decision

Implement a unified artifact governance system with:

1. **ArtifactRegistry** - Central registry for all artifacts with type-based retrieval and retention enforcement
2. **TraceabilityEngine** - Cross-artifact linking for build→test→release chains
3. **ChangelogManager** - SemVer-compliant changelog with validation

### Artifact Types

```cpp
enum class ArtifactType : uint8_t {
  kValidationReport = 0,   // Profile qualification reports
  kIncidentEvidence = 1,   // Field incident snapshots
  kRecoveryProof = 2,      // Recovery evidence
  kTestMatrix = 3,         // Non-regression results
  kGateReport = 4,         // CI gate evaluations
  kTelemetryBaseline = 5,  // Radio KPI baselines
  kBuildLog = 6,           // Build artifacts
  kReleaseManifest = 7     // Release metadata
};
```

### Traceability Links

```cpp
// Build → Test → Release chain
TraceabilityEngine::linkBuildToTest(build_id, test_id);
TraceabilityEngine::linkTestToRelease(test_id, release_id);

// Incident → Artifact for RCA
TraceabilityEngine::linkIncidentToArtifact(incident_id, artifact_id);

// Full chain retrieval for RCA
std::array<TraceabilityLink, 16> chain;
size_t count = TraceabilityEngine::getFullTraceChain(artifact_id, chain);
```

### Retention Policy

| Artifact | Min Days | Max Days | Auto-Delete |
|----------|----------|----------|-------------|
| ValidationReport | 90 | 180 | Yes |
| IncidentEvidence | 90 | 180 | Yes |
| RecoveryProof | 90 | 180 | Yes |
| TestMatrix | 90 | 180 | Yes |
| GateReport | 90 | 180 | Yes |
| TelemetryBaseline | 180 | 365 | No |
| BuildLog | 30 | 90 | Yes |
| ReleaseManifest | 365 | 730 | No |

## Consequences

### Positive

- Complete traceability from build to release
- RCA support for field incidents
- Audit-ready artifact retention
- SemVer-compliant changelog

### Negative

- Storage overhead for artifact registry
- Need to integrate with existing validation modules

### Neutral

- Fixed-size structs for embedded compatibility (no heap allocation)

## Implementation

- Headers: `include/loradriver/artifact_governance.hpp`, `include/loradriver/versioning.hpp`
- Implementation: `src/governance/`
- Tests: `tests/host/test_artifact_registry.cpp`, `tests/host/test_versioning_governance.cpp`, `tests/host/test_traceability_engine.cpp`, `tests/host/test_changelog_manager.cpp`

## Related

- ADR 0004: Profile Qualification Matrix (Story 3.1)
- ADR 0005: CI Quality Gates (Story 3.2)
- ADR 0006: Non-Regression Suites (Story 3.3)
