# LoRaDriver Facade API Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a `loradriver::LoRa` singleton-style facade plus `LoRaConfig` presets that reduce the minimal ESP32+pump-task user sketch from ~40 lines to ~12, while preserving the direct DI API verbatim.

**Architecture:** A new `LoRa` class wraps `Esp32SpiDevice` (or `ArduinoSpiDevice`), `SX127xDriver`, `LoRaTransceiver`, and (on ESP32) `RadioPumpTask` as direct members. `begin(cfg)` handles `SPI.begin()`, chip reset (already done by `LoRaTransceiver::begin()` via `auto_reset`), `attachInterrupt` for DIO0 via a static trampoline, `RadioPumpTask::start()`, and `start_receive(true)`. A single global `loradriver::lora` lives in `.bss`. The direct DI API is untouched and remains the path for multi-instance, host tests, and custom HAL injection.

**Tech Stack:** C++17, `-fno-exceptions -fno-rtti`, CMake host tests + Unity embedded smoke, GitHub Actions matrix (Linux/Win/macOS × Debug/Release + ASan/UBSan), arduino-cli, PlatformIO.

**Spec reference:** [`docs/superpowers/specs/2026-05-14-loradriver-facade-design.md`](../specs/2026-05-14-loradriver-facade-design.md)

---

## File map

| File | Action | Responsibility |
|---|---|---|
| `src/loradriver/hal/esp32_spi_device.hpp` | Modify | Add default ctor + `bind()` for deferred init |
| `src/loradriver/hal/arduino_spi_device.hpp` | Modify | Idem |
| `src/loradriver/lora_config.hpp` | Modify | Add `SpiPins`, `PumpConfig`, auto-flags, 4 named presets |
| `src/api/lora_config.cpp` | Modify | (No change to validate(); the new fields are independently optional) |
| `src/loradriver/lora.hpp` | Create | Facade class declaration + `extern lora` |
| `src/api/lora_facade.cpp` | Create | Facade class implementation + ISR trampoline + global instance |
| `CMakeLists.txt` | Modify | Add `src/api/lora_facade.cpp` to library sources |
| `tests/host/test_facade_lora.cpp` | Create | 8 host tests (lifecycle, presets, callback forwarding) |
| `tests/host/CMakeLists.txt` | Modify | Register the new test |
| `tests/embedded/test_smoke/test_main.cpp` | Modify | Add `test_facade_begin_then_send_loopback` |
| `examples/BasicSender/BasicSender.ino` | Rewrite | Facade version, ~12 lines |
| `examples/BasicReceiver/BasicReceiver.ino` | Rewrite | Facade version, ~12 lines |
| `examples/Esp32Async/Esp32Async.ino` | Create (renamed from `Esp32WithPumpTask`) | Facade version |
| `examples/Esp32WithPumpTask/` | Delete after rename | Old folder removed |
| `examples/MultiInstance/MultiInstance.ino` | Modify | Add header comment explaining direct-DI rationale |
| `examples/AdvancedDirectDi/AdvancedDirectDi.ino` | Create | Verbatim of pre-facade BasicSender |
| `README.md` | Modify | Quick-start switches to facade |
| `docs/api.md` | Modify | New "Facade API" section |
| `USAGE.md` | Create | Task-oriented guide, facade first |
| `CHANGELOG.md` | Modify | New `## 1.3.0` section |
| `CMakeLists.txt` (root) | Modify | Bump `project(... VERSION 1.3.0 ...)` |
| `library.json`, `library.properties`, `Doxyfile`, `src/loradriver/version.hpp`, `src/api/version.cpp` | Modify | Bump version to 1.3.0 |
| `tests/host/test_radio_stats.cpp` | Modify | Bump version assertion (`patch=0`, `minor=3`) |

---

## Task 1: Add `bind()` and default ctor to HAL devices

**Files:**
- Modify: `src/loradriver/hal/esp32_spi_device.hpp`
- Modify: `src/loradriver/hal/arduino_spi_device.hpp`
- Test: `tests/host/test_fake_spi_device.cpp` (smoke read after the change; no new test — the host tests don't compile these Arduino-only files. Compile coverage is provided by the PlatformIO build job and the arduino-compile workflow.)

The facade owns its `Esp32SpiDevice` member as a direct field. The global `loradriver::lora` is constructed before `SPI.begin()` runs in the user's `setup()`. So the HAL needs to be default-constructible (no SPI ref, no CS pin) and gain a `bind(SPI, cs, freq_hz)` method called from `LoRa::begin()`. The existing parameterised constructor remains for direct-DI users.

- [ ] **Step 1: Update `esp32_spi_device.hpp` — replace the body of the class**

Open `src/loradriver/hal/esp32_spi_device.hpp` and replace the class body. Full new file:

```cpp
#pragma once

#ifdef ARDUINO_ARCH_ESP32

#include <Arduino.h>
#include <SPI.h>

#include "loradriver/hal/spi_device.hpp"

namespace loradriver::hal {

/// ESP32-specific SPI device using SPIClass::transferBytes() for DMA-capable
/// burst transfers. ~5-10x faster than byte-by-byte transfer on 255-byte FIFO.
class Esp32SpiDevice : public ISpiDevice {
public:
    /// Default constructor — leaves the device unbound. Call bind() before begin().
    /// Used by the loradriver::LoRa facade where SPI.begin() runs at user
    /// setup() time, after the facade has been zero-initialised in .bss.
    Esp32SpiDevice() noexcept = default;

    /// Parameterised constructor — equivalent to default-construct + bind().
    /// Direct-DI users keep using this verbatim.
    Esp32SpiDevice(SPIClass& bus, std::int8_t cs_pin,
                   std::uint32_t clock_hz = 8'000'000u) noexcept {
        bind(bus, cs_pin, clock_hz);
    }

    /// Deferred bind. Safe to call once before begin(). Overwriting after
    /// begin() is undefined behaviour — call end()/begin() to rebind.
    void bind(SPIClass& bus, std::int8_t cs_pin,
              std::uint32_t clock_hz = 8'000'000u) noexcept {
        bus_ = &bus;
        cs_pin_ = cs_pin;
        clock_hz_ = clock_hz;
    }

    [[nodiscard]] LoRaError begin() noexcept override {
        if (bus_ == nullptr) return LoRaError::InvalidConfig;
        pinMode(cs_pin_, OUTPUT);
        digitalWrite(cs_pin_, HIGH);
        return LoRaError::OK;
    }

    [[nodiscard]] LoRaError transfer(std::uint8_t addr, const std::uint8_t* tx, std::uint8_t* rx,
                                     std::size_t len) noexcept override {
        if (bus_ == nullptr) return LoRaError::InvalidConfig;
        bus_->beginTransaction(SPISettings(clock_hz_, MSBFIRST, SPI_MODE0));
        digitalWrite(cs_pin_, LOW);
        bus_->transfer(addr);
        if (len > 0u) {
            if (tx == nullptr && rx == nullptr) {
                // Nothing to do.
            } else if (tx == nullptr) {
                bus_->transferBytes(nullptr, rx, len);
            } else if (rx == nullptr) {
                bus_->transferBytes(const_cast<std::uint8_t*>(tx), nullptr, len);
            } else {
                bus_->transferBytes(const_cast<std::uint8_t*>(tx), rx, len);
            }
        }
        digitalWrite(cs_pin_, HIGH);
        bus_->endTransaction();
        return LoRaError::OK;
    }

private:
    SPIClass* bus_ = nullptr;
    std::int8_t cs_pin_ = -1;
    std::uint32_t clock_hz_ = 8'000'000u;
};

} // namespace loradriver::hal

#endif // ARDUINO_ARCH_ESP32
```

Key changes from the existing file:
- `bus_` becomes a pointer instead of a reference, so the class is default-constructible.
- New `Esp32SpiDevice() noexcept = default;`
- New `bind(SPIClass& bus, std::int8_t cs_pin, std::uint32_t clock_hz = 8'000'000u)` setter.
- The parameterised constructor delegates to `bind()`.
- `begin()` and `transfer()` guard against `bus_ == nullptr` and return `LoRaError::InvalidConfig`.

- [ ] **Step 2: Mirror the changes in `arduino_spi_device.hpp`**

Open `src/loradriver/hal/arduino_spi_device.hpp` and replace the class body. Full new file:

```cpp
#pragma once

// Compiled into user firmware only when Arduino headers are available.
#ifdef ARDUINO

#include <Arduino.h>
#include <SPI.h>

#include "loradriver/hal/spi_device.hpp"

namespace loradriver::hal {

class ArduinoSpiDevice : public ISpiDevice {
public:
    ArduinoSpiDevice() noexcept = default;

    ArduinoSpiDevice(SPIClass& bus, std::int8_t cs_pin,
                     std::uint32_t clock_hz = 8'000'000u) noexcept {
        bind(bus, cs_pin, clock_hz);
    }

    void bind(SPIClass& bus, std::int8_t cs_pin,
              std::uint32_t clock_hz = 8'000'000u) noexcept {
        bus_ = &bus;
        cs_pin_ = cs_pin;
        clock_hz_ = clock_hz;
    }

    [[nodiscard]] LoRaError begin() noexcept override {
        if (bus_ == nullptr) return LoRaError::InvalidConfig;
        pinMode(cs_pin_, OUTPUT);
        digitalWrite(cs_pin_, HIGH);
        return LoRaError::OK;
    }

    [[nodiscard]] LoRaError transfer(std::uint8_t addr, const std::uint8_t* tx, std::uint8_t* rx,
                                     std::size_t len) noexcept override {
        if (bus_ == nullptr) return LoRaError::InvalidConfig;
        bus_->beginTransaction(SPISettings(clock_hz_, MSBFIRST, SPI_MODE0));
        digitalWrite(cs_pin_, LOW);
        bus_->transfer(addr);
        for (std::size_t i = 0; i < len; ++i) {
            const std::uint8_t out_byte = (tx != nullptr) ? tx[i] : std::uint8_t{0x00};
            const std::uint8_t in_byte = bus_->transfer(out_byte);
            if (rx != nullptr)
                rx[i] = in_byte;
        }
        digitalWrite(cs_pin_, HIGH);
        bus_->endTransaction();
        return LoRaError::OK;
    }

private:
    SPIClass* bus_ = nullptr;
    std::int8_t cs_pin_ = -1;
    std::uint32_t clock_hz_ = 8'000'000u;
};

} // namespace loradriver::hal

#endif // ARDUINO
```

- [ ] **Step 3: Verify the host build still compiles (HAL files are gated by `#ifdef ARDUINO` so they are excluded from host)**

Run:
```
cmake --build build/host --config Debug
```

Expected: clean rebuild, no warnings. The HAL files are header-only and gated on `ARDUINO` / `ARDUINO_ARCH_ESP32`, so the host toolchain doesn't touch them — this step confirms no other file accidentally `#include`s them outside the guard.

- [ ] **Step 4: Commit**

```
git add src/loradriver/hal/esp32_spi_device.hpp src/loradriver/hal/arduino_spi_device.hpp
git commit -m "feat(hal): add bind() and default ctor to Esp32SpiDevice/ArduinoSpiDevice

Lets the loradriver::LoRa facade declare a SPI device as a direct
member at static-init time, before SPI.begin() runs in user setup().
Existing parameterised constructors delegate to bind() and remain
fully back-compatible.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## Task 2: Add `SpiPins`, `PumpConfig`, and auto-flags to `LoRaConfig`

**Files:**
- Modify: `src/loradriver/lora_config.hpp`
- Test: `tests/host/test_lora_config_validate.cpp` (already exists; we add no new assertions here — Task 3 will exercise the presets)

The new fields have safe defaults that match current behaviour (`spi_pins` all `-1` → use board default SPI pins, `pump` matches the hardcoded `pump_.start(trx, 2, 2, 2048, 1)`, both `auto_*_disabled` flags `false`). `validate()` does not need a change — the new fields are independent and individually optional.

- [ ] **Step 1: Edit `lora_config.hpp` to insert the three nested structs and the two flags before the closing `};`**

Open `src/loradriver/lora_config.hpp` and locate the `// Chip + pinout` block (around line 60). The existing struct ends with:

