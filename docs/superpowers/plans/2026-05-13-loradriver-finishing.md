# LoRaDriver Production Finishing Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close the residual production-readiness gaps after v1.1.0 — latent bugs, missing library-grade plumbing (CI/Doxygen/lint), and hardware specifics that need datasheet-exact behaviour.

**Architecture:** Three phases. P0 fixes the actual bugs we know about (lifecycle, watchdog, OCP restore, errata 2.3 stub). P1 closes the library-grade plumbing gaps (CI run, Doxygen, clang-format, exception build). P2 lands the remaining hardware-specific items (full errata 2.3, runtime image recalibration on frequency change, OCP auto-trim on TX power change). Each phase leaves the repo green and consumer-buildable.

**Tech Stack:** C++17, CMake host tests, PlatformIO ESP32, GitHub Actions, Doxygen, clang-format.

**Repos:**
- Target: `D:\DEV\C++\LoRaDriver` (currently on tag `v1.1.0` + 2 post-tag fixes on `main`)
- Consumer: `D:\DEV\PlatformIO\SYNC-SIGNAL-LORA\SYNC-SIGNAL-LORA`

**Conventions:**
- Branch: `finishing/v1.2` off `main`.
- TDD strict: failing test → minimal impl → green → commit.
- Hardware tasks marked `🔌 HARDWARE GATE` — code prepared, flash/observation deferred to user.
- After all phases done, tag `v1.2.0`.

---

## Branch setup (run once)

- [ ] **Step 1: Create branch**

```bash
git -C D:/DEV/C++/LoRaDriver checkout main
git -C D:/DEV/C++/LoRaDriver checkout -b finishing/v1.2
git -C D:/DEV/C++/LoRaDriver status
```

Expected: `On branch finishing/v1.2`, clean tree.

---

## Phase P0 — Latent bug fixes (5 tasks)

### Task P0.1: Cycle de vie `begin()`/`end()`/`begin()` répété

**Files:**
- Modify: `src/chips/sx127x/sx127x_driver.cpp`
- Modify: `tests/host/test_sx127x_init_sequence.cpp`

- [ ] **Step 1: Write the failing test**

Add to `tests/host/test_sx127x_init_sequence.cpp` just before `int main()`:

```cpp
bool TestBeginEndBeginCycleSucceeds() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.auto_reset = false;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    drv.end();
    // Second begin must re-run the full init sequence, not return AlreadyInitialized.
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    return true;
}

bool TestBeginEndBeginAppliesNewConfig() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c1 = MakeCfg();
    c1.auto_reset = false;
    c1.sync_word = 0x12;
    LD_EXPECT_EQ(drv.begin(c1), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kSyncWord), std::uint8_t{0x12});
    drv.end();

    LoRaConfig c2 = c1;
    c2.sync_word = 0x34;
    LD_EXPECT_EQ(drv.begin(c2), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kSyncWord), std::uint8_t{0x34});
    return true;
}
```

Register both in `main()`:

```cpp
    LD_RUN(TestBeginEndBeginCycleSucceeds);
    LD_RUN(TestBeginEndBeginAppliesNewConfig);
```

- [ ] **Step 2: Run to verify behaviour**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure -R sx127x_init 2>&1 | tail -5
```

Expected: tests pass already if `end()` correctly resets `initialized_`. If they fail (e.g. `AlreadyInitialized` on second begin or shadows stale), continue.

- [ ] **Step 3: Reset runtime state in end()**

In `src/chips/sx127x/sx127x_driver.cpp`, find `SX127xDriver::end()` and replace with:

```cpp
void SX127xDriver::end() noexcept {
    if (!initialized_) return;
    (void)set_op_mode(opmode::kLoRaSleep);
    // Reset all runtime state so a subsequent begin() restarts from a clean slate.
    initialized_ = false;
    tx_in_progress_ = false;
    tx_deadline_ms_ = 0;
    rx_silence_deadline_ms_ = 0;
    op_mode_shadow_ = 0;
    cad_auto_rx_ = false;
    irq_head_ = 0;
    irq_tail_ = 0;
    event_cb_ = nullptr;
    cfg_ = LoRaConfig{};
}
```

- [ ] **Step 4: Build, test, commit**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure
```

Expected: 9 test executables pass, no regressions.

```bash
git -C D:/DEV/C++/LoRaDriver add src/chips/sx127x/sx127x_driver.cpp tests/host/test_sx127x_init_sequence.cpp
git -C D:/DEV/C++/LoRaDriver commit -m "$(cat <<'EOF'
fix: SX127xDriver::end() resets all runtime state for clean re-begin

Previously end() set chip to sleep and flipped initialized_=false but
left tx_in_progress_, deadlines, shadows, ring buffer indices, and
callback storage from the prior session. A subsequent begin() with a
new config could see stale state.

Now end() zeroes every member that begin() doesn't unconditionally
re-initialise, so begin/end/begin cycles are safe and apply the new
config cleanly.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

### Task P0.2: `RadioPumpTask::stop()` deadlock safety

**Files:**
- Modify: `include/loradriver/platform/esp32/radio_pump_task.hpp`

Pure code-review fix. No host test possible (FreeRTOS-only). Validation deferred to hardware soak.

- [ ] **Step 1: Verify the current stop() loop has a hard upper bound**

Read the current implementation:

```bash
grep -n "void stop()" -A 20 D:/DEV/C++/LoRaDriver/include/loradriver/platform/esp32/radio_pump_task.hpp
```

Expected output shows the `for (int i = 0; i < 60 && task_ != nullptr; ++i)` loop with the `vTaskDelete(task_)` fallback. That bound (60 × 10 ms = 600 ms) is correct; the deadlock concern is that we *also* call `vTaskNotifyGiveFromISR(t)` from the task context — that's actually safe (FreeRTOS docs explicitly permit `vTaskNotifyGiveFromISR` from non-ISR context too), but a clearer call is `xTaskNotifyGive`.

- [ ] **Step 2: Replace the ISR-style notify with the regular notify**

In `include/loradriver/platform/esp32/radio_pump_task.hpp`, modify the `stop()` method. Find:

```cpp
        // Nudge in case task is blocked in ulTaskNotifyTake.
        BaseType_t woken = pdFALSE;
        vTaskNotifyGiveFromISR(t, &woken);
```

Replace with:

```cpp
        // Nudge in case task is blocked in ulTaskNotifyTake.
        xTaskNotifyGive(t);
```

- [ ] **Step 3: Add `stop_timeout_ms` tunable so callers can extend the wait**

In the same file, after the `period_ms_` member (or wherever member init lives), add:

```cpp
    std::uint32_t stop_timeout_ms_ = 1000;  // total budget waiting for task to exit
