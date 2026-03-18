# Artifact Retention Policy

This document defines the artifact retention policy for LoRaDriver validation and incident artifacts.

## Overview

Artifacts are retained for a configurable period (90-180 days by default) to support:
- Release traceability
- Regression investigation
- Audit compliance
- Root cause analysis

## Artifact Types

| Type | Code | Description | Min Days | Max Days | Auto-Delete |
|------|------|-------------|----------|----------|-------------|
| ValidationReport | 0 | Profile qualification reports | 90 | 180 | Yes |
| IncidentEvidence | 1 | Field incident snapshots | 90 | 180 | Yes |
| RecoveryProof | 2 | Recovery evidence from non-regression | 90 | 180 | Yes |
| TestMatrix | 3 | Non-regression suite results | 90 | 180 | Yes |
| GateReport | 4 | CI gate evaluation results | 90 | 180 | Yes |
| TelemetryBaseline | 5 | Radio KPI baselines | 180 | 365 | No |
| BuildLog | 6 | Build artifacts | 30 | 90 | Yes |
| ReleaseManifest | 7 | Release metadata | 365 | 730 | No |

## Retention Enforcement

### V1 Default Policy

- **Minimum retention**: 90 days
- **Maximum retention**: 180 days (365 for telemetry, 730 for releases)
- **Compression threshold**: 30-90 days (type-dependent)
- **Auto-delete**: Enabled for most types, disabled for telemetry and releases

### Purge Behavior

```cpp
// Example: Purge expired artifacts
uint32_t current_timestamp = getCurrentTimestamp();
bool purged = ArtifactRegistry::purgeExpired(current_timestamp);
```

## Policy Configuration

Policies can be customized at runtime:

```cpp
RetentionPolicy custom_policy = {
    .artifact_type = ArtifactType::kValidationReport,
    .min_retention_days = 120,
    .max_retention_days = 365,
    .auto_delete_expired = true,
    .compress_after_days = true,
    .compress_threshold = 90
};
ArtifactRegistry::setRetentionPolicy(ArtifactType::kValidationReport, custom_policy);
```

## Audit Support

All artifact operations are logged and can be queried:

```cpp
// Get expired artifacts for audit
std::array<ArtifactMetadata, 32> expired;
size_t count = ArtifactRegistry::getExpiredArtifacts(expired, current_timestamp);
```

## Compliance

This policy satisfies:
- **NFR20**: Artifacts retained 90-180 days for traceability
- **FR18**: Product teams can retain artifacts per defined policy
