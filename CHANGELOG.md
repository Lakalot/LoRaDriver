# Changelog

## 1.3.0 — 2026-05-14

Facade API release. Reduces the minimal ESP32 + pump-task user sketch
from ~40 lines to ~12, while preserving the existing direct DI API
verbatim. Strictly additive — no breaking changes.

### Added

- `loradriver::LoRa` facade class (`src/loradriver/lora.hpp` +
  `src/api/lora_facade.cpp`) plus the global instance `loradriver::lora`.
  `begin(cfg)` automatically:
  - Calls `SPI.begin()` (or `SPI.begin(sck, miso, mosi)` if `cfg.spi_pins`
    is set);
  - Binds the SPI device member;
  - Runs `transceiver().begin(cfg)`;
  - Attaches `attachInterrupt(cfg.pin_dio0, ...)` to a static IRAM
    trampoline;
  - (ESP32) starts `RadioPumpTask` with `cfg.pump.*` parameters;
  - Enters `start_receive(true)` continuous mode.
- Four `LoRaConfig` named presets, all `constexpr` zero-cost:
  `esp32_sx1276_868mhz`, `esp32_sx1278_433mhz`, `arduino_sx1276_868mhz`,
  `arduino_sx1278_433mhz`.
- `LoRaConfig::SpiPins` for boards with non-default SPI bus pinouts.
- `LoRaConfig::PumpConfig` for tunable pump task parameters; defaults
  match the values previously hardcoded in `pump_.start(trx, 2, 2, 2048, 1)`.
- `LoRaConfig::facade_auto_start_receive` (default true) and
  `LoRaConfig::facade_auto_pump` (default true) — opt-outs for sender-
  only / polling-only sketches.
- `examples/Esp32Async/` (renamed from `Esp32WithPumpTask`) showing the
  facade's `send_async` flow.
- `examples/AdvancedDirectDi/` preserving the pre-facade explicit DI
  pattern as a first-class example.
- `USAGE.md` task-oriented guide.

### Changed

- `Esp32SpiDevice` and `ArduinoSpiDevice` gain a default constructor
  and a `bind(SPI, cs, clock_hz)` setter for deferred init. The
  existing parameterised constructors delegate to `bind()` and remain
  back-compatible.
- `LoRaConfig::validate()` now rejects partial `spi_pins` configs
  (either all three SCK/MISO/MOSI are -1 or all three are >= 0).
- `examples/BasicSender`, `examples/BasicReceiver`, and (renamed)
  `examples/Esp32Async` rewritten using the facade. `examples/MultiInstance`
  unchanged structurally; gains a header comment explaining why it stays
  on the direct DI API.
- CI: `arduino-compile.yml` and `platformio.yml` sketch lists updated
  for the renamed/new examples.
- README quick-start, `docs/api.md` (new "Facade API" section).

### Notes

- The direct DI API (`Esp32SpiDevice` + `SX127xDriver` +
  `LoRaTransceiver` + `RadioPumpTask`) is **unchanged and first-class**.
  Multi-instance, host tests with `FakeSpiDevice`, and custom HAL
  injection continue to go through it.

## 1.2.1 — 2026-05-14

Tooling and packaging release. No runtime / register-level changes —
public radio API is byte-identical to 1.2.0.

### Added

- Full CI/CD pipeline: matrix host tests (Linux + Windows + macOS,
  Debug + Release), dedicated MSVC `/EHs-c- /GR-` job, ASan + UBSan,
  clang-format lint, clang-tidy static analysis, CodeQL security scan,
  PlatformIO + Arduino IDE compile checks, Doxygen → GitHub Pages,
  tag-triggered release packaging.
- `src/LoRaDriver.h` umbrella header — re-exports every public namespaced
  header. Required for arduino-cli library discovery (discovery is
  shallow on `src/`, so a top-level anchor is needed). Optional for
  PlatformIO / CMake consumers; the namespaced includes still work.

### Changed

- Public header layout: `include/loradriver/*.hpp` → `src/loradriver/*.hpp`.
  No `#include` change for users — `loradriver/...` paths still resolve.
  CMake consumers using `add_subdirectory(LoRaDriver)` are unaffected
  (the library target's PUBLIC include dir is updated). Direct
  consumers who pinned `target_include_directories(... include/)` must
  switch to `src/`.
- `Doxyfile` `PROJECT_NUMBER` corrected to match release version
  (was stuck at `0.1.0`).

### Fixed

- Sanitizer build: pass `-fno-sanitize=vptr` — the library is built
  with `-fno-rtti`, so UBSan's vptr check would link-fail without it.
- `tools/lint.sh`: executable bit restored.
- `lora_packet.hpp`: trailing comment alignment now clang-format-clean.

### Docs

- README rewritten: badges, "Why this driver?" pitch with the layered
  DI diagram, full v1.2 feature list, supported hardware table,
  installation for PlatformIO + Arduino IDE + CMake, quick-start,
  documentation index, build/sanitize commands, semver policy,
  contribution checklist.
- `docs/api.md` `lib_deps` example switched from `symlink://` to the
  pinned-tag git URL — much easier for downstream PlatformIO users.
- `docs/api.md` "CI scope" section rewritten to describe the seven new
  workflows instead of the old Ubuntu-only matrix.

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