```

In `start()`, accept an optional parameter and store it. Change the signature:

```cpp
    bool start(LoRaTransceiver& trx,
               std::uint32_t period_ms = 2,
               UBaseType_t priority = 2,
               std::uint32_t stack_words = 2048,
               BaseType_t core_id = 1,
               std::uint8_t tx_queue_depth = 4,
               std::uint32_t stop_timeout_ms = 1000) {
```

Add this line near the top of `start()` (after `period_ms_ = ...`):

```cpp
        stop_timeout_ms_ = stop_timeout_ms;
```

In `stop()`, replace the hardcoded loop bound:

```cpp
        // Wait up to stop_timeout_ms_ for the task to drain its cycle.
        const std::uint32_t step_ms = 10;
        const std::uint32_t iters = (stop_timeout_ms_ + step_ms - 1) / step_ms;
        for (std::uint32_t i = 0; i < iters && task_ != nullptr; ++i) {
            vTaskDelay(pdMS_TO_TICKS(step_ms));
        }
```

- [ ] **Step 4: Verify ESP32 build still compiles**

```bash
"$HOME/.platformio/penv/Scripts/pio.exe" run -d D:/DEV/PlatformIO/SYNC-SIGNAL-LORA/SYNC-SIGNAL-LORA -e esp32dev 2>&1 | tail -5
```

Expected: SUCCESS.

- [ ] **Step 5: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add include/loradriver/platform/esp32/radio_pump_task.hpp
git -C D:/DEV/C++/LoRaDriver commit -m "$(cat <<'EOF'
fix(pump): use task-context notify in stop() + configurable timeout

stop() called vTaskNotifyGiveFromISR from the main task context. That's
not undefined behaviour but it's the wrong API — xTaskNotifyGive is the
non-ISR counterpart and is what should be used here.

Added stop_timeout_ms parameter (default 1000 ms, up from a fixed 600 ms)
so callers running long sends (high SF, large payload) can give the task
more grace before the last-resort vTaskDelete.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

### Task P0.3: OCP trim restore in `set_ocp_enabled`

**Files:**
- Modify: `src/chips/sx127x/sx127x_driver.cpp`
- Modify: `tests/host/test_sx127x_runtime_setters.cpp`

- [ ] **Step 1: Write the failing test**

Add to `tests/host/test_sx127x_runtime_setters.cpp` before `int main()`:

```cpp
bool TestSetOcpEnabledPreservesTrim() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.ocp_ma = 100;  // trim = (100-45)/5 = 11 = 0x0B
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    const std::uint8_t trim_initial = spi.reg(reg::kOcp) & 0x1Fu;
    LD_EXPECT_EQ(trim_initial, std::uint8_t{0x0B});

    LD_EXPECT_EQ(drv.set_ocp_enabled(false), LoRaError::OK);
    LD_EXPECT_EQ(drv.set_ocp_enabled(true),  LoRaError::OK);

    // After enable/disable cycle the trim bits must be unchanged.
    LD_EXPECT_EQ(static_cast<std::uint8_t>(spi.reg(reg::kOcp) & 0x1Fu), trim_initial);
    return true;
}
```

Register in `main()`:

```cpp
    LD_RUN(TestSetOcpEnabledPreservesTrim);
```

- [ ] **Step 2: Run to confirm behaviour**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure -R runtime_setters 2>&1 | tail -5
```

Expected: passes. Current `set_ocp_enabled` only touches bit 5 via `|=` / `&= ~0x20u` so the low 5 bits (trim) are preserved on the FakeSpiDevice. The test exists to guard against future regressions.

- [ ] **Step 3: Verify implementation is robust by reading then writing instead of relying on read-modify-write order**

The current implementation already reads RegOcp, ORs/ANDs bit 5, and writes back. That's the right pattern. No code change needed — the test pins the behaviour for future refactors.

- [ ] **Step 4: Commit (test-only)**

```bash
git -C D:/DEV/C++/LoRaDriver add tests/host/test_sx127x_runtime_setters.cpp
git -C D:/DEV/C++/LoRaDriver commit -m "test: pin set_ocp_enabled() trim preservation contract

Adds a regression test asserting that toggling OCP enable/disable does
not alter the low 5 trim bits. The current implementation already uses
read-modify-write on bit 5 only; this test guards against a future
refactor that might switch to a constant-write pattern and silently
zero the trim.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task P0.4: Errata 2.3 — IfFreq registers (placeholder for full impl in P2)

**Files:**
- Modify: `src/chips/sx127x/sx127x_driver.cpp`
- Modify: `tests/host/test_sx127x_init_sequence.cpp`

This task implements the **register writes** (the safe, mechanical part of errata 2.3). The full BW-conditional logic is in P2.1.

- [ ] **Step 1: Add register addresses to the constants header**

In `src/chips/sx127x/sx127x_registers.hpp`, find the `reg` namespace block and add (alphabetical-ish to match existing layout):

```cpp
constexpr std::uint8_t kIfFreq1            = 0x2F;
constexpr std::uint8_t kIfFreq2            = 0x30;
```

Find the right place near the existing block — these go between `kRssiWideband (0x2C)` and `kDetectionOptimize (0x31)`.

- [ ] **Step 2: Write the failing test**

Add to `tests/host/test_sx127x_init_sequence.cpp`:

```cpp
bool TestErrata23WritesIfFreqRegisters() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.bandwidth_hz = 125'000u;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    // Errata 2.3 §2.3 table for 125 kHz: RegIfFreq1=0x40, RegIfFreq2=0x00
    LD_EXPECT_EQ(spi.reg(reg::kIfFreq1), std::uint8_t{0x40});
    LD_EXPECT_EQ(spi.reg(reg::kIfFreq2), std::uint8_t{0x00});
    return true;
}
```

Register in `main()`:

```cpp
    LD_RUN(TestErrata23WritesIfFreqRegisters);
```

- [ ] **Step 3: Run to verify RED**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure -R init_sequence 2>&1 | tail -5
```

Expected: build OK, test fails because `kIfFreq1` is never written.

- [ ] **Step 4: Extend `apply_errata` with IfFreq writes for 125 kHz default**

In `src/chips/sx127x/sx127x_driver.cpp`, find `apply_errata` and replace its body:

```cpp
LoRaError SX127xDriver::apply_errata(std::uint32_t bw_hz, std::uint32_t freq_hz) noexcept {
    LoRaError e;

    // Errata 2.1: High BW optimisation when BW = 500 kHz and high-band
    if (bw_hz == 500'000u && freq_hz >= 525'000'000u) {
        if ((e = spi_.write_register(reg::kHighBwOptimize1, 0x02)) != LoRaError::OK) return e;
        if ((e = spi_.write_register(reg::kHighBwOptimize2, 0x64)) != LoRaError::OK) return e;
    } else {
        if ((e = spi_.write_register(reg::kHighBwOptimize1, 0x03)) != LoRaError::OK) return e;
    }

    // Errata 2.3: Receiver spurious reception of a LoRa signal.
    // The chip generates a spurious frequency reference at the IF; the
    // datasheet gives a per-BW table for RegIfFreq1/2 + a NOP on
    // RegDetectOptimize bit 7. For BW ≥ 500 kHz the errata workaround is
    // to set bit 7 of DetectOptimize (0x80) and write IfFreq1/2 = 0x00.
    // Below 500 kHz, clear bit 7 and use BW-specific IfFreq values.
    // Full table implemented in P2.1; here we land the 125 kHz default.
    if (bw_hz < 500'000u) {
        // Read-modify-write DetectOptimize to clear bit 7 without disturbing
        // the SF6/non-SF6 setting (already applied earlier).
        std::uint8_t det = 0;
        if ((e = spi_.read_register(reg::kDetectionOptimize, det)) != LoRaError::OK) return e;
        det &= ~0x80u;
        if ((e = spi_.write_register(reg::kDetectionOptimize, det)) != LoRaError::OK) return e;

        // BW=125kHz table value. P2.1 will switch on bw_hz for the others.
        if ((e = spi_.write_register(reg::kIfFreq1, 0x40)) != LoRaError::OK) return e;
        if ((e = spi_.write_register(reg::kIfFreq2, 0x00)) != LoRaError::OK) return e;
    } else {
        std::uint8_t det = 0;
        if ((e = spi_.read_register(reg::kDetectionOptimize, det)) != LoRaError::OK) return e;
        det |= 0x80u;
        if ((e = spi_.write_register(reg::kDetectionOptimize, det)) != LoRaError::OK) return e;
        if ((e = spi_.write_register(reg::kIfFreq1, 0x00)) != LoRaError::OK) return e;
        if ((e = spi_.write_register(reg::kIfFreq2, 0x00)) != LoRaError::OK) return e;
    }

    return LoRaError::OK;
}
```

