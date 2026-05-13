# LoRaDriver v1.0 Hardening Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bring LoRaDriver v1.0 from "happy-path working" to "production-ready 1.1" by addressing the 29 known gaps catalogued after the v1.0 rewrite (P0 functional safety, P1 production robustness, P2 quality/DX, P3 features).

**Architecture:** 5 phases, each leaves the repo in a buildable + testable state. P0 first (hardware safety + bring-up). Then P1a (runtime watchdogs + verify) and P1b (FSM hardening). Then P2 (DX/CI/docs/portability). Then P3 (optional features). User can stop after any phase.

**Tech Stack:** C++17, CMake host tests, PlatformIO ESP32 embedded smoke, GitHub Actions CI, Doxygen, clang-format/tidy.

**Plan source (the 29 points):** see assistant message on 2026-05-13 in the brainstorming thread; reproduced here per-task as needed.

**Repos:**
- Target: `D:\DEV\C++\LoRaDriver` (already on tag v1.0.0)
- Consumer: `D:\DEV\PlatformIO\SYNC-SIGNAL-LORA\SYNC-SIGNAL-LORA`

**Conventions:**
- Branch: create `hardening/v1.1` off `main` for the work.
- Naming: `snake_case` methods/members, `PascalCase` types/enums, `kPascalCase` constants — same as v1.0.
- Every fallible function: `[[nodiscard]] LoRaError noexcept`.
- TDD strict: failing test → minimal impl → green → commit. Conventional Commits messages.
- Hardware gates: tasks needing real ESP32+SX1276/78 are flagged `🔌 HARDWARE GATE` and pause for user verification but do not block subsequent code tasks.

---

## Branch setup (run once before Phase P0)

- [ ] **Step 1: Create the hardening branch**

```bash
git -C D:/DEV/C++/LoRaDriver checkout main
git -C D:/DEV/C++/LoRaDriver pull --ff-only 2>&1 || true
git -C D:/DEV/C++/LoRaDriver checkout -b hardening/v1.1
git -C D:/DEV/C++/LoRaDriver status
```

Expected: `On branch hardening/v1.1`, working tree clean.

---

## Phase P0 — Functional safety (10 tasks)

Addresses the 6 P0 items: SX1278 hardware validation, embedded test execution, internal reset GPIO, runtime chip-alive check, mode-transition verify, polling-only fallback.

### Task P0.1: Internal reset GPIO pulse (point #3)

**Files:**
- Modify: `include/loradriver/lora_config.hpp`
- Modify: `src/chips/sx127x/sx127x_driver.cpp`
- Modify: `tests/host/test_sx127x_init_sequence.cpp`

- [ ] **Step 1: Add `auto_reset` field to LoRaConfig**

In `include/loradriver/lora_config.hpp`, add after `bool isr_snapshot = false;`:

```cpp
    bool          auto_reset        = true;   // Driver pulses pin_reset before init
    std::uint16_t reset_low_ms      = 2;      // RST low duration
    std::uint16_t reset_settle_ms   = 10;     // Wait after RST high before SPI
```

- [ ] **Step 2: Write the failing test (host: verify auto_reset call recorded)**

Add to `tests/host/test_sx127x_init_sequence.cpp` before `int main()`:

```cpp
// Records reset pin pulse calls without needing real GPIO.
class ResetCountingFakeSpi : public FakeSpiDevice {
public:
    int reset_calls() const { return reset_calls_; }
    void notify_reset_pulse() { ++reset_calls_; }
private:
    int reset_calls_ = 0;
};

bool TestBeginPulsesResetWhenAutoResetTrue() {
    ResetCountingFakeSpi spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.auto_reset = true;
    // Inject the reset hook into the driver via a static thunk variable
    // (driver will be modified in Step 4 to call SX127xDriver::s_reset_hook_).
    SX127xDriver::s_reset_hook_ = [&spi]() { spi.notify_reset_pulse(); };
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    SX127xDriver::s_reset_hook_ = nullptr;
    LD_EXPECT_EQ(spi.reset_calls(), 1);
    return true;
}

bool TestBeginSkipsResetWhenAutoResetFalse() {
    ResetCountingFakeSpi spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.auto_reset = false;
    SX127xDriver::s_reset_hook_ = [&spi]() { spi.notify_reset_pulse(); };
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    SX127xDriver::s_reset_hook_ = nullptr;
    LD_EXPECT_EQ(spi.reset_calls(), 0);
    return true;
}
```

Add the test invocations in `main()`:

```cpp
    LD_RUN(TestBeginPulsesResetWhenAutoResetTrue);
    LD_RUN(TestBeginSkipsResetWhenAutoResetFalse);
```

- [ ] **Step 3: Run to verify RED**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -5
```

Expected: compile error `s_reset_hook_` is not a member of `SX127xDriver`.

- [ ] **Step 4: Add the reset hook to SX127xDriver**

In `include/loradriver/chips/sx127x_driver.hpp`, add inside the public section:

```cpp
    /// Host-test injection point: function called by begin() in lieu of GPIO.
    /// On Arduino targets this stays nullptr and the driver pulses pin_reset
    /// directly via digitalWrite.
    using ResetHook = std::function<void()>;
    static inline ResetHook s_reset_hook_{};
```

Add `#include <functional>` near the top.

In `src/chips/sx127x/sx127x_driver.cpp`, add a helper just above `apply_init_sequence`:

```cpp
namespace {
void pulse_reset(const LoRaConfig& cfg) noexcept {
    if (SX127xDriver::s_reset_hook_) {
        SX127xDriver::s_reset_hook_();
        return;
    }
#ifdef ARDUINO
    if (cfg.pin_reset < 0) return;
    pinMode(cfg.pin_reset, OUTPUT);
    digitalWrite(cfg.pin_reset, LOW);
    delay(cfg.reset_low_ms);
    digitalWrite(cfg.pin_reset, HIGH);
    delay(cfg.reset_settle_ms);
#else
    (void)cfg;  // host build with no hook: skip
#endif
}
}  // namespace
```

In the same file, modify `LoRaError SX127xDriver::begin(const LoRaConfig& cfg) noexcept` — insert this **after** the `validate()` check, **before** `spi_.begin()`:

```cpp
    if (cfg.auto_reset) {
        pulse_reset(cfg);
    }
```

- [ ] **Step 5: Build and test**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure 2>&1 | tail -5
```

Expected: all tests pass, including the two new ones.

- [ ] **Step 6: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add include/loradriver/lora_config.hpp include/loradriver/chips/sx127x_driver.hpp src/chips/sx127x/sx127x_driver.cpp tests/host/test_sx127x_init_sequence.cpp
git -C D:/DEV/C++/LoRaDriver commit -m "$(cat <<'EOF'
feat: internal GPIO reset pulse in SX127xDriver::begin

Driver now pulses pin_reset LOW/HIGH before SPI init when LoRaConfig::
auto_reset=true (default). Eliminates the silent-init-failure class
where the chip stays in FSK mode from a previous boot.

Reset hook is injectable for host tests (s_reset_hook_); on Arduino
targets, defaults to digitalWrite + delay using cfg.pin_reset/
reset_low_ms/reset_settle_ms.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

### Task P0.2: Mode-transition read-back verify (point #5)

**Files:**
- Modify: `src/chips/sx127x/sx127x_driver.cpp`
- Modify: `tests/host/test_sx127x_init_sequence.cpp`

- [ ] **Step 1: Write the failing test**

Add to `tests/host/test_sx127x_init_sequence.cpp`:

```cpp
// Fake that drops every write to RegOpMode (chip is dead but bus says OK).
class DeadOpModeFakeSpi : public FakeSpiDevice {
public:
    [[nodiscard]] LoRaError transfer(std::uint8_t addr,
                                     const std::uint8_t* tx,
                                     std::uint8_t* rx,
                                     std::size_t len) noexcept override {
        const bool is_write = (addr & 0x80u) != 0u;
        const std::uint8_t r = addr & 0x7Fu;
        if (is_write && r == 0x01 /*OpMode*/) {
            // Silently swallow the write, then let read see the old value.
            return LoRaError::OK;
        }
        return FakeSpiDevice::transfer(addr, tx, rx, len);
    }
};

bool TestBeginDetectsDeadOpModeRegister() {
    DeadOpModeFakeSpi spi;
    SX127xDriver drv(spi);
    // begin() should fail at the FSK→LoRa sleep verify step because OpMode
    // never updates, so the LoRa-mode bit stays 0.
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::SpiVerifyMismatch);
    return true;
}
```

Register: `LD_RUN(TestBeginDetectsDeadOpModeRegister);` in `main()`.

- [ ] **Step 2: Run to verify it passes already (existing verify covers init)**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure -R init_sequence 2>&1 | tail -5
```

Expected: passes — confirms init-time verify works.

- [ ] **Step 3: Now write the failing test for runtime mode transitions**

Add to the same file:

```cpp
bool TestStandbyToTxVerifiesOpMode() {
    DeadOpModeFakeSpi spi;
    SX127xDriver drv(spi);
    // First, neutralise the fake during init so begin() succeeds.
    // We patch the fake AFTER begin() by toggling a flag.
    // Simpler: separate concrete class. Inline:
    FakeSpiDevice good;
    SX127xDriver drv_ok(good);
    LD_EXPECT_EQ(drv_ok.begin(MakeCfg()), LoRaError::OK);

    // Inject a fail-after-init flag by replacing the SPI with a wrapper.
    // For this test we use the standard fake + a runtime injection:
    // we'll simulate "OpMode register stuck" by pre-setting reg[0x01]=0x81
    // (Standby), starting TX which writes 0x83, then re-setting reg[0x01]
    // back to 0x81 before the read-back check happens.
    //
    // Cleaner alternative: add a one-shot `dead_after_writes_` counter to
    // FakeSpiDevice. We'll add that helper to keep this test readable.
    good.set_dead_after_writes(reg::kOpMode, 1);

    const std::uint8_t buf[2] = {0xAA, 0x55};
    LD_EXPECT_EQ(drv_ok.start_transmit(buf, 2, 1000), LoRaError::SpiVerifyMismatch);
    return true;
}
```

Register: `LD_RUN(TestStandbyToTxVerifiesOpMode);` in `main()`.

- [ ] **Step 4: Add `set_dead_after_writes` to FakeSpiDevice**

In `tests/host/fake_spi_device.hpp`, add to the public section:

```cpp
    /// After `count` successful writes to `reg`, every subsequent write to
    /// that register is silently dropped (bus says OK but storage unchanged).
    /// Used to simulate a chip register that has gone dead.
    void set_dead_after_writes(std::uint8_t reg, std::uint32_t count) noexcept {
        dead_reg_ = reg;
        dead_remaining_writes_ = count;
        dead_active_ = true;
    }
```

Then in the `transfer()` method, replace the write-path block:

```cpp
        if (is_write) {
            if (fail_writes_) return LoRaError::SpiFailure;
            if (tx == nullptr) return LoRaError::NullArgument;
            ++write_count_;
            for (std::size_t i = 0; i < len; ++i) {
                const std::size_t idx = (reg + i) % kRegCount;
                // Dead-register simulation: drop writes after the budget runs out.
                if (dead_active_ && static_cast<std::uint8_t>(idx) == dead_reg_) {
                    if (dead_remaining_writes_ == 0u) {
                        writes_.push_back({static_cast<std::uint8_t>(idx), tx[i]});
                        continue;  // record the attempt, but don't mutate regs_
                    }
                    --dead_remaining_writes_;
                }
                regs_[idx] = tx[i];
                writes_.push_back({static_cast<std::uint8_t>(idx), tx[i]});
            }
        }
```

And add the private members:

```cpp
    std::uint8_t  dead_reg_ = 0;
    std::uint32_t dead_remaining_writes_ = 0;
    bool          dead_active_ = false;
```

- [ ] **Step 5: Run to verify RED**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure -R init_sequence 2>&1 | tail -10
```

Expected: `TestStandbyToTxVerifiesOpMode` fails because `start_transmit` doesn't verify OpMode after writing.

- [ ] **Step 6: Add read-back verify to set_op_mode**

In `src/chips/sx127x/sx127x_driver.cpp`, replace `SX127xDriver::set_op_mode` with:

```cpp
LoRaError SX127xDriver::set_op_mode(std::uint8_t mode) noexcept {
    LoRaError e = spi_.write_register(reg::kOpMode, mode);
    if (e != LoRaError::OK) return e;
    // Read-back verify on critical mode transitions: TX, RX (any), CAD.
    // Skip verify on sleep/standby pairs since the chip mode bit dance is
    // already covered by the init sequence verify step.
    const bool needs_verify = (mode == opmode::kLoRaTx ||
                               mode == opmode::kLoRaRxCont ||
                               mode == opmode::kLoRaRxSingle ||
                               mode == opmode::kLoRaCad);
    if (needs_verify) {
        std::uint8_t readback = 0;
        e = spi_.read_register(reg::kOpMode, readback);
        if (e != LoRaError::OK) return e;
        if (readback != mode) return LoRaError::SpiVerifyMismatch;
    }
    op_mode_shadow_ = mode;
    return LoRaError::OK;
}
```

- [ ] **Step 7: Build, test, commit**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure 2>&1 | tail -5
```

Expected: all tests pass.

```bash
git -C D:/DEV/C++/LoRaDriver add include/loradriver/chips/sx127x_driver.hpp src/chips/sx127x/sx127x_driver.cpp tests/host/fake_spi_device.hpp tests/host/test_sx127x_init_sequence.cpp
git -C D:/DEV/C++/LoRaDriver commit -m "$(cat <<'EOF'
feat: verify OpMode read-back on critical mode transitions

set_op_mode() now reads OpMode back after writing TX, RxCont, RxSingle,
or CAD modes and returns SpiVerifyMismatch if the chip didn't latch.
Catches the silent failure where SPI write returns OK but the chip is
unreachable (brown-out, sleep glitch, MISO stuck high).

Sleep/standby transitions skip the verify because the init sequence
already covers them.

FakeSpiDevice gains set_dead_after_writes(reg, count) to simulate a
register that has stopped accepting writes.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

### Task P0.3: Polling-only fallback in process_events (point #6)

**Files:**
- Modify: `include/loradriver/lora_config.hpp`
- Modify: `src/chips/sx127x/sx127x_driver.cpp`
- Modify: `tests/host/test_sx127x_irq_queue.cpp`

- [ ] **Step 1: Add `polling_mode` flag**

In `include/loradriver/lora_config.hpp`, after `bool auto_reset = true;`:

```cpp
    bool          polling_mode      = false;  // process_events() reads RegIrqFlags
                                              // every call regardless of ring buffer.
                                              // Use when no DIO0 ISR is attached.
