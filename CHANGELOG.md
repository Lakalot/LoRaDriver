# Changelog

All notable changes to this project are documented in this file.

The format is based on Keep a Changelog and this project follows Semantic Versioning.

## [Unreleased]

### Added

- Deterministic V1 initialization pipeline with explicit startup phase events and bounded failure exits.
- Strict V1 profile and SPI range validation gates with typed startup failure classifications.
- Initialization diagnostics surface via `lastDiagnosticCode()` for triage-oriented failure context.
- Initialization diagnostics surface via `lastDiagnosticContext()` with typed profile/band/IRQ context.
- Expanded host and embedded smoke tests for deterministic init ordering, unsupported profile rejection, and SPI guardrails.
- Deterministic TX lifecycle API via `send()` with explicit transition/event ordering and typed invalid-payload failure path.
- Deterministic RX lifecycle API via `startReceive()` with explicit listening/in-progress callbacks and deterministic return-to-listen behavior.
- Host and embedded smoke coverage for TX/RX event sequencing, illegal entry rejection, IRQ profile parity, and repeated TX/RX cycle stability.
- Stable integration lifecycle now includes `sleep()` and `standby()` to support a no-fork baseline firmware integration path.
- Integration contract documentation now defines canonical V1 onboarding flow, explicit deviation points, and adapter-boundary guardrails.
- `kSleep` and `kStandby` events added to `RadioEvent` enum for deterministic power-state observation.
- Sleep/standby operations now emit events consistent with TX/RX deterministic event pattern.
- Diagnostic codes for sleep/standby lifecycle (5101-5104, 5201-5204) documented in contracts.md.
- Deterministic timeout recovery API via `recoverFromTimeout()` with typed recovery outcomes and fixed event ordering (`kTimeout` -> `kRecoveryCompleted`).
- Timeout recovery diagnostics documented for success, guard rejection, and transition failure points (6100, 6200/6201, 6301, 6401+).
- Granular diagnostic events for init lifecycle phases: `kInitPhaseStart`, `kConfigValidated`, `kChipDetected`.
- `DiagnosticContext` extended with driver version (`version_major`, `version_minor`, `version_patch`), sequence counter, and timestamp field.
- `IncidentSnapshot` struct for comprehensive incident capture with standardized field order for support handoff.
- `IncidentSnapshot::formatTo()` method produces stable, parseable output format for incident reporting.
- `LoRaDriver::captureIncidentSnapshot()` method to capture incident context on demand.
- `LoRaDriver::currentSequence()` method to observe operation sequence counter.
- `version.hpp` header with `LORADRIVER_VERSION_*` macros for compile-time version information.
- Host tests for event ordering determinism, diagnostic context completeness, and incident snapshot format stability.
- `LoRaDriver::setTimestampSource()` method to inject platform-specific millisecond clock for diagnostic timestamps.
- Sequence counter now advances on TX, RX, and recovery operations (not just init phases).
- `updateDiagnosticContext()` helper ensures version, sequence, and timestamp fields are consistently populated on all paths.

- `IncidentCategory` enum with stable numeric codes for field incident classification (1000-9000 range).
- `IncidentSeverity` enum with stable severity levels (`kInfo`, `kWarning`, `kCritical`).
- `EscalationPath` enum for standardized incident ownership routing.
- `IncidentClassification` struct with taxonomy version, category, severity, escalation path, and suggested playbook.
- `classifyIncident()` function for deterministic incident classification from `IncidentSnapshot`.
- Error-to-category mapping table for automatic classification of all `LoRaError` codes.
- ADR 0001 extended with incident classification taxonomy documentation.
- Host tests for classification determinism, category code stability, severity mapping, and integration with driver snapshot capture.

- `ProfileQualificationMatrix` for hardware profile qualification governance (Story 3.1).
- `ProfileGovernance` class for status change proposals with audit trail.
- `QualificationReport` struct and serializer for release qualification evidence.
- V1 validated profile count: 8 (SX1276/SX1278 x 433/868 x DIO0/DIO0+DIO1).
- ADR 0004: Profile Qualification Governance Model.

- `CiGateEngine` for blocking CI quality gates with Go/No-Go thresholds (Story 3.2).
- `GateReport` struct for gate evaluation results and serialization.
- `GateWaiver` workflow for temporary gate bypass with approval tracking.
- 11 V1 blocking quality gates for init, TX/RX, IRQ, timeout, recovery, and integration.
- Release channel policies (Regular/Hotfix) with waiver approver lists.
- ADR 0005: Go/No-Go Governance Model.

- `NonRegressionSuite` and `NonRegressionCase` structs for deterministic baseline validation (Story 3.3).
- `RecoveryEvidenceCollector` for timeout and sleep/wakeup recovery evidence.
- `IncidentPatternMapper` for incident-to-regression-case mapping.
- `SuiteExecutionReport` for regression test results.
- ADR 0006: Non-Regression Suite Design.

- `ArtifactRegistry` for artifact tracking with retention policy enforcement (Story 3.4).
- `ArtifactType` enum with 8 artifact types (validation reports, incident evidence, recovery proofs, etc.).
- `RetentionPolicy` struct with V1 defaults (90-180 days).
- `TraceabilityEngine` for build→test→release chain linking and RCA support.
- `ChangelogManager` for SemVer-compliant changelog with validation.
- `SemVerVersion` struct with parsing, comparison, and formatting.
- `ChangelogEntry` struct with breaking change and security fix validation.
- ADR 0007: Artifact Traceability.
- Documentation: `docs/governance/artifact-retention.md`, `docs/governance/versioning-policy.md`.

### Fixed

- `begin()` now preserves initialized runtime state when called again and returns `kAlreadyInitialized` without destructive reset.
- Hardware bring-up phase callback failures are now classified as `kHardwareInitFailure`.

## [0.1.0] - 2026-02-22

### Added

- Initial architecture-first project baseline.
- PlatformIO and host CMake/CTest build lanes.
- Public API placeholders and scoped module layout.
