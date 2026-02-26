# API Contracts (Baseline)

This document captures the baseline public contract for LoRaDriver V1.

## Public API Boundary

- Public headers must stay under `include/loradriver/`.
- Public API must not expose chip- or platform-specific types.
- Fallible operations use `[[nodiscard]] LoRaError`.

## Baseline Types

- `LoRaError`: typed failure/success contract.
- `RadioEvent`: callback event surface.
- `RadioConfig`: supported profile declaration.
- `LoRaDriver`: public entry point.

## Initialization Contract (V1)

- Startup entrypoint is `begin(const RadioConfig&)`.
- `initialize(const RadioConfig&)` is retained as a compatibility alias to `begin`.
- Deterministic startup emits events in this fixed order:
  1. `kInitPhaseStart` (`detail_code = 1000`)
  2. `kInitValidate` (`detail_code = 1100`)
  3. `kChipDetected` (`detail_code` = chip enum value)
  4. `kConfigValidated` (`detail_code = 1500`)
  5. `kInitBindAdapters` (`detail_code = 1200`)
  6. `kInitHardwareBringUp` (`detail_code = 1300`)
  7. `kInitialized` (`detail_code = 0`)
- V1 profile scope remains limited to SX1276/SX1278, bands 433/868, and DIO0-only or DIO0+DIO1.
- SPI frequency is validated in `[4 MHz, 8 MHz]` with explicit rejection outside range.

## Error and Diagnostics Contract

- `LoRaError` differentiates initialization failures with explicit typed outcomes:
  - `kInvalidConfig`
  - `kUnsupportedProfile`
  - `kHardwareInitFailure`
  - `kTransitionGuardFailure`
  - `kTimeoutRecovered`
  - `kTimeoutRecoveryFailure`
  - `kAlreadyInitialized`
- `LoRaDriver::lastDiagnosticCode()` exposes minimum triage context for latest failure.
- `LoRaDriver::lastDiagnosticContext()` exposes typed context for latest operation:
  - `version_major`, `version_minor`, `version_patch`: driver version (always populated)
  - `error`: typed error code
  - `detail_code`: numeric diagnostic code
  - `chip`, `band`, `dio_routing`: profile configuration
  - `sequence`: operation sequence counter (increments on init phases, TX, RX, and recovery)
  - `timestamp_ms`: timestamp from injectable `TimestampSource` (0 if not configured)
- `LoRaDriver::setTimestampSource(TimestampSource)` allows host firmware to inject a platform-specific millisecond clock (e.g., `millis()` on Arduino). If not set, all timestamp fields default to 0.

### Incident Snapshot (V1)

- `LoRaDriver::captureIncidentSnapshot()` returns an `IncidentSnapshot` struct for comprehensive incident capture:
  - `version_major`, `version_minor`, `version_patch`: driver version at compile time
  - `error`: typed error code
  - `detail_code`: numeric diagnostic code
  - `chip`, `band`, `dio_routing`: profile configuration
  - `sequence`: operation sequence counter
  - `timestamp_ms`: timestamp context (reserved for future use)
- `IncidentSnapshot::formatTo(char* buffer, size_t buffer_size)` produces a stable, parseable output format:
  - Format: `LORADRIVER_INCIDENT:v=X.Y.Z;e=E;c=C;b=B;d=D;dc=DC;seq=S;ts=T;`
  - All fields are present in fixed order for reliable parsing
  - Minimum buffer size: `IncidentSnapshot::kFormatBufferSize` (256 bytes)

### Diagnostic Detail Encoding (V1)

- Unsupported-profile `detail_code` uses a stable encoding: `(reason * 100) + (chip * 10) + (band * 3) + dio`.
- `reason` values currently used by startup validation:
  - `2101`: unsupported chip
  - `2102`: unsupported band
  - `2103`: unsupported IRQ routing
- Startup phase diagnostics:
  - `1100`: validate phase
  - `1200`: bind adapters phase
  - `1300`: hardware bring-up phase