```

- [ ] **Step 2: Write the failing test**

Add to `tests/host/test_sx127x_irq_queue.cpp`:

```cpp
bool TestPollingModeReadsIrqFlagsWithoutInterrupt() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.polling_mode = true;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);

    // Do NOT call handle_interrupt — simulate "no DIO0 wired".
    int rxdone_count = 0;
    drv.set_event_callback([&rxdone_count](RadioEvent ev, int) {
        if (ev == RadioEvent::RxDone) ++rxdone_count;
    });
    spi.set_register(reg::kIrqFlags, irq::kRxDone);
    drv.process_events();
    LD_EXPECT_EQ(rxdone_count, 1);
    return true;
}

bool TestNonPollingModeIgnoresIrqFlagsWithoutInterrupt() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.polling_mode = false;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);

    int rxdone_count = 0;
    drv.set_event_callback([&rxdone_count](RadioEvent ev, int) {
        if (ev == RadioEvent::RxDone) ++rxdone_count;
    });
    spi.set_register(reg::kIrqFlags, irq::kRxDone);
    drv.process_events();  // no handle_interrupt → ring empty → no event
    LD_EXPECT_EQ(rxdone_count, 0);
    return true;
}
```

Register both in `main()`.

- [ ] **Step 3: Run to verify RED**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
```

Expected: compile error — `polling_mode` not used by driver yet.

(After build success but test fail: `TestPollingModeReadsIrqFlagsWithoutInterrupt` returns 0 instead of 1.)

- [ ] **Step 4: Implement polling fallback**

In `src/chips/sx127x/sx127x_driver.cpp`, in `process_events()`, change the IRQ drain loop. Replace this block:

```cpp
    // Drain IRQ queue (max kIrqQueueSize iterations)
    std::uint8_t iters = 0;
    while (irq_tail_ != irq_head_ && iters < kIrqQueueSize) {
```

with:

```cpp
    // Drain IRQ queue (max kIrqQueueSize iterations).
    // In polling_mode, synthesize a queue entry so the loop runs once per
    // process_events() call even without handle_interrupt being invoked.
    if (cfg_.polling_mode && irq_tail_ == irq_head_) {
        // Push a synthetic marker; the read of RegIrqFlags below decides if
        // any real IRQ is pending.
        const std::uint8_t next = static_cast<std::uint8_t>((irq_head_ + 1u) % kIrqQueueSize);
        if (next != irq_tail_) {
            irq_queue_[irq_head_] = 1u;
            irq_head_ = next;
        }
    }

    std::uint8_t iters = 0;
    while (irq_tail_ != irq_head_ && iters < kIrqQueueSize) {
```

Inside the drain loop, after reading `flags`, add an early-out so a synthetic poll with empty flags doesn't emit spurious events. Find:

```cpp
        std::uint8_t flags = 0;
        if (spi_.read_register(reg::kIrqFlags, flags) != LoRaError::OK) continue;

        // Clear flags (write 1 to clear)
        (void)spi_.write_register(reg::kIrqFlags, irq::kClearAll);
        ++stats_.irq_events_processed;
```

and change to:

```cpp
        std::uint8_t flags = 0;
        if (spi_.read_register(reg::kIrqFlags, flags) != LoRaError::OK) continue;
        if (flags == 0u) continue;  // synthetic poll with nothing to do

        // Clear flags (write 1 to clear)
        (void)spi_.write_register(reg::kIrqFlags, irq::kClearAll);
        ++stats_.irq_events_processed;
```

- [ ] **Step 5: Build, test, commit**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure 2>&1 | tail -5
```

Expected: all tests pass.

```bash
git -C D:/DEV/C++/LoRaDriver add include/loradriver/lora_config.hpp src/chips/sx127x/sx127x_driver.cpp tests/host/test_sx127x_irq_queue.cpp
git -C D:/DEV/C++/LoRaDriver commit -m "$(cat <<'EOF'
feat: polling-only fallback when no DIO0 ISR is attached

LoRaConfig::polling_mode=true makes process_events() read RegIrqFlags
every call regardless of whether handle_interrupt was invoked. Lets
boards without DIO0 wiring still receive packets via plain poll() loops.

A flags==0 read is silently dropped (no event, no flag clear) so the
fallback doesn't pollute stats on idle calls.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

### Task P0.4: Runtime chip-alive heartbeat (point #4)

**Files:**
- Modify: `include/loradriver/chips/sx127x_driver.hpp`
- Modify: `src/chips/sx127x/sx127x_driver.cpp`
- Modify: `tests/host/test_sx127x_irq_queue.cpp`

- [ ] **Step 1: Write the failing test**

Add to `tests/host/test_sx127x_irq_queue.cpp`:

```cpp
bool TestHeartbeatDetectsDeadChip() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    // Chip becomes unresponsive: RegVersion reads 0xFF
    spi.set_chip_version(0xFF);
    LD_EXPECT_EQ(drv.check_alive(), LoRaError::UnsupportedChip);
    return true;
}

bool TestHeartbeatPassesOnLiveChip() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(drv.check_alive(), LoRaError::OK);
    return true;
}
```

Register both in `main()`.

- [ ] **Step 2: Run to verify RED**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
```

Expected: `check_alive` not declared.

- [ ] **Step 3: Add `check_alive` to the public API**

In `include/loradriver/chips/sx127x_driver.hpp`, add a public method declaration near `chip_version()`:

```cpp
    /// Re-reads RegVersion and returns OK if it still matches the expected
    /// signature (0x12). Returns UnsupportedChip if the chip has gone away
    /// (brown-out, ESD reset, etc.). Cheap: one SPI byte read.
    [[nodiscard]] LoRaError check_alive() noexcept;
```

- [ ] **Step 4: Implement in cpp**

In `src/chips/sx127x/sx127x_driver.cpp`, add near the bottom (before `}  // namespace loradriver::chips`):

```cpp
LoRaError SX127xDriver::check_alive() noexcept {
    if (!initialized_) return LoRaError::NotInitialized;
    std::uint8_t v = 0;
    const LoRaError e = spi_.read_register(reg::kVersion, v);
    if (e != LoRaError::OK) return e;
    if (v != kVersionExpected) return LoRaError::UnsupportedChip;
    return LoRaError::OK;
}
```

- [ ] **Step 5: Expose via LoRaTransceiver façade**

In `include/loradriver/lora_transceiver.hpp`, add after `chip_version()`:

```cpp
    /// Heartbeat: cheap RegVersion read. Returns OK if chip still responds.
    [[nodiscard]] LoRaError check_alive() noexcept {
        if (state_ == State::Uninit) return LoRaError::NotInitialized;
        return driver_.check_alive();
    }
```

We also need to add `check_alive` to the `IRadioDriver` interface. In `include/loradriver/radio_driver.hpp`, add a pure-virtual declaration:

```cpp
    [[nodiscard]] virtual LoRaError check_alive() noexcept = 0;
```

And the `SX127xDriver` declaration in `sx127x_driver.hpp` should `override` it — change its declaration to:

```cpp
    [[nodiscard]] LoRaError check_alive() noexcept override;
```

- [ ] **Step 6: Build, test, commit**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure 2>&1 | tail -5
```

Expected: all tests pass.

```bash
git -C D:/DEV/C++/LoRaDriver add include/loradriver/chips/sx127x_driver.hpp include/loradriver/radio_driver.hpp include/loradriver/lora_transceiver.hpp src/chips/sx127x/sx127x_driver.cpp tests/host/test_sx127x_irq_queue.cpp
git -C D:/DEV/C++/LoRaDriver commit -m "$(cat <<'EOF'
feat: runtime chip-alive heartbeat via RegVersion

New check_alive() method on IRadioDriver/SX127xDriver/LoRaTransceiver
re-reads RegVersion (0x42) and returns UnsupportedChip if the chip
has gone away. One SPI byte read; ~5us at 8MHz.

Pump tasks / user loops can call this periodically to detect ESD or
brown-out resets and trigger a re-init.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

### Task P0.5: Embedded smoke test — SX1276 hardware loopback 🔌 HARDWARE GATE

**Files:**
- Modify: `tests/embedded/smoke/test_main.cpp`
- Create: `docs/hardware-smoke.md`

- [ ] **Step 1: Extend smoke test with a TX→RX self-loopback check**

Replace the entire content of `tests/embedded/smoke/test_main.cpp`:

```cpp
#include <Arduino.h>
#include <SPI.h>
#include <unity.h>

#include "loradriver/chips/sx127x_driver.hpp"
#include "loradriver/hal/esp32_spi_device.hpp"
#include "loradriver/lora_config.hpp"
#include "loradriver/lora_transceiver.hpp"

using namespace loradriver;

namespace {

constexpr std::int8_t kPinSS    = 5;
constexpr std::int8_t kPinReset = 14;
constexpr std::int8_t kPinDio0  = 26;

hal::Esp32SpiDevice g_spi(SPI, kPinSS);
chips::SX127xDriver g_drv(g_spi);
LoRaTransceiver     g_trx(g_drv);

LoRaConfig make_cfg() {
    LoRaConfig c;
    c.chip = ChipModel::SX1276;
    c.frequency_hz = 868'100'000u;
    c.spreading_factor = 7;
    c.bandwidth_hz = 125'000u;
    c.tx_power_dbm = 14;
    c.pin_ss = kPinSS; c.pin_reset = kPinReset; c.pin_dio0 = kPinDio0;
    return c;
}

void IRAM_ATTR isr_dio0() { g_trx.handle_interrupt(); }

}  // namespace

void setUp() {}
void tearDown() {}

void test_chip_version_is_0x12() {
    SPI.begin();
    TEST_ASSERT_EQUAL(static_cast<int>(LoRaError::OK),
                      static_cast<int>(g_trx.begin(make_cfg())));
    TEST_ASSERT_EQUAL_HEX8(0x12, g_trx.chip_version());
}

void test_check_alive() {
    TEST_ASSERT_EQUAL(static_cast<int>(LoRaError::OK),
                      static_cast<int>(g_trx.check_alive()));
}

void test_tx_blocking_returns_ok() {
    const std::uint8_t payload[] = {'p','i','n','g'};
    TEST_ASSERT_EQUAL(static_cast<int>(LoRaError::OK),
                      static_cast<int>(g_trx.send(payload, 4, 2000)));
}

void test_start_receive_then_self_send_loopback() {
    attachInterrupt(digitalPinToInterrupt(kPinDio0), isr_dio0, RISING);

    volatile bool got_packet = false;
    static std::uint8_t got[16];
    static std::size_t got_len = 0;
    g_trx.on_receive([&](const LoRaPacket&, const std::uint8_t* d, std::size_t n) {
        got_len = (n < sizeof(got)) ? n : sizeof(got);
        memcpy(got, d, got_len);
        got_packet = true;
    });

    TEST_ASSERT_EQUAL(static_cast<int>(LoRaError::OK),
                      static_cast<int>(g_trx.start_receive(true)));

    // Switch to TX briefly, send, then RX should re-arm.
    const std::uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
    TEST_ASSERT_EQUAL(static_cast<int>(LoRaError::OK),
                      static_cast<int>(g_trx.send(payload, 4, 2000)));
    TEST_ASSERT_EQUAL(static_cast<int>(LoRaError::OK),
                      static_cast<int>(g_trx.start_receive(true)));

    // Wait up to 5 seconds for a packet from a paired device. If running
    // solo, this asserts the chip didn't crash but won't actually receive.
    const std::uint32_t t0 = millis();
    while (!got_packet && (millis() - t0) < 5000) {
        g_trx.poll();
        delay(10);
    }
    // Soft assertion: log result rather than failing — solo runs are OK.
    Serial.printf("[smoke] rx_received=%s len=%u\n",
                  got_packet ? "yes" : "no", static_cast<unsigned>(got_len));
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_chip_version_is_0x12);
    RUN_TEST(test_check_alive);
    RUN_TEST(test_tx_blocking_returns_ok);
    RUN_TEST(test_start_receive_then_self_send_loopback);
    UNITY_END();
}

void loop() {}
```

- [ ] **Step 2: Document the hardware procedure**

Content for `docs/hardware-smoke.md`:

```markdown
# Hardware smoke test

How to validate LoRaDriver on a real ESP32 + SX1276 / SX1278 module.

## Wiring (DOIT ESP32 DevKit example)

| ESP32 pin | Module pin |
|-----------|------------|
| GPIO 5    | NSS / CS   |
| GPIO 14   | RST / NRESET |
| GPIO 26   | DIO0       |
| GPIO 18   | SCK        |
| GPIO 19   | MISO       |
| GPIO 23   | MOSI       |
| 3V3       | VCC        |
| GND       | GND        |

If your pinout differs, edit `tests/embedded/smoke/test_main.cpp` constants
`kPinSS`, `kPinReset`, `kPinDio0` before flashing.

## Running

```bash
pio test -d D:/DEV/C++/LoRaDriver -e smoke --upload-port COMx --test-port COMx
```

(Replace `COMx` with your port; on Linux/macOS, `/dev/ttyUSB0` etc.)

Unity will run 4 tests and print PASS/FAIL per test.

## Expected results

- `test_chip_version_is_0x12` — PASS unconditionally (validates SPI bus + reset GPIO).
- `test_check_alive` — PASS (validates the heartbeat).
- `test_tx_blocking_returns_ok` — PASS as long as the chip didn't crash during TX.
- `test_start_receive_then_self_send_loopback` — PASS on the firmware side; whether a packet is actually received depends on whether a second device is paired and transmitting.

If `chip_version_is_0x12` fails:
1. Check wiring with a multimeter — CS, RST should be 3.3V idle.
2. Probe SCK with an oscilloscope during boot — should see 8 MHz bursts.
3. Try a slower SPI clock: `cfg.spi_frequency_hz = 1'000'000` in the test.

## Variant: SX1278

To validate the SX1278 path, change the test config:

```cpp
c.chip = ChipModel::SX1278;
c.frequency_hz = 433'920'000u;
```

and re-flash. The driver applies the same init sequence; SX1278 is
distinguished by `LoRaConfig::validate()` (rejects 868 MHz) and by the
RSSI offset (-157 dBm low-band vs -164 high-band).
```

- [ ] **Step 3: Verify the test compiles via PlatformIO**

```bash
"$HOME/.platformio/penv/Scripts/pio.exe" test -d D:/DEV/C++/LoRaDriver -e smoke --without-uploading --without-testing 2>&1 | tail -10
```

Expected: compilation succeeds. (`--without-uploading --without-testing` builds only.)

- [ ] **Step 4: 🔌 HARDWARE GATE — flash and run on real ESP32**

Run the user-facing command:

```bash
"$HOME/.platformio/penv/Scripts/pio.exe" test -d D:/DEV/C++/LoRaDriver -e smoke --upload-port COM3 --test-port COM3
```

