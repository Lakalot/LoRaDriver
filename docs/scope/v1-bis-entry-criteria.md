# V1-bis Entry Criteria: SX126x Onboarding Prerequisites

## Purpose

This document defines the objective, auditable prerequisites that must be satisfied before the
SX126x hardware family (SX1261, SX1262) can be onboarded into the LoRaDriver mainline. Meeting
all criteria is required to open a V1-bis scope milestone.

The V1-bis readiness decision is deterministic: every criterion below is binary (pass / fail) and
must be traceable to CI evidence or an explicit governance artefact.

---

## 1. SX126x Profile Matrix Extension

### Prerequisite 1.1 — Minimum profile coverage

The test matrix must be extended to include at minimum:

| Chip | Band | DIO routing |
|---|---|---|
| SX1261 | 433 MHz | DIO1 (single-pin, SX126x convention) |
| SX1261 | 868 MHz | DIO1 |
| SX1262 | 433 MHz | DIO1 |
| SX1262 | 868 MHz | DIO1 |

SX126x uses a different DIO routing convention from SX127x. The host-side `RadioConfig` must be
extended (or a new config type introduced) before matrix validation can proceed.

**Gate condition:** `ProfileQualificationMatrix` records ≥ 4 qualified SX126x entries with status
`kValidated`.

### Prerequisite 1.2 — Hardware adapter implementation

A SX126x hardware adapter (`src/chips/sx126x/`) must exist and pass the full adapter contract:
- SPI initialisation sequence validated against SX1261/SX1262 datasheet register map
- TX path: packet transmission with TxDone confirmation
- RX path: packet reception with CRC pass/fail discrimination
- IRQ path: DIO1 interrupt routing for all interrupt types

**Gate condition:** Adapter contract tests pass 100% in CI for all 4 matrix profiles.

---

## 2. Parity Validation Scope

The SX126x adapter must demonstrate functional parity with the SX127x baseline across the
following scenario categories:

| Category | Parity scope | Reference SX127x tests |
|---|---|---|
| Initialisation | `begin()` succeeds for all 4 profiles; unsupported profiles return `kUnsupportedProfile` | `test_lora_driver_*init*` |
| TX | `send()` delivers payload and emits `kTxCompleted` | `test_lora_driver_*tx*` |
| RX | `startReceive()` enters listen mode and emits `kRxDone` on packet receipt | `test_lora_driver_*rx*` |
| IRQ | DIO1 interrupt fires and is routed to the correct `RadioEvent` | `test_lora_driver_*irq*` |
| Timeout recovery | `recoverFromTimeout()` returns deterministic `LoRaError` after RX/TX timeout | `test_lora_driver_*timeout*` |
| Sleep / wakeup | `sleep()` enters low-power state; `standby()` restores operational state | `test_lora_driver_*sleep*` |

**Gate condition:** All parity tests pass 100% for each of the 4 SX126x profiles with zero
exclusions and no waivers on blocking gates.

---

## 3. Regression Budget and Gate Thresholds

### Prerequisite 3.1 — Existing SX127x regression budget

Before V1-bis scope opens, the existing SX127x test matrix must remain stable:

| Metric | Required threshold | Gate ID (inherited) |
|---|---|---|
| SX127x blocking gate pass rate | 100% | `GATE-INIT-001` … `GATE-INTG-001` |
| Regression test pass rate | ≥ 100% (zero regression failures) | All `non_regression` gates |
| Waived blocking gates | 0 (no open waivers on blocking gates) | — |

**Gate condition:** A complete CI gate report for the SX127x profile set is produced and shows
`release_blocked = false` with 0 blocking failures and 0 open waivers.

### Prerequisite 3.2 — CI gate policy carry-over

All gate thresholds and categories defined in the current `CiGateEngine` policy
(`include/loradriver/ci_gates.hpp`) apply unchanged to SX126x profiles. No relaxation of existing
gate severity levels (`kBlocking`, `kWarning`, `kAdvisory`) is permitted for V1-bis onboarding.

New gates may be added for SX126x-specific scenarios, but existing gates may not be downgraded.

**Gate condition:** Gate rule definitions in `kGateRules_` are unchanged or additive only; no
existing gate's `severity` field is lowered.

---

## 4. Entry Trigger and Evidence Requirements

### Trigger condition

V1-bis scope is considered open when ALL of the following are simultaneously true:

1. Prerequisite 1.1 — SX126x profile matrix extension: **PASS**
2. Prerequisite 1.2 — Hardware adapter implementation: **PASS**
3. Parity validation scope (§2): **PASS** (all 6 categories, all 4 profiles)
4. Regression budget — SX127x gate report (§3.1): **PASS** (`release_blocked = false`)
5. Gate policy carry-over (§3.2): **PASS** (no gate severity downgrade)
6. **Sustained CI stability window:** The above CI gate report must show 0 blocking failures
   for a minimum of **5 consecutive CI runs** on the main branch immediately preceding the
   V1-bis milestone tag.

### Required evidence artefacts

Each of the above prerequisites must be backed by a traceable artefact:

| Prerequisite | Required artefact |
|---|---|
| 1.1 Profile matrix | `ProfileQualificationMatrix` output with ≥ 4 SX126x entries |
| 1.2 Adapter contract | CI test run link with 100% pass rate for adapter contract suite |
| Parity scope (§2) | CI test run link with 100% pass rate per category and profile |
| Regression budget (§3.1) | Serialised `GateReport` showing `release_blocked = false` |
| Gate carry-over (§3.2) | Diff or audit log confirming no gate severity downgrade |
| CI stability window | CI run history showing ≥ 5 consecutive passing runs |

All artefacts must be stored under `_bmad-output/implementation-artifacts/` or in the CI
artefact retention system defined in the governance policy before the V1-bis milestone tag is
applied.

---

## Relationship to V1 Scope Boundary

SX126x runtime support remains deferred in V1. The V1 boundary is:

- **In scope (V1):** SX1276, SX1278 — 433 MHz, 868 MHz — DIO0-only, DIO0+DIO1
- **Deferred (V1-bis):** SX1261, SX1262 — pending all criteria above

See [`docs/scope/v1-support-boundaries.md`](v1-support-boundaries.md) for the authoritative V1
scope definition.

---

## Cross-references

- V1 support scope: [`docs/scope/v1-support-boundaries.md`](v1-support-boundaries.md)
- Power profile trade-offs: [`docs/scope/power-profile-comparison.md`](power-profile-comparison.md)
- Integration guide (V1 profiles): [`docs/api/integration-guide.md`](../api/integration-guide.md)
- CI gate definitions: `include/loradriver/ci_gates.hpp`
- Profile qualification matrix: `include/loradriver/profile_qualification.hpp`
- Artifact governance policy: `include/loradriver/artifact_governance.hpp`