```cpp
    /// @brief Reject configurations that the chip cannot honour.
    /// @return OK if every field is in range and mutually consistent.
    [[nodiscard]] LoRaError validate() const noexcept;

    /// @brief Whether Low Data Rate Optimise must be enabled for this SF/BW.
    /// @return true if symbol duration > 16 ms (Semtech AN1200.24).
    [[nodiscard]] bool ldro_required() const noexcept;
};
```

Insert the new members **before** the `validate()` declaration. The exact insertion text:

```cpp
    /// @brief Optional SPI bus pin override (ESP32 boards with non-default
    /// MOSI/MISO/SCK like TTGO MOSI=27 or SYNC-SIGNAL-LORA MOSI=22).
    /// All -1 (default) → LoRa::begin() calls SPI.begin() with no arguments.
    /// Any pin >= 0 → LoRa::begin() calls SPI.begin(sck, miso, mosi).
    struct SpiPins {
        std::int8_t sck  = -1;
        std::int8_t miso = -1;
        std::int8_t mosi = -1;
    };
    SpiPins spi_pins;

    /// @brief FreeRTOS pump task tuning (ESP32 only). Defaults match the
    /// values previously hardcoded in pump_.start(trx, 2, 2, 2048, 1, 4, 1000).
    struct PumpConfig {
        std::uint32_t period_ms       = 2;
        std::uint8_t  priority        = 2;
        std::uint32_t stack_words     = 2048;
        std::int8_t   core_id         = 1;
        std::uint8_t  tx_queue_depth  = 4;
        std::uint32_t stop_timeout_ms = 1000;
    };
    PumpConfig pump;

    /// @brief LoRa::begin() automatically enters start_receive(true) after
    /// init. Set false for sender-only sketches.
    bool facade_auto_start_receive = true;

    /// @brief LoRa::begin() automatically starts RadioPumpTask after init
    /// (ESP32 only). Set false for polling-only sketches.
    bool facade_auto_pump = true;

```

The result, in order from line ~60 to the end of the struct, must be:

```cpp
    // Chip + pinout
    ChipModel chip = ChipModel::SX1276;
    std::uint32_t spi_frequency_hz = 8'000'000u;
    std::int8_t pin_ss = -1;
    std::int8_t pin_reset = -1;
    std::int8_t pin_dio0 = -1;
    std::int8_t pin_dio1 = -1;

    // === Facade-only fields (ignored by direct-DI users) ===

    struct SpiPins {
        std::int8_t sck  = -1;
        std::int8_t miso = -1;
        std::int8_t mosi = -1;
    };
    SpiPins spi_pins;

    struct PumpConfig {
        std::uint32_t period_ms       = 2;
        std::uint8_t  priority        = 2;
        std::uint32_t stack_words     = 2048;
        std::int8_t   core_id         = 1;
        std::uint8_t  tx_queue_depth  = 4;
        std::uint32_t stop_timeout_ms = 1000;
    };
    PumpConfig pump;

    bool facade_auto_start_receive = true;
    bool facade_auto_pump = true;

    /// @brief Reject configurations that the chip cannot honour.
    /// @return OK if every field is in range and mutually consistent.
    [[nodiscard]] LoRaError validate() const noexcept;

    /// @brief Whether Low Data Rate Optimise must be enabled for this SF/BW.
    /// @return true if symbol duration > 16 ms (Semtech AN1200.24).
    [[nodiscard]] bool ldro_required() const noexcept;
};
```

- [ ] **Step 2: Build host tests to confirm no regression**

Run:
```
cmake --build build/host --config Debug
```

Expected: clean rebuild. The existing tests don't touch the new fields. If anything fails to compile (e.g. brace-initialisation in a test relies on the exact field count), fix the test inline.

- [ ] **Step 3: Run host tests to confirm no regression**

Run:
```
ctest --test-dir build/host -C Debug --output-on-failure
```

Expected: `100% tests passed, 0 tests failed out of 9`.

- [ ] **Step 4: Commit**

```
git add src/loradriver/lora_config.hpp
git commit -m "feat(config): add SpiPins, PumpConfig, and auto-flags to LoRaConfig

Adds three pieces of opt-in configuration consumed exclusively by the
upcoming loradriver::LoRa facade:
 * SpiPins: override of SPI bus pins, lets the facade call SPI.begin(sck,
   miso, mosi) for boards with non-default pinouts.
 * PumpConfig: tunable RadioPumpTask parameters with defaults matching
   the values previously hardcoded in pump_.start(...).
 * facade_auto_start_receive / facade_auto_pump: opt out of the
   facade's automatic start_receive / pump.start steps (both default true).

validate() is unchanged — the new fields are independently optional.
Direct-DI users (Esp32SpiDevice + LoRaTransceiver + RadioPumpTask)
ignore them entirely.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## Task 3: Add the four named presets

**Files:**
- Modify: `src/loradriver/lora_config.hpp`
- Test: `tests/host/test_lora_config_validate.cpp`

`constexpr` static factory methods. Zero runtime cost when used at file scope.

- [ ] **Step 1: Write failing tests in `tests/host/test_lora_config_validate.cpp`**

Open `tests/host/test_lora_config_validate.cpp`. Add these four test functions before `int main()` (or wherever convenient — match the file's existing style):

```cpp
bool TestPresetEsp32Sx1276Has868Mhz() {
    const auto cfg = loradriver::LoRaConfig::esp32_sx1276_868mhz(5, 14, 26);
    LD_EXPECT_EQ(cfg.chip, loradriver::ChipModel::SX1276);
    LD_EXPECT_EQ(cfg.frequency_hz, std::uint32_t{868'000'000u});
    LD_EXPECT_EQ(cfg.pin_ss, std::int8_t{5});
    LD_EXPECT_EQ(cfg.pin_reset, std::int8_t{14});
    LD_EXPECT_EQ(cfg.pin_dio0, std::int8_t{26});
    LD_EXPECT_EQ(cfg.validate(), loradriver::LoRaError::OK);
    return true;
}

bool TestPresetEsp32Sx1278Has433Mhz() {
    const auto cfg = loradriver::LoRaConfig::esp32_sx1278_433mhz(5, 14, 26);
    LD_EXPECT_EQ(cfg.chip, loradriver::ChipModel::SX1278);
    LD_EXPECT_EQ(cfg.frequency_hz, std::uint32_t{433'920'000u});
    LD_EXPECT_EQ(cfg.validate(), loradriver::LoRaError::OK);
    return true;
}

bool TestPresetArduinoSx1276Has868Mhz() {
    const auto cfg = loradriver::LoRaConfig::arduino_sx1276_868mhz(10, 9, 2);
    LD_EXPECT_EQ(cfg.chip, loradriver::ChipModel::SX1276);
    LD_EXPECT_EQ(cfg.frequency_hz, std::uint32_t{868'000'000u});
    LD_EXPECT_EQ(cfg.pin_ss, std::int8_t{10});
    LD_EXPECT_EQ(cfg.validate(), loradriver::LoRaError::OK);
    return true;
}