### TX/RX Diagnostic Detail Codes (V1)

- TX lifecycle diagnostics:
  - `3100`: TX preparing phase
  - `3200`: TX in progress phase
  - `3301`: TX failed - invalid payload (null, zero-length, or >255 bytes)
  - `3401`: TX rejected - driver not initialized
  - `3402`: TX rejected - illegal state entry (not Ready/Listening)
  - `3403`-`3412`: TX transition guard failures during lifecycle

- RX lifecycle diagnostics:
  - `4100`: RX listening phase
  - `4200`: RX in progress phase
  - `4401`: RX rejected - driver not initialized
  - `4402`: RX rejected - illegal state entry (not Ready/Listening)
  - `4403`-`4408`: RX transition guard failures during lifecycle

- Transition guard failures (3403+, 4403+): These occur when callback throws during event emission or when FSM transition is unexpectedly rejected. The base code plus offset indicates the specific failure point in the lifecycle.

### Sleep/Standby Diagnostic Detail Codes (V1)

- Sleep lifecycle diagnostics:
  - `5101`: Sleep rejected - driver not initialized
  - `5102`: Sleep rejected - illegal state entry (not Ready/Listening)
  - `5103`: Sleep transition guard failure
  - `5104`: Sleep event emission failure (callback threw)

- Standby lifecycle diagnostics:
  - `5201`: Standby rejected - driver not initialized
  - `5202`: Standby rejected - illegal state entry (not Idle/Listening/Ready)
  - `5203`: Standby transition guard failure
  - `5204`: Standby event emission failure (callback threw)

### Timeout Recovery Diagnostic Detail Codes (V1)

- Timeout/recovery diagnostics:
  - `6100`: timeout detected event emitted during deterministic recovery flow
  - `6200`: recovery completed event (DIO0+DIO1 profile)
  - `6201`: recovery completed event (DIO0-only profile granularity)
  - `6301`: timeout recovery rejected - driver not initialized
  - `6401`: timeout recovery rejected - illegal state entry (not Listening/RxInProgress/TxInProgress)
  - `6402`-`6404`: timeout recovery transition or event emission failure points

## Callback Shape

- Contract preserved for future stories: `std::function<void(RadioEvent, int)>`.
- Callback implementers SHOULD NOT throw exceptions. If a callback throws during startup event emission, initialization fails with typed error handling (`kTransitionGuardFailure` or `kHardwareInitFailure` depending on phase). The driver defensively catches exceptions to prevent undefined behavior, but this is a safety net—not a supported usage pattern.

## Deterministic TX/RX Contract (V1)

- `send(const std::uint8_t* payload, std::size_t size)` is valid only from deterministic entry states (`Ready` or `Listening`).
- TX lifecycle emits a stable sequence:
  1. `kTxPreparing` (`detail_code = 3100`)
  2. `kTxInProgress` (`detail_code = 3200`)
  3. `kTxCompleted` (`detail_code = 0` for DIO0+DIO1, `1` for DIO0-only granularity)
- Invalid TX payloads (null, zero-length, or >255 bytes) emit deterministic failure lifecycle `kTxPreparing` -> `kTxFailed` (`detail_code = 3301`) and return `kInvalidConfig`.
- `startReceive()` is valid only from deterministic entry states (`Ready` or `Listening`) and emits:
  1. `kRxListening` (`detail_code = 4100`)
  2. `kRxInProgress` (`detail_code = 4200`)
  3. `kRxDone` (`detail_code = 0` for DIO0+DIO1, `1` for DIO0-only granularity)
- Post-RX state deterministically returns to `Listening` with no implicit retries.

## TX/RX Error and Guardrail Contract

- Illegal entry or transition path in TX/RX returns `kTransitionGuardFailure` with typed diagnostics.
- Calling `send()`/`startReceive()` before successful `begin()` returns `kNotInitialized` with diagnosable detail code.
- DIO0-only profile is allowed to degrade event granularity detail codes only; correctness and stability semantics remain identical to DIO0+DIO1.

