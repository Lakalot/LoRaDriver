# Power Profile Comparison: IRQ_MINIMAL vs IRQ_EXTENDED

> **Informative — Non-blocking for V1 release.**
> This document provides reference guidance for hardware wiring decisions. No measured bench data
> is required for V1 qualification. Values and trade-offs below are estimated from SX127x
> application notes and representative operating assumptions.

## Overview

LoRaDriver V1 supports two DIO routing configurations that differ in how hardware interrupt lines
are connected from the SX127x radio module to the host microcontroller:

| Configuration | `RadioConfig::DioRouting` enum | DIO lines wired |
|---|---|---|
| **IRQ_MINIMAL** | `kDio0Only` | DIO0 only |
| **IRQ_EXTENDED** | `kDio0Dio1` | DIO0 + DIO1 |

Both configurations are fully supported for V1 profiles (SX1276 and SX1278 on 433 MHz / 868 MHz).

---

## DIO Line Role Reference

For SX127x in LoRa mode:

| DIO pin | Default function (LoRa mode) | Covered by |
|---|---|---|
| DIO0 | TxDone / RxDone | Both modes |
| DIO1 | RxTimeout | IRQ_EXTENDED only |

In `kDio0Only` mode, RX timeout detection relies on polling or a software timer rather than a
dedicated hardware interrupt. In `kDio0Dio1` mode, DIO1 signals the timeout condition directly,
allowing the driver to recover deterministically without additional polling overhead.

---

## Operating Scenario Comparison

### TX Active (transmit burst)

| Attribute | IRQ_MINIMAL (`kDio0Only`) | IRQ_EXTENDED (`kDio0Dio1`) |
|---|---|---|
| TxDone interrupt | DIO0 (both modes) | DIO0 (both modes) |
| Extra MCU pin needed | No | Yes (DIO1) |
| MCU wake events | 1 per TX | 1 per TX |
| Power difference | — | Negligible (DIO1 idle) |

TX path behaviour is identical between modes; DIO1 carries no additional interrupt load during TX.

### RX Listening (continuous receive)

| Attribute | IRQ_MINIMAL (`kDio0Only`) | IRQ_EXTENDED (`kDio0Dio1`) |
|---|---|---|
| RxDone interrupt | DIO0 (both modes) | DIO0 (both modes) |
| RxTimeout signal | Software fallback | DIO1 hardware interrupt |
| Polling overhead | Present when timeout guard active | Eliminated |
| Determinism on timeout | Dependent on polling interval | Immediate hardware signal |
| Estimated MCU overhead | +1–3 µA (poll cycle budget) | Negligible |

The main operational difference surfaces during RX: `kDio0Dio1` provides a hardware signal for
`kTimeout` events, which the driver maps to `RadioEvent::kTimeout` and recovers via
`recoverFromTimeout()`. In `kDio0Only` mode, the driver relies on an internal timeout guard path,
which may add marginal polling overhead.

### Sleep / Low-Power Idle

| Attribute | IRQ_MINIMAL (`kDio0Only`) | IRQ_EXTENDED (`kDio0Dio1`) |
|---|---|---|
| Radio sleep current | ~1 µA (SX127x spec) | ~1 µA (SX127x spec) |
| DIO1 leakage path | n/a (unconnected) | Negligible pull-up leakage |
| Wake-up source | DIO0 (or software) | DIO0 or DIO1 |

In sleep mode, both configurations draw equivalent radio current. If DIO1 is connected but the
MCU input has a pull-up resistor, there may be a sub-µA leakage path; this is implementation-
specific and board-level dependent.

---

## Summary: When to Choose Each Mode

| Consideration | Prefer IRQ_MINIMAL | Prefer IRQ_EXTENDED |
|---|---|---|
| MCU GPIO budget constrained | ✓ | |
| Deterministic RX timeout handling required | | ✓ |
| Lowest possible BOM / wiring complexity | ✓ | |
| Strict timeout recovery latency requirements | | ✓ |
| Standard LoRa P2P V1 use cases | ✓ (sufficient) | ✓ (recommended) |

Both modes meet V1 functional requirements. `kDio0Dio1` is the recommended wiring for products
where deterministic timeout recovery is a product-level quality requirement.

---

## Cross-references

- Supported profiles and V1 scope: [`docs/scope/v1-support-boundaries.md`](v1-support-boundaries.md)
- API integration and callback contract: [`docs/api/integration-guide.md`](../api/integration-guide.md)
- `RadioConfig::DioRouting` enum definition: `include/loradriver/radio_config.hpp`
- `RadioEvent::kTimeout` and recovery flow: `include/loradriver/radio_event.hpp`