- [ ] **Step 5: Build, test, commit**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure 2>&1 | tail -5
```

Expected: all tests pass.

```bash
git -C D:/DEV/C++/LoRaDriver add src/chips/sx127x/sx127x_registers.hpp src/chips/sx127x/sx127x_driver.cpp tests/host/test_sx127x_init_sequence.cpp
git -C D:/DEV/C++/LoRaDriver commit -m "$(cat <<'EOF'
feat(errata): partial errata 2.3 — DetectOptimize MSB + IfFreq scaffold

SX1276 errata 2.3 (RX spurious reception): the chip generates an IF
artifact that needs RegDetectOptimize bit 7 toggled per BW and
RegIfFreq1/2 set to per-BW values.

This task lands the 125 kHz default (most common). P2.1 will extend
the conditional with the full table (7.8/10.4/15.6/20.8/31.25/41.7/
62.5/125/250/500 kHz).

Bit 7 of DetectOptimize is set for BW≥500kHz (clears spurious IF),
cleared otherwise (preserves SF6 detection settings from
apply_modem_config).

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

### Task P0.5: Image-calibration cost amortization documentation

**Files:**
- Modify: `src/chips/sx127x/sx127x_driver.cpp`

Image calibration runs unconditionally in `begin()` and polls for up to 1 ms. That's correct; it's documented as a known cost. This task adds a `skip_image_calibration` config flag for callers who re-init frequently and want to skip the re-calibration.

- [ ] **Step 1: Add the config flag**

In `include/loradriver/lora_config.hpp`, locate the optimisation block (right after `tcxo_enabled`) and append:

```cpp
    bool          skip_image_calibration = false;  // Skip the 1ms RX image
                                                   // calibration at init.
                                                   // Use when re-initing
                                                   // frequently on a chip
                                                   // already calibrated.
```

- [ ] **Step 2: Honour the flag in apply_init_sequence**

In `src/chips/sx127x/sx127x_driver.cpp`, find the image-calibration block:

```cpp
    if ((e = set_op_mode(opmode::kFskSleep))  != LoRaError::OK) return e;
    if ((e = run_rx_image_calibration()) != LoRaError::OK) return e;
    if ((e = set_op_mode(opmode::kLoRaSleep)) != LoRaError::OK) return e;
```

Replace with:

```cpp
    if ((e = set_op_mode(opmode::kFskSleep))  != LoRaError::OK) return e;
    if (!cfg.skip_image_calibration) {
        if ((e = run_rx_image_calibration()) != LoRaError::OK) return e;
    }
    if ((e = set_op_mode(opmode::kLoRaSleep)) != LoRaError::OK) return e;
```

- [ ] **Step 3: Write the test**

Add to `tests/host/test_sx127x_init_sequence.cpp`:

```cpp
bool TestSkipImageCalibrationOmitsImageCalWrite() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.skip_image_calibration = true;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    // No write with bit 6 set on RegImageCal should appear in the log.
    for (const auto& w : spi.writes()) {
        if (w.reg == reg::kImageCal && (w.value & 0x40u) != 0u) {
            return false;
        }
    }
    return true;
}
```

Register in `main()`:

```cpp
    LD_RUN(TestSkipImageCalibrationOmitsImageCalWrite);
```

- [ ] **Step 4: Build, test, commit**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure
```

Expected: all pass.

```bash
git -C D:/DEV/C++/LoRaDriver add include/loradriver/lora_config.hpp src/chips/sx127x/sx127x_driver.cpp tests/host/test_sx127x_init_sequence.cpp
git -C D:/DEV/C++/LoRaDriver commit -m "feat: LoRaConfig::skip_image_calibration to bypass 1ms init cost

For callers that re-init frequently on a chip already calibrated (mode
switching, profile reload), skipping the FSK-mode ImageCal cuts ~1ms
off the boot path. Defaults to false (calibration runs) so cold-start
sensitivity is preserved.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## Phase P1 — Library-grade plumbing (4 tasks)

### Task P1.1: clang-format the entire codebase

**Files:** all `.cpp` and `.hpp` in `src/`, `include/loradriver/`, `tests/host/`.

- [ ] **Step 1: Run clang-format in-place on all tracked source files**

```bash
cd D:/DEV/C++/LoRaDriver
clang-format --version 2>&1 | head -1
```

Expected: `clang-format version 16.x` (or any v15+).

If clang-format is not installed:
```bash
choco install llvm -y 2>&1 | tail -3
```

Then format:

```bash
cd D:/DEV/C++/LoRaDriver
git ls-files 'src/*.cpp' 'src/**/*.cpp' 'src/**/*.hpp' \
              'include/loradriver/*.hpp' 'include/loradriver/**/*.hpp' \
              'tests/host/*.cpp' 'tests/host/*.hpp' | xargs clang-format -i
```

- [ ] **Step 2: Verify the build still passes after formatting**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug 2>&1 | tail -3
```

Expected: 100% tests pass.

- [ ] **Step 3: Run the lint script to confirm the codebase is now format-clean**

```bash
bash D:/DEV/C++/LoRaDriver/tools/lint.sh 2>&1 | tail -5
```

Expected: empty output and zero exit code (everything in canonical format).

- [ ] **Step 4: Commit the formatting churn separately**

```bash
git -C D:/DEV/C++/LoRaDriver add -A
git -C D:/DEV/C++/LoRaDriver commit -m "chore: clang-format pass on entire codebase

Brings every .cpp/.hpp file into canonical .clang-format style so the
CI lint job (introduced in v1.1.0) becomes useful. No behavioural
changes; host tests still 100% green.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task P1.2: Doxygen annotations on public API

**Files:**
- Modify: `include/loradriver/lora_transceiver.hpp`
- Modify: `include/loradriver/radio_driver.hpp`
- Modify: `include/loradriver/lora_config.hpp`
- Modify: `include/loradriver/lora_error.hpp`
- Modify: `include/loradriver/lora_packet.hpp`
- Modify: `include/loradriver/radio_event.hpp`
- Modify: `include/loradriver/radio_stats.hpp`
- Modify: `include/loradriver/version.hpp`

- [ ] **Step 1: Annotate `LoRaTransceiver` (the main user-facing class)**

