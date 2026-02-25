# Product Firmware Integration Guide (V1)

This guide defines the standard integration path for consuming LoRaDriver in product firmware without per-project forks.

## Baseline Contract

- Consume only public headers under `include/loradriver/`.
- Keep chip and platform specialization in internal adapters (`src/chips/`, `src/platform/`).
- Use stable API calls for runtime flow: `begin`, `send`, `startReceive`, `recoverFromTimeout`, `sleep`, `standby`.
- Treat unsupported profile combinations as typed failures (`LoRaError`) and surface diagnostics for triage.

## Canonical Integration Flow

1. Build `RadioConfig` with a supported SX127x V1 profile.
2. Create `LoRaDriver` and optionally register an event callback.
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