Wait for the user to confirm PASS/FAIL output. Do not proceed to P0.6 until user confirms.

- [ ] **Step 5: Commit (whether HW is available or not)**

```bash
git -C D:/DEV/C++/LoRaDriver add tests/embedded/smoke/test_main.cpp docs/hardware-smoke.md
git -C D:/DEV/C++/LoRaDriver commit -m "$(cat <<'EOF'
test: extend embedded smoke with check_alive + tx + rx loopback

Adds three Unity tests on top of the existing init check:
  - test_check_alive: heartbeat read RegVersion
  - test_tx_blocking_returns_ok: full TX cycle on real chip
  - test_start_receive_then_self_send_loopback: RX/TX/RX with optional
    paired-device packet capture

Hardware procedure documented in docs/hardware-smoke.md including
wiring table, expected pass criteria, and SX1278 variant.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

### Task P0.6: SX1278 host-test coverage (point #1)

The hardware run will exercise SX1276; the host side already accepts SX1278 but
no test exercises the SX1278 code path through `apply_init_sequence` with the
low-band frequency. Add explicit coverage.

**Files:**
- Modify: `tests/host/test_sx127x_init_sequence.cpp`

- [ ] **Step 1: Write the failing test**

Add to `tests/host/test_sx127x_init_sequence.cpp`:

```cpp
bool TestBeginSx1278At433MHzWritesCorrectFrf() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.chip = ChipModel::SX1278;
    c.frequency_hz = 433'920'000u;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);

    const std::uint64_t frf = (static_cast<std::uint64_t>(433'920'000u) << 19) / 32'000'000ull;
    LD_EXPECT_EQ(spi.reg(reg::kFrMsb), static_cast<std::uint8_t>((frf >> 16) & 0xFF));
    LD_EXPECT_EQ(spi.reg(reg::kFrMid), static_cast<std::uint8_t>((frf >> 8) & 0xFF));
    LD_EXPECT_EQ(spi.reg(reg::kFrLsb), static_cast<std::uint8_t>(frf & 0xFF));
    return true;
}

bool TestBeginSx1278RejectsHighBandFrequency() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.chip = ChipModel::SX1278;
    c.frequency_hz = 868'000'000u;  // illegal for SX1278
    LD_EXPECT_EQ(drv.begin(c), LoRaError::InvalidConfig);
    return true;
}
```

Register both in `main()`.

- [ ] **Step 2: Build + test**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure 2>&1 | tail -5
```

Expected: both new tests pass (validate() already enforces band/chip coupling
from v1.0; init sequence covers FRF formula for any frequency).

- [ ] **Step 3: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add tests/host/test_sx127x_init_sequence.cpp
git -C D:/DEV/C++/LoRaDriver commit -m "test: cover SX1278 init path at 433 MHz + reject 868 MHz

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task P0.7: SX1278 hardware smoke 🔌 HARDWARE GATE

- [ ] **Step 1: Flash the smoke test with SX1278 module**

Edit `tests/embedded/smoke/test_main.cpp` `make_cfg()`:

```cpp
    c.chip = ChipModel::SX1278;
    c.frequency_hz = 433'920'000u;
```

Then:

```bash
"$HOME/.platformio/penv/Scripts/pio.exe" test -d D:/DEV/C++/LoRaDriver -e smoke --upload-port COM3 --test-port COM3
```

Expected: 4 Unity tests pass. **Do not commit this edit** — it's a temporary local change.

- [ ] **Step 2: Revert the temporary edit**

```bash
git -C D:/DEV/C++/LoRaDriver checkout tests/embedded/smoke/test_main.cpp
```

---

## Phase P1a — Runtime watchdogs and verify (6 tasks)

Addresses P1 items #7 (RX-silence watchdog), #8 (FIFO split TX/RX), #11
(symbol_timeout), #12 (RX image calibration), #15 (clean end()), #18
(pump_pending atomicity).

### Task P1a.1: RX-silence watchdog (point #7)

**Files:**
- Modify: `include/loradriver/lora_config.hpp`
- Modify: `include/loradriver/chips/sx127x_driver.hpp`
- Modify: `src/chips/sx127x/sx127x_driver.cpp`
- Modify: `tests/host/test_sx127x_irq_queue.cpp`

- [ ] **Step 1: Add config knob**

In `include/loradriver/lora_config.hpp`, after `polling_mode`:

```cpp
    std::uint32_t rx_silence_timeout_ms = 0;  // 0 = disabled. >0 = emit
                                              // RxTimeout if no RxDone seen
                                              // in this window during
                                              // RX_CONTINUOUS.
```

- [ ] **Step 2: Write the failing test**

Add to `tests/host/test_sx127x_irq_queue.cpp`:

```cpp
bool TestRxSilenceWatchdogFiresWhenIdle() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.rx_silence_timeout_ms = 0;  // first verify disabled mode
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    LD_EXPECT_EQ(drv.start_receive(true), LoRaError::OK);
    int rx_timeouts = 0;
    drv.set_event_callback([&rx_timeouts](RadioEvent ev, int) {
        if (ev == RadioEvent::RxTimeout) ++rx_timeouts;
    });
    // 100 process_events calls with no IRQ → no timeout because feature disabled
    for (int i = 0; i < 100; ++i) drv.process_events();
    LD_EXPECT_EQ(rx_timeouts, 0);
    return true;
}

bool TestRxSilenceWatchdogTriggersAfterTimeout() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.rx_silence_timeout_ms = 1;  // 1 ms
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    LD_EXPECT_EQ(drv.start_receive(true), LoRaError::OK);
    int rx_timeouts = 0;
    drv.set_event_callback([&rx_timeouts](RadioEvent ev, int) {
        if (ev == RadioEvent::RxTimeout) ++rx_timeouts;
    });
    // Sleep at least 2 ms to exceed the deadline, then poll.
    #ifdef _WIN32
        Sleep(5);
    #else
        usleep(5000);
    #endif
    drv.process_events();
    LD_EXPECT(rx_timeouts >= 1);
    return true;
}
```

Add the platform sleep includes at the top of the test file:

```cpp
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
```

Register both in `main()`.

- [ ] **Step 3: Build to verify RED**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -5
```

Expected: build succeeds, `TestRxSilenceWatchdogTriggersAfterTimeout` fails
because no watchdog is wired up.

- [ ] **Step 4: Add the watchdog state to SX127xDriver**

In `include/loradriver/chips/sx127x_driver.hpp` private section, add:

```cpp
    std::uint32_t rx_silence_deadline_ms_ = 0;  // 0 = disarmed
```

- [ ] **Step 5: Arm the watchdog when entering RX_CONTINUOUS**

In `src/chips/sx127x/sx127x_driver.cpp`, modify `start_receive` — replace
the existing function body with:

```cpp
LoRaError SX127xDriver::start_receive(bool continuous) noexcept {
    if (!initialized_) return LoRaError::NotInitialized;
    LoRaError e;
    if ((e = set_op_mode(opmode::kLoRaStandby)) != LoRaError::OK) return e;
    if ((e = spi_.write_register(reg::kFifoRxBaseAddr, 0)) != LoRaError::OK) return e;
    if ((e = spi_.write_register(reg::kFifoAddrPtr, 0)) != LoRaError::OK) return e;
    if ((e = spi_.write_register(reg::kDioMapping1, dio::kDio0RxDone)) != LoRaError::OK) return e;
    e = set_op_mode(continuous ? opmode::kLoRaRxCont : opmode::kLoRaRxSingle);
    if (e == LoRaError::OK && continuous && cfg_.rx_silence_timeout_ms > 0) {
        rx_silence_deadline_ms_ = now_ms() + cfg_.rx_silence_timeout_ms;
    } else {
        rx_silence_deadline_ms_ = 0;
    }
    return e;
}
```

- [ ] **Step 6: Check the watchdog in process_events**

In `process_events()`, add at the top — just before the existing TX watchdog
block — a sibling check:

```cpp
    // RX silence watchdog
    if (rx_silence_deadline_ms_ != 0u && now_ms() >= rx_silence_deadline_ms_) {
        rx_silence_deadline_ms_ = 0u;  // disarm to avoid storm
        ++stats_.rx_timeout;
        emit(RadioEvent::RxTimeout, 0);
        // Force back to standby and re-arm RX so caller can decide
        (void)set_op_mode(opmode::kLoRaStandby);
    }
```

Also reset the deadline when a real RxDone arrives. In the RxDone case
of the drain loop (just after `++stats_.rx_done;`), add:

```cpp
            if (cfg_.rx_silence_timeout_ms > 0u) {
                rx_silence_deadline_ms_ = now_ms() + cfg_.rx_silence_timeout_ms;
            }
```

- [ ] **Step 7: Build, test, commit**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure 2>&1 | tail -5
```

Expected: all tests pass.

```bash
git -C D:/DEV/C++/LoRaDriver add include/loradriver/lora_config.hpp include/loradriver/chips/sx127x_driver.hpp src/chips/sx127x/sx127x_driver.cpp tests/host/test_sx127x_irq_queue.cpp
git -C D:/DEV/C++/LoRaDriver commit -m "$(cat <<'EOF'
feat: RX silence watchdog with configurable timeout

LoRaConfig::rx_silence_timeout_ms (0=disabled) arms a deadline when
start_receive(true) succeeds. If process_events() runs past the
deadline with no RxDone in between, emits RadioEvent::RxTimeout and
forces standby so the caller can react (auto-reset, mode change,
re-arm RX, etc.). Deadline is refreshed on every successful RxDone.

Catches the "DIO0 stuck low" / "chip wedged" class of failures that
the existing per-mode watchdog can't see.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

### Task P1a.2: FIFO base addresses split TX/RX (point #8)

**Files:**
- Modify: `src/chips/sx127x/sx127x_driver.cpp`
- Modify: `tests/host/test_sx127x_init_sequence.cpp`

- [ ] **Step 1: Write the failing test**

Add to `tests/host/test_sx127x_init_sequence.cpp`:

```cpp
bool TestInitSetsTxBaseTo0AndRxBaseTo128() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kFifoTxBaseAddr), std::uint8_t{0});
    LD_EXPECT_EQ(spi.reg(reg::kFifoRxBaseAddr), std::uint8_t{128});
    return true;
}
```

Register in `main()`.

- [ ] **Step 2: Run to verify RED**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure -R init_sequence 2>&1 | tail -5
```

Expected: fails — both are 0.

- [ ] **Step 3: Split the FIFO**

In `src/chips/sx127x/sx127x_driver.cpp`, find the init-sequence block:

```cpp
    // FIFO base addresses (split FIFO: TX=0, RX=0 — overwrite-safe via FifoAddrPtr)
    if ((e = spi_.write_register(reg::kFifoTxBaseAddr, 0)) != LoRaError::OK) return e;
    if ((e = spi_.write_register(reg::kFifoRxBaseAddr, 0)) != LoRaError::OK) return e;
```

Replace with:

```cpp
    // FIFO base addresses: split halves so concurrent TX prep and RX
    // cannot stomp each other. TX writes from 0, RX receives into 128.
    if ((e = spi_.write_register(reg::kFifoTxBaseAddr, 0)) != LoRaError::OK) return e;
    if ((e = spi_.write_register(reg::kFifoRxBaseAddr, 128)) != LoRaError::OK) return e;
```

In `start_transmit`, the existing line:

```cpp
    if ((e = spi_.write_register(reg::kFifoAddrPtr, 0)) != LoRaError::OK) return e;
```

is correct (TX base = 0 → ptr = 0).

In `start_receive`, find:

```cpp
    if ((e = spi_.write_register(reg::kFifoAddrPtr, 0)) != LoRaError::OK) return e;
```

Replace with:

```cpp
    if ((e = spi_.write_register(reg::kFifoAddrPtr, 128)) != LoRaError::OK) return e;
```

- [ ] **Step 4: Update existing RX test that expects FifoAddrPtr=0**

In `tests/host/test_sx127x_rx_path.cpp` — the test currently doesn't assert
on FifoAddrPtr (it asserts `FifoRxBaseAddr` is 0). Update:

Find `TestStartReceiveContinuous`:

```cpp
    LD_EXPECT_EQ(spi.reg(reg::kFifoRxBaseAddr), std::uint8_t{0});
```

Replace with:

```cpp
    LD_EXPECT_EQ(spi.reg(reg::kFifoRxBaseAddr), std::uint8_t{128});
```

- [ ] **Step 5: Update read_packet snapshot tests**

In `tests/host/test_sx127x_rx_path.cpp`, both `TestReadPacketCopiesFromFifo`
and `TestReadPacketClampsToMaxLen` set `kFifoRxCurrentAddr` to 0 and put
data at `kFifo + i`. These tests still work because the driver reads
`FifoRxCurrentAddr` directly — but the layout the test simulates is now
unrealistic (real chip would put RX data at offset 128). Tests don't need
to change: they validate the FIFO copy mechanism with an arbitrary base.

- [ ] **Step 6: Build, test, commit**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure 2>&1 | tail -5
```

Expected: all tests pass.

```bash
git -C D:/DEV/C++/LoRaDriver add src/chips/sx127x/sx127x_driver.cpp tests/host/test_sx127x_init_sequence.cpp tests/host/test_sx127x_rx_path.cpp
git -C D:/DEV/C++/LoRaDriver commit -m "$(cat <<'EOF'
feat: split FIFO base addresses TX=0 RX=128

Previously both TX and RX bases were 0, so a packet arriving while a
TX buffer was still being prepared could overwrite it (rare in pure
P2P, possible in pump-task mode where async TX races against RX).

Now TX writes from address 0 and RX receives into 128 — 128 bytes each
side of the 256-byte FIFO. Caller-visible max payload remains 255 bytes
because start_transmit writes from FifoAddrPtr=0 with no offset wrap
within a single packet.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

### Task P1a.3: Symbol-timeout computed from SF/BW (point #11)

**Files:**
- Modify: `src/chips/sx127x/sx127x_driver.cpp`
- Modify: `tests/host/test_sx127x_init_sequence.cpp`

- [ ] **Step 1: Write the failing test**

Add to `tests/host/test_sx127x_init_sequence.cpp`:

```cpp
bool TestSymbolTimeoutUsesConfigValueDirectly() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.symbol_timeout = 0x140;  // 320, fits in 10 bits
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    // SymbTimeoutLsb stores low 8 bits.
    LD_EXPECT_EQ(spi.reg(reg::kSymbTimeoutLsb), std::uint8_t{0x40});
    // ModemConfig2 low 2 bits store the MSB.
    LD_EXPECT_EQ(static_cast<std::uint8_t>(spi.reg(reg::kModemConfig2) & 0x03u),
                 std::uint8_t{0x01});
    return true;
}
```

