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

### Fixed

- `begin()` now preserves initialized runtime state when called again and returns `kAlreadyInitialized` without destructive reset.
- Hardware bring-up phase callback failures are now classified as `kHardwareInitFailure`.

## [0.1.0] - 2026-02-22

### Added

- Initial architecture-first project baseline.
- PlatformIO and host CMake/CTest build lanes.
- Public API placeholders and scoped module layout.