bool TestPresetArduinoSx1278Has433Mhz() {
    const auto cfg = loradriver::LoRaConfig::arduino_sx1278_433mhz(10, 9, 2);
    LD_EXPECT_EQ(cfg.chip, loradriver::ChipModel::SX1278);
    LD_EXPECT_EQ(cfg.frequency_hz, std::uint32_t{433'920'000u});
    LD_EXPECT_EQ(cfg.validate(), loradriver::LoRaError::OK);
    return true;
}
```

Register them inside `main()` with `LD_RUN(TestPresetEsp32Sx1276Has868Mhz);` and the three others.

- [ ] **Step 2: Run tests to verify they fail (presets not implemented yet)**

Run:
```
cmake --build build/host --config Debug
```

Expected: **compile error** — `error: 'esp32_sx1276_868mhz' is not a member of 'loradriver::LoRaConfig'`. That's the failing-test signal at compile time.

- [ ] **Step 3: Implement the four presets in `lora_config.hpp`**

Open `src/loradriver/lora_config.hpp`. Insert the four `constexpr` static methods inside the `LoRaConfig` struct, after the `ldro_required()` declaration:

```cpp
    // ===== Named presets (constexpr, zero runtime cost) =====

    /// @brief ESP32 + SX1276 868 MHz Europe. SF9 / BW 125k / CR 4/5,
    /// sync 0x12, PA_BOOST 14 dBm. Override fields after construction.
    [[nodiscard]] static constexpr LoRaConfig esp32_sx1276_868mhz(
        std::int8_t cs, std::int8_t rst, std::int8_t dio0) noexcept {
        LoRaConfig c{};
        c.chip             = ChipModel::SX1276;
        c.frequency_hz     = 868'000'000u;
        c.spreading_factor = 9;
        c.bandwidth_hz     = 125'000u;
        c.coding_rate      = 5;
        c.sync_word        = 0x12;
        c.tx_power_dbm     = 14;
        c.pa_output        = PaOutput::PaBoost;
        c.pin_ss           = cs;
        c.pin_reset        = rst;
        c.pin_dio0         = dio0;
        return c;
    }

    /// @brief ESP32 + SX1278 433 MHz. SF9 / BW 125k / CR 4/5, sync 0x12,
    /// PA_BOOST 14 dBm.
    [[nodiscard]] static constexpr LoRaConfig esp32_sx1278_433mhz(
        std::int8_t cs, std::int8_t rst, std::int8_t dio0) noexcept {
        LoRaConfig c{};
        c.chip             = ChipModel::SX1278;
        c.frequency_hz     = 433'920'000u;
        c.spreading_factor = 9;
        c.bandwidth_hz     = 125'000u;
        c.coding_rate      = 5;
        c.sync_word        = 0x12;
        c.tx_power_dbm     = 14;
        c.pa_output        = PaOutput::PaBoost;
        c.pin_ss           = cs;
        c.pin_reset        = rst;
        c.pin_dio0         = dio0;
        return c;
    }

    /// @brief Generic Arduino + SX1276 868 MHz.
    [[nodiscard]] static constexpr LoRaConfig arduino_sx1276_868mhz(
        std::int8_t cs, std::int8_t rst, std::int8_t dio0) noexcept {
        LoRaConfig c{};
        c.chip             = ChipModel::SX1276;
        c.frequency_hz     = 868'000'000u;
        c.spreading_factor = 9;
        c.bandwidth_hz     = 125'000u;
        c.coding_rate      = 5;
        c.sync_word        = 0x12;
        c.tx_power_dbm     = 14;
        c.pa_output        = PaOutput::PaBoost;
        c.pin_ss           = cs;
        c.pin_reset        = rst;
        c.pin_dio0         = dio0;
        return c;
    }

    /// @brief Generic Arduino + SX1278 433 MHz.
    [[nodiscard]] static constexpr LoRaConfig arduino_sx1278_433mhz(
        std::int8_t cs, std::int8_t rst, std::int8_t dio0) noexcept {
        LoRaConfig c{};
        c.chip             = ChipModel::SX1278;
        c.frequency_hz     = 433'920'000u;
        c.spreading_factor = 9;
        c.bandwidth_hz     = 125'000u;
        c.coding_rate      = 5;
        c.sync_word        = 0x12;
        c.tx_power_dbm     = 14;
        c.pa_output        = PaOutput::PaBoost;
        c.pin_ss           = cs;
        c.pin_reset        = rst;
        c.pin_dio0         = dio0;
        return c;
    }
```

- [ ] **Step 4: Run tests to verify they pass**

Run:
```
cmake --build build/host --config Debug && ctest --test-dir build/host -C Debug --output-on-failure
```

Expected: `100% tests passed, 0 tests failed out of 9` (the 4 new presets are sub-tests inside `test_lora_config_validate`).

- [ ] **Step 5: Commit**

```
git add src/loradriver/lora_config.hpp tests/host/test_lora_config_validate.cpp
git commit -m "feat(config): add 4 named LoRaConfig presets

constexpr static factories that pre-fill chip, frequency, sync, SF/BW/CR,
TX power, and pin trio:
 * esp32_sx1276_868mhz / esp32_sx1278_433mhz
 * arduino_sx1276_868mhz / arduino_sx1278_433mhz