Register in `main()`.

- [ ] **Step 2: Run to verify RED**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure -R init_sequence 2>&1 | tail -10
```

Expected: fails — current code forces MSB to `0b11`.

- [ ] **Step 3: Compute SymbTimeoutMsb correctly**

In `src/chips/sx127x/sx127x_driver.cpp`, replace `apply_modem_config`'s
ModemConfig2 line and the SymbTimeoutLsb line:

```cpp
    // ModemConfig2: SF[7:4] | TxContinuous[3]=0 | CRC[2] | SymbTimeoutMsb[1:0]
    const std::uint16_t symb_to = (cfg.symbol_timeout > 0x3FFu)
        ? std::uint16_t{0x3FFu}
        : cfg.symbol_timeout;
    const std::uint8_t mc2 = static_cast<std::uint8_t>(
        (cfg.spreading_factor << 4) |
        (cfg.crc_enabled ? 0x04u : 0x00u) |
        ((symb_to >> 8) & 0x03u));
    if ((e = spi_.write_register(reg::kModemConfig2, mc2)) != LoRaError::OK) return e;

    // SymbTimeoutLsb
    if ((e = spi_.write_register(reg::kSymbTimeoutLsb,
                                 static_cast<std::uint8_t>(symb_to & 0xFFu))) != LoRaError::OK) return e;
```

- [ ] **Step 4: Update existing init test that asserted MSB=0b11**

In `tests/host/test_sx127x_init_sequence.cpp`, no other test asserts
`kSymbTimeoutLsb` or the low 2 bits of `kModemConfig2`. Search and confirm:

```bash
grep -n "SymbTimeout\|& 0x03" D:/DEV/C++/LoRaDriver/tests/host/test_sx127x_init_sequence.cpp
```

Expected: only the new test references these.

- [ ] **Step 5: Build, test, commit**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure 2>&1 | tail -5
```

Expected: all pass.

```bash
git -C D:/DEV/C++/LoRaDriver add src/chips/sx127x/sx127x_driver.cpp tests/host/test_sx127x_init_sequence.cpp
git -C D:/DEV/C++/LoRaDriver commit -m "feat: write 10-bit SymbTimeout from LoRaConfig::symbol_timeout

Previously forced the MSB to 0b11 (max). Now derives MSB/LSB from
cfg.symbol_timeout (clamped to 0x3FF), letting RxSingle timeouts
shorter than ~1024 symbols actually take effect.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task P1a.4: RX image calibration on init (point #12)

**Files:**
- Modify: `src/chips/sx127x/sx127x_registers.hpp`
- Modify: `src/chips/sx127x/sx127x_driver.cpp`
- Modify: `tests/host/test_sx127x_init_sequence.cpp`

- [ ] **Step 1: Add RegImageCal address constant**

In `src/chips/sx127x/sx127x_registers.hpp`, add to the `reg` namespace:

```cpp
constexpr std::uint8_t kImageCal           = 0x3B;
```

Note: 0x3B already exists in v1.0 as `kInvertIq2`. SX1276 datasheet maps
0x3B to RegImageCal in **FSK mode** and RegInvertIq2 in **LoRa mode**.
In LoRa mode the calibration register is at 0x3B in **FSK access**. We
must temporarily switch to FSK, calibrate, then return to LoRa.

Replace the existing `constexpr std::uint8_t kInvertIq2 = 0x3B;` with
the same constant under a different name **and** keep the InvertIq2 name
as an alias for clarity:

```cpp
constexpr std::uint8_t kImageCal           = 0x3B;  // FSK mode access
constexpr std::uint8_t kInvertIq2          = kImageCal;  // LoRa mode access alias
```

- [ ] **Step 2: Write the failing test**

Add to `tests/host/test_sx127x_init_sequence.cpp`:

```cpp
bool TestBeginCalibratesRxImage() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    // Calibration writes RegImageCal with ImageCalStart bit (0x40) set.
    bool saw_cal = false;
    for (const auto& w : spi.writes()) {
        if (w.reg == reg::kImageCal && (w.value & 0x40u) != 0u) saw_cal = true;
    }
    LD_EXPECT(saw_cal);
    return true;
}
```

Register in `main()`.

- [ ] **Step 3: Run to verify RED**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure -R init_sequence 2>&1 | tail -5
```

Expected: fails — no write to kImageCal yet.

- [ ] **Step 4: Implement calibration step**

In `src/chips/sx127x/sx127x_driver.cpp`, add a helper just below the `bw_code`
function:

```cpp
LoRaError SX127xDriver::run_rx_image_calibration() noexcept {
    // Datasheet §4.2.3.8: image calibration must be done in FSK mode.
    // We're called between FSK sleep and LoRa sleep in apply_init_sequence,
    // so the chip is already in FSK access mode.
    LoRaError e;
    std::uint8_t v = 0;
    if ((e = spi_.read_register(reg::kImageCal, v)) != LoRaError::OK) return e;
    v |= 0x40u;  // ImageCalStart bit
    if ((e = spi_.write_register(reg::kImageCal, v)) != LoRaError::OK) return e;

    // Wait for ImageCalRunning to clear (bit 5). Bounded poll, ~1 ms.
    for (int i = 0; i < 100; ++i) {
        if (spi_.read_register(reg::kImageCal, v) != LoRaError::OK) return LoRaError::SpiFailure;
        if ((v & 0x20u) == 0u) return LoRaError::OK;
#ifdef ARDUINO
        delayMicroseconds(10);
#endif
    }
    return LoRaError::OK;  // best-effort; chip may have completed silently
}
```

Add the declaration in `include/loradriver/chips/sx127x_driver.hpp` private:

```cpp
    [[nodiscard]] LoRaError run_rx_image_calibration() noexcept;
```

Then in `apply_init_sequence`, **between** the `kFskSleep` write and the
`kLoRaSleep` write, insert:

```cpp
    // Image calibration must run in FSK mode (datasheet §4.2.3.8).
    if ((e = run_rx_image_calibration()) != LoRaError::OK) return e;
```

So that section becomes:

```cpp
    // FSK sleep → image calibration → LoRa sleep
    if ((e = set_op_mode(opmode::kFskSleep)) != LoRaError::OK) return e;
    if ((e = run_rx_image_calibration()) != LoRaError::OK) return e;
    if ((e = set_op_mode(opmode::kLoRaSleep)) != LoRaError::OK) return e;
```

- [ ] **Step 5: Build, test, commit**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure 2>&1 | tail -5
```

Expected: all tests pass.

```bash
git -C D:/DEV/C++/LoRaDriver add src/chips/sx127x/sx127x_registers.hpp include/loradriver/chips/sx127x_driver.hpp src/chips/sx127x/sx127x_driver.cpp tests/host/test_sx127x_init_sequence.cpp
git -C D:/DEV/C++/LoRaDriver commit -m "$(cat <<'EOF'
feat: RX image calibration during init

apply_init_sequence now runs ImageCalStart in FSK sleep before
switching to LoRa, as required by datasheet §4.2.3.8. Recovers a
few dB of RX sensitivity that the cold-boot defaults miss.

Bounded poll on ImageCalRunning bit (≤1 ms). Best-effort exit if the
chip doesn't clear the bit — calibration may have completed silently.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

### Task P1a.5: Clean shutdown — LoRaTransceiver::end() detaches resources (point #15)

**Files:**
- Modify: `include/loradriver/lora_transceiver.hpp`
- Modify: `src/api/lora_transceiver.cpp`
- Modify: `tests/host/test_transceiver_fsm.cpp`

- [ ] **Step 1: Write the failing test**

Add to `tests/host/test_transceiver_fsm.cpp`:

```cpp
bool TestEndClearsCallbacks() {
    FakeSpiDevice spi; SX127xDriver drv(spi); LoRaTransceiver trx(drv);
    LD_EXPECT_EQ(trx.begin(MakeCfg()), LoRaError::OK);
    int rx_callback_calls = 0;
    trx.on_receive([&rx_callback_calls](const LoRaPacket&, const std::uint8_t*, std::size_t) {
        ++rx_callback_calls;
    });
    trx.end();
    // Try to invoke the callback by simulating a driver event — should be a no-op
    // because end() cleared the callback storage.
    spi.set_register(reg::kIrqFlags, irq::kRxDone);
    spi.set_register(reg::kRxNbBytes, 4);
    drv.handle_interrupt();
    drv.process_events();   // direct on driver, not via transceiver
    LD_EXPECT_EQ(rx_callback_calls, 0);
    return true;
}
```

Register in `main()`.

- [ ] **Step 2: Run to verify RED or GREEN**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure -R transceiver_fsm 2>&1 | tail -5
```

This test might already pass because the driver's event_cb_ is set during
begin() and after end() the driver is also reset. The exact behaviour depends
on whether `IRadioDriver::end()` clears the callback. Currently it doesn't.

- [ ] **Step 3: Make end() truly clean up**

In `src/api/lora_transceiver.cpp`, replace `LoRaTransceiver::end()`:

```cpp
void LoRaTransceiver::end() noexcept {
    if (state_ == State::Uninit) return;
    // Detach driver callback first so any in-flight IRQ can't reach
    // user code through a dead lambda capture.
    driver_.set_event_callback(nullptr);
    driver_.end();
    packet_cb_  = {};
    event_cb_   = {};
    tx_done_cb_ = {};
    state_ = State::Uninit;
}
```

- [ ] **Step 4: Build, test, commit**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure 2>&1 | tail -5
```

Expected: all pass.

```bash
git -C D:/DEV/C++/LoRaDriver add src/api/lora_transceiver.cpp tests/host/test_transceiver_fsm.cpp
git -C D:/DEV/C++/LoRaDriver commit -m "$(cat <<'EOF'
feat: LoRaTransceiver::end() detaches driver callback + clears user CBs

Previously end() set the chip to sleep but left the user-provided
packet/event/tx_done callbacks live, so an IRQ arriving during
teardown could still call into a moribund LoRaTransceiver.

Now end() first nulls the driver event callback, then clears all
user callback std::function storage, then transitions the FSM to
Uninit. Caller-side dangling-ISR cleanup (attachInterrupt/
detachInterrupt) remains the caller's responsibility — documented
in docs/api.md (Phase P2).

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

### Task P1a.6: RadioPumpTask — cooperative stop + tx_pending atomicity (points #17, #18)

**Files:**
- Modify: `include/loradriver/platform/esp32/radio_pump_task.hpp`

(No host tests — pump task is ESP32-only. Hardware-gated.)

- [ ] **Step 1: Switch from immediate vTaskDelete to cooperative stop**

In `include/loradriver/platform/esp32/radio_pump_task.hpp`, replace the `stop()`
method:

```cpp
    void stop() {
        const TaskHandle_t t = task_;
        if (t == nullptr) return;
        // Signal the task to exit; it will vTaskDelete(nullptr) itself.
        stop_requested_ = true;
        // Nudge the task in case it's blocked in ulTaskNotifyTake.
        BaseType_t woken = pdFALSE;
        vTaskNotifyGiveFromISR(t, &woken);
        // Wait up to ~600 ms (500 ms send timeout + 100 ms slack).
        for (int i = 0; i < 60 && task_ != nullptr; ++i) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        if (task_ != nullptr) {
            // Task didn't honour the request — last-resort kill.
            vTaskDelete(task_);
            task_ = nullptr;
        }
        stop_requested_ = false;
        tx_pending_ = false;
    }
```

Add `volatile bool stop_requested_ = false;` to the private members
alongside `tx_pending_`.

Modify the `task_entry` while loop to honour `stop_requested_`:

```cpp
        while (self->task_ != nullptr && !self->stop_requested_) {
```

At the very end of the function (after the `while` body), before
`vTaskDelete(nullptr)`, set `self->task_ = nullptr;` so `stop()` can see it:

```cpp
        }
        self->task_ = nullptr;
        vTaskDelete(nullptr);
```

- [ ] **Step 2: Protect tx_pending_ with the existing portMUX_TYPE**

In `enqueue_packet`, `task_entry`, the `tx_pending_` accesses already go
through `portENTER_CRITICAL` for metrics. The `tx_pending_` boolean itself
is read/written without the mux. Since `bool` on ESP32 is naturally atomic
(8-bit aligned, single-instruction stores), the existing code is technically
safe but the convention should be explicit.

Add `volatile` and document the invariant: in the existing struct, replace
`volatile bool         tx_pending_ = false;` (already volatile in v1.0) —
no change needed; just add a doc comment above the member:

```cpp
    // Written only by the pump task; read by stop(). volatile bool is
    // atomic on ESP32 (8-bit aligned single-instruction store).
    volatile bool         tx_pending_ = false;
```

- [ ] **Step 3: Commit (no host test possible — validated by smoke + consumer)**

```bash
git -C D:/DEV/C++/LoRaDriver add include/loradriver/platform/esp32/radio_pump_task.hpp
git -C D:/DEV/C++/LoRaDriver commit -m "$(cat <<'EOF'
fix: RadioPumpTask::stop() now cooperative, not immediate

stop() previously called vTaskDelete(task_) from the main task,
killing the pump mid-trx_->send() if a TX was in flight. That left
the SPI bus and chip in an undefined state.

Now stop() sets stop_requested_, nudges the task via notify, waits
up to ~600 ms for the task to drain the current cycle and exit
cleanly (vTaskDelete(nullptr) on itself). Last-resort kill remains
in case the task is wedged.

Documents tx_pending_'s atomicity (ESP32 volatile bool = single
instruction; safe across the main↔pump boundary).

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

---

## Phase P1b — FSM and API hardening (5 tasks)

Addresses P1 items #9 (CAD auto-rx), #10 (invert_iq), #13 (TCXO), #14
(setLnaGain runtime), #16 (setOcpDisabled).

### Task P1b.1: CAD auto-RX (point #9)

**Files:**
- Modify: `include/loradriver/chips/sx127x_driver.hpp`
- Modify: `src/chips/sx127x/sx127x_driver.cpp`
- Modify: `include/loradriver/lora_transceiver.hpp`
- Modify: `src/api/lora_transceiver.cpp`
- Modify: `tests/host/test_transceiver_fsm.cpp`

- [ ] **Step 1: Add `start_cad(auto_rx=false)` overload**

In `include/loradriver/radio_driver.hpp`, change the `start_cad` signature:

```cpp
    [[nodiscard]] virtual LoRaError start_cad(bool auto_rx = false) noexcept = 0;
```

In `include/loradriver/chips/sx127x_driver.hpp`, change to:

```cpp
    [[nodiscard]] LoRaError start_cad(bool auto_rx = false) noexcept override;
```

Add a private member:

