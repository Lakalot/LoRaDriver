# Product Firmware Integration Guide (V1)

This guide defines the standard integration path for consuming LoRaDriver in product firmware without per-project forks.

## Baseline Contract

- Consume only public headers under `include/loradriver/`.
- Keep chip and platform specialization in internal adapters (`src/chips/`, `src/platform/`).
- Use stable API calls for runtime flow: `begin`, `send`, `startReceive`, `recoverFromTimeout`, `sleep`, `standby`.
- Treat unsupported profile combinations as typed failures (`LoRaError`) and surface diagnostics for triage.

## Canonical Integration Flow

1. Build `RadioConfig` with a supported SX127x V1 profile.
2. Create `LoRaDriver` and optionally register an event callback and timestamp source.
3. Call `begin(config)` and verify `LoRaError::kOk`.
4. Execute transmit and receive operations (`send`, `startReceive`).
5. If runtime detects RX/TX timeout conditions, execute deterministic recovery with `recoverFromTimeout()` and verify typed outcome.
6. Enter low power with `sleep()`.
7. Return to active state with `standby()` before further data-path operations.

## Deviation Points and Checks

### Profile config

- Allowed chips: `kSx1276`, `kSx1278`
- Rejected chips return `kUnsupportedProfile` with diagnostic context

### IRQ mode

- Allowed modes: `kDio0Only`, `kDio0Dio1`
- Unsupported routing is rejected with typed diagnostics

### Band selection

- Allowed bands: `k433`, `k868`
- Unsupported bands are rejected with typed diagnostics

## Onboarding Checklist

- [ ] Public usage compiles with headers in `include/loradriver/` only
- [ ] `begin` succeeds for supported profile combinations
- [ ] Unsupported profile combinations fail with typed diagnostics
- [ ] `send` and `startReceive` operate deterministically after successful initialization
- [ ] Timeout paths use `recoverFromTimeout` and return typed deterministic diagnostics/events
- [ ] `sleep` and `standby` restore expected runtime flow without adapter leaks
- [ ] No integration code depends directly on `src/chips/` or `src/platform/`
- [ ] Incident snapshots can be captured via `captureIncidentSnapshot()` for support handoff
- [ ] `IncidentSnapshot::formatTo()` produces stable output for incident reporting
- [ ] Optional: `setTimestampSource()` configured for non-zero timestamps in diagnostics

## Incident Capture for Support Handoff

When an incident occurs, capture diagnostic context for support:

```cpp
// Capture incident snapshot
auto snapshot = driver.captureIncidentSnapshot();

// Format to buffer for reporting
char buffer[loradriver::IncidentSnapshot::kFormatBufferSize];
snapshot.formatTo(buffer, sizeof(buffer));
// buffer now contains: LORADRIVER_INCIDENT:v=1.0.0;e=...;c=...;b=...;d=...;dc=...;seq=...;ts=...;
```

This standardized format ensures all required evidence is attached in support tickets.

### Injecting a Timestamp Source

To populate timestamp fields in diagnostic context and incident snapshots, inject a platform-specific clock:

```cpp
// Arduino/ESP32 example
driver.setTimestampSource([]() -> std::uint32_t { return millis(); });
```

Without a timestamp source, all `timestamp_ms` fields default to 0.

## See Also

- Full step-by-step integration guide (PlatformIO, CMake, profile examples): [`docs/api/integration-guide.md`](integration-guide.md)
- V1 support scope and deferred items: [`docs/scope/v1-support-boundaries.md`](../scope/v1-support-boundaries.md)
