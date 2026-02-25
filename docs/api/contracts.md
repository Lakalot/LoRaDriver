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
  1. `kInitValidate`
  2. `kInitBindAdapters`
  3. `kInitHardwareBringUp`
  4. `kInitialized`
- V1 profile scope remains limited to SX1276/SX1278, bands 433/868, and DIO0-only or DIO0+DIO1.
- SPI frequency is validated in `[4 MHz, 8 MHz]` with explicit rejection outside range.

## Error and Diagnostics Contract

- `LoRaError` differentiates initialization failures with explicit typed outcomes:
  - `kInvalidConfig`
  - `kUnsupportedProfile`
  - `kHardwareInitFailure`
  - `kTransitionGuardFailure`
  - `kAlreadyInitialized`
- `LoRaDriver::lastDiagnosticCode()` exposes minimum triage context for latest failure.
- `LoRaDriver::lastDiagnosticContext()` exposes typed context for latest failure:
  - `error`
  - `detail_code`
  - `chip`
  - `band`
  - `dio_routing`

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

## Shutdown Contract (V1)

- `shutdown()` is valid only when driver is initialized; returns `kNotInitialized` otherwise.
- Shutdown transitions FSM to `Idle` state through proper FSM authority (not direct mutation).
- No event is emitted during shutdown; this is intentional to avoid callback complexity during teardown.
- After shutdown, driver must be re-initialized via `begin()` before any TX/RX operations.