```cpp
    bool cad_auto_rx_ = false;
```

- [ ] **Step 2: Implement auto-rx in CAD path**

In `src/chips/sx127x/sx127x_driver.cpp`, replace `start_cad`:

```cpp
LoRaError SX127xDriver::start_cad(bool auto_rx) noexcept {
    if (!initialized_) return LoRaError::NotInitialized;
    cad_auto_rx_ = auto_rx;
    LoRaError e = spi_.write_register(reg::kDioMapping1, dio::kDio0CadDone);
    if (e != LoRaError::OK) return e;
    return set_op_mode(opmode::kLoRaCad);
}
```

In `process_events`, in the `kCadDone` branch, replace:

```cpp
        if (flags & irq::kCadDone) {
            const int detected = (flags & irq::kCadDetected) ? 1 : 0;
            emit(RadioEvent::CadDone, detected);
        }
```

with:

```cpp
        if (flags & irq::kCadDone) {
            const bool detected = (flags & irq::kCadDetected) != 0u;
            emit(RadioEvent::CadDone, detected ? 1 : 0);
            if (cad_auto_rx_ && detected) {
                (void)start_receive(true);
            }
            cad_auto_rx_ = false;
        }
```

- [ ] **Step 3: Pass through in transceiver**

In `include/loradriver/lora_transceiver.hpp`, change:

```cpp
    [[nodiscard]] LoRaError start_cad(bool auto_rx = false) noexcept;
```

In `src/api/lora_transceiver.cpp`, replace `start_cad`:

```cpp
LoRaError LoRaTransceiver::start_cad(bool auto_rx) noexcept {
    if (state_ == State::Uninit) return LoRaError::NotInitialized;
    const LoRaError e = driver_.start_cad(auto_rx);
    if (e == LoRaError::OK) state_ = State::Cad;
    return e;
}
```

- [ ] **Step 4: Write test**

Add to `tests/host/test_transceiver_fsm.cpp`:

```cpp
bool TestCadAutoRxStartsReceiveOnDetection() {
    FakeSpiDevice spi; SX127xDriver drv(spi); LoRaTransceiver trx(drv);
    LD_EXPECT_EQ(trx.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(trx.start_cad(/*auto_rx=*/true), LoRaError::OK);
    LD_EXPECT(trx.state() == LoRaTransceiver::State::Cad);

    spi.set_register(reg::kIrqFlags, irq::kCadDone | irq::kCadDetected);
    drv.handle_interrupt();
    trx.poll();
    // After CAD done with detected, driver should have auto-entered RxCont.
    // The transceiver façade was in Cad; the driver moved the chip but the
    // façade state didn't update because RadioEvent::RxDone hasn't arrived.
    // Verify OpMode register changed to RxCont:
    LD_EXPECT(spi.reg(reg::kOpMode) == opmode::kLoRaRxCont ||
              spi.reg(reg::kOpMode) == opmode::kLoRaStandby);
    return true;
}
```

Register in `main()`.

- [ ] **Step 5: Build, test, commit**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure 2>&1 | tail -5
```

Expected: all pass.

```bash
git -C D:/DEV/C++/LoRaDriver add include/loradriver/radio_driver.hpp include/loradriver/chips/sx127x_driver.hpp src/chips/sx127x/sx127x_driver.cpp include/loradriver/lora_transceiver.hpp src/api/lora_transceiver.cpp tests/host/test_transceiver_fsm.cpp
git -C D:/DEV/C++/LoRaDriver commit -m "feat: CAD auto-RX — start_cad(auto_rx=true) enters RX on detection

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task P1b.2: Wire LoRaConfig::invert_iq (point #10)

**Files:**
- Modify: `src/chips/sx127x/sx127x_driver.cpp`
- Modify: `tests/host/test_sx127x_init_sequence.cpp`

- [ ] **Step 1: Write the failing test**

Add to `tests/host/test_sx127x_init_sequence.cpp`:

```cpp
bool TestInvertIqWritesBothRegisters() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.invert_iq = true;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    // RegInvertIq (0x33) bit 6 set + bits[0:5] = 0x27 mandate from datasheet
    LD_EXPECT_EQ(static_cast<std::uint8_t>(spi.reg(reg::kInvertIq) & 0x40u),
                 std::uint8_t{0x40});
    // RegInvertIq2 (0x3B) datasheet value 0x19 when inverted
    LD_EXPECT_EQ(spi.reg(reg::kInvertIq2), std::uint8_t{0x19});
    return true;
}

bool TestInvertIqDisabledKeepsDefaults() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.invert_iq = false;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    // Non-inverted: RegInvertIq bit 6 cleared, bits[0:5] = 0x27 default
    LD_EXPECT_EQ(spi.reg(reg::kInvertIq) & 0x40u, std::uint8_t{0});
    return true;
}
```

Register both in `main()`.

- [ ] **Step 2: Run to verify RED**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure -R init_sequence 2>&1 | tail -5
```

Expected: fails.

- [ ] **Step 3: Apply invert_iq in init**

In `src/chips/sx127x/sx127x_driver.cpp`, modify `apply_init_sequence`.
Between the LNA write and the errata call, insert:

```cpp
    // Invert IQ (datasheet table 23, RegInvertIq + RegInvertIq2)
    // Standard: 0x27, Inverted: 0x67 (bit 6 set) + RegInvertIq2 = 0x19
    const std::uint8_t inv_iq  = cfg.invert_iq ? std::uint8_t{0x67} : std::uint8_t{0x27};
    const std::uint8_t inv_iq2 = cfg.invert_iq ? std::uint8_t{0x19} : std::uint8_t{0x1D};
    if ((e = spi_.write_register(reg::kInvertIq,  inv_iq))  != LoRaError::OK) return e;
    if ((e = spi_.write_register(reg::kInvertIq2, inv_iq2)) != LoRaError::OK) return e;
```

Caveat: `reg::kInvertIq2` and `reg::kImageCal` are the same address. In LoRa
mode this register acts as InvertIq2; in FSK mode (during calibration only)
it acts as ImageCal. Since invert_iq is written after calibration (we're in
LoRa standby by now), this is correct.

- [ ] **Step 4: Build, test, commit**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure 2>&1 | tail -5
```

Expected: all pass.

```bash
git -C D:/DEV/C++/LoRaDriver add src/chips/sx127x/sx127x_driver.cpp tests/host/test_sx127x_init_sequence.cpp
git -C D:/DEV/C++/LoRaDriver commit -m "feat: wire LoRaConfig::invert_iq into init sequence

Writes RegInvertIq (0x33) and RegInvertIq2 (0x3B) per datasheet
table 23. Standard side gets 0x27 / 0x1D; inverted (gateway downlink,
asymmetric uplink/downlink topologies) gets 0x67 / 0x19.

Previously invert_iq was a silent no-op in the config struct.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task P1b.3: TCXO support (point #13)

**Files:**
- Modify: `include/loradriver/lora_config.hpp`
- Modify: `src/chips/sx127x/sx127x_registers.hpp`
- Modify: `src/chips/sx127x/sx127x_driver.cpp`
- Modify: `tests/host/test_sx127x_init_sequence.cpp`

- [ ] **Step 1: Add config field**

In `include/loradriver/lora_config.hpp`, after `lna_boost_rx`:

```cpp
    bool          tcxo_enabled      = false;  // external 32 MHz TCXO clock
                                              // (TTGO LoRa32, Heltec WiFi LoRa).
```

- [ ] **Step 2: Add register address**

In `src/chips/sx127x/sx127x_registers.hpp`, in the `reg` namespace:

```cpp
constexpr std::uint8_t kTcxo               = 0x4B;
```

- [ ] **Step 3: Write the failing test**

Add to `tests/host/test_sx127x_init_sequence.cpp`:

```cpp
bool TestTcxoEnabledSetsTcxoInputBit() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.tcxo_enabled = true;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    // RegTcxo bit 4 (TcxoInputOn) set
    LD_EXPECT_EQ(static_cast<std::uint8_t>(spi.reg(reg::kTcxo) & 0x10u),
                 std::uint8_t{0x10});
    return true;
}

bool TestTcxoDisabledLeavesXtalDefault() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.tcxo_enabled = false;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kTcxo) & 0x10u, std::uint8_t{0});
    return true;
}
```

Register both in `main()`.

- [ ] **Step 4: Apply tcxo_enabled in init**

In `src/chips/sx127x/sx127x_driver.cpp`, in `apply_init_sequence`,
**before** the `kFskSleep` write (TCXO must be configured before clock
is needed), insert:

```cpp
    // TCXO config: if enabled, set TcxoInputOn (bit 4). Datasheet §6.6.
    {
        std::uint8_t tcxo = 0;
        if ((e = spi_.read_register(reg::kTcxo, tcxo)) != LoRaError::OK) return e;
        if (cfg.tcxo_enabled) tcxo |= 0x10u; else tcxo &= ~0x10u;
        if ((e = spi_.write_register(reg::kTcxo, tcxo)) != LoRaError::OK) return e;
    }
```

- [ ] **Step 5: Build, test, commit**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure 2>&1 | tail -5
```

Expected: all pass.

```bash
git -C D:/DEV/C++/LoRaDriver add include/loradriver/lora_config.hpp src/chips/sx127x/sx127x_registers.hpp src/chips/sx127x/sx127x_driver.cpp tests/host/test_sx127x_init_sequence.cpp
git -C D:/DEV/C++/LoRaDriver commit -m "feat: external TCXO clock support via cfg.tcxo_enabled

TTGO LoRa32, Heltec WiFi LoRa boards have an external 32 MHz TCXO
on the SX127x's NRESET/TCXO pin. Without TcxoInputOn set, the chip
uses its internal crystal oscillator — which is absent on those
boards. Result was a silent init pass but no TX/RX.

Reads RegTcxo, sets/clears bit 4 per cfg, writes back. Done before
FSK sleep so the clock is correct from boot.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task P1b.4: Runtime LNA gain setter (point #14)

**Files:**
- Modify: `include/loradriver/radio_driver.hpp`
- Modify: `include/loradriver/chips/sx127x_driver.hpp`
- Modify: `src/chips/sx127x/sx127x_driver.cpp`
- Modify: `include/loradriver/lora_transceiver.hpp`
- Modify: `tests/host/test_sx127x_runtime_setters.cpp`

- [ ] **Step 1: Add the interface method**

In `include/loradriver/radio_driver.hpp`, add a pure-virtual:

```cpp
    /// Set LNA gain. gain=0 → AGC on; gain=1..6 → AGC off, LnaGain=G1..G6.
    /// Invalid values return InvalidConfig.
    [[nodiscard]] virtual LoRaError set_lna_gain(std::uint8_t gain) noexcept = 0;
```

- [ ] **Step 2: Override in SX127xDriver**

In `include/loradriver/chips/sx127x_driver.hpp`, declare in public:

```cpp
    [[nodiscard]] LoRaError set_lna_gain(std::uint8_t gain) noexcept override;
```

In `src/chips/sx127x/sx127x_driver.cpp`, add:

```cpp
LoRaError SX127xDriver::set_lna_gain(std::uint8_t gain) noexcept {
    if (!initialized_) return LoRaError::NotInitialized;
    if (gain > 6u) return LoRaError::InvalidConfig;
    if (gain == 0u) {
        // AGC on, ModemConfig3 bit 2 set
        std::uint8_t mc3 = 0;
        const LoRaError e1 = spi_.read_register(reg::kModemConfig3, mc3);
        if (e1 != LoRaError::OK) return e1;
        mc3 |= 0x04u;
        return spi_.write_register(reg::kModemConfig3, mc3);
    }
    // AGC off + RegLna LnaGain field
    std::uint8_t mc3 = 0;
    if (spi_.read_register(reg::kModemConfig3, mc3) != LoRaError::OK) return LoRaError::SpiFailure;
    mc3 &= ~0x04u;
    const LoRaError e2 = spi_.write_register(reg::kModemConfig3, mc3);
    if (e2 != LoRaError::OK) return e2;
    const std::uint8_t lna = static_cast<std::uint8_t>(
        (gain << 5) | (cfg_.lna_boost_rx ? 0x03u : 0x00u));
    return spi_.write_register(reg::kLna, lna);
}
```

- [ ] **Step 3: Expose via LoRaTransceiver**

In `include/loradriver/lora_transceiver.hpp`, add near the other setters:

```cpp
    [[nodiscard]] LoRaError set_lna_gain(std::uint8_t gain) noexcept {
        if (state_ == State::Uninit) return LoRaError::NotInitialized;
        return driver_.set_lna_gain(gain);
    }
```

- [ ] **Step 4: Write tests**

Add to `tests/host/test_sx127x_runtime_setters.cpp`:

```cpp
bool TestSetLnaGainRejectsOutOfRange() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(drv.set_lna_gain(7), LoRaError::InvalidConfig);
    return true;
}

bool TestSetLnaGainZeroEnablesAgc() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    // Set non-AGC first
    LD_EXPECT_EQ(drv.set_lna_gain(3), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kModemConfig3) & 0x04u, std::uint8_t{0});
    // Now AGC on
    LD_EXPECT_EQ(drv.set_lna_gain(0), LoRaError::OK);
    LD_EXPECT_EQ(static_cast<std::uint8_t>(spi.reg(reg::kModemConfig3) & 0x04u),
                 std::uint8_t{0x04});
    return true;
}

bool TestSetLnaGainSpecificValueDisablesAgc() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(drv.set_lna_gain(2), LoRaError::OK);
    LD_EXPECT_EQ(static_cast<std::uint8_t>((spi.reg(reg::kLna) >> 5) & 0x07u),
                 std::uint8_t{2});
    LD_EXPECT_EQ(spi.reg(reg::kModemConfig3) & 0x04u, std::uint8_t{0});
    return true;
}
```

Register all three in `main()`.

- [ ] **Step 5: Build, test, commit**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure 2>&1 | tail -5
```

Expected: all pass.

```bash
git -C D:/DEV/C++/LoRaDriver add include/loradriver/radio_driver.hpp include/loradriver/chips/sx127x_driver.hpp src/chips/sx127x/sx127x_driver.cpp include/loradriver/lora_transceiver.hpp tests/host/test_sx127x_runtime_setters.cpp
git -C D:/DEV/C++/LoRaDriver commit -m "feat: runtime LNA gain setter (0=AGC, 1-6=manual)

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task P1b.5: OCP enable/disable runtime (point #16)

**Files:**
- Modify: `include/loradriver/radio_driver.hpp`
- Modify: `include/loradriver/chips/sx127x_driver.hpp`
- Modify: `src/chips/sx127x/sx127x_driver.cpp`
- Modify: `include/loradriver/lora_transceiver.hpp`
- Modify: `tests/host/test_sx127x_runtime_setters.cpp`

- [ ] **Step 1: Add interface method**

In `include/loradriver/radio_driver.hpp`:

```cpp
    /// Enable or disable OverCurrent Protection. Disabling permits testing
    /// at higher trim levels than 240 mA — use with care.
    [[nodiscard]] virtual LoRaError set_ocp_enabled(bool enabled) noexcept = 0;
