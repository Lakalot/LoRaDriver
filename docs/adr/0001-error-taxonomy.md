# ADR 0001: Error Taxonomy Baseline

## Status

Accepted

## Context

V1 requires typed, deterministic error handling with no runtime exceptions.

## Decision

Use `LoRaError` as the public typed status for fallible operations.

For deterministic initialization in V1 scope, classify startup failures explicitly as invalid configuration, unsupported profile,
hardware initialization failure, transition guard failure, and already initialized violations.

Expose a minimum typed diagnostic context for the latest failure (`error`, `detail_code`, `chip`, `band`, `dio_routing`) so
unsupported profile and startup phase failures are operationally diagnosable without ad-hoc log parsing.

## Incident Classification Taxonomy

### Category Codes

| Code  | Category            | Probable Causes                                      | Escalation     |
|-------|---------------------|------------------------------------------------------|----------------|
| 1000  | `kTimeoutRelated`   | SPI timeout, RX/TX timeout, IRQ not received         | Support L1     |
| 2000  | `kIrqAnomaly`       | IRQ storm, missed IRQ, DIO routing mismatch          | Engineering    |
| 3000  | `kConfigError`      | Invalid config, unsupported profile, band mismatch   | Support L2     |
| 4000  | `kRuntimeTransition`| Illegal FSM state, recovery failure                  | Engineering    |
| 5000  | `kHardwareFault`    | SPI failure, chip not responding                     | Hardware Team  |
| 9000  | `kUnknown`          | Uncategorized error                                  | Support L1     |

### Severity Levels

| Severity      | Value | Criteria                                               | Response SLA |
|---------------|-------|--------------------------------------------------------|--------------|
| `kInfo`       | 0     | Normal operation events, recoverable without intervention | Log only  |
| `kWarning`    | 1     | Recoverable errors, degraded performance               | 24h response |
| `kCritical`   | 2     | Unrecoverable, requires reset or manual intervention   | 4h response  |

### Escalation Paths

| Path           | Value | Responsibility                        |
|----------------|-------|---------------------------------------|
| `kSupportL1`   | 0     | First-line support, basic triage      |
| `kSupportL2`   | 1     | Advanced support, configuration issues|
| `kEngineering` | 2     | Development team, code-level issues   |
| `kHardwareTeam`| 3     | Hardware team, physical issues        |

### Error-to-Category Mapping

| LoRaError                   | Category              | Severity   | Escalation     | Playbook                 |
|-----------------------------|-----------------------|------------|----------------|--------------------------|
| `kOk`                       | `kUnknown`            | `kInfo`    | `kSupportL1`   | no-incident              |
| `kTimeoutRecovered`         | `kTimeoutRelated`     | `kWarning` | `kSupportL1`   | timeout-recovery         |
| `kTimeoutRecoveryFailure`   | `kTimeoutRelated`     | `kCritical`| `kEngineering` | timeout-recovery-failure |
| `kInvalidConfig`            | `kConfigError`        | `kWarning` | `kSupportL2`   | config-validation        |
| `kUnsupportedProfile`       | `kConfigError`        | `kWarning` | `kSupportL2`   | profile-validation       |
| `kHardwareInitFailure`      | `kHardwareFault`      | `kCritical`| `kHardwareTeam`| hardware-init            |
| `kTransitionGuardFailure`   | `kRuntimeTransition`  | `kWarning` | `kEngineering` | state-transition         |
| `kAlreadyInitialized`       | `kConfigError`        | `kInfo`    | `kSupportL1`   | init-check               |
| `kNotInitialized`           | `kConfigError`        | `kWarning` | `kSupportL1`   | init-required            |
| `kNotImplemented`           | `kRuntimeTransition`  | `kWarning` | `kEngineering` | feature-stub             |

### Backward Compatibility

- Category codes use stable numeric values (1000, 2000, etc.) for historical comparability
- Taxonomy version field (`taxonomy_version_major`, `taxonomy_version_minor`) tracks evolution
- New categories must be added with new code ranges; existing codes must not be reused
- Deprecated categories should be marked but remain in the taxonomy for trend analysis

## Consequences

- Public API remains explicit and machine-checkable.
- Error growth will be centralized and versioned.
- Startup failures are diagnosable through typed outcomes plus a minimum diagnostic detail code.
- Incident classification provides deterministic, standardized response paths.
- Historical incident data remains comparable across taxonomy versions.
