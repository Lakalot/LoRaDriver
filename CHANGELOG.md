# Changelog

## 1.0.0 — 2026-05-13

Full rewrite. The v0.1 governance/FSM scaffold is replaced by a real
register-level driver inspired by the proven LoRaDriverBak v2.1.0.

### Breaking

- Repo gutted; v0.1 governance/validation/CI shell removed.
- Public API replaced: `loradriver::LoRaTransceiver`, `loradriver::chips::SX127xDriver`,
  `loradriver::LoRaConfig`, `loradriver::hal::{ISpiDevice, ArduinoSpiDevice, Esp32SpiDevice}`,
  `loradriver::platform::esp32::RadioPumpTask`.
- `LoRaError` pruned to radio codes only (governance codes removed).
- Method naming switched to snake_case (`start_receive`, `handle_interrupt`,
  `enqueue_packet`, …).

### Added

- Full SX1276 + SX1278 register-level driver: TX/RX/CAD/sleep/standby + runtime tuning.
- Chip detection (`RegVersion == 0x12`).
- Errata 2.1 (BW 500 kHz high-band) applied.
- LDRO auto-selection per SF/BW (Semtech AN1200.24).
- PaBoost + PaDac high-power path (20 dBm).
- OCP trim.
- ISR-safe IRQ ring buffer (16 entries) with overflow stat.
- Watchdog TX timeout in `process_events()`.
- ESP32 DMA SPI via `transferBytes`.
- FreeRTOS `RadioPumpTask`: ISR-notified task, TX queue, auto RX restore.
- 10 host test files (49 assertions) + 1 embedded smoke.
- 3 Arduino examples (BasicSender, BasicReceiver, Esp32WithPumpTask).

### Removed

- SX126x driver (out of scope for v1.0).
- Governance layer (ci_gates, ota_gate, release_monitoring, rollback_governance,
  non_regression, profile_qualification, artifact_registry, changelog_manager,
  traceability_engine, versioning, incident_classification, incident_snapshot).
- Stub modules (`keep*ModuleLinked` placeholders in src/{core,infra,internal,platform/*}).

## 0.1.0 — 2026-02-22 (deprecated)

Initial governance/FSM scaffold. Did not transmit or receive on real silicon.