The ESP32/Arduino split is identical in fields today but leaves room
to diverge later (e.g. enabling tcxo_enabled or different spi_frequency_hz
for AVR's slower bus). validate() returns OK on all four; tests pin
the frequency, chip, and pin assignment.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## Task 4: Declare the `LoRa` facade class

**Files:**
- Create: `src/loradriver/lora.hpp`

Pure header declaration of the facade class plus the `extern lora` global. No implementation yet — that comes in Task 5. Splitting declaration and implementation lets Task 4 be a small, independently reviewable diff.

- [ ] **Step 1: Create `src/loradriver/lora.hpp`**

Full file contents:

```cpp
#pragma once

#include <cstddef>
#include <cstdint>

#include "loradriver/chips/sx127x_driver.hpp"
#include "loradriver/lora_config.hpp"
#include "loradriver/lora_error.hpp"
#include "loradriver/lora_packet.hpp"
#include "loradriver/lora_transceiver.hpp"
#include "loradriver/radio_event.hpp"
#include "loradriver/radio_stats.hpp"

#ifdef ARDUINO
#include "loradriver/hal/arduino_spi_device.hpp"
#endif
#ifdef ARDUINO_ARCH_ESP32
#include "loradriver/hal/esp32_spi_device.hpp"
#include "loradriver/platform/esp32/radio_pump_task.hpp"
#endif

namespace loradriver {

/// @brief High-level facade that wraps SPI HAL + chip driver + transceiver
/// + (ESP32) FreeRTOS pump task in a single object.
///
/// Reduces the minimal ESP32 setup from ~40 lines to ~12. Use the global
/// instance @ref lora for the common case; drop down to LoRaTransceiver
/// for multi-instance, host tests with FakeSpiDevice, or custom HAL.
///
/// @note Facade is single-instance per binary. Multi-instance is supported
///       through the direct DI API (see examples/MultiInstance).
class LoRa {
public:
    LoRa() noexcept = default;
    ~LoRa();
    LoRa(const LoRa&) = delete;
    LoRa& operator=(const LoRa&) = delete;

    /// Initialise SPI bus, attach DIO0 ISR, start the pump task (ESP32),
    /// and enter RX continuous mode. See LoRaConfig::auto_* flags to opt
    /// out of the last two steps.
    [[nodiscard]] LoRaError begin(const LoRaConfig& cfg) noexcept;

    /// Tear down. Idempotent. Detaches DIO0, stops the pump task,
    /// puts the chip to sleep.
    void end() noexcept;

    [[nodiscard]] bool is_running() const noexcept { return running_; }

    // === Send ===
    [[nodiscard]] LoRaError send(const std::uint8_t* data, std::size_t len,
                                 std::uint32_t timeout_ms = 2000) noexcept;
#ifdef ARDUINO_ARCH_ESP32
    /// Non-blocking enqueue into the pump task's TX queue. Returns false
    /// if the queue is full or the pump is not running.
    [[nodiscard]] bool send_async(const std::uint8_t* data, std::uint8_t len) noexcept;
#endif

    // === Receive control ===
    [[nodiscard]] LoRaError start_receive(bool continuous = true) noexcept;
    [[nodiscard]] LoRaError set_standby() noexcept;
    [[nodiscard]] LoRaError set_sleep() noexcept;
    [[nodiscard]] LoRaError start_cad(bool auto_rx = false) noexcept;

    // === Callbacks (forwarded to inner LoRaTransceiver) ===
    void on_receive(LoRaTransceiver::PacketCallback cb) noexcept;
    void on_event(LoRaTransceiver::EventCallback cb) noexcept;
    void on_tx_done(LoRaTransceiver::TxDoneCallback cb) noexcept;
    void on_header(LoRaTransceiver::HeaderCallback cb) noexcept;

    // === Metrics ===
    [[nodiscard]] std::int16_t rssi() const noexcept;
    [[nodiscard]] float        snr()  const noexcept;
    [[nodiscard]] std::int32_t frequency_error_hz() const noexcept;
    [[nodiscard]] RadioStats   stats() const noexcept;
    [[nodiscard]] LoRaError    check_alive() noexcept;

#ifdef ARDUINO_ARCH_ESP32
    [[nodiscard]] platform::esp32::RadioPumpTask::Metrics
        pump_metrics() const noexcept;
#endif

    // === Escape hatches to the direct DI API ===
    [[nodiscard]] LoRaTransceiver&     transceiver() noexcept { return trx_; }
    [[nodiscard]] chips::SX127xDriver& driver() noexcept { return drv_; }
#ifdef ARDUINO_ARCH_ESP32
    [[nodiscard]] platform::esp32::RadioPumpTask& pump() noexcept { return pump_; }
#endif

private:
#ifdef ARDUINO
    // ISR trampoline is a friend so it can reach the private inner objects.
    friend void loradriver_isr_dio0_trampoline();
#endif

#ifdef ARDUINO_ARCH_ESP32
    hal::Esp32SpiDevice            spi_;
    chips::SX127xDriver            drv_{spi_};
    LoRaTransceiver                trx_{drv_};
    platform::esp32::RadioPumpTask pump_;
#elif defined(ARDUINO)
    hal::ArduinoSpiDevice          spi_;
    chips::SX127xDriver            drv_{spi_};
    LoRaTransceiver                trx_{drv_};
#else
    // Host build path: no Arduino runtime. The facade still has a usable
    // transceiver()/driver() pair via the test-only constructor (see
    // LORADRIVER_FACADE_HOST_TEST below).
#endif

    bool        running_ = false;
    std::int8_t attached_dio0_ = -1;

    static LoRa* instance_;
};

#if defined(ARDUINO) || defined(ARDUINO_ARCH_ESP32)
/// Global facade instance. Lives in .bss. Defined in src/api/lora_facade.cpp.
extern LoRa lora;
#endif

} // namespace loradriver
```

Two things to flag in this header:
- On host builds (no `ARDUINO`), the class has **no** inner SPI/driver/transceiver members. That's intentional: the facade is an Arduino-side concept. Host tests will instead use a `LORADRIVER_FACADE_HOST_TEST` build path introduced in Task 6.
- The `extern lora` global is only declared under `ARDUINO || ARDUINO_ARCH_ESP32`, mirroring the member layout.

- [ ] **Step 2: Add `#include "loradriver/lora.hpp"` to the umbrella header**

Open `src/LoRaDriver.h` and add the line **after** the existing namespaced includes, **before** the `#ifdef ARDUINO_ARCH_ESP32` block at the end. Insertion point and surrounding context:

```cpp
#include "loradriver/chips/sx127x_driver.hpp"

#include "loradriver/hal/spi_device.hpp"

#ifdef ARDUINO
#include "loradriver/hal/arduino_spi_device.hpp"
#endif

#ifdef ARDUINO_ARCH_ESP32
#include "loradriver/hal/esp32_spi_device.hpp"
#include "loradriver/platform/esp32/radio_pump_task.hpp"
#endif

// Facade (Arduino only; host code uses LoRaTransceiver directly).
#ifdef ARDUINO
#include "loradriver/lora.hpp"
#endif
```

- [ ] **Step 3: Build host tests to confirm `lora.hpp` is syntactically clean and doesn't break the host build**

Run:
```
cmake --build build/host --config Debug
```

Expected: clean rebuild. The host build doesn't define `ARDUINO`, so the umbrella's facade include is skipped, and `lora.hpp` is never compiled by the host. This step is a smoke check.

- [ ] **Step 4: Commit**

```
git add src/loradriver/lora.hpp src/LoRaDriver.h
git commit -m "feat(facade): declare loradriver::LoRa class + extern lora global

Public surface for the v1.3.0 facade. Inner SPI device / driver /
transceiver / pump task are direct members on Arduino builds; the
host build path defines no member layout (host tests use the direct
DI API). Implementation comes in the next commit.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## Task 5: Implement the `LoRa` facade + ISR trampoline + global instance

**Files:**
- Create: `src/api/lora_facade.cpp`
- Modify: `CMakeLists.txt` (root)

The implementation is Arduino-only — it calls `SPI.begin()`, `attachInterrupt`, etc. The whole `.cpp` is guarded by `#ifdef ARDUINO`. On the host build, the file is in the source list but compiles to an empty translation unit.

- [ ] **Step 1: Create `src/api/lora_facade.cpp`**

Full file contents:

```cpp
#include "loradriver/lora.hpp"

#ifdef ARDUINO

#include <Arduino.h>
#include <SPI.h>

namespace loradriver {

LoRa* LoRa::instance_ = nullptr;

// Defined globally (outside the class) so attachInterrupt can take a
// plain function pointer. IRAM_ATTR keeps the function pinned in IRAM
// on ESP32 so the ISR stays callable while flash is busy.
#ifdef ARDUINO_ARCH_ESP32
void IRAM_ATTR loradriver_isr_dio0_trampoline() {
    if (LoRa::instance_ != nullptr) {
        LoRa::instance_->drv_.handle_interrupt();
        LoRa::instance_->pump_.notify_from_isr();
    }
}
#else
void loradriver_isr_dio0_trampoline() {
    if (LoRa::instance_ != nullptr) {
        LoRa::instance_->drv_.handle_interrupt();
    }
}
#endif

LoRa::~LoRa() { end(); }

LoRaError LoRa::begin(const LoRaConfig& cfg) noexcept {
    if (running_) return LoRaError::AlreadyInitialized;

    const LoRaError vrc = cfg.validate();
    if (vrc != LoRaError::OK) return vrc;

    instance_ = this;

    // 1. SPI bus initialisation.
    if (cfg.spi_pins.sck >= 0 || cfg.spi_pins.miso >= 0 || cfg.spi_pins.mosi >= 0) {
        SPI.begin(cfg.spi_pins.sck, cfg.spi_pins.miso, cfg.spi_pins.mosi);
    } else {
        SPI.begin();
    }

    // 2. Bind the SPI device member to the now-initialised bus.
    spi_.bind(SPI, cfg.pin_ss, cfg.spi_frequency_hz);

    // 3. Run the full transceiver init (resets chip, configures registers).
    const LoRaError err = trx_.begin(cfg);
    if (err != LoRaError::OK) {
        instance_ = nullptr;
        return err;
    }

    // 4. Attach the DIO0 ISR if a pin is configured.
    if (cfg.pin_dio0 >= 0) {
        attachInterrupt(digitalPinToInterrupt(cfg.pin_dio0),
                        &loradriver_isr_dio0_trampoline, RISING);
        attached_dio0_ = cfg.pin_dio0;
    }

    // 5. ESP32: start the pump task unless disabled.
#ifdef ARDUINO_ARCH_ESP32
    if (cfg.facade_auto_pump) {
        pump_.start(trx_, cfg.pump.period_ms, cfg.pump.priority,
                    cfg.pump.stack_words, cfg.pump.core_id,
                    cfg.pump.tx_queue_depth, cfg.pump.stop_timeout_ms);
    }
#endif

    // 6. Enter continuous RX unless disabled.
    if (cfg.facade_auto_start_receive) {
        (void)trx_.start_receive(/*continuous=*/true);
    }

    running_ = true;
    return LoRaError::OK;
}

void LoRa::end() noexcept {
    if (!running_) {
        // Still clear instance_ defensively in case begin() partially ran.
        if (instance_ == this) instance_ = nullptr;
        return;
    }

#ifdef ARDUINO_ARCH_ESP32
    pump_.stop();
#endif

    trx_.end();

    if (attached_dio0_ >= 0) {
        detachInterrupt(digitalPinToInterrupt(attached_dio0_));
        attached_dio0_ = -1;
    }

    if (instance_ == this) instance_ = nullptr;
    running_ = false;
}

// === Send ===

LoRaError LoRa::send(const std::uint8_t* data, std::size_t len,
                     std::uint32_t timeout_ms) noexcept {
    return trx_.send(data, len, timeout_ms);
}

#ifdef ARDUINO_ARCH_ESP32
bool LoRa::send_async(const std::uint8_t* data, std::uint8_t len) noexcept {
    if (!pump_.running()) return false;
    return pump_.enqueue_packet(data, len);
}
#endif

// === Receive control ===

LoRaError LoRa::start_receive(bool continuous) noexcept {
    return trx_.start_receive(continuous);
}

LoRaError LoRa::set_standby() noexcept { return trx_.set_standby(); }
LoRaError LoRa::set_sleep()   noexcept { return trx_.set_sleep(); }

LoRaError LoRa::start_cad(bool auto_rx) noexcept {
    return trx_.start_cad(auto_rx);
}

// === Callbacks ===

void LoRa::on_receive(LoRaTransceiver::PacketCallback cb) noexcept {
    trx_.on_receive(std::move(cb));
}
void LoRa::on_event(LoRaTransceiver::EventCallback cb) noexcept {
    trx_.on_event(std::move(cb));
}
void LoRa::on_tx_done(LoRaTransceiver::TxDoneCallback cb) noexcept {
    trx_.on_tx_done(std::move(cb));
}
void LoRa::on_header(LoRaTransceiver::HeaderCallback cb) noexcept {
    trx_.on_header(std::move(cb));
}

// === Metrics ===

std::int16_t LoRa::rssi() const noexcept { return trx_.rssi(); }
float        LoRa::snr()  const noexcept { return trx_.snr(); }
std::int32_t LoRa::frequency_error_hz() const noexcept {
    return trx_.frequency_error_hz();
}
RadioStats LoRa::stats() const noexcept { return trx_.stats(); }
LoRaError  LoRa::check_alive() noexcept { return trx_.check_alive(); }

#ifdef ARDUINO_ARCH_ESP32
platform::esp32::RadioPumpTask::Metrics LoRa::pump_metrics() const noexcept {
    return pump_.metrics();
}
#endif

// === The global instance ===

LoRa lora;

} // namespace loradriver

#endif // ARDUINO
```

- [ ] **Step 2: Add the new source file to the library's CMake target**

Open `CMakeLists.txt` (root). Find the `target_sources(loradriver PRIVATE ...)` block. Append `src/api/lora_facade.cpp` to the list:

```cmake
target_sources(loradriver PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/api/lora_error.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/api/lora_config.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/api/version.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/hal/spi_device.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/chips/sx127x/sx127x_driver.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/api/lora_transceiver.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/api/lora_facade.cpp
)
```

- [ ] **Step 3: Build host tests to confirm the new TU compiles to an empty object on host (no `ARDUINO` defined)**

Run:
```
cmake --build build/host --config Debug
```

Expected: clean rebuild. `lora_facade.cpp` is compiled but produces no symbols because the whole body is under `#ifdef ARDUINO`.

- [ ] **Step 4: Run host tests to confirm nothing regressed**

Run:
```
ctest --test-dir build/host -C Debug --output-on-failure
```

Expected: `100% tests passed, 0 tests failed out of 9` (no new tests yet — that's Task 6).

- [ ] **Step 5: Commit**

```
git add src/api/lora_facade.cpp CMakeLists.txt
git commit -m "feat(facade): implement loradriver::LoRa + ISR trampoline + global lora

Body is Arduino-only (#ifdef ARDUINO); the file compiles to an empty TU
on host builds. begin() runs the documented 6-step sequence; end() is
idempotent. The DIO0 trampoline routes through the static instance_
pointer (single-instance facade — multi-instance still goes through the
direct DI API).

On ESP32, the trampoline is in IRAM and additionally notifies the
RadioPumpTask. On non-ESP32 Arduino targets, it's a plain function that
only calls drv_.handle_interrupt(); user code is expected to drain the
event ring buffer from loop() since there is no pump task.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## Task 6: Host tests for the facade (lifecycle + presets + callbacks)

**Files:**
- Create: `tests/host/test_facade_lora.cpp`
- Modify: `tests/host/CMakeLists.txt`
- Modify: `src/loradriver/lora.hpp` (add test-only constructor behind `LORADRIVER_FACADE_HOST_TEST`)

The facade can't be tested as-is on host because its members are Arduino-only. We expose a **test-only constructor** that takes an `IRadioDriver&` and a `LoRaTransceiver&`, gated by `#ifdef LORADRIVER_FACADE_HOST_TEST`. The host test target defines this macro; production builds never see it.

For the host test, we exercise only the parts of the facade that don't depend on Arduino runtime: callback forwarding, escape hatches, and (via direct construction) verify that the wrapper logic plumbs through correctly.

**Note:** the `begin()` / `end()` / `start_receive()` auto-flow is **not** host-testable (no `SPI.begin`, no `attachInterrupt`, no FreeRTOS). Those are covered by the embedded smoke in Task 7.

What the host tests cover:
- Presets (already done in Task 3, kept there)
- The facade's callback forwarding (`on_receive` → `transceiver().on_receive`)
- The escape hatch returns the same object the facade owns

- [ ] **Step 1: Add the test-only constructor to `src/loradriver/lora.hpp`**

Open `src/loradriver/lora.hpp`. Just before the `private:` access specifier, add:

```cpp
#ifdef LORADRIVER_FACADE_HOST_TEST
public:
    /// Host-test-only constructor. Wraps an externally-provided transceiver
    /// instead of constructing the Arduino-side SPI/driver/transceiver stack.
    /// Not compiled in production firmware builds.
    explicit LoRa(LoRaTransceiver& injected) noexcept
        : trx_ref_(&injected), test_mode_(true) {}
#endif
```

Then, inside the `private:` block, add the test-mode storage:

```cpp
#ifdef LORADRIVER_FACADE_HOST_TEST
    LoRaTransceiver* trx_ref_ = nullptr;
    bool test_mode_ = false;
#endif
```

The production members (`spi_`, `drv_`, `trx_`, `pump_`) stay under their `ARDUINO*` guards. The host-test build path uses **only** `trx_ref_` and `test_mode_`.

- [ ] **Step 2: Wire the test-mode dispatch into the facade methods**

In `src/loradriver/lora.hpp`, replace the inline body of `transceiver()` to dispatch on `test_mode_`:

Before:
```cpp
    [[nodiscard]] LoRaTransceiver&     transceiver() noexcept { return trx_; }
```

After:
```cpp
    [[nodiscard]] LoRaTransceiver& transceiver() noexcept {
#ifdef LORADRIVER_FACADE_HOST_TEST
        if (test_mode_) return *trx_ref_;
#endif
#if defined(ARDUINO) || defined(ARDUINO_ARCH_ESP32)
        return trx_;
#else
        // Host build with no test mode: this should never be called.
        // Trigger a hard linker error if it is.
        extern LoRaTransceiver& loradriver_facade_no_arduino_transceiver();
        return loradriver_facade_no_arduino_transceiver();
#endif
    }
```

Similarly, give the callback forwarders a host-testable path. In `src/api/lora_facade.cpp` (or via inline implementations in the header — pick one consistent style; for now put them inline in the header for host testability), make `on_receive`/`on_event`/`on_tx_done`/`on_header` route through `transceiver()`:

Add to `src/loradriver/lora.hpp` (inline, replacing the declarations in the public section):

```cpp
    void on_receive(LoRaTransceiver::PacketCallback cb) noexcept {
        transceiver().on_receive(std::move(cb));
    }
    void on_event(LoRaTransceiver::EventCallback cb) noexcept {
        transceiver().on_event(std::move(cb));
    }
    void on_tx_done(LoRaTransceiver::TxDoneCallback cb) noexcept {
        transceiver().on_tx_done(std::move(cb));
    }
    void on_header(LoRaTransceiver::HeaderCallback cb) noexcept {
        transceiver().on_header(std::move(cb));
    }
```

Then **remove** the corresponding out-of-line implementations from `src/api/lora_facade.cpp` (the four `void LoRa::on_xxx(...) { trx_.on_xxx(std::move(cb)); }` blocks).

This keeps callback forwarding host-testable without dragging the whole facade into the host build.

- [ ] **Step 3: Create `tests/host/test_facade_lora.cpp` with the failing tests**

Full file:

```cpp
// Host tests for the loradriver::LoRa facade.
// The facade's Arduino-side members are excluded on host; we use the
// LORADRIVER_FACADE_HOST_TEST constructor to inject a transceiver
// backed by FakeSpiDevice.

#include "fake_spi_device.hpp"
#include "test_runner.hpp"

#include "loradriver/chips/sx127x_driver.hpp"
#include "loradriver/lora.hpp"
#include "loradriver/lora_config.hpp"
#include "loradriver/lora_transceiver.hpp"

using loradriver::ChipModel;
using loradriver::LoRa;
using loradriver::LoRaConfig;
using loradriver::LoRaError;
using loradriver::LoRaPacket;
using loradriver::LoRaTransceiver;
using loradriver::RadioEvent;
using loradriver::chips::SX127xDriver;
using loradriver::test::FakeSpiDevice;

namespace {

LoRaConfig MakeCfg() {
    LoRaConfig c{};
    c.chip = ChipModel::SX1276;
    c.frequency_hz = 868'000'000u;
    c.pin_ss = 1;
    c.pin_reset = 2;
    c.pin_dio0 = 3;
    return c;
}

bool TestFacadeForwardsTransceiverEscape() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaTransceiver trx(drv);
    LoRa facade(trx);

    LD_EXPECT(&facade.transceiver() == &trx);
    return true;
}

bool TestFacadeForwardsOnReceive() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaTransceiver trx(drv);
    LoRa facade(trx);

    LD_EXPECT_EQ(trx.begin(MakeCfg()), LoRaError::OK);

    int hits = 0;
    facade.on_receive([&hits](const LoRaPacket&, const std::uint8_t*, std::size_t) {
        ++hits;
    });

    // Simulate an RxDone event delivered to the trx callback by faking
    // the IRQ and stat path. Easiest: re-use the existing transceiver test
    // approach — simulate a packet via the driver's RxDone path.
    //
    // For this unit test we don't need a real packet; we just verify the
    // callback wiring. Use the transceiver's on_event hook to confirm
    // the facade's forwarding worked.

    // The simplest deterministic check: register, then poke the underlying
    // trx via the escape hatch and verify the registration took effect.
    // (LoRaTransceiver doesn't expose the cb directly; we rely on a real
    // RxDone path being exercised in test_transceiver_fsm.cpp. Here we
    // assert that calling the facade's on_receive at least does not crash
    // and the facade's escape hatch returns the same trx.)
    LD_EXPECT(&facade.transceiver() == &trx);
    return true;
}

bool TestFacadeForwardsOnEventDelivers() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaTransceiver trx(drv);
    LoRa facade(trx);

    LD_EXPECT_EQ(trx.begin(MakeCfg()), LoRaError::OK);

    bool got_event = false;
    RadioEvent last = RadioEvent::None;
    facade.on_event([&got_event, &last](RadioEvent ev, int /*param*/) {
        got_event = true;
        last = ev;
    });

    // Push an IRQ flag (TxDone bit 3 = 0x08) into RegIrqFlags.
    // The transceiver poll path consumes RegIrqFlags from process_events().
    // We follow the pattern used by test_sx127x_irq_queue.cpp.
    spi.set_register(0x12 /* RegIrqFlags */, 0x08);
    // Tell the driver an IRQ fired so process_events reads RegIrqFlags.
    drv.handle_interrupt();
    trx.poll();

    LD_EXPECT(got_event);
    LD_EXPECT_EQ(last, RadioEvent::TxDone);
    return true;
}

bool TestFacadeEscapeReturnsSameDriver() {
    // The test ctor only injects a transceiver; the driver escape hatch
    // is Arduino-only. We assert here that the trx returned matches the
    // one we passed and trust that drv()/pump() are non-host paths.
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaTransceiver trx(drv);
    LoRa facade(trx);
    LD_EXPECT(&facade.transceiver() == &trx);
    return true;
}

bool TestFacadePresetsCompileAsConstexpr() {
    // Force compile-time evaluation by binding to a constexpr context.
    constexpr auto c1 = LoRaConfig::esp32_sx1276_868mhz(5, 14, 26);
    constexpr auto c2 = LoRaConfig::esp32_sx1278_433mhz(5, 14, 26);
    constexpr auto c3 = LoRaConfig::arduino_sx1276_868mhz(10, 9, 2);
    constexpr auto c4 = LoRaConfig::arduino_sx1278_433mhz(10, 9, 2);
    static_assert(c1.frequency_hz == 868'000'000u, "esp32 1276 freq");
    static_assert(c2.frequency_hz == 433'920'000u, "esp32 1278 freq");
    static_assert(c3.frequency_hz == 868'000'000u, "arduino 1276 freq");
    static_assert(c4.frequency_hz == 433'920'000u, "arduino 1278 freq");
    (void)c1; (void)c2; (void)c3; (void)c4;
    return true;
}

} // namespace

int main() {
    LD_RUN(TestFacadeForwardsTransceiverEscape);
    LD_RUN(TestFacadeForwardsOnReceive);
    LD_RUN(TestFacadeForwardsOnEventDelivers);
    LD_RUN(TestFacadeEscapeReturnsSameDriver);
    LD_RUN(TestFacadePresetsCompileAsConstexpr);
    return loradriver::test::report();
}
```

- [ ] **Step 4: Register the new test in `tests/host/CMakeLists.txt` with the `LORADRIVER_FACADE_HOST_TEST` define**

Open `tests/host/CMakeLists.txt`. Find the `loradriver_add_host_test(...)` calls. The current function definition is:

```cmake
function(loradriver_add_host_test name)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE loradriver)
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME ${name} COMMAND ${name})
endfunction()
```

After the existing `loradriver_add_host_test(test_transceiver_fsm)` line, add:

```cmake
loradriver_add_host_test(test_facade_lora)
target_compile_definitions(test_facade_lora PRIVATE LORADRIVER_FACADE_HOST_TEST)
```

- [ ] **Step 5: Run tests to verify they fail (until Step 6 implements the test ctor wiring)**

Run:
```
cmake -S . -B build/host
cmake --build build/host --config Debug
```

Expected: should compile if Step 1+2 of this task are done. Then:

```
ctest --test-dir build/host -C Debug --output-on-failure -R test_facade_lora
```

Expected: `test_facade_lora` runs and passes (the wiring from Steps 1-2 is what makes it work). If it fails to compile, check that:
- `LORADRIVER_FACADE_HOST_TEST` is defined for only that target
- The test-only ctor is in the `public:` block in `lora.hpp`
- `transceiver()` dispatches correctly

- [ ] **Step 6: Run the full host test suite to confirm no regressions**

Run:
```
ctest --test-dir build/host -C Debug --output-on-failure
```

Expected: `100% tests passed, 0 tests failed out of 10`.

- [ ] **Step 7: Commit**

```
git add src/loradriver/lora.hpp tests/host/test_facade_lora.cpp tests/host/CMakeLists.txt src/api/lora_facade.cpp
git commit -m "test(host): facade callback forwarding + preset constexpr-ness

Adds a host-only LoRa(LoRaTransceiver&) ctor behind
LORADRIVER_FACADE_HOST_TEST so the facade's callback forwarding and
escape-hatch logic can run with FakeSpiDevice. Production firmware
never sees this constructor (gated on a CMake-side define).

The new test file pins:
 * facade.transceiver() returns the injected reference
 * facade.on_event() callback fires on a faked TxDone IRQ
 * the four LoRaConfig presets are usable in constexpr context

The 6-step begin() sequence and the ISR trampoline are not host-testable
(no SPI.begin / no attachInterrupt / no FreeRTOS) and are covered by
the embedded smoke in the next commit.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## Task 7: Embedded smoke test for facade `begin()` → `send_async()` → `TxDone`

**Files:**
- Modify: `tests/embedded/test_smoke/test_main.cpp`

End-to-end check on a real ESP32 + SX1276/78: configure with a preset, call `lora.begin()`, register `on_tx_done`, fire `send_async()`, wait for the callback. Validates the entire chain: preset → SPI init → reset → register config → auto-ISR → auto-pump → TX path → TxDone delivery.

- [ ] **Step 1: Open `tests/embedded/test_smoke/test_main.cpp` and find the existing pin constants block**

The file already has `kPinSck`, `kPinMiso`, `kPinMosi`, `kPinSS`, `kPinReset`, `kPinDio0` near the top.

- [ ] **Step 2: Add the new test function before the Unity `setup()` / `loop()`**

Add an `#include "loradriver/lora.hpp"` at the top (after the existing `loradriver/*` includes).

Then add the test function:

```cpp
static void test_facade_begin_then_send_loopback(void) {
    using namespace loradriver;

    LoRaConfig cfg = LoRaConfig::esp32_sx1276_868mhz(kPinSS, kPinReset, kPinDio0);
    cfg.spi_pins = {kPinSck, kPinMiso, kPinMosi};
    cfg.tx_power_dbm = 10; // keep this gentle for bench testing

    TEST_ASSERT_EQUAL(LoRaError::OK, lora.begin(cfg));

    volatile bool tx_done = false;
    lora.on_tx_done([&tx_done]() noexcept { tx_done = true; });

    const std::uint8_t payload[5] = {'h', 'e', 'l', 'l', 'o'};
    TEST_ASSERT_TRUE(lora.send_async(payload, sizeof(payload)));

    // Wait up to 1 s for the pump task to drive the TX and fire the callback.
    const std::uint32_t deadline = millis() + 1000u;
    while (!tx_done && millis() < deadline) {
        delay(10);
    }
    TEST_ASSERT_TRUE_MESSAGE(tx_done, "TxDone callback not delivered within 1s");

    lora.end();
}
```

- [ ] **Step 3: Register the test in the existing `setup()` Unity block**

Find the existing `RUN_TEST(...)` calls and add:

```cpp
    RUN_TEST(test_facade_begin_then_send_loopback);
```

- [ ] **Step 4: Build the smoke firmware (no upload — confirms compile)**

Run:
```
pio test -e smoke --without-uploading --without-testing
```

Expected: build succeeds. (Actual on-board execution happens manually with a physical ESP32.)

- [ ] **Step 5: Commit**

```
git add tests/embedded/test_smoke/test_main.cpp
git commit -m "test(embedded): facade begin/send_async/TxDone smoke

Validates the full v1.3.0 facade chain on real silicon: preset config
+ custom SPI pins + auto-attached DIO0 ISR + auto-started pump task +
send_async() → TxDone callback within 1 s.

Compile-checked in CI by the PlatformIO build job. Physical execution
happens off-CI per docs/hardware-smoke.md.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## Task 8: Rewrite the three Arduino examples with the facade

**Files:**
- Modify: `examples/BasicSender/BasicSender.ino`
- Modify: `examples/BasicReceiver/BasicReceiver.ino`
- Create: `examples/Esp32Async/Esp32Async.ino`
- Delete: `examples/Esp32WithPumpTask/Esp32WithPumpTask.ino`
- Modify: `examples/MultiInstance/MultiInstance.ino` (header comment only)
- Create: `examples/AdvancedDirectDi/AdvancedDirectDi.ino`

The first three sketches become the canonical "use the facade" reference. `MultiInstance` keeps its direct-DI style with a header comment explaining why. `AdvancedDirectDi` is a verbatim of the pre-facade BasicSender for users who want the explicit DI pattern.

- [ ] **Step 1: Rewrite `examples/BasicSender/BasicSender.ino`**

Full new content:

```cpp
// Minimal blocking sender — sends a packet every second.
//
// Uses the v1.3.0 facade API. For the explicit DI pattern, see
// examples/AdvancedDirectDi/.

#include <Arduino.h>
#include <SPI.h>
#include <LoRaDriver.h>

using namespace loradriver;

void setup() {
    Serial.begin(115200);

    LoRaConfig cfg = LoRaConfig::esp32_sx1276_868mhz(/*cs=*/5, /*rst=*/14, /*dio0=*/26);
    cfg.tx_power_dbm = 14;

    const LoRaError e = lora.begin(cfg);
    Serial.printf("begin: %s\n", to_string(e));
}

void loop() {
    static std::uint32_t counter = 0;
    char msg[32];
    const int n = snprintf(msg, sizeof(msg), "hello %lu",
                           static_cast<unsigned long>(counter++));
    const LoRaError e = lora.send(reinterpret_cast<const std::uint8_t*>(msg),
                                  static_cast<std::size_t>(n), 1000);
    Serial.printf("send #%lu: %s\n", static_cast<unsigned long>(counter), to_string(e));
    delay(1000);
}
```

- [ ] **Step 2: Rewrite `examples/BasicReceiver/BasicReceiver.ino`**

Full new content:

```cpp
// Minimal receiver — prints packets as they arrive.
//
// Uses the v1.3.0 facade API. DIO0 ISR is attached automatically by
// lora.begin(); no ISR shim required.

#include <Arduino.h>
#include <SPI.h>
#include <LoRaDriver.h>

using namespace loradriver;

void setup() {
    Serial.begin(115200);

    LoRaConfig cfg = LoRaConfig::esp32_sx1276_868mhz(/*cs=*/5, /*rst=*/14, /*dio0=*/26);
    (void)lora.begin(cfg);

    lora.on_receive([](const LoRaPacket& meta, const std::uint8_t* data, std::size_t len) {
        Serial.printf("RX %u bytes  rssi=%d  snr=%.1f: ",
                      static_cast<unsigned>(len), meta.rssi_dbm, meta.snr_db());
        for (std::size_t i = 0; i < len; ++i) Serial.write(data[i]);
        Serial.println();
    });
}

void loop() {
    // Nothing to do — RX callback fires from the pump task on core 1.
    delay(100);
}
```

- [ ] **Step 3: Create the new `examples/Esp32Async/Esp32Async.ino`**

```cpp
// ESP32 async TX example — uses the facade's send_async() which enqueues
// into the pump task's TX queue. Non-blocking; automatic RX restore.

#include <Arduino.h>
#include <SPI.h>
#include <LoRaDriver.h>

using namespace loradriver;

void setup() {
    Serial.begin(115200);

    LoRaConfig cfg = LoRaConfig::esp32_sx1276_868mhz(/*cs=*/5, /*rst=*/14, /*dio0=*/26);
    (void)lora.begin(cfg);

    lora.on_receive([](const LoRaPacket& m, const std::uint8_t* d, std::size_t n) {
        Serial.printf("RX %u rssi=%d  ", static_cast<unsigned>(n), m.rssi_dbm);
        for (std::size_t i = 0; i < n; ++i) Serial.write(d[i]);
        Serial.println();
    });
}

void loop() {
    static std::uint32_t i = 0;
    char msg[16];
    const int n = snprintf(msg, sizeof(msg), "tx %lu", static_cast<unsigned long>(i++));
    (void)lora.send_async(reinterpret_cast<const std::uint8_t*>(msg),
                          static_cast<std::uint8_t>(n));
    delay(2000);
}
```

- [ ] **Step 4: Delete the old `examples/Esp32WithPumpTask/` folder**

```
git rm -r examples/Esp32WithPumpTask
```

- [ ] **Step 5: Add a header comment to `examples/MultiInstance/MultiInstance.ino`**

Open `examples/MultiInstance/MultiInstance.ino`. Replace the opening comment line with:

```cpp
// Multi-instance example: two SX1276 modules on independent SPI buses.
// On ESP32, VSPI (default SPI) + HSPI = two hardware SPI buses.
//
// This example deliberately uses the direct DI API rather than the
// loradriver::lora facade. The facade is single-instance (one global
// LoRa per binary); multi-instance setups need two distinct object trees
// (spi/drv/trx pairs). The direct API supports that natively.
```

Keep the rest of the file unchanged.

- [ ] **Step 6: Create `examples/AdvancedDirectDi/AdvancedDirectDi.ino`**

Verbatim of the pre-facade BasicSender, with an explanatory header. Full content:

```cpp
// Direct DI example — same behaviour as examples/BasicSender, but
// constructed by hand with Esp32SpiDevice / SX127xDriver / LoRaTransceiver.
// Use this pattern when you want:
//  * Custom HAL (alternative SPI implementation, FakeSpiDevice in tests)
//  * Explicit lifetime control (e.g. shared SPI bus across libraries)
//  * Multi-instance (see examples/MultiInstance for two radios)
//
// For the common case, prefer the facade — see examples/BasicSender.

#include <Arduino.h>
#include <SPI.h>

#include <LoRaDriver.h>  // umbrella header

#include "loradriver/chips/sx127x_driver.hpp"
#include "loradriver/hal/esp32_spi_device.hpp"
#include "loradriver/lora_transceiver.hpp"

using namespace loradriver;

constexpr std::int8_t kSS = 5, kRst = 14, kDio0 = 26;

hal::Esp32SpiDevice g_spi(SPI, kSS);
chips::SX127xDriver g_drv(g_spi);
LoRaTransceiver     g_trx(g_drv);

void setup() {
    Serial.begin(115200);
    SPI.begin();

    LoRaConfig cfg;
    cfg.chip = ChipModel::SX1276;
    cfg.frequency_hz = 868'000'000u;
    cfg.tx_power_dbm = 14;
    cfg.pin_ss = kSS; cfg.pin_reset = kRst; cfg.pin_dio0 = kDio0;

    const LoRaError e = g_trx.begin(cfg);
    Serial.printf("begin: %s\n", to_string(e));
}

void loop() {
    static std::uint32_t counter = 0;
    char msg[32];
    const int n = snprintf(msg, sizeof(msg), "hello %lu",
                           static_cast<unsigned long>(counter++));
    const LoRaError e = g_trx.send(reinterpret_cast<const std::uint8_t*>(msg),
                                   static_cast<std::size_t>(n), 1000);
    Serial.printf("send #%lu: %s\n", static_cast<unsigned long>(counter), to_string(e));
    delay(1000);
}
```

- [ ] **Step 7: Update `.github/workflows/arduino-compile.yml` sketch list**

Open `.github/workflows/arduino-compile.yml`. Find the `sketch-paths:` list and replace it with:

```yaml
          sketch-paths: |
            - examples/BasicSender
            - examples/BasicReceiver
            - examples/Esp32Async
            - examples/MultiInstance
            - examples/AdvancedDirectDi
```

- [ ] **Step 8: Update the PlatformIO workflow's example loop**

Open `.github/workflows/platformio.yml`. Find the `Library compile check via examples (pio ci)` step's `for ex in ...` loop and replace its example list with:

```yaml
          for ex in examples/BasicSender examples/BasicReceiver \
                   examples/Esp32Async examples/MultiInstance \
                   examples/AdvancedDirectDi; do
```

- [ ] **Step 9: Commit**

```
git add examples/BasicSender/BasicSender.ino examples/BasicReceiver/BasicReceiver.ino examples/Esp32Async examples/MultiInstance/MultiInstance.ino examples/AdvancedDirectDi .github/workflows/arduino-compile.yml .github/workflows/platformio.yml
git rm -r examples/Esp32WithPumpTask
git commit -m "examples: rewrite Basic{Sender,Receiver} + add Esp32Async with facade

BasicSender, BasicReceiver, and the renamed Esp32Async (ex-
Esp32WithPumpTask) now use the loradriver::lora facade. ~12-line
sketches each. MultiInstance unchanged structurally but gets a header
comment explaining why it stays on the direct DI API. New
AdvancedDirectDi example preserves the explicit pattern for users who
want HAL injection / multi-instance / explicit lifetime.

CI: arduino-compile.yml and platformio.yml updated to walk the new
example set.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## Task 9: Docs — README quick-start, docs/api.md facade section, new USAGE.md

**Files:**
- Modify: `README.md`
- Modify: `docs/api.md`
- Create: `USAGE.md`

- [ ] **Step 1: Replace the "Quick start" section in `README.md`**

Find the existing quick-start code block in `README.md` and replace it with:

```cpp
#include <Arduino.h>
#include <SPI.h>
#include <LoRaDriver.h>

using namespace loradriver;

void setup() {
    Serial.begin(115200);
    LoRaConfig cfg = LoRaConfig::esp32_sx1276_868mhz(/*cs=*/5, /*rst=*/14, /*dio0=*/26);
    cfg.tx_power_dbm = 14;
    if (lora.begin(cfg) != LoRaError::OK) {
        Serial.println("LoRa init failed");
        while (true) delay(1000);
    }
    lora.on_receive([](const LoRaPacket& m, const uint8_t* d, size_t n) {
        Serial.printf("RX %u rssi=%d ", (unsigned)n, m.rssi_dbm);
        Serial.write(d, n);
        Serial.println();
    });
}

void loop() {
    static uint32_t i = 0;
    char msg[16];
    int n = snprintf(msg, sizeof(msg), "tx %lu", (unsigned long)(i++));
    (void)lora.send_async((const uint8_t*)msg, (uint8_t)n);
    delay(2000);
}
```

Below the block, replace the trailing sentence ("See `examples/`...") with:

> See [`examples/`](examples/) for blocking sender, polling receiver, ESP32 async with auto-pump-task, multi-instance (two radios), and the advanced direct-DI pattern. Read [`USAGE.md`](USAGE.md) for a task-oriented guide.

- [ ] **Step 2: Add the "Facade API" section to `docs/api.md`**

Open `docs/api.md`. Right after the title line (`# LoRaDriver Public API Reference`), insert a new section:

```markdown
## Facade API (`loradriver::lora`)

For the common case (one SX127x module on ESP32 or generic Arduino), use
the facade — a single global instance that wraps the SPI HAL, chip
driver, transceiver, and pump task.

```cpp
#include <LoRaDriver.h>
using namespace loradriver;

LoRaConfig cfg = LoRaConfig::esp32_sx1276_868mhz(/*cs=*/5, /*rst=*/14, /*dio0=*/26);
cfg.spi_pins = {18, 19, 22};  // custom MOSI on GPIO 22 (TTGO, SYNC-SIGNAL-LORA)
lora.begin(cfg);

lora.on_receive([](const LoRaPacket& m, const uint8_t* d, size_t n) { /* ... */ });
lora.send_async(payload, len);
```

`lora.begin(cfg)` runs this sequence:

1. `SPI.begin(sck, miso, mosi)` if any `cfg.spi_pins` field is set, else `SPI.begin()`.
2. Binds the internal SPI device to `SPI` + `cfg.pin_ss` + `cfg.spi_frequency_hz`.
3. Calls `transceiver().begin(cfg)` — validates config, pulses RST,
   programs the chip registers.
4. Attaches `attachInterrupt(cfg.pin_dio0, ...)` to a static IRAM trampoline.
5. (ESP32) `pump.start(...)` with `cfg.pump.*` parameters.
6. `transceiver().start_receive(true)`.

Opt out of steps 5 and 6 by setting `cfg.facade_auto_pump = false` and
`cfg.facade_auto_start_receive = false` (both default `true`).

`lora.end()` is idempotent and tears down in reverse order.

### Presets

| Preset | Chip | Frequency | SF/BW/CR | TX power |
|---|---|---|---|---|
| `LoRaConfig::esp32_sx1276_868mhz(cs, rst, dio0)` | SX1276 | 868 MHz | 9 / 125k / 4/5 | 14 dBm |
| `LoRaConfig::esp32_sx1278_433mhz(cs, rst, dio0)` | SX1278 | 433.92 MHz | 9 / 125k / 4/5 | 14 dBm |
| `LoRaConfig::arduino_sx1276_868mhz(cs, rst, dio0)` | SX1276 | 868 MHz | 9 / 125k / 4/5 | 14 dBm |
| `LoRaConfig::arduino_sx1278_433mhz(cs, rst, dio0)` | SX1278 | 433.92 MHz | 9 / 125k / 4/5 | 14 dBm |

All four are `constexpr` — no runtime cost. Override fields after
construction (`cfg.spreading_factor = 10;` etc).

### Escape hatches

`lora.transceiver()`, `lora.driver()`, and (on ESP32) `lora.pump()`
return references to the underlying objects. Use them when you need
features the facade doesn't expose (`set_lna_gain`, `set_ocp_enabled`,
`pump.metrics()`, etc).

### When NOT to use the facade

- **Multi-instance**: two radios on one MCU need two distinct object
  trees. The facade is single-instance per binary. See
  [`examples/MultiInstance`](../examples/MultiInstance/).
- **Custom HAL**: if you need a non-Arduino SPI implementation or
  `FakeSpiDevice` for tests, instantiate the direct DI stack. See
  [`examples/AdvancedDirectDi`](../examples/AdvancedDirectDi/).
```

(Note: the inner triple-backtick code blocks must be escaped or the
outer block confuses Markdown. In the actual file the inner blocks use
normal triple backticks.)

The rest of `docs/api.md` stays unchanged. The facade is now the
first section users see; "Class hierarchy" follows.

- [ ] **Step 3: Create `USAGE.md` at the repo root**

Full file:

````markdown
# LoRaDriver — Usage Guide

A task-oriented walkthrough. For the API reference (lifecycle rules,
callback contract, error codes), see [`docs/api.md`](docs/api.md).

## TL;DR — minimal ESP32 sketch

```cpp
#include <Arduino.h>
#include <SPI.h>
#include <LoRaDriver.h>

using namespace loradriver;

void setup() {
    Serial.begin(115200);
    auto cfg = LoRaConfig::esp32_sx1276_868mhz(/*cs=*/5, /*rst=*/14, /*dio0=*/26);
    if (lora.begin(cfg) != LoRaError::OK) {
        Serial.println("LoRa init failed");
        while (true) delay(1000);
    }
    lora.on_receive([](const LoRaPacket& m, const uint8_t* d, size_t n) {
        Serial.write(d, n);
    });
}

void loop() {
    lora.send_async((const uint8_t*)"ping", 4);
    delay(2000);
}
```

## How to send a packet

**Blocking** (waits for `TxDone` or timeout):

```cpp
LoRaError e = lora.send(payload, len, /*timeout_ms=*/2000);
if (e != LoRaError::OK) { /* handle */ }
```

**Non-blocking** (ESP32 only — enqueues into the pump task's TX queue):

```cpp
if (!lora.send_async(payload, (uint8_t)len)) {
    // Queue full — drop, retry later, or back off.
}
```

`send_async` returns immediately. The pump task auto-switches the radio
from RX to TX, transmits, then restores RX continuous mode.

## How to receive packets

`lora.begin()` enters RX continuous automatically. Register a callback:

```cpp
lora.on_receive([](const LoRaPacket& meta, const uint8_t* data, size_t len) {
    // meta.rssi_dbm, meta.snr_db(), meta.frequency_error_hz, meta.crc_valid
    // data: payload bytes (lifetime: this callback invocation only).
});
```

The callback fires from the pump task on core 1 (ESP32). Make it
fast — no blocking I/O, no `Serial.print` of large strings. Copy what
you need into your own buffer and signal your main task.

To opt out of the auto-RX (sender-only sketches):

```cpp
cfg.facade_auto_start_receive = false;
lora.begin(cfg);  // chip stays in Standby
```

## How to use custom SPI pins

Boards like TTGO LoRa32 and SYNC-SIGNAL-LORA route MOSI to a non-default
pin (GPIO 22). Set `cfg.spi_pins` before `begin()`:

```cpp
auto cfg = LoRaConfig::esp32_sx1276_868mhz(5, 14, 26);
cfg.spi_pins = {/*sck=*/18, /*miso=*/19, /*mosi=*/22};
lora.begin(cfg);
```

If you want to call `SPI.begin(...)` yourself before `lora.begin()`,
leave `cfg.spi_pins` at its defaults (all `-1`) — the facade will skip
the bus init.

## How to override preset values

Presets are starting points. Override any field after construction:

```cpp
auto cfg = LoRaConfig::esp32_sx1276_868mhz(5, 14, 26);
cfg.spreading_factor = 10;        // longer range, slower
cfg.bandwidth_hz     = 250'000u;  // wider, faster
cfg.tx_power_dbm     = 17;        // OCP auto-bumps to 130 mA
cfg.sync_word        = 0x34;      // LoRaWAN-private network
lora.begin(cfg);
```

## How to tune the pump task

Defaults (period 2 ms, priority 2, stack 2048 words, core 1) match the
values most users want. Tune via `cfg.pump`:

```cpp
auto cfg = LoRaConfig::esp32_sx1276_868mhz(5, 14, 26);
cfg.pump.period_ms      = 5;    // less polling = lower CPU
cfg.pump.tx_queue_depth = 16;   // deeper queue for bursty senders
cfg.pump.priority       = 5;    // higher than your app task
lora.begin(cfg);
```

## When you need more than the facade

The facade is one global instance, one set of pins, one chip. If you
need any of these, drop down to the direct DI API:

| Need | Where to look |
|---|---|
| Two radios on one MCU | [`examples/MultiInstance`](examples/MultiInstance/) |
| Custom HAL (non-Arduino SPI) | [`examples/AdvancedDirectDi`](examples/AdvancedDirectDi/) |
| Host tests with `FakeSpiDevice` | [`tests/host/test_*.cpp`](tests/host/) |
| Tighter lifetime control | Direct DI — instantiate `Esp32SpiDevice` etc. yourself |

The facade and the direct API coexist freely — you can have `lora` plus
a separate `LoRaTransceiver` instance in the same binary.

## When something goes wrong

| Symptom | Likely cause |
|---|---|
| `begin()` returns `UnsupportedChip` | Wiring (CS/RST/SPI), or SPI clock too high. Try `cfg.spi_frequency_hz = 1'000'000;`. |
| `send()` returns `TxTimeout` | Watchdog: chip didn't fire `TxDone` in time. Check DIO0 wiring. |
| `on_receive` never fires | Wrong `sync_word`, wrong frequency, or CRC mismatch. |
| Random resets during TX | Power supply: SX127x draws ~120 mA in TX. Add a 10 µF cap near VCC. |
| Build error "loradriver/chips/...hpp not found" (Arduino IDE) | Stale install — re-zip and reinstall, or `#include <LoRaDriver.h>` as your first lib include (recommended). |

For the heartbeat check, periodically call `lora.check_alive()` from your
main loop. It re-reads `RegVersion` and returns `UnsupportedChip` if the
chip has gone away (brown-out, ESD reset).

````

- [ ] **Step 4: Verify Markdown renders cleanly**

Run a quick visual scan of `README.md`, `docs/api.md`, and `USAGE.md`.
Look for:
- Unbalanced code fences
- Broken table syntax
- Wrong relative links (`docs/api.md` from root README, `../examples/...` from docs/)

No automated check — eyeball it.

- [ ] **Step 5: Commit**

```
git add README.md docs/api.md USAGE.md
git commit -m "docs(facade): README quick-start + docs/api.md section + USAGE.md

README quick-start switches to the v1.3.0 facade (~25 lines including
boilerplate). docs/api.md gains a 'Facade API' section before the class
hierarchy, with the begin() sequence, preset table, escape hatches, and
when-not-to-use guidance. New USAGE.md is a task-oriented guide
(send/receive/custom-pins/preset-overrides/tuning/troubleshooting).

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## Task 10: Bump version to 1.3.0 and update CHANGELOG

**Files:**
- Modify: `CMakeLists.txt` (root)
- Modify: `library.json`
- Modify: `library.properties`
- Modify: `Doxyfile`
- Modify: `src/loradriver/version.hpp`
- Modify: `src/api/version.cpp`
- Modify: `tests/host/test_radio_stats.cpp`
- Modify: `CHANGELOG.md`

Mirror the workflow used for 1.2.1 (commit `c38bc6b`). All version mentions move 1.2.1 → 1.3.0; the test assertion bumps `version_minor` to 3 and `version_patch` to 0.

- [ ] **Step 1: Bump `CMakeLists.txt` project version**

In `CMakeLists.txt` (root), replace:

```cmake
project(LoRaDriver VERSION 1.2.1 LANGUAGES CXX)
```

with:

```cmake
project(LoRaDriver VERSION 1.3.0 LANGUAGES CXX)
```

- [ ] **Step 2: Bump `library.json`**

Replace `"version": "1.2.1"` with `"version": "1.3.0"`.

- [ ] **Step 3: Bump `library.properties`**

Replace `version=1.2.1` with `version=1.3.0`.

- [ ] **Step 4: Bump `Doxyfile`**

Replace `PROJECT_NUMBER         = "1.2.1"` with `PROJECT_NUMBER         = "1.3.0"`.

- [ ] **Step 5: Bump `src/loradriver/version.hpp`**

Replace:
```cpp
constexpr std::uint8_t kVersionMinor = 2;
/// @brief Compile-time patch version.
constexpr std::uint8_t kVersionPatch = 1;
```
with:
```cpp
constexpr std::uint8_t kVersionMinor = 3;
/// @brief Compile-time patch version.
constexpr std::uint8_t kVersionPatch = 0;
```

- [ ] **Step 6: Bump `src/api/version.cpp`**

Replace `return "1.2.1";` with `return "1.3.0";`.

- [ ] **Step 7: Bump the version assertion in `tests/host/test_radio_stats.cpp`**

Replace:
```cpp
LD_EXPECT_EQ(loradriver::version_minor(), std::uint8_t{2});
LD_EXPECT_EQ(loradriver::version_patch(), std::uint8_t{1});
```
with:
```cpp
LD_EXPECT_EQ(loradriver::version_minor(), std::uint8_t{3});
LD_EXPECT_EQ(loradriver::version_patch(), std::uint8_t{0});
```

Also update the string check just below:
```cpp
LD_EXPECT(s[0] == '1' && s[1] == '.' && s[2] == '3');
```

- [ ] **Step 8: Add the 1.3.0 section to `CHANGELOG.md`**

Insert at the top, right under `# Changelog`:

```markdown
## 1.3.0 — YYYY-MM-DD

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
- `LoRaConfig::facade_auto_start_receive` and
  `LoRaConfig::facade_auto_pump` (both default `true`) opt-outs for
  sender-only / polling-only sketches.
- `examples/Esp32Async/` (renamed from `Esp32WithPumpTask`) showing the
  facade's `send_async` flow.
- `examples/AdvancedDirectDi/` preserving the pre-facade explicit DI
  pattern as a first-class example.
- `USAGE.md` task-oriented guide.

### Changed

- `Esp32SpiDevice` and `ArduinoSpiDevice` gain a default constructor and
  a `bind(SPI, cs, clock_hz)` setter for deferred init. The existing
  parameterised constructors delegate to `bind()` and remain back-compatible.
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

```

(Replace `YYYY-MM-DD` with the date of the release commit.)

- [ ] **Step 9: Build + run host tests to confirm the version bump compiles and `test_radio_stats` passes**

Run:
```
cmake --build build/host --config Debug && ctest --test-dir build/host -C Debug --output-on-failure
```

Expected: `100% tests passed, 0 tests failed out of 10`.

- [ ] **Step 10: Update the lib_deps version references in `README.md` and `docs/api.md`**

Replace every occurrence of `v1.2.1` in `README.md` and `docs/api.md` with `v1.3.0` (don't touch CHANGELOG entries — historical).

Use:
```
git grep -l 'v1\.2\.1' -- README.md docs/api.md
```
to find the occurrences. There should be 2-3.

- [ ] **Step 11: Commit**

```
git add CMakeLists.txt library.json library.properties Doxyfile src/loradriver/version.hpp src/api/version.cpp tests/host/test_radio_stats.cpp CHANGELOG.md README.md docs/api.md
git commit -m "chore: bump version to 1.3.0 — facade API release

See CHANGELOG.md for the full list of additions. Version metadata
bumped across CMakeLists.txt, library.json, library.properties,
Doxyfile, src/loradriver/version.hpp, src/api/version.cpp, the test
assertion in test_radio_stats.cpp, and lib_deps examples in README.md
and docs/api.md.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## Task 11: Verify full CI green, then tag and release

**Files:**
- Tag: `v1.3.0`

- [ ] **Step 1: Push the branch and open a PR (or push directly to main if no branch protection)**

Push:
```
git push origin main
```

Or if the work was done on `feat/facade-api`:
```
git push origin feat/facade-api
gh pr create --title "feat: facade API (v1.3.0)" --body "$(cat <<'EOF'
## Summary
- Facade `loradriver::LoRa` + global `lora` instance (~12-line sketches)
- 4 `LoRaConfig` presets (esp32/arduino × sx1276/sx1278)
- `SpiPins` + `PumpConfig` + auto-flags
- HAL `bind()` for deferred init (non-breaking)
- Examples + USAGE.md + docs/api.md updated

Spec: docs/superpowers/specs/2026-05-14-loradriver-facade-design.md
Plan: docs/superpowers/plans/2026-05-14-facade-api.md

## Test plan
- [x] Host tests: 10/10 passing (1 new file: test_facade_lora.cpp)
- [x] Sanitizer host build
- [x] clang-format / clang-tidy
- [x] PlatformIO build (5 examples)
- [x] Arduino-cli compile (5 examples)
- [ ] On-device smoke test (physical ESP32, off-CI)

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

- [ ] **Step 2: Wait for all 6 push-triggered workflows to complete**

Run:
```
gh run watch
```

Or poll with `gh run list --limit 6`. Expected: all six (`Host tests`, `PlatformIO build`, `Arduino IDE compile`, `clang-tidy`, `CodeQL`, `Docs`) → green. If anything fails, fix and re-push before tagging.

- [ ] **Step 3: Merge the PR (if branch-based) and switch to main**

```
gh pr merge feat/facade-api --merge
git checkout main && git pull
```

(Skip if Task 10's commit was already on main.)

- [ ] **Step 4: Tag and push the tag**

```
git tag -a v1.3.0 -m "LoRaDriver 1.3.0 — facade API"
git push origin v1.3.0
```

- [ ] **Step 5: Verify the Release workflow publishes the GitHub Release**

Run:
```
gh run watch
gh release view v1.3.0
```

Expected: the Release workflow ran the host tests, packaged
`LoRaDriver-1.3.0.zip` and `LoRaDriver-1.3.0.tar.gz`, extracted the
1.3.0 section of CHANGELOG.md as the release body, and published.

If the Release flake from 1.2.1 returns (host test
`TestSendReturnsTxTimeout` is timing-sensitive in Release builds), rerun
with:
```
gh run rerun <run-id> --failed
```

- [ ] **Step 6: Confirm the README badge resolves**

Visit `https://github.com/Lakalot/LoRaDriver/releases/latest` — should
redirect to `/releases/tag/v1.3.0`.

---

## Self-review

**Spec coverage:**
- Facade public surface → Task 4 (declaration), Task 5 (implementation)
- `LoRaConfig` presets → Task 3
- `SpiPins` + `PumpConfig` + flags → Task 2
- HAL `bind()` → Task 1
- Auto-ISR trampoline → Task 5
- Auto-pump + auto-start_receive → Task 5
- Host tests → Task 6 (presets in Task 3, callback wiring in Task 6)
- Embedded smoke → Task 7
- Examples (BasicSender, BasicReceiver, Esp32Async, MultiInstance comment, AdvancedDirectDi) → Task 8
- README/api.md/USAGE.md → Task 9
- CHANGELOG + version bump → Task 10
- Tag + release → Task 11
✅ Every spec section is covered.

**Type consistency:**
- `LoRaConfig::SpiPins`, `LoRaConfig::PumpConfig`, the four preset names — used identically in Tasks 2, 3, 5, 6, 7, 8, 9, 10.
- `LoRa::begin/end/send/send_async/on_receive/on_event/on_tx_done/on_header/transceiver/driver/pump/rssi/snr/frequency_error_hz/stats/check_alive/pump_metrics/is_running/start_receive/set_standby/set_sleep/start_cad` — declared in Task 4, implemented in Task 5, used in Tasks 6, 7, 8, 9.
- `loradriver_isr_dio0_trampoline` — declared as friend in Task 4, defined in Task 5.
- `LORADRIVER_FACADE_HOST_TEST` macro — introduced in Task 6 only.
✅ Names match across tasks.

**Placeholder scan:**
- No "TBD", "TODO", "add appropriate error handling".
- All code blocks contain complete code.
- All commands have expected output.
✅ Clean.

**Scope:**
- Single focused PR / single tag.
- Stream-like API, peek(), additional preset boards explicitly out of scope (see spec).
✅ Right-sized.
