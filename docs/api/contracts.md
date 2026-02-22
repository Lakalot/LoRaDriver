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

## Callback Shape

- Contract preserved for future stories: `std::function<void(RadioEvent, int)>`.