In `include/loradriver/lora_transceiver.hpp`, find the `class LoRaTransceiver {` line and prepend:

```cpp
/// @brief High-level LoRa transceiver façade with FSM and packet dispatch.
///
/// Owns a reference to an IRadioDriver (caller manages driver lifetime).
/// Provides blocking @ref send and a packet callback model for RX.
///
/// State machine: Uninit → begin() → Standby. Standby ↔ Sleep / Tx /
/// RxSingle / RxContinuous / Cad. All transitions enforced — invalid
/// calls return LoRaError::InvalidState.
///
/// @note Thread safety: single-threaded by default. Use the optional
/// RadioPumpTask on ESP32 to serialise main-task + ISR access.
///
/// @par Example
/// @code
/// loradriver::hal::Esp32SpiDevice spi(SPI, /*cs=*/5);
/// loradriver::chips::SX127xDriver drv(spi);
/// loradriver::LoRaTransceiver trx(drv);
///
/// LoRaConfig cfg;
/// cfg.chip = ChipModel::SX1276;
/// cfg.frequency_hz = 868'000'000u;
/// cfg.pin_ss = 5; cfg.pin_reset = 14; cfg.pin_dio0 = 26;
/// trx.begin(cfg);
/// trx.start_receive(true);
/// @endcode
class LoRaTransceiver {
```

Annotate each public method. Replace the current method block with:

```cpp
    /// @brief Initialise the radio with the given configuration.
    /// @param cfg Validated by LoRaConfig::validate() first.
    /// @return OK on success; InvalidConfig if validate() fails; pass-through
    ///         from driver_.begin() otherwise.
    [[nodiscard]] LoRaError begin(const LoRaConfig& cfg) noexcept;

    /// @brief Tear down: detach callbacks, put chip to sleep, reset FSM.
    /// @note The DIO0 interrupt attached by the user must be detached
    ///       separately (the driver does not own that GPIO line).
    void end() noexcept;

    /// @brief Put the chip into the lowest-power state with retain.
    [[nodiscard]] LoRaError set_sleep() noexcept;

    /// @brief Bring the chip back to ready-to-RX/TX without re-init.
    [[nodiscard]] LoRaError set_standby() noexcept;

    /// @brief Blocking transmit. Returns when TxDone fires or timeout expires.
    /// @param data Payload bytes (≤ 255).
    /// @param len  Number of bytes (must be > 0).
    /// @param timeout_ms Total wait budget in milliseconds.
    /// @return OK / TxTimeout / SpiFailure / NullArgument / TxBufferTooLarge.
    [[nodiscard]] LoRaError send(const std::uint8_t* data, std::size_t len,
                                 std::uint32_t timeout_ms = 2000) noexcept;

    /// @brief Put the radio in continuous or single-shot RX mode.
    /// @param continuous If true, RXCONTINUOUS (stays in RX after each packet).
    [[nodiscard]] LoRaError start_receive(bool continuous = true) noexcept;

    /// @brief Trigger Channel Activity Detection.
    /// @param auto_rx If true and a signal is detected, automatically
    ///                enters RX_CONTINUOUS on CadDone.
    [[nodiscard]] LoRaError start_cad(bool auto_rx = false) noexcept;

    /// @brief Register the RX-packet callback (invoked from poll()).
    /// @note Must be noexcept — driver is built with -fno-exceptions.
    void on_receive(PacketCallback cb) noexcept;

    /// @brief Register the general radio-event callback.
    void on_event(EventCallback cb) noexcept;

    /// @brief Register the TxDone / TxTimeout callback.
    void on_tx_done(TxDoneCallback cb) noexcept;

    /// @brief Register the ValidHeader (pre-RxDone) callback.
    void on_header(HeaderCallback cb) noexcept;

    /// @brief Drain pending events from the ring buffer + run watchdogs.
    /// @note Must be called from a single thread (main loop or pump task).
    void poll() noexcept;
```

- [ ] **Step 2: Annotate `LoRaConfig`**

In `include/loradriver/lora_config.hpp`, prepend the struct:

```cpp
/// @brief Complete radio configuration passed to LoRaTransceiver::begin().
///
/// All defaults are sane for SX1276 868 MHz LoRa P2P. Override per
/// member as needed. Call validate() before passing to begin().
struct LoRaConfig {
```

(Field-level comments already exist in the struct and are readable; no
need to add `@brief` to each — the inline `// comment` are enough for
Doxygen with `EXTRACT_ALL=NO` config. Keep them.)

Annotate `validate()`:

```cpp
    /// @brief Reject configurations that the chip cannot honour.
    /// @return OK if every field is in range and mutually consistent.
    [[nodiscard]] LoRaError validate() const noexcept;

    /// @brief Whether Low Data Rate Optimise must be enabled for this SF/BW.
    /// @return true if symbol duration > 16 ms (Semtech AN1200.24).
    [[nodiscard]] bool ldro_required() const noexcept;
```

- [ ] **Step 3: Annotate `LoRaError`**

In `include/loradriver/lora_error.hpp`, prepend the enum:

```cpp
/// @brief Result code returned by every fallible driver/transceiver method.
///
/// Cast to int gives the underlying value; use to_string() for log output.
enum class LoRaError : std::uint8_t {
```

Annotate `to_string`:

```cpp
/// @brief Human-readable name of an error code. Stable across versions.
const char* to_string(LoRaError e) noexcept;
```

- [ ] **Step 4: Annotate `LoRaPacket`, `RadioEvent`, `RadioStats`, `version.hpp`**

Each gets a single `/// @brief` line on the type and on each public method.

For `include/loradriver/lora_packet.hpp`:

```cpp
/// @brief Per-packet metadata delivered alongside the payload to on_receive.
struct LoRaPacket {
```

And on `snr_db()`:

```cpp
    /// @brief Return SNR as floating-point dB (snr_q4 / 4.0).
    [[nodiscard]] float snr_db() const noexcept {
        return static_cast<float>(snr_q4) / 4.0f;
    }
```

For `include/loradriver/radio_event.hpp`:

```cpp
/// @brief Low-level radio events emitted from process_events().
enum class RadioEvent : std::uint8_t {
```

For `include/loradriver/radio_stats.hpp`:

```cpp
/// @brief Cumulative counters and last-packet metrics.
///
/// Snapshot-by-value via LoRaTransceiver::stats(). Trivially copyable.
struct RadioStats {
```

For `include/loradriver/version.hpp`:

```cpp
/// @brief Compile-time major version. Matches CMake project() VERSION.
constexpr std::uint8_t kVersionMajor = 1;
/// @brief Compile-time minor version.
constexpr std::uint8_t kVersionMinor = 1;
/// @brief Compile-time patch version.
constexpr std::uint8_t kVersionPatch = 0;

/// @brief Runtime major version (matches kVersionMajor at build time).
[[nodiscard]] std::uint8_t version_major() noexcept;
/// @brief Runtime minor version.
[[nodiscard]] std::uint8_t version_minor() noexcept;
/// @brief Runtime patch version.
[[nodiscard]] std::uint8_t version_patch() noexcept;
/// @brief Runtime version string "MAJOR.MINOR.PATCH".
[[nodiscard]] const char* version_string() noexcept;
```

