# Changelog

## 1.2.0 — 2026-05-13

Production-finishing release. Closes the residual gaps after v1.1.0:
latent bugs, library plumbing, hardware-specific deep work.

### Fixed (P0)

- `SX127xDriver::end()` now resets all runtime state, making
  `begin()/end()/begin()` cycles safe.
- `RadioPumpTask::stop()` uses the correct non-ISR notify API
  (`xTaskNotifyGive`) and exposes a configurable `stop_timeout_ms`
  (default 1000 ms, up from 600).
- Regression test pins `set_ocp_enabled()` trim-preservation contract.

### Added (P0)

- Errata 2.3 scaffold: `RegIfFreq1/2` and `RegDetectOptimize` bit 7
  written conditionally on BW (full table in P2.1).
- `LoRaConfig::skip_image_calibration` bypasses the 1 ms FSK-mode
  calibration when re-initing on an already-calibrated chip.

### Added (P1)

- clang-format applied to the entire codebase; lint gate now meaningful.
- Doxygen `@brief` annotations on every public type/field/method.
- `LORADRIVER_NO_EXCEPTIONS_MSVC=ON` CMake option to validate the
  noexcept contract under MSVC `/EHs-c- /GR-` (validated locally; not
  in CI to stay within the free Actions tier).
- Branch `finishing/v1.2` pushed to GitHub. CI matrix reduced to
  `ubuntu-latest` only (build+test, sanitizers, clang-format lint) so
  the workflow runs on the free public-repo tier. See docs/api.md
  "CI scope" for the rationale and how to add Windows/macOS jobs back.

### Added (P2)

- Full errata 2.3 IfFreq table per BW: 7.8 kHz (0x48), 10.4–41.7 kHz
  (0x44), 62.5–250 kHz (0x40), 500 kHz (0x00 + DetectOptimize bit 7).
- Runtime RX image recalibration triggered when `set_frequency()`
  delta exceeds 5%. Uses 64-bit arithmetic to avoid uint32 overflow on
  large jumps (e.g. 868→433 MHz).
- OCP auto-trim on high-power TX (`dBm > 17` + PA_BOOST sets OCP ≥
  130 mA per datasheet §3.4.1; restores user value when stepping down).

## 1.1.0 — 2026-05-13

Hardening release. See the 29-point gap list resolved in
`docs/superpowers/plans/2026-05-13-loradriver-hardening.md`.

### Added (P0)

- `LoRaConfig::auto_reset` (default true) — driver pulses RST itself.
- `IRadioDriver::check_alive()` — RegVersion heartbeat for runtime liveness.
- `LoRaConfig::polling_mode` — `process_events()` works without a DIO0 ISR.
- Mode-transition read-back verify on TX / RX / CAD entry.
- Extended embedded smoke test (check_alive + TX + loopback) + `docs/hardware-smoke.md`.
- Host coverage for SX1278 init path.

### Added (P1)

- `LoRaConfig::rx_silence_timeout_ms` — RX idle watchdog with RxTimeout event.
- Split FIFO base addresses (TX=0, RX=128) to prevent concurrent stomp.
- 10-bit `symbol_timeout` properly written across ModemConfig2 + SymbTimeoutLsb.
- RX image calibration during init (datasheet §4.2.3.8).
- `LoRaTransceiver::end()` now clears callbacks and detaches driver hook.
- `RadioPumpTask::stop()` is cooperative — no more mid-send `vTaskDelete`.
- `start_cad(auto_rx=true)` enters RX automatically on detection.
- `LoRaConfig::invert_iq` now wired to RegInvertIq / RegInvertIq2.
- `LoRaConfig::tcxo_enabled` for boards with external 32 MHz TCXO.
- `IRadioDriver::set_lna_gain(0..6)` runtime LNA control.
- `IRadioDriver::set_ocp_enabled(bool)` runtime OCP control.

### Changed

- `read_packet` returns `LoRaError` with out-param length (was: `int`).
- loradriver target builds under `-fno-exceptions -fno-rtti` on Clang/GCC.
- Removed defensive `try/catch` around event callbacks (callbacks must be
  noexcept — documented in `docs/api.md`).

### Added (P2)

- `loradriver::version_major/minor/patch/string()` runtime accessors.
- GitHub Actions CI for host tests (Linux/Windows/macOS) + sanitizers job + lint.
- `LORADRIVER_SANITIZERS=ON` CMake option (AddressSan + UBSan).
- `tools/lint.sh` clang-format dry-run script.
- Multi-instance example (`examples/MultiInstance/`).
- API reference (`docs/api.md`) covering lifecycle, lib_deps, ISR contract,
  random byte caveat, out-of-scope features.

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