```

- [ ] **Step 2: Implement**

In `include/loradriver/chips/sx127x_driver.hpp` public:

```cpp
    [[nodiscard]] LoRaError set_ocp_enabled(bool enabled) noexcept override;
```

In `src/chips/sx127x/sx127x_driver.cpp`:

```cpp
LoRaError SX127xDriver::set_ocp_enabled(bool enabled) noexcept {
    if (!initialized_) return LoRaError::NotInitialized;
    std::uint8_t v = 0;
    LoRaError e = spi_.read_register(reg::kOcp, v);
    if (e != LoRaError::OK) return e;
    if (enabled) v |= 0x20u; else v &= ~0x20u;
    return spi_.write_register(reg::kOcp, v);
}
```

In `include/loradriver/lora_transceiver.hpp`:

```cpp
    [[nodiscard]] LoRaError set_ocp_enabled(bool enabled) noexcept {
        if (state_ == State::Uninit) return LoRaError::NotInitialized;
        return driver_.set_ocp_enabled(enabled);
    }
```

- [ ] **Step 3: Test**

Add to `tests/host/test_sx127x_runtime_setters.cpp`:

```cpp
bool TestSetOcpEnabledTogglesBit5() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(drv.set_ocp_enabled(false), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kOcp) & 0x20u, std::uint8_t{0});
    LD_EXPECT_EQ(drv.set_ocp_enabled(true), LoRaError::OK);
    LD_EXPECT_EQ(static_cast<std::uint8_t>(spi.reg(reg::kOcp) & 0x20u),
                 std::uint8_t{0x20});
    return true;
}
```

Register in `main()`.

- [ ] **Step 4: Build, test, commit**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure 2>&1 | tail -5
```

Expected: all pass.

```bash
git -C D:/DEV/C++/LoRaDriver add include/loradriver/radio_driver.hpp include/loradriver/chips/sx127x_driver.hpp src/chips/sx127x/sx127x_driver.cpp include/loradriver/lora_transceiver.hpp tests/host/test_sx127x_runtime_setters.cpp
git -C D:/DEV/C++/LoRaDriver commit -m "feat: runtime OCP enable/disable for RF lab work

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## Phase P2 — Quality, DX, CI, portability, docs (10 tasks)

Addresses all 10 P2 items: #19 Doxygen, #20 CI, #21 clang-format/tidy,
#22 runtime version, #23 -fno-exceptions, #24 sanitizers, #25 portable
lib_deps, #26 multi-instance, #27 random doc, #28 read_packet signature,
#29 begin-after-begin doc.

### Task P2.1: Add `loradriver::version_*()` runtime accessors (#22)

**Files:**
- Create: `include/loradriver/version.hpp`
- Create: `src/api/version.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/host/test_radio_stats.cpp`

- [ ] **Step 1: Create version header**

Content for `include/loradriver/version.hpp`:

```cpp
#pragma once

#include <cstdint>

namespace loradriver {

constexpr std::uint8_t kVersionMajor = 1;
constexpr std::uint8_t kVersionMinor = 1;
constexpr std::uint8_t kVersionPatch = 0;

[[nodiscard]] std::uint8_t version_major() noexcept;
[[nodiscard]] std::uint8_t version_minor() noexcept;
[[nodiscard]] std::uint8_t version_patch() noexcept;
[[nodiscard]] const char* version_string() noexcept;  // "1.1.0"

}  // namespace loradriver
```

- [ ] **Step 2: Create impl**

Content for `src/api/version.cpp`:

```cpp
#include "loradriver/version.hpp"

namespace loradriver {

std::uint8_t version_major() noexcept { return kVersionMajor; }
std::uint8_t version_minor() noexcept { return kVersionMinor; }
std::uint8_t version_patch() noexcept { return kVersionPatch; }
const char*  version_string() noexcept { return "1.1.0"; }

}  // namespace loradriver
```

- [ ] **Step 3: Wire into CMake**

Append to `target_sources` in `CMakeLists.txt`:

```cmake
  ${CMAKE_CURRENT_SOURCE_DIR}/src/api/version.cpp
```

- [ ] **Step 4: Test**

Add to `tests/host/test_radio_stats.cpp`:

```cpp
#include "loradriver/version.hpp"
```

Add test:

```cpp
bool TestVersionAccessors() {
    LD_EXPECT_EQ(loradriver::version_major(), std::uint8_t{1});
    LD_EXPECT_EQ(loradriver::version_minor(), std::uint8_t{1});
    LD_EXPECT_EQ(loradriver::version_patch(), std::uint8_t{0});
    const char* s = loradriver::version_string();
    LD_EXPECT(s[0] == '1' && s[1] == '.' && s[2] == '1');
    return true;
}
```

Register in `main()`.

- [ ] **Step 5: Build, test, commit**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure 2>&1 | tail -3
```

Expected: all pass.

```bash
git -C D:/DEV/C++/LoRaDriver add include/loradriver/version.hpp src/api/version.cpp CMakeLists.txt tests/host/test_radio_stats.cpp
git -C D:/DEV/C++/LoRaDriver commit -m "feat: add loradriver::version_*() runtime accessors

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task P2.2: Switch `read_packet` to `LoRaError` return (#28)

**Files:**
- Modify: `include/loradriver/radio_driver.hpp`
- Modify: `include/loradriver/chips/sx127x_driver.hpp`
- Modify: `src/chips/sx127x/sx127x_driver.cpp`
- Modify: `src/api/lora_transceiver.cpp`
- Modify: `tests/host/test_sx127x_rx_path.cpp`

- [ ] **Step 1: Change interface signature**

In `include/loradriver/radio_driver.hpp`, replace:

```cpp
    [[nodiscard]] virtual int read_packet(std::uint8_t* buf, std::size_t max_len) noexcept = 0;
```

with:

```cpp
    [[nodiscard]] virtual LoRaError read_packet(std::uint8_t* buf,
                                                std::size_t max_len,
                                                std::size_t& out_len) noexcept = 0;
```

- [ ] **Step 2: Update SX127xDriver**

In `include/loradriver/chips/sx127x_driver.hpp`:

```cpp
    [[nodiscard]] LoRaError read_packet(std::uint8_t* buf,
                                        std::size_t max_len,
                                        std::size_t& out_len) noexcept override;
```

In `src/chips/sx127x/sx127x_driver.cpp`, replace `read_packet`:

```cpp
LoRaError SX127xDriver::read_packet(std::uint8_t* buf,
                                    std::size_t max_len,
                                    std::size_t& out_len) noexcept {
    out_len = 0;
    if (!initialized_) return LoRaError::NotInitialized;
    if (buf == nullptr || max_len == 0u) return LoRaError::NullArgument;

    std::uint8_t rx_addr = 0;
    std::uint8_t nb_bytes = 0;
    LoRaError e;
    if ((e = spi_.read_register(reg::kFifoRxCurrentAddr, rx_addr)) != LoRaError::OK) return e;
    if ((e = spi_.read_register(reg::kRxNbBytes, nb_bytes)) != LoRaError::OK) return e;
    if (nb_bytes == 0u) return LoRaError::OK;  // no bytes available, not an error

    const std::size_t to_read = (nb_bytes <= max_len) ? nb_bytes : max_len;
    if ((e = spi_.write_register(reg::kFifoAddrPtr, rx_addr)) != LoRaError::OK) return e;
    if ((e = spi_.burst_read(reg::kFifo, buf, to_read)) != LoRaError::OK) return e;
    out_len = to_read;
    return LoRaError::OK;
}
```

- [ ] **Step 3: Update LoRaTransceiver::on_driver_event**

In `src/api/lora_transceiver.cpp`, in the `RxDone` case, replace:

```cpp
            const int n = driver_.read_packet(rx_buf_, sizeof(rx_buf_));
            if (n > 0 && packet_cb_) {
```

with:

```cpp
            std::size_t n = 0;
            const LoRaError rr = driver_.read_packet(rx_buf_, sizeof(rx_buf_), n);
            if (rr == LoRaError::OK && n > 0 && packet_cb_) {
```

And update the cast in the metadata block:

```cpp
                meta.length             = static_cast<std::uint8_t>(n);
```

(already a `std::size_t` source; the explicit cast is preserved.)

- [ ] **Step 4: Update RX tests**

In `tests/host/test_sx127x_rx_path.cpp`, replace tests calling
`drv.read_packet(...)` returning int with the new signature.

`TestReadPacketRejectsNull`:

```cpp
bool TestReadPacketRejectsNull() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    std::size_t out = 0;
    LD_EXPECT_EQ(drv.read_packet(nullptr, 10, out), LoRaError::NullArgument);
    return true;
}
```

`TestReadPacketCopiesFromFifo`:

```cpp
bool TestReadPacketCopiesFromFifo() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    const std::uint8_t payload[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    for (std::size_t i = 0; i < 4; ++i) {
        spi.set_register(static_cast<std::uint8_t>(reg::kFifo + i), payload[i]);
    }
    spi.set_register(reg::kFifoRxCurrentAddr, 0);
    spi.set_register(reg::kRxNbBytes, 4);
    std::uint8_t out[8]{};
    std::size_t n = 0;
    LD_EXPECT_EQ(drv.read_packet(out, sizeof(out), n), LoRaError::OK);
    LD_EXPECT_EQ(n, std::size_t{4});
    for (int i = 0; i < 4; ++i) LD_EXPECT_EQ(out[i], payload[i]);
    return true;
}
```

`TestReadPacketClampsToMaxLen`:

```cpp
bool TestReadPacketClampsToMaxLen() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    for (std::size_t i = 0; i < 10; ++i) {
        spi.set_register(static_cast<std::uint8_t>(reg::kFifo + i), static_cast<std::uint8_t>(i));
    }
    spi.set_register(reg::kFifoRxCurrentAddr, 0);
    spi.set_register(reg::kRxNbBytes, 10);
    std::uint8_t out[4]{};
    std::size_t n = 0;
    LD_EXPECT_EQ(drv.read_packet(out, sizeof(out), n), LoRaError::OK);
    LD_EXPECT_EQ(n, std::size_t{4});
    return true;
}
```

- [ ] **Step 5: Build, test, commit**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure 2>&1 | tail -5
```

Expected: all pass.

```bash
git -C D:/DEV/C++/LoRaDriver add include/loradriver/radio_driver.hpp include/loradriver/chips/sx127x_driver.hpp src/chips/sx127x/sx127x_driver.cpp src/api/lora_transceiver.cpp tests/host/test_sx127x_rx_path.cpp
git -C D:/DEV/C++/LoRaDriver commit -m "$(cat <<'EOF'
refactor: read_packet returns LoRaError + out_len reference

Previously returned int (-1 on error, n on success) — inconsistent
with the rest of the radio API. Now matches the LoRaError convention:
returns OK with out_len=0 when no packet is available, NullArgument
on bad inputs, SpiFailure on bus errors.

Breaking change for direct IRadioDriver consumers; LoRaTransceiver
façade users are unaffected (façade hides the call).

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

### Task P2.3: GitHub Actions CI for host tests (#20)

**Files:**
- Create: `.github/workflows/host-tests.yml`

- [ ] **Step 1: Create workflow**

Content for `.github/workflows/host-tests.yml`:

```yaml
name: Host tests

on:
  push:
    branches: [main, "rewrite/**", "hardening/**"]
  pull_request:
    branches: [main]

jobs:
  build-and-test:
    runs-on: ${{ matrix.os }}
    strategy:
      fail-fast: false
      matrix:
        os: [ubuntu-latest, windows-latest, macos-latest]
    steps:
      - uses: actions/checkout@v4

      - name: Configure CMake
        run: cmake -S . -B build/host

      - name: Build
        run: cmake --build build/host --config Debug

      - name: Run host tests
        run: ctest --test-dir build/host -C Debug --output-on-failure
```

- [ ] **Step 2: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add .github/workflows/host-tests.yml
git -C D:/DEV/C++/LoRaDriver commit -m "ci: GitHub Actions matrix (Linux/Windows/macOS) for host tests

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

(Activation requires pushing to GitHub. Not done in this plan.)

### Task P2.4: Build under -fno-exceptions and remove try/catch (#23)

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `src/chips/sx127x/sx127x_driver.cpp`

- [ ] **Step 1: Add -fno-exceptions to the loradriver target**

In `CMakeLists.txt`, after `target_include_directories(loradriver ...)`:

```cmake
if(NOT MSVC)
  target_compile_options(loradriver PRIVATE -fno-exceptions -fno-rtti)
endif()
```

MSVC handles exceptions differently — leave it alone there; we'll check the
firmware compile via PlatformIO + Xtensa toolchain which respects
`-fno-exceptions` already.

- [ ] **Step 2: Remove the try/catch in emit()**

In `src/chips/sx127x/sx127x_driver.cpp`, replace `emit()`:

```cpp
void SX127xDriver::emit(RadioEvent ev, int param) noexcept {
    if (event_cb_) {
        event_cb_(ev, param);
    }
}
```

(We document in API docs that user callbacks must not throw.)

- [ ] **Step 3: Build under MSVC (host) and verify no regression**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure 2>&1 | tail -3
```

Expected: pass.

- [ ] **Step 4: Verify ESP32 build still succeeds**

```bash
"$HOME/.platformio/penv/Scripts/pio.exe" run -d D:/DEV/PlatformIO/SYNC-SIGNAL-LORA/SYNC-SIGNAL-LORA -e esp32dev 2>&1 | tail -5
```

Expected: SUCCESS.

- [ ] **Step 5: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add CMakeLists.txt src/chips/sx127x/sx127x_driver.cpp
git -C D:/DEV/C++/LoRaDriver commit -m "$(cat <<'EOF'
feat: build loradriver target under -fno-exceptions on Clang/GCC

Removes the defensive try/catch around user event callbacks. Callbacks
must be noexcept (documented in API docs). Reduces firmware binary size
and removes a class of UB from the noexcept declarations.

MSVC is left alone — host tests still build with default exception
support but the noexcept invariants apply equally.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

### Task P2.5: Doxygen comments on public API (#19)

**Files:**
- Modify: `include/loradriver/lora_transceiver.hpp`
- Modify: `include/loradriver/lora_config.hpp`
- Modify: `include/loradriver/radio_driver.hpp`
- Modify: `include/loradriver/lora_packet.hpp`
- Modify: `include/loradriver/radio_event.hpp`
- Modify: `include/loradriver/radio_stats.hpp`
- Modify: `include/loradriver/lora_error.hpp`
- Create: `Doxyfile`