- [ ] **Step 5: Annotate `IRadioDriver`**

In `include/loradriver/radio_driver.hpp`, prepend the class:

```cpp
/// @brief Chip-agnostic radio driver interface.
///
/// Implemented by SX127xDriver (and future SX126xDriver). All fallible
/// methods return LoRaError. Callbacks must be noexcept.
class IRadioDriver {
```

Then add a one-line `@brief` above each pure-virtual method. For brevity in this plan, the engineer should mirror the docstrings already present in `lora_transceiver.hpp` for matching methods (begin, end, set_sleep, set_standby, start_transmit, start_receive, start_cad, read_packet, set_frequency, set_tx_power, set_spreading_factor, set_bandwidth, set_lna_gain, set_ocp_enabled, start_continuous_wave, packet_rssi, packet_snr, frequency_error_hz, current_rssi, random_byte, get_stats, reset_stats, check_alive, set_event_callback, process_events, handle_interrupt).

- [ ] **Step 6: Generate Doxygen and verify no warnings on public API**

```bash
doxygen D:/DEV/C++/LoRaDriver/Doxyfile 2>&1 | grep -i "warning" | head -20
```

If `doxygen` is not installed:
```bash
choco install doxygen.install -y 2>&1 | tail -3
```

Expected: zero warnings about undocumented public types or methods. If warnings remain, fix them by adding the missing annotations.

- [ ] **Step 7: Verify build still passes (Doxygen comments don't affect compilation)**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug 2>&1 | tail -3
```

Expected: 100% tests pass.

- [ ] **Step 8: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add include/loradriver/
git -C D:/DEV/C++/LoRaDriver commit -m "$(cat <<'EOF'
docs: Doxygen annotations on public API

Adds @brief / @param / @return / @note blocks on every public type and
method in include/loradriver/. Doxygen now emits zero warnings on the
public-API extraction (Doxyfile EXTRACT_ALL=NO).

Existing per-field // comments in LoRaConfig retained — they're picked
up by Doxygen as member descriptions.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

### Task P1.3: Build under -fno-exceptions on host to validate the no-throw contract

**Files:**
- Modify: `CMakeLists.txt`

The CMake gate is in place but only triggers on Clang/GCC. On MSVC host
we never exercise the no-exceptions path. This task adds a CMake option
to force the loradriver target to build with `/EHs-c-` on MSVC, used
exclusively in CI.

- [ ] **Step 1: Add the option**

In `D:/DEV/C++/LoRaDriver/CMakeLists.txt`, find the existing block:

```cmake
if(NOT MSVC)
  target_compile_options(loradriver PRIVATE -fno-exceptions -fno-rtti)
endif()
```

Replace with:

```cmake
option(LORADRIVER_NO_EXCEPTIONS_MSVC "Force /EHs-c- on MSVC to validate noexcept invariants" OFF)

if(NOT MSVC)
  target_compile_options(loradriver PRIVATE -fno-exceptions -fno-rtti)
elseif(LORADRIVER_NO_EXCEPTIONS_MSVC)
  # MSVC: clear default /EHsc then add /EHs-c- (no exceptions, no RTTI).
  string(REGEX REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  target_compile_options(loradriver PRIVATE /EHs-c- /GR-)
endif()
```

- [ ] **Step 2: Build with the flag on**

```bash
rm -rf D:/DEV/C++/LoRaDriver/build/host-noex
cmake -S D:/DEV/C++/LoRaDriver -B D:/DEV/C++/LoRaDriver/build/host-noex -DLORADRIVER_NO_EXCEPTIONS_MSVC=ON
cmake --build D:/DEV/C++/LoRaDriver/build/host-noex 2>&1 | tail -10
```

Expected: build succeeds. If it fails (typically `<chrono>` or `<functional>` pulling in throwing internals), the failure tells you which file/symbol — investigate and either guard the call or accept that MSVC + standard library cannot run without exceptions cleanly. **If the build fails fundamentally**, document the limitation in CHANGELOG and skip the test run.

- [ ] **Step 3: Run the tests under the no-exceptions binary**

```bash
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host-noex -C Debug --output-on-failure 2>&1 | tail -5
```

Expected: 100% tests pass.

- [ ] **Step 4: Add the option to the CI sanitizers job (as a separate job)**

In `D:/DEV/C++/LoRaDriver/.github/workflows/host-tests.yml`, find the `sanitizers:` job. After it, append a sibling job:

```yaml
  no-exceptions-msvc:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - name: Configure
        run: cmake -S . -B build/noex -DLORADRIVER_NO_EXCEPTIONS_MSVC=ON
      - name: Build
        run: cmake --build build/noex --config Debug
      - name: Run
        run: ctest --test-dir build/noex -C Debug --output-on-failure
```

- [ ] **Step 5: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add CMakeLists.txt .github/workflows/host-tests.yml
git -C D:/DEV/C++/LoRaDriver commit -m "$(cat <<'EOF'
ci: validate noexcept contract on MSVC via LORADRIVER_NO_EXCEPTIONS_MSVC

Adds a CMake option that flips MSVC to /EHs-c- /GR- (no exceptions, no
RTTI) for the loradriver target. New CI job runs the host tests under
that configuration on windows-latest, catching any path that
implicitly assumes exception support.

Default OFF so the regular MSVC dev experience is unchanged.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

### Task P1.4: Push to GitHub + verify CI green + publish tags

**Files:** none (git remote operations).

- [ ] **Step 1: Confirm the user wants to push (sanity check the remote)**

```bash
git -C D:/DEV/C++/LoRaDriver remote -v
```

Expected: an `origin` remote pointing at GitHub. If not configured, the engineer should ask the user for the GitHub URL and add it via `git remote add origin <url>`.

- [ ] **Step 2: Push the finishing branch**

```bash
git -C D:/DEV/C++/LoRaDriver push -u origin finishing/v1.2
```

Expected: branch pushed; PR URL appears in stderr.

- [ ] **Step 3: Wait for CI to run and confirm green**

Ask the user to check the GitHub Actions tab. All four jobs (host
matrix on Linux/Windows/macOS, sanitizers, no-exceptions-msvc, lint)
should turn green. If any fails, the engineer fixes the underlying
issue (compile error, formatting drift, sanitizer leak) before
proceeding.

- [ ] **Step 4: After CI green, merge to main + tag**

Done in the final `Merge & tag v1.2.0` task at the end of the plan. Do not push tags here.

- [ ] **Step 5: Commit (no file change; this is a workflow gate marker)**

No commit on this task. Just verify push succeeded and proceed.

---

## Phase P2 — Hardware-specific deep work (3 tasks)

### Task P2.1: Full errata 2.3 table per bandwidth

**Files:**
- Modify: `src/chips/sx127x/sx127x_driver.cpp`
- Modify: `tests/host/test_sx127x_init_sequence.cpp`

- [ ] **Step 1: Write tests for every BW value in the errata table**

Add to `tests/host/test_sx127x_init_sequence.cpp`:

```cpp
struct Errata23Case {
    std::uint32_t bw_hz;
    std::uint8_t  expected_iffreq1;
    std::uint8_t  expected_iffreq2;
};

// Errata 2.3 §2.3 table (SX1276/77/78/79 errata note v1.1):
// BW (kHz)  | IfFreq1 | IfFreq2
//   7.8     | 0x48    | 0x00
//  10.4     | 0x44    | 0x00
//  15.6     | 0x44    | 0x00
//  20.8     | 0x44    | 0x00
//  31.25    | 0x44    | 0x00
//  41.7     | 0x44    | 0x00
//  62.5     | 0x40    | 0x00
// 125       | 0x40    | 0x00
// 250       | 0x40    | 0x00
// 500       | 0x00    | 0x00  (DetectOptimize bit 7 set instead)

bool TestErrata23TableFor7800Hz() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.bandwidth_hz = 7'800u;
    c.spreading_factor = 12;  // 7.8 kHz BW requires SF≥11
    c.symbol_timeout = 100;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kIfFreq1), std::uint8_t{0x48});
    LD_EXPECT_EQ(spi.reg(reg::kIfFreq2), std::uint8_t{0x00});
    return true;
}