## Standard Product Firmware Integration Contract (V1)

- Product firmware integrations MUST use only public headers from `include/loradriver/`.
- Product firmware integrations MUST call public API methods only; do not bind directly to adapter internals in `src/chips/` or `src/platform/`.
- `sleep()` and `standby()` are part of the stable integration lifecycle and do not require any project-specific fork to use with supported V1 profiles.

## Power Management Contract (V1)

### sleep()

- Valid entry states: `Ready`, `Listening`
- Transitions to: `Idle`
- Emits: `kSleep` event (detail_code = 0)
- Use case: Enter low-power mode when radio will be idle for extended period
- After sleep, call `standby()` before TX/RX operations

### standby()

- Valid entry states: `Idle`, `Listening`, `Ready`
- Transitions to: `Ready` (no-op if already in Ready)
- Emits: `kStandby` event (detail_code = 0)
- Use case: Return to active runtime state after sleep

### State Transition Rules

- `sleep()` from `kRxInProgress` is rejected with `kTransitionGuardFailure` (interrupt active RX first)
- `standby()` from any valid state returns driver to `Ready` for immediate TX/RX operations

### Canonical integration lifecycle (stable API only)

1. Initialize with `begin(config)`.
2. Register events with `setEventCallback(callback)` if runtime telemetry is required.
3. Execute datapath operations with `send()` and `startReceive()`.
4. Move to low-power mode with `sleep()`.
5. Return to active runtime with `standby()`.

Supported baseline flow uses these stable calls: `begin`, `send`, `startReceive`, `sleep`, `standby`.

## Timeout Recovery Contract (V1)

- `recoverFromTimeout()` is valid only from `Listening`, `RxInProgress`, or `TxInProgress`.
- Recovery path is deterministic and FSM-owned:
  1. Transition to `TimeoutRecovering`
  2. Emit `kTimeout` (`detail_code = 6100`)
  3. Transition to `Ready`
  4. Emit `kRecoveryCompleted` (`detail_code = 6200` for DIO0+DIO1, `6201` for DIO0-only)
- Successful timeout recovery returns `LoRaError::kTimeoutRecovered` with typed diagnostic context.
- Illegal entry state returns `LoRaError::kTransitionGuardFailure` with detail code `6401`.
- Transition or callback failures during recovery return `LoRaError::kTimeoutRecoveryFailure` with typed diagnostics.

## SX127x V1 Onboarding and Deviation Points

The onboarding baseline is intentionally strict so teams can integrate without default forks.

### Profile configuration

- Allowed chips: `kSx1276`, `kSx1278`.
- Any other chip value is rejected with `kUnsupportedProfile` and typed diagnostics.

### IRQ routing mode

- Allowed IRQ modes: `kDio0Only`, `kDio0Dio1`.
- Other routing values are rejected with explicit typed diagnostics and no hidden fallback.

### Band settings

- Allowed bands: `k433`, `k868`.
- Unsupported bands are rejected with `kUnsupportedProfile`.

### Onboarding acceptance checks

- `begin()` returns `kOk` for supported SX127x V1 combinations and deterministic startup events are emitted.
- Unsupported profile combinations return typed errors with non-zero diagnostic details.
- Event callback shape remains `std::function<void(RadioEvent, int)>` across all supported profiles.
- Adapter boundaries remain internal: no chip/platform internals are required from public integration code.

## Shutdown Contract (V1)

- `shutdown()` is valid only when driver is initialized; returns `kNotInitialized` otherwise.
- Shutdown transitions FSM to `Idle` state through proper FSM authority (not direct mutation).
- No event is emitted during shutdown; this is intentional to avoid callback complexity during teardown.
- After shutdown, driver must be re-initialized via `begin()` before any TX/RX operations.

## Power Lifecycle Events (V1)

- `kSleep`: Emitted after successful transition to low-power Idle state
- `kStandby`: Emitted after successful transition back to Ready state