- [ ] **Step 1: Annotate public types**

Add Doxygen `///` comments above each public class, enum, and method in the
listed headers. Example pattern for `LoRaTransceiver` in
`include/loradriver/lora_transceiver.hpp` — replace the class declaration
opening:

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
class LoRaTransceiver {
```

Apply equivalent annotations on every public method. For brevity, this
plan does not repeat all annotations — the engineer should add one
@brief + @param + @return block per method, following the existing
comment style already present on some methods.

- [ ] **Step 2: Create Doxyfile**

A minimal Doxyfile that builds HTML from headers. Content for `Doxyfile`:

```
PROJECT_NAME           = LoRaDriver
PROJECT_NUMBER         = 1.1.0
PROJECT_BRIEF          = "Clean C++17 driver for Semtech SX1276/SX1278"
OUTPUT_DIRECTORY       = docs/doxygen
INPUT                  = include README.md
USE_MDFILE_AS_MAINPAGE = README.md
RECURSIVE              = YES
EXTRACT_ALL            = NO
EXTRACT_PRIVATE        = NO
HIDE_UNDOC_MEMBERS     = NO
GENERATE_HTML          = YES
GENERATE_LATEX         = NO
QUIET                  = YES
WARNINGS               = YES
WARN_IF_UNDOCUMENTED   = YES
```

- [ ] **Step 3: Test doxygen runs (if installed)**

```bash
doxygen Doxyfile 2>&1 | tail -10 || echo "doxygen not installed — skip"
```

If installed, expected: HTML in `docs/doxygen/html/index.html`.

- [ ] **Step 4: Add docs/doxygen to .gitignore**

In `.gitignore` (create if missing), add:

```
docs/doxygen/
```

- [ ] **Step 5: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add include/loradriver/ Doxyfile .gitignore
git -C D:/DEV/C++/LoRaDriver commit -m "docs: Doxygen comments on public API + Doxyfile

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task P2.6: clang-format + clang-tidy gates (#21)

**Files:**
- Modify: `.clang-format` (keep existing)
- Modify: `.clang-tidy` (keep existing)
- Create: `tools/lint.sh`
- Modify: `.github/workflows/host-tests.yml`

- [ ] **Step 1: Verify existing config files**

```bash
cat D:/DEV/C++/LoRaDriver/.clang-format
cat D:/DEV/C++/LoRaDriver/.clang-tidy
```

Expected: files exist with reasonable defaults (carried over from v0.1).
If they do not exist, content for `.clang-format`:

```yaml
BasedOnStyle: Google
ColumnLimit: 100
IndentWidth: 4
```

Content for `.clang-tidy`:

```yaml
Checks: >
  -*,
  bugprone-*,
  cert-*,
  cppcoreguidelines-pro-type-cstyle-cast,
  modernize-use-nullptr,
  modernize-use-override,
  performance-*,
  readability-redundant-*
WarningsAsErrors: ''
HeaderFilterRegex: 'include/loradriver/.*'
```

- [ ] **Step 2: Create lint script**

Content for `tools/lint.sh`:

```bash
#!/usr/bin/env bash
# Runs clang-format --dry-run --Werror on all source files. Non-zero exit if
# any file needs reformatting.
set -euo pipefail

cd "$(dirname "$0")/.."

mapfile -t files < <(git ls-files 'src/*.cpp' 'src/**/*.cpp' \
                     'include/loradriver/**/*.hpp' \
                     'tests/host/*.cpp' 'tests/host/*.hpp')

clang-format --dry-run --Werror "${files[@]}"
```

```bash
chmod +x D:/DEV/C++/LoRaDriver/tools/lint.sh
```

- [ ] **Step 3: Add a lint job to CI**

In `.github/workflows/host-tests.yml`, append:

```yaml
  lint:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install clang-format
        run: sudo apt-get install -y clang-format
      - name: Run clang-format
        run: ./tools/lint.sh
```

- [ ] **Step 4: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add .clang-format .clang-tidy tools/lint.sh .github/workflows/host-tests.yml
git -C D:/DEV/C++/LoRaDriver commit -m "ci: clang-format lint script + CI job

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task P2.7: Sanitizer build target (#24)

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `.github/workflows/host-tests.yml`

- [ ] **Step 1: Add a sanitizer option to CMake**

In `CMakeLists.txt`, add near the top after `set(CMAKE_CXX_EXTENSIONS OFF)`:

```cmake
option(LORADRIVER_SANITIZERS "Enable AddressSanitizer + UndefinedBehaviorSanitizer for host build" OFF)

if(LORADRIVER_SANITIZERS)
  if(MSVC)
    message(WARNING "Sanitizers requested but MSVC support is partial — skipping.")
  else()
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
  endif()
endif()
```

- [ ] **Step 2: Add sanitizer CI job**

In `.github/workflows/host-tests.yml`, append:

```yaml
  sanitizers:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Configure with sanitizers
        run: cmake -S . -B build/asan -DLORADRIVER_SANITIZERS=ON
      - name: Build
        run: cmake --build build/asan
      - name: Run
        run: ctest --test-dir build/asan --output-on-failure
```

- [ ] **Step 3: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add CMakeLists.txt .github/workflows/host-tests.yml
git -C D:/DEV/C++/LoRaDriver commit -m "ci: optional AddressSan + UBSan host build (LORADRIVER_SANITIZERS=ON)

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task P2.8: Document multi-instance + 2 examples on different SPI buses (#26)

**Files:**
- Create: `examples/MultiInstance/MultiInstance.ino`
- Modify: `README.md`

- [ ] **Step 1: Write the example**

Content for `examples/MultiInstance/MultiInstance.ino`:

```cpp
// Multi-instance example: two SX1276 modules on independent SPI buses.
// On ESP32, VSPI (HSPI on some boards) is the second hardware SPI.
#include <Arduino.h>
#include <SPI.h>

#include "loradriver/chips/sx127x_driver.hpp"
#include "loradriver/hal/esp32_spi_device.hpp"
#include "loradriver/lora_transceiver.hpp"

using namespace loradriver;

SPIClass SPI2(HSPI);

// Instance A — VSPI bus, CS=5
hal::Esp32SpiDevice  spiA(SPI, /*cs=*/5);
chips::SX127xDriver  drvA(spiA);
LoRaTransceiver      trxA(drvA);

// Instance B — HSPI bus, CS=15
hal::Esp32SpiDevice  spiB(SPI2, /*cs=*/15);
chips::SX127xDriver  drvB(spiB);
LoRaTransceiver      trxB(drvB);

void setup() {
    Serial.begin(115200);
    SPI.begin();          // VSPI default pins
    SPI2.begin(14, 12, 13);  // HSPI: SCK=14, MISO=12, MOSI=13

    LoRaConfig cfgA;
    cfgA.chip = ChipModel::SX1276;
    cfgA.frequency_hz = 868'000'000u;
    cfgA.pin_ss = 5; cfgA.pin_reset = 4; cfgA.pin_dio0 = 26;
    trxA.begin(cfgA);

    LoRaConfig cfgB;
    cfgB.chip = ChipModel::SX1276;
    cfgB.frequency_hz = 868'200'000u;  // different channel
    cfgB.pin_ss = 15; cfgB.pin_reset = 17; cfgB.pin_dio0 = 16;
    trxB.begin(cfgB);

    Serial.printf("A version=%02X  B version=%02X\n",
                  trxA.chip_version(), trxB.chip_version());
}

void loop() {
    trxA.poll();
    trxB.poll();
    delay(2);
}
```

- [ ] **Step 2: Document in README**

In `README.md`, add a section before "Build host tests":

```markdown
## Multiple instances

The driver supports multiple SX127x modules on independent SPI buses.
Each instance is fully independent — no shared state, no singletons.
See `examples/MultiInstance/` for a two-module ESP32 example.
```

- [ ] **Step 3: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add examples/MultiInstance/MultiInstance.ino README.md
git -C D:/DEV/C++/LoRaDriver commit -m "docs: multi-instance example (two SX1276 modules on VSPI+HSPI)

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task P2.9: API docs page covering migration, lifecycle, random caveat (#25, #27, #29)

**Files:**
- Create: `docs/api.md`

- [ ] **Step 1: Write docs/api.md**

Content for `docs/api.md`:

```markdown
# LoRaDriver Public API Reference

## Class hierarchy

```
ISpiDevice (HAL)
    └── ArduinoSpiDevice / Esp32SpiDevice / FakeSpiDevice
SX127xDriver : public IRadioDriver
LoRaTransceiver  ← façade for everyday use
RadioPumpTask    ← optional ESP32 FreeRTOS pump
```

## Lifecycle

`LoRaTransceiver::begin(cfg)` puts the driver in `Standby` state. Calling
`begin()` again on an already-initialised instance returns
`LoRaError::AlreadyInitialized` — the config is **not** re-applied. To
change config, call `end()` first, then `begin(new_cfg)`.

`end()` cleanly tears down: detaches the driver event callback, clears all
user callbacks, puts the chip to sleep, returns the FSM to `Uninit`. The
caller is still responsible for `detachInterrupt(pin)` if they had wired
the ISR — the driver does not own the GPIO line.

## lib_deps (PlatformIO consumers)

Local development (Windows path):
```
lib_deps = symlink://D:/DEV/C++/LoRaDriver
```

Local development (Linux/macOS):
```
lib_deps = symlink:///home/you/dev/LoRaDriver
```

Production / shared:
```
lib_deps = https://github.com/Lakalot/LoRaDriver.git#v1.1.0
```

## Random bytes

`IRadioDriver::random_byte()` reads `RegRssiWideband` once. The returned
byte contains a few bits of entropy from the wideband noise floor — useful
as a seed but NOT cryptographically secure. For higher-quality randomness,
read multiple bytes spaced across receive idle periods and feed them into
a hash (SHA-256) or a CSPRNG.

## Callback contract

User-provided callbacks (`on_receive`, `on_event`, `on_tx_done`) MUST be
noexcept. The driver is built with `-fno-exceptions` on Clang/GCC; a
throwing callback is undefined behaviour. If your callback can fail, store
the error in a flag and handle it from the main loop instead.

## ISR responsibilities

The DIO0 ISR must:
1. Call `driver_.handle_interrupt()` (cheap: pushes a ring entry).
2. Optionally call `pump.notify_from_isr()` if using `RadioPumpTask`.
3. Do nothing else SPI-related — the driver reads `RegIrqFlags` from
   `process_events()`, not from the ISR.

Mark the ISR with `IRAM_ATTR` on ESP32 so it lives in IRAM and stays
callable while flash is busy.
```

- [ ] **Step 2: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add docs/api.md
git -C D:/DEV/C++/LoRaDriver commit -m "docs: API reference covering lifecycle, lib_deps, random, ISR contract

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task P2.10: Bump version metadata to 1.1.0 across manifests

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `library.json`
- Modify: `library.properties`
- Modify: `CHANGELOG.md`

- [ ] **Step 1: Bump version everywhere**

In `CMakeLists.txt`:

```cmake
project(LoRaDriver VERSION 1.1.0 LANGUAGES CXX)
```

In `library.json`:

```json
    "version": "1.1.0",
```

In `library.properties`:

```
version=1.1.0
```

In `CHANGELOG.md`, prepend at the top below the `# Changelog` line:

```markdown
## 1.1.0 — 2026-05-13

Hardening release. See the 29-point gap list resolved across
`docs/superpowers/plans/2026-05-13-loradriver-hardening.md`.

### Added (P0)

- `LoRaConfig::auto_reset` (default true) — driver pulses RST itself.
- `IRadioDriver::check_alive()` — RegVersion heartbeat for runtime liveness.
- `LoRaConfig::polling_mode` — process_events() works without a DIO0 ISR.
- Mode-transition read-back verify on TX / RX / CAD entry.
- Extended embedded smoke test with check_alive + TX + loopback.
- Host coverage for SX1278 init path.

### Added (P1)

- `LoRaConfig::rx_silence_timeout_ms` — RX idle watchdog with
  RadioEvent::RxTimeout emission.
- Split FIFO base addresses (TX=0, RX=128) to prevent concurrent stomp.
- 10-bit `symbol_timeout` properly written across ModemConfig2 and
  SymbTimeoutLsb.
- RX image calibration during init (datasheet §4.2.3.8).
- `LoRaTransceiver::end()` now clears callbacks and detaches driver hook.
- `RadioPumpTask::stop()` is cooperative — no more mid-send vTaskDelete.
- `start_cad(auto_rx=true)` enters RX automatically on detection.
- `LoRaConfig::invert_iq` is now wired to RegInvertIq / RegInvertIq2.
- `LoRaConfig::tcxo_enabled` for boards with external TCXO.
- `IRadioDriver::set_lna_gain(0..6)` runtime LNA control.
- `IRadioDriver::set_ocp_enabled(bool)` runtime OCP control.

### Changed

- `read_packet` returns LoRaError with out-param length (was: int).
- loradriver target builds under `-fno-exceptions` on Clang/GCC.
- Removed defensive try/catch around event callbacks (callbacks must
  be noexcept — documented in docs/api.md).

### Added (P2)

- `loradriver::version_major/minor/patch/string()` runtime accessors.
- GitHub Actions CI for host tests (Linux/Windows/macOS).
- `LORADRIVER_SANITIZERS=ON` CMake option (AddressSan + UBSan).
- clang-format lint script + CI job.
- Doxygen API docs (`docs/doxygen/`).
- Multi-instance example (`examples/MultiInstance/`).
- API reference (`docs/api.md`) covering lifecycle, lib_deps,
  ISR contract, random byte caveat.

```

- [ ] **Step 2: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add CMakeLists.txt library.json library.properties CHANGELOG.md
git -C D:/DEV/C++/LoRaDriver commit -m "chore: bump version metadata to 1.1.0

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## Phase P3 — Optional features (5 tasks)

Addresses the 8 P3 items. Each is independent and skippable.

### Task P3.1: ContinuousWave (CW) test mode

**Files:**
- Modify: `include/loradriver/radio_driver.hpp`
- Modify: `include/loradriver/chips/sx127x_driver.hpp`
- Modify: `src/chips/sx127x/sx127x_driver.cpp`
- Modify: `tests/host/test_sx127x_tx_path.cpp`

- [ ] **Step 1: Add interface method**

In `include/loradriver/radio_driver.hpp`:

```cpp
    /// Enter continuous-wave (unmodulated carrier) mode for RF certification
    /// tests. Caller must call set_standby() to exit.
    [[nodiscard]] virtual LoRaError start_continuous_wave() noexcept = 0;
```

- [ ] **Step 2: Implement**

In `include/loradriver/chips/sx127x_driver.hpp`:

```cpp
    [[nodiscard]] LoRaError start_continuous_wave() noexcept override;
```

In `src/chips/sx127x/sx127x_driver.cpp`:

```cpp
LoRaError SX127xDriver::start_continuous_wave() noexcept {
    if (!initialized_) return LoRaError::NotInitialized;
    // ModemConfig2 bit 3 = TxContinuousMode
    LoRaError e;
    std::uint8_t mc2 = 0;
    if ((e = spi_.read_register(reg::kModemConfig2, mc2)) != LoRaError::OK) return e;
    mc2 |= 0x08u;
    if ((e = spi_.write_register(reg::kModemConfig2, mc2)) != LoRaError::OK) return e;
    return set_op_mode(opmode::kLoRaTx);
}
```

- [ ] **Step 3: Test**

In `tests/host/test_sx127x_tx_path.cpp`:

```cpp
bool TestStartContinuousWaveSetsTxContBit() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(drv.start_continuous_wave(), LoRaError::OK);
    LD_EXPECT_EQ(static_cast<std::uint8_t>(spi.reg(reg::kModemConfig2) & 0x08u),
                 std::uint8_t{0x08});
    LD_EXPECT_EQ(spi.reg(reg::kOpMode), opmode::kLoRaTx);
    return true;
}
```

Register in `main()`.

- [ ] **Step 4: Build, test, commit**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure 2>&1 | tail -3
```

Expected: pass.

```bash
git -C D:/DEV/C++/LoRaDriver add include/loradriver/radio_driver.hpp include/loradriver/chips/sx127x_driver.hpp src/chips/sx127x/sx127x_driver.cpp tests/host/test_sx127x_tx_path.cpp
git -C D:/DEV/C++/LoRaDriver commit -m "feat: start_continuous_wave() for RF certification tests

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task P3.2: Add ChipModel::SX1277 and SX1279

**Files:**
- Modify: `include/loradriver/lora_config.hpp`
- Modify: `src/api/lora_config.cpp`
- Modify: `tests/host/test_lora_config_validate.cpp`

- [ ] **Step 1: Extend enum**

In `include/loradriver/lora_config.hpp`:

```cpp
enum class ChipModel : std::uint8_t { SX1276, SX1277, SX1278, SX1279 };
```

- [ ] **Step 2: Validation per chip**

In `src/api/lora_config.cpp`, in `validate()` replace the chip/freq block:

```cpp
    // Frequency vs chip
    switch (chip) {
        case ChipModel::SX1278:
            // SX1278: 137-525 MHz
            if (!freq_in_range_sx1278(frequency_hz)) return LoRaError::InvalidConfig;
            break;
        case ChipModel::SX1277:
            // SX1277: 137-1020 MHz, same as SX1276 except SF max=9
            if (!freq_in_range_sx1276(frequency_hz)) return LoRaError::InvalidConfig;
            if (spreading_factor > 9) return LoRaError::InvalidConfig;
            break;
        case ChipModel::SX1279:
            // SX1279: 137-960 MHz (no upper sub-GHz extension)
            if (frequency_hz < 137'000'000u || frequency_hz > 960'000'000u) {
                return LoRaError::InvalidConfig;
            }
            break;
        case ChipModel::SX1276:
        default:
            if (!freq_in_range_sx1276(frequency_hz)) return LoRaError::InvalidConfig;
            break;
    }
```

- [ ] **Step 3: Test**

Add to `tests/host/test_lora_config_validate.cpp`:

```cpp
bool TestSx1277RejectsSf10AndAbove() {
    LoRaConfig c = MakeValidSx1276();
    c.chip = ChipModel::SX1277;
    c.spreading_factor = 10;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    return true;
}

bool TestSx1279Allows868MHz() {
    LoRaConfig c = MakeValidSx1276();
    c.chip = ChipModel::SX1279;
    c.frequency_hz = 868'000'000u;
    LD_EXPECT_EQ(c.validate(), LoRaError::OK);
    return true;
}
```

Register both in `main()`.

- [ ] **Step 4: Build, test, commit**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure 2>&1 | tail -3
```

Expected: pass.

```bash
git -C D:/DEV/C++/LoRaDriver add include/loradriver/lora_config.hpp src/api/lora_config.cpp tests/host/test_lora_config_validate.cpp
git -C D:/DEV/C++/LoRaDriver commit -m "feat: support SX1277 + SX1279 (band/SF validation only)

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task P3.3: Implicit-header SF6 explicit support

**Files:**
- Modify: `src/chips/sx127x/sx127x_driver.cpp`
- Modify: `tests/host/test_sx127x_init_sequence.cpp`

- [ ] **Step 1: Write test**

Add to `tests/host/test_sx127x_init_sequence.cpp`:

```cpp
bool TestSf6ImplicitHeaderInitOk() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.spreading_factor = 6;
    c.implicit_header = true;
    c.crc_enabled = true;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    // ModemConfig1 bit 0 should be 1 (implicit header)
    LD_EXPECT_EQ(static_cast<std::uint8_t>(spi.reg(reg::kModemConfig1) & 0x01u),
                 std::uint8_t{0x01});
    // DetectionOptimize (0x31) for SF6: 0x05
    // DetectionThreshold (0x37) for SF6: 0x0C
    LD_EXPECT_EQ(spi.reg(reg::kDetectionOptimize), std::uint8_t{0x05});
    LD_EXPECT_EQ(spi.reg(reg::kDetectionThreshold), std::uint8_t{0x0C});
    return true;
}
```

Register in `main()`.

- [ ] **Step 2: Implement SF6 special-case in modem config**

In `src/chips/sx127x/sx127x_driver.cpp`, at the end of `apply_modem_config`,
before the final return, add:

```cpp
    // SF6 requires specific DetectionOptimize and DetectionThreshold values
    // (datasheet table 28). Other SF: defaults are fine.
    if (cfg.spreading_factor == 6u) {
        if ((e = spi_.write_register(reg::kDetectionOptimize, 0x05)) != LoRaError::OK) return e;
        if ((e = spi_.write_register(reg::kDetectionThreshold, 0x0C)) != LoRaError::OK) return e;
    } else {
        if ((e = spi_.write_register(reg::kDetectionOptimize, 0x03)) != LoRaError::OK) return e;
        if ((e = spi_.write_register(reg::kDetectionThreshold, 0x0A)) != LoRaError::OK) return e;
    }
```

- [ ] **Step 3: Build, test, commit**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure 2>&1 | tail -3
```

Expected: pass.

```bash
git -C D:/DEV/C++/LoRaDriver add src/chips/sx127x/sx127x_driver.cpp tests/host/test_sx127x_init_sequence.cpp
git -C D:/DEV/C++/LoRaDriver commit -m "feat: SF6 init writes DetectionOptimize=0x05 and Threshold=0x0C

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task P3.4: Valid-header pre-RxDone callback

The `RadioEvent::ValidHeader` is already emitted by `process_events`. This
task documents and adds a `on_header` convenience callback on the
transceiver façade.

**Files:**
- Modify: `include/loradriver/lora_transceiver.hpp`
- Modify: `src/api/lora_transceiver.cpp`
- Modify: `tests/host/test_transceiver_fsm.cpp`

- [ ] **Step 1: Add the callback**

In `include/loradriver/lora_transceiver.hpp`, in the `using ... Callback`
block, add:

```cpp
    using HeaderCallback  = std::function<void()>;
```

In the public members:

```cpp
    void on_header(HeaderCallback cb) noexcept;
```

In private:

```cpp
    HeaderCallback   header_cb_{};
```

- [ ] **Step 2: Implement**

In `src/api/lora_transceiver.cpp`, add near the other callback setters:

```cpp
void LoRaTransceiver::on_header(HeaderCallback cb) noexcept { header_cb_ = std::move(cb); }
```

In `on_driver_event`, add a case in the switch:

```cpp
        case RadioEvent::ValidHeader:
            if (header_cb_) header_cb_();
            break;
```

- [ ] **Step 3: Test**

Add to `tests/host/test_transceiver_fsm.cpp`:

```cpp
bool TestHeaderCallbackFiresOnValidHeader() {
    FakeSpiDevice spi; SX127xDriver drv(spi); LoRaTransceiver trx(drv);
    LD_EXPECT_EQ(trx.begin(MakeCfg()), LoRaError::OK);
    bool fired = false;
    trx.on_header([&]() { fired = true; });
    spi.set_register(reg::kIrqFlags, irq::kValidHeader);
    drv.handle_interrupt();
    trx.poll();
    LD_EXPECT(fired);
    return true;
}
```

Register in `main()`.

- [ ] **Step 4: Build, test, commit**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | tail -3
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure 2>&1 | tail -3
```

Expected: pass.

```bash
git -C D:/DEV/C++/LoRaDriver add include/loradriver/lora_transceiver.hpp src/api/lora_transceiver.cpp tests/host/test_transceiver_fsm.cpp
git -C D:/DEV/C++/LoRaDriver commit -m "feat: on_header callback for ValidHeader IRQ (pre-RxDone wake-up)

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task P3.5: FHSS, RxDutyCycle — explicitly out of scope, document

**Files:**
- Modify: `docs/api.md`

- [ ] **Step 1: Add a "Not implemented" section to docs/api.md**

Append to `docs/api.md`:

```markdown
## Out of scope for v1.1

The following datasheet features are intentionally not exposed:

- **FHSS (Frequency Hopping Spread Spectrum)** — `RegHopPeriod` + IRQ
  `FhssChangeChannel`. Useful for high-bitrate links over noisy bands
  but adds significant state machine complexity. Open an issue if you
  need it.
- **RxDutyCycle (battery-economising RX)** — datasheet §4.1.4.4.
  Trades latency for power. Implementable in user code via a timer that
  alternates `set_sleep()` / `start_receive(false)`.
- **SX126x family** — different chip family (command-based protocol
  instead of register-based). Out of scope for the SX127x driver; a
  separate `loradriver::chips::SX126xDriver` could be added in a future
  major version.
```

- [ ] **Step 2: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add docs/api.md
git -C D:/DEV/C++/LoRaDriver commit -m "docs: explicit out-of-scope list (FHSS, RxDutyCycle, SX126x)

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## Final task: Merge to main + tag v1.1.0

- [ ] **Step 1: Run full host suite once more from scratch**

```bash
rm -rf D:/DEV/C++/LoRaDriver/build
cmake -S D:/DEV/C++/LoRaDriver -B D:/DEV/C++/LoRaDriver/build/host
cmake --build D:/DEV/C++/LoRaDriver/build/host
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host -C Debug --output-on-failure
```

Expected: `100% tests passed, 0 tests failed`.

- [ ] **Step 2: Verify consumer still builds**

```bash
"$HOME/.platformio/penv/Scripts/pio.exe" run -d D:/DEV/PlatformIO/SYNC-SIGNAL-LORA/SYNC-SIGNAL-LORA -e esp32dev 2>&1 | tail -5
```

Expected: SUCCESS.

- [ ] **Step 3: Merge to main**

```bash
git -C D:/DEV/C++/LoRaDriver checkout main
git -C D:/DEV/C++/LoRaDriver merge --no-ff hardening/v1.1 -m "Merge LoRaDriver v1.1 hardening

Closes the 29 known gaps in v1.0:
  - P0 safety (auto-reset, verify, heartbeat, polling-only, hardware-tested)
  - P1 robustness (RX watchdog, split FIFO, symbol_timeout, image cal,
    end() cleanup, cooperative pump stop, CAD auto-rx, invert_iq, TCXO,
    LNA gain, OCP toggle)
  - P2 quality (version accessors, read_packet signature, CI, sanitizers,
    Doxygen, clang-format, multi-instance, API docs)
  - P3 features (CW, SX1277/79, SF6, header callback, scope doc)

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

- [ ] **Step 4: Tag**

```bash
git -C D:/DEV/C++/LoRaDriver tag -a v1.1.0 -m "LoRaDriver v1.1.0 — production hardening"
git -C D:/DEV/C++/LoRaDriver log --oneline -5
git -C D:/DEV/C++/LoRaDriver tag -l 'v*'
```

Do NOT push automatically — user decides when to publish.

---

## Coverage map — 29 points → tasks

| Point | Description | Task(s) |
|---|---|---|
| #1 | SX1278 hardware untested | P0.6 + P0.7 (HW gate) |
| #2 | Embedded tests never run | P0.5 (HW gate) |
| #3 | Reset GPIO delegated to caller | P0.1 |
| #4 | No runtime chip-alive check | P0.4 |
| #5 | set_op_mode no read-back | P0.2 |
| #6 | process_events depends on ISR ring | P0.3 |
| #7 | No RX-silence watchdog | P1a.1 |
| #8 | FIFO TX/RX collisions | P1a.2 |
| #9 | start_cad no auto-RX | P1b.1 |
| #10 | invert_iq silently ignored | P1b.2 |
| #11 | symbol_timeout MSB forced 0b11 | P1a.3 |
| #12 | No RX image calibration | P1a.4 |
| #13 | No TCXO support | P1b.3 |
| #14 | No setLnaGain runtime | P1b.4 |
| #15 | end() doesn't clean callbacks | P1a.5 |
| #16 | No setOcpDisabled | P1b.5 |
| #17 | Pump stop kills mid-send | P1a.6 |
| #18 | tx_pending not atomic | P1a.6 |
| #19 | No Doxygen | P2.5 |
| #20 | No CI configured | P2.3 |
| #21 | clang-format/tidy unused | P2.6 |
| #22 | No runtime version | P2.1 |
| #23 | -fno-exceptions not tested | P2.4 |
| #24 | No sanitizer build | P2.7 |
| #25 | symlink:// Windows-specific | P2.9 (docs) |
| #26 | Multi-instance undocumented | P2.8 |
| #27 | random_byte one byte only | P2.9 (docs) |
| #28 | read_packet returns int | P2.2 |
| #29 | begin-after-begin undocumented | P2.9 (docs) |

(Plus P3.1-P3.5 for ContinuousWave, SX1277/79, SF6, header callback,
out-of-scope doc — the 8 P3 items listed in the original 29 are covered
by these 5 tasks since several items collapse onto the same change.)

All 29 points have at least one implementing task.

---

## Execution handoff

Plan complete and saved to `docs/superpowers/plans/2026-05-13-loradriver-hardening.md`.

**Two execution options:**

1. **Subagent-Driven (recommended)** — I dispatch a fresh subagent per
   task, review between tasks, fast iteration. Same approach as v1.0
   rewrite that just finished.
2. **Inline Execution** — Execute tasks in this session using
   executing-plans, batch execution with checkpoints.

Which approach?