bool TestErrata23TableFor62500Hz() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.bandwidth_hz = 62'500u;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kIfFreq1), std::uint8_t{0x40});
    LD_EXPECT_EQ(spi.reg(reg::kIfFreq2), std::uint8_t{0x00});
    return true;
}

bool TestErrata23TableFor41700Hz() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.bandwidth_hz = 41'700u;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kIfFreq1), std::uint8_t{0x44});
    LD_EXPECT_EQ(spi.reg(reg::kIfFreq2), std::uint8_t{0x00});
    return true;
}

bool TestErrata23TableFor500000HzHighBand() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.bandwidth_hz = 500'000u;
    c.frequency_hz = 868'000'000u;  // high-band so BW500 is legal
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kIfFreq1), std::uint8_t{0x00});
    LD_EXPECT_EQ(spi.reg(reg::kIfFreq2), std::uint8_t{0x00});
    // DetectOptimize bit 7 must be set for BW=500kHz
    LD_EXPECT(static_cast<std::uint8_t>(spi.reg(reg::kDetectionOptimize) & 0x80u) != 0u);
    return true;
}
```

Register all in `main()`:

```cpp
    LD_RUN(TestErrata23TableFor7800Hz);
    LD_RUN(TestErrata23TableFor62500Hz);
    LD_RUN(TestErrata23TableFor41700Hz);
    LD_RUN(TestErrata23TableFor500000HzHighBand);
```

- [ ] **Step 2: Run to verify RED (P0.4 only handled 125 kHz)**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure -R init_sequence 2>&1 | tail -15
```

Expected: 7800Hz and 41700Hz tests fail. 62500Hz might pass (same value as 125 kHz). 500kHz passes (P0.4 already handles it).

- [ ] **Step 3: Replace the 125 kHz hardcode with the full table**

In `src/chips/sx127x/sx127x_driver.cpp`, find the errata 2.3 block in `apply_errata`. Replace the `if (bw_hz < 500'000u) { ... }` branch with:

```cpp
    if (bw_hz < 500'000u) {
        std::uint8_t det = 0;
        if ((e = spi_.read_register(reg::kDetectionOptimize, det)) != LoRaError::OK) return e;
        det &= ~0x80u;
        if ((e = spi_.write_register(reg::kDetectionOptimize, det)) != LoRaError::OK) return e;

        // Per-BW IfFreq1 from errata 2.3 table.
        std::uint8_t if_freq1 = 0x40;  // 62.5, 125, 250 kHz default
        if (bw_hz == 7'800u) {
            if_freq1 = 0x48;
        } else if (bw_hz <= 41'700u) {
            // 10.4, 15.6, 20.8, 31.25, 41.7 kHz
            if_freq1 = 0x44;
        }
        if ((e = spi_.write_register(reg::kIfFreq1, if_freq1)) != LoRaError::OK) return e;
        if ((e = spi_.write_register(reg::kIfFreq2, 0x00))   != LoRaError::OK) return e;
    } else {
```

- [ ] **Step 4: Build, test, commit**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure
```

Expected: all init sequence tests pass.

```bash
git -C D:/DEV/C++/LoRaDriver add src/chips/sx127x/sx127x_driver.cpp tests/host/test_sx127x_init_sequence.cpp
git -C D:/DEV/C++/LoRaDriver commit -m "feat(errata): full errata 2.3 IfFreq table per BW

Per the SX1276/77/78/79 errata note v1.1 §2.3:
  BW =   7.8 kHz: RegIfFreq1=0x48, IfFreq2=0x00
  BW = 10.4–41.7 kHz: 0x44, 0x00
  BW = 62.5–250 kHz: 0x40, 0x00
  BW = 500 kHz: 0x00, 0x00 (+ DetectOptimize bit 7 set)

Recovers RX sensitivity at low BW values that were silently degraded
by the spurious IF artifact.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task P2.2: Runtime image recalibration on large frequency change

**Files:**
- Modify: `include/loradriver/chips/sx127x_driver.hpp`
- Modify: `src/chips/sx127x/sx127x_driver.cpp`
- Modify: `tests/host/test_sx127x_runtime_setters.cpp`

Datasheet §4.2.3.8: when frequency changes by more than ~5%, the chip's
image rejection drifts. Re-running ImageCal restores it. This task wires
that recalibration into `set_frequency()`.

- [ ] **Step 1: Write the failing test**

Add to `tests/host/test_sx127x_runtime_setters.cpp`:

```cpp
bool TestSetFrequencyLargeJumpTriggersRecalibration() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.frequency_hz = 868'000'000u;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    spi.clear_writes();

    // 868 MHz → 433 MHz is a ~50% drop, well past the 5% threshold.
    LD_EXPECT_EQ(drv.set_frequency(433'920'000u), LoRaError::OK);

    // ImageCal write with bit 6 set should appear in the log.
    bool saw_cal = false;
    for (const auto& w : spi.writes()) {
        if (w.reg == reg::kImageCal && (w.value & 0x40u) != 0u) saw_cal = true;
    }
    LD_EXPECT(saw_cal);
    return true;
}

bool TestSetFrequencySmallJumpSkipsRecalibration() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.frequency_hz = 868'000'000u;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    spi.clear_writes();

    // 868 → 868.5 MHz is a 0.06% change, well below threshold.
    LD_EXPECT_EQ(drv.set_frequency(868'500'000u), LoRaError::OK);

    for (const auto& w : spi.writes()) {
        if (w.reg == reg::kImageCal && (w.value & 0x40u) != 0u) return false;
    }
    return true;
}
```

Register both in `main()`:

```cpp
    LD_RUN(TestSetFrequencyLargeJumpTriggersRecalibration);
    LD_RUN(TestSetFrequencySmallJumpSkipsRecalibration);
```

- [ ] **Step 2: Run to verify RED**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure -R runtime_setters 2>&1 | tail -5
```

Expected: first test fails (no recalibration triggered).

- [ ] **Step 3: Implement conditional recalibration**

In `src/chips/sx127x/sx127x_driver.cpp`, find `SX127xDriver::set_frequency`:

```cpp
LoRaError SX127xDriver::set_frequency(std::uint32_t hz) noexcept {
    if (!initialized_) return LoRaError::NotInitialized;
    return apply_frequency(hz);
}
```

Replace with:

```cpp
LoRaError SX127xDriver::set_frequency(std::uint32_t hz) noexcept {
    if (!initialized_) return LoRaError::NotInitialized;

    // Datasheet §4.2.3.8: recalibrate RX image if the new frequency
    // differs from the calibrated one by more than ~5%.
    const std::uint32_t prev = cfg_.frequency_hz;
    const std::uint32_t larger  = (hz > prev) ? hz : prev;
    const std::uint32_t smaller = (hz > prev) ? prev : hz;
    const bool large_jump = (larger - smaller) * 20u > prev;  // > 5%

    LoRaError e = apply_frequency(hz);
    if (e != LoRaError::OK) return e;

    if (large_jump && !cfg_.skip_image_calibration) {
        // ImageCal requires FSK mode access; bracket the call with mode hops.
        if ((e = set_op_mode(opmode::kLoRaStandby)) != LoRaError::OK) return e;
        if ((e = set_op_mode(opmode::kFskSleep))   != LoRaError::OK) return e;
        if ((e = run_rx_image_calibration())       != LoRaError::OK) return e;
        if ((e = set_op_mode(opmode::kLoRaSleep))  != LoRaError::OK) return e;
        if ((e = set_op_mode(opmode::kLoRaStandby))!= LoRaError::OK) return e;
    }
    return LoRaError::OK;
}
```

- [ ] **Step 4: Build, test, commit**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure
```

Expected: all tests pass.

```bash
git -C D:/DEV/C++/LoRaDriver add src/chips/sx127x/sx127x_driver.cpp tests/host/test_sx127x_runtime_setters.cpp
git -C D:/DEV/C++/LoRaDriver commit -m "$(cat <<'EOF'
feat: runtime RX image recalibration on >5% frequency change

Datasheet §4.2.3.8 specifies that RX image rejection degrades when the
operating frequency moves more than ~5% from the last calibration
point. set_frequency() now compares the new value against the previously
stored one and runs the FSK-mode ImageCal cycle when the delta exceeds
the threshold.

Skipped if cfg_.skip_image_calibration is true so callers that re-init
intentionally can opt out.

The mode dance (Standby → FSK sleep → cal → LoRa sleep → Standby) adds
~1.5 ms to a large jump; small (<5%) channel hops remain instant.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

### Task P2.3: OCP auto-trim based on `set_tx_power` high-power path

**Files:**
- Modify: `src/chips/sx127x/sx127x_driver.cpp`
- Modify: `tests/host/test_sx127x_runtime_setters.cpp`

Datasheet §3.4.1: when PA_BOOST + PaDac=0x87 (high-power, dBm > 17),
OCP must be at least 130 mA, otherwise the chip self-throttles. Today
`set_tx_power()` flips PaDac but leaves OCP at whatever the user
configured at init.

- [ ] **Step 1: Write the failing test**

Add to `tests/host/test_sx127x_runtime_setters.cpp`:

```cpp
bool TestSetTxPowerHighPowerRaisesOcpTrim() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.ocp_ma = 100;  // trim 0x0B
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);

    // Below 17 dBm: no change to OCP.
    LD_EXPECT_EQ(drv.set_tx_power(14, PaOutput::PaBoost), LoRaError::OK);
    LD_EXPECT_EQ(static_cast<std::uint8_t>(spi.reg(reg::kOcp) & 0x1Fu),
                 std::uint8_t{0x0B});

    // Step into high-power: OCP must rise to at least 130 mA (trim 0x10 = 16,
    // formula 130mA → 130 > 120 so (130+30)/10 = 16).
    LD_EXPECT_EQ(drv.set_tx_power(20, PaOutput::PaBoost), LoRaError::OK);
    LD_EXPECT(static_cast<std::uint8_t>(spi.reg(reg::kOcp) & 0x1Fu) >= 0x10);
    return true;
}

bool TestSetTxPowerLowPowerRestoresUserOcp() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.ocp_ma = 100;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);

    // Go to high-power, then back to low.
    LD_EXPECT_EQ(drv.set_tx_power(20, PaOutput::PaBoost), LoRaError::OK);
    LD_EXPECT_EQ(drv.set_tx_power(14, PaOutput::PaBoost), LoRaError::OK);

    // After returning to low-power, user's original OCP trim is restored.
    LD_EXPECT_EQ(static_cast<std::uint8_t>(spi.reg(reg::kOcp) & 0x1Fu),
                 std::uint8_t{0x0B});
    return true;
}
```

Register both in `main()`:

```cpp
    LD_RUN(TestSetTxPowerHighPowerRaisesOcpTrim);
    LD_RUN(TestSetTxPowerLowPowerRestoresUserOcp);
```

- [ ] **Step 2: Run to verify RED**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure -R runtime_setters 2>&1 | tail -5
```

Expected: first test fails.

- [ ] **Step 3: Auto-trim OCP in set_tx_power**

In `src/chips/sx127x/sx127x_driver.cpp`, find `SX127xDriver::set_tx_power`:

```cpp
LoRaError SX127xDriver::set_tx_power(std::int8_t dbm, PaOutput out) noexcept {
    if (!initialized_) return LoRaError::NotInitialized;
    cfg_.tx_power_dbm = dbm;
    cfg_.pa_output    = out;
    return apply_tx_power(dbm, out);
}
```

Replace with:

```cpp
LoRaError SX127xDriver::set_tx_power(std::int8_t dbm, PaOutput out) noexcept {
    if (!initialized_) return LoRaError::NotInitialized;
    cfg_.tx_power_dbm = dbm;
    cfg_.pa_output    = out;
    LoRaError e = apply_tx_power(dbm, out);
    if (e != LoRaError::OK) return e;

    // High-power PA_BOOST (>17 dBm with PaDac=0x87) needs OCP ≥ 130 mA per
    // datasheet §3.4.1. Below 17 dBm, restore the user's configured OCP.
    if (out == PaOutput::PaBoost && dbm > 17) {
        const std::uint8_t required_ma = 130u;
        if (cfg_.ocp_ma < required_ma) {
            return apply_ocp(required_ma);
        }
    } else {
        // Restore user-configured OCP if we previously raised it.
        return apply_ocp(cfg_.ocp_ma);
    }
    return LoRaError::OK;
}
```

- [ ] **Step 4: Build, test, commit**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure
```

Expected: all tests pass.

```bash
git -C D:/DEV/C++/LoRaDriver add src/chips/sx127x/sx127x_driver.cpp tests/host/test_sx127x_runtime_setters.cpp
git -C D:/DEV/C++/LoRaDriver commit -m "$(cat <<'EOF'
feat: auto-bump OCP to ≥130 mA on high-power TX (dBm > 17)

Datasheet §3.4.1: PA_BOOST + PaDac=0x87 (high-power path, dBm > 17)
requires OcpTrim corresponding to at least 130 mA, otherwise the chip
silently throttles output. Previously set_tx_power() flipped PaDac
without touching OCP, so callers using LoRaConfig::ocp_ma=100 got
nominal 14 dBm even when asking for 20.

set_tx_power() now:
  - On dbm > 17 + PaBoost: writes OCP to max(user_setting, 130 mA).
  - On dbm ≤ 17: restores the user-configured OCP from cfg_.ocp_ma.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

---

## Final: Merge to main + tag v1.2.0 + push tags

- [ ] **Step 1: Full reconfigure + test from scratch**

```bash
rm -rf D:/DEV/C++/LoRaDriver/build
cmake -S D:/DEV/C++/LoRaDriver -B D:/DEV/C++/LoRaDriver/build/host
cmake --build D:/DEV/C++/LoRaDriver/build/host
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure 2>&1 | tail -5
```

Expected: `100% tests passed`.

- [ ] **Step 2: Consumer still builds**

```bash
"$HOME/.platformio/penv/Scripts/pio.exe" run -d D:/DEV/PlatformIO/SYNC-SIGNAL-LORA/SYNC-SIGNAL-LORA -e esp32dev 2>&1 | tail -5
```

Expected: SUCCESS.

- [ ] **Step 3: Embedded smoke still builds**

```bash
"$HOME/.platformio/penv/Scripts/pio.exe" test -d D:/DEV/C++/LoRaDriver -e smoke --without-uploading --without-testing 2>&1 | tail -5
```

Expected: `smoke:test_smoke [PASSED]`.

- [ ] **Step 4: 🔌 HARDWARE GATE — re-flash and observe**

Ask the user to flash and run on their ESP32:

```bash
"$HOME/.platformio/penv/Scripts/pio.exe" test -d D:/DEV/C++/LoRaDriver -e smoke
```

Expected: all 4 Unity tests `[PASSED]`.

- [ ] **Step 5: Bump version metadata to 1.2.0**

In `D:/DEV/C++/LoRaDriver/CMakeLists.txt`:

```cmake
project(LoRaDriver VERSION 1.2.0 LANGUAGES CXX)
```

In `D:/DEV/C++/LoRaDriver/include/loradriver/version.hpp`:

```cpp
constexpr std::uint8_t kVersionMajor = 1;
constexpr std::uint8_t kVersionMinor = 2;
constexpr std::uint8_t kVersionPatch = 0;
```

In `D:/DEV/C++/LoRaDriver/src/api/version.cpp`:

```cpp
const char*  version_string() noexcept { return "1.2.0"; }
```

In `D:/DEV/C++/LoRaDriver/library.json`:

```json
    "version": "1.2.0",
```

In `D:/DEV/C++/LoRaDriver/library.properties`:

```
version=1.2.0
```

Update the host test in `tests/host/test_radio_stats.cpp` (`TestVersionAccessors`):

```cpp
    LD_EXPECT_EQ(loradriver::version_minor(), std::uint8_t{2});
    ...
    LD_EXPECT(s[0] == '1' && s[1] == '.' && s[2] == '2');
```

Run:

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug 2>&1 | tail -3
```

Expected: green.

- [ ] **Step 6: Update CHANGELOG.md**

Prepend below the `# Changelog` line:

```markdown
## 1.2.0 — 2026-05-13

Production-finishing release. Closes the residual gaps after v1.1.0:
latent bugs, library plumbing, hardware-specific deep work.

### Fixed (P0)

- `SX127xDriver::end()` now resets all runtime state, making
  `begin()/end()/begin()` cycles safe.
- `RadioPumpTask::stop()` uses the correct non-ISR notify API and
  exposes a configurable `stop_timeout_ms` (default 1000 ms, up from 600).
- Test pins `set_ocp_enabled()` trim-preservation contract.

### Added (P0)

- Partial errata 2.3 scaffold: `RegIfFreq1/2` and `RegDetectOptimize`
  bit 7 are written conditionally on BW.
- `LoRaConfig::skip_image_calibration` bypasses the 1 ms FSK-mode
  calibration when re-initing on an already-calibrated chip.

### Added (P1)

- clang-format applied to the entire codebase; lint gate now meaningful.
- Doxygen `@brief`/`@param`/`@return`/`@note` on every public type,
  field, and method.
- `LORADRIVER_NO_EXCEPTIONS_MSVC=ON` CMake option to validate the
  noexcept contract under MSVC `/EHs-c-` (CI job added).
- Codebase pushed; CI matrix (3 OS + sanitizers + no-exceptions-MSVC +
  lint) green.

### Added (P2)

- Full errata 2.3 IfFreq table: per-BW values from 7.8 kHz to 500 kHz.
- Runtime RX image recalibration triggered when `set_frequency()`
  delta exceeds 5%.
- OCP auto-trim on high-power TX (`dBm > 17` + PA_BOOST sets OCP ≥
  130 mA per datasheet §3.4.1; restores user value when stepping down).
```

Commit the version bump:

```bash
git -C D:/DEV/C++/LoRaDriver add CMakeLists.txt include/loradriver/version.hpp src/api/version.cpp library.json library.properties tests/host/test_radio_stats.cpp CHANGELOG.md
git -C D:/DEV/C++/LoRaDriver commit -m "chore: bump version metadata to 1.2.0"
```

- [ ] **Step 7: Merge to main + tag**

```bash
git -C D:/DEV/C++/LoRaDriver checkout main
git -C D:/DEV/C++/LoRaDriver merge --no-ff finishing/v1.2 -m "Merge LoRaDriver v1.2 production finishing

Closes 13 residual gaps after v1.1.0:
  - P0 bugs (begin/end cycle, pump stop API, OCP test, errata 2.3 stub, skip-cal)
  - P1 plumbing (clang-format, Doxygen, no-exceptions MSVC, CI push)
  - P2 hardware (full errata 2.3 table, runtime recal, OCP auto-trim)

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"

git -C D:/DEV/C++/LoRaDriver tag -a v1.2.0 -m "LoRaDriver v1.2.0 — production finishing"
git -C D:/DEV/C++/LoRaDriver tag -l 'v*'
```

- [ ] **Step 8: Push (user decision)**

Ask the user whether to push:

> "v1.2.0 ready locally. Push to GitHub with `git push origin main --tags`?"

Wait for confirmation. Do not push automatically.

---

## Coverage map — 13 items → tasks

| Item | Phase task |
|---|---|
| `RadioPumpTask::stop()` deadlock | P0.2 |
| ImageCal blocks 1 ms in `begin()` | P0.5 |
| Errata 2.3 not implemented | P0.4 (stub) + P2.1 (full) |
| `set_ocp_enabled` trim restore | P0.3 (test) + P2.3 (auto-trim covers it too) |
| `begin/end/begin` cycle | P0.1 |
| CI never run | P1.4 |
| Doxygen never generated | P1.2 |
| clang-format never run | P1.1 |
| `-fno-exceptions` never built E2E | P1.3 |
| Tags not pushed | P1.4 / Final.Step 8 |
| Errata 2.3 (RegIfFreq1/2) | P2.1 |
| Runtime calibration on freq change | P2.2 |
| OCP auto-trim on `set_tx_power` | P2.3 |

All 13 items covered.

---

## Execution handoff

Plan complete and saved to `docs/superpowers/plans/2026-05-13-loradriver-finishing.md`.

**Two execution options:**

1. **Subagent-Driven (recommended)** — Fresh subagent per task, two-stage review (spec + code quality) between tasks. Same pattern as v1.0 and v1.1.
2. **Inline Execution** — Execute tasks in this session using executing-plans, batch execution with checkpoints.

Which approach?
