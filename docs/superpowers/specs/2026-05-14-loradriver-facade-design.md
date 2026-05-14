# LoRaDriver Facade API — Design Spec

**Date:** 2026-05-14
**Target version:** 1.3.0
**Status:** Approved (pending user review)

## Motivation

Today, the minimal ESP32 + pump-task setup with LoRaDriver is ~40 lines.
Comparison reference: `sandeepmistry/arduino-LoRa` does the equivalent in
~5 lines via a `LoRa` singleton, at the cost of a non-testable, non-multi-
instance design.

The goal is to bring our user-facing line count down to ~10–12 lines
without sacrificing the architectural strengths that distinguish this
driver (DI, testability, multi-instance, no heap, noexcept).

The brief, in user words:

> "Je veux simplifier le code à utiliser, éviter si possible de devoir
> faire ceci par exemple ou de faire des multiple include
> `g_trx.start_receive(true); attachInterrupt(...); g_pump.start(...)`"

Concrete pain points identified from `examples/Esp32WithPumpTask/` and
from the real-world `SYNC-SIGNAL-LORA/lora_handler.cpp`:

1. Five `#include` lines for the namespaced headers.
2. Four globals to declare (`spi`, `drv`, `trx`, `pump`).
3. Manual ISR shim that routes DIO0 → `driver.handle_interrupt()` +
   `pump.notify_from_isr()` via a static `instance_` pointer.
4. Manual `SPI.begin(sck, miso, mosi)` + reset GPIO toggle.
5. Manual `attachInterrupt(...)` + `pump.start(trx, 2, 2, 2048, 1)` with
   obscure positional parameters.
6. Manual `start_receive(true)` to enter RX mode.

All six are mechanical glue that the library can encapsulate.

## Non-goals

- **No singleton lock-in.** The direct DI API
  (`Esp32SpiDevice` + `SX127xDriver` + `LoRaTransceiver` + `RadioPumpTask`)
  remains first-class and unchanged. The facade is strictly additive.
- **No multi-instance support in the facade itself.** Multi-instance
  goes through the direct API (`examples/MultiInstance/`). This
  matches the trade-off chosen against the registry approach (Option 2
  in brainstorming): the singleton is simpler, smaller, and 95 % of
  users only need one radio.
- **No stream-like API** (`write`/`read`/`available`/`print`). Separate
  spec.
- **No `peek()` on the receive buffer.** Separate spec.

## High-level architecture

```
┌──────────────────────────────────────────────────────────────────┐
│  loradriver::LoRa  (facade, singleton)                           │
│                                                                  │
│   begin(cfg)      ────────────────┐                              │
│   end()                           │ wraps                        │
│   send / send_async               │                              │
│   on_receive / on_event           ▼                              │
│   ...                                                            │
│                  ┌─────────────────────────────────────────────┐ │
│                  │  Esp32SpiDevice (or ArduinoSpiDevice)       │ │
│                  │  SX127xDriver                               │ │
│                  │  LoRaTransceiver                            │ │
│                  │  RadioPumpTask          (ESP32 only)        │ │
│                  └─────────────────────────────────────────────┘ │
│                                                                  │
│   transceiver()  ────────────────► escape hatch to direct API    │
│   driver()                                                       │
│   pump()                                                         │
└──────────────────────────────────────────────────────────────────┘

extern LoRa lora;  // global instance, defined in src/api/lora_facade.cpp
```

The facade owns all four sub-objects as direct members. It is not a
pointer-to-impl; layout is fully known at compile time. Cost: ~440 B in
`.bss` for the single global instance.

## Public surface

### `LoRaConfig` additions

```cpp
struct LoRaConfig {
    // ... (all existing fields unchanged)

    /// @brief Optional SPI bus pin override. All -1 → SPI.begin().
    /// Any pin set → SPI.begin(sck, miso, mosi).
    struct SpiPins {
        std::int8_t sck  = -1;
        std::int8_t miso = -1;
        std::int8_t mosi = -1;
    };
    SpiPins spi_pins;

    /// @brief FreeRTOS pump task tuning (ESP32). Defaults match the
    /// values previously hardcoded in pump_.start(trx, 2, 2, 2048, 1).
    struct PumpConfig {
        std::uint32_t period_ms       = 2;
        std::uint8_t  priority        = 2;
        std::uint32_t stack_words     = 2048;
        std::int8_t   core_id         = 1;
        std::uint8_t  tx_queue_depth  = 4;
        std::uint32_t stop_timeout_ms = 1000;
    };
    PumpConfig pump;

    /// @brief Skip the implicit start_receive(true) at the end of
    /// LoRa::begin(). For sender-only sketches.
    bool auto_start_receive_disabled = false;

    /// @brief ESP32 only: skip the implicit RadioPumpTask::start() at
    /// the end of LoRa::begin(). For polling-only sketches.
    bool auto_pump_disabled = false;

    // ===== Presets (constexpr, zero runtime cost) =====

    [[nodiscard]] static constexpr LoRaConfig esp32_sx1276_868mhz(
        std::int8_t cs, std::int8_t rst, std::int8_t dio0) noexcept;

    [[nodiscard]] static constexpr LoRaConfig esp32_sx1278_433mhz(
        std::int8_t cs, std::int8_t rst, std::int8_t dio0) noexcept;

    [[nodiscard]] static constexpr LoRaConfig arduino_sx1276_868mhz(
        std::int8_t cs, std::int8_t rst, std::int8_t dio0) noexcept;

    [[nodiscard]] static constexpr LoRaConfig arduino_sx1278_433mhz(
        std::int8_t cs, std::int8_t rst, std::int8_t dio0) noexcept;
};
```

Preset defaults:

| Preset | Chip | freq_hz | SF | BW | CR | sync | tx_pwr | pa_output |
|---|---|---|---|---|---|---|---|---|
| `esp32_sx1276_868mhz` | SX1276 | 868'000'000 | 9 | 125 kHz | 4/5 | 0x12 | 14 | PaBoost |
| `esp32_sx1278_433mhz` | SX1278 | 433'920'000 | 9 | 125 kHz | 4/5 | 0x12 | 14 | PaBoost |
| `arduino_sx1276_868mhz` | SX1276 | 868'000'000 | 9 | 125 kHz | 4/5 | 0x12 | 14 | PaBoost |
| `arduino_sx1278_433mhz` | SX1278 | 433'920'000 | 9 | 125 kHz | 4/5 | 0x12 | 14 | PaBoost |

The ESP32 / Arduino split today is identical in fields. It exists to
let us diverge later (e.g. enable `tcxo_enabled` for known ESP32 LoRa
boards, set `spi_frequency_hz` differently for AVR's slower SPI) without
breaking the named-preset API.

### Class `LoRa`

```cpp
namespace loradriver {

class LoRa {
public:
    // === Lifecycle ===
    [[nodiscard]] LoRaError begin(const LoRaConfig& cfg) noexcept;
    void                    end() noexcept;
    [[nodiscard]] bool      is_running() const noexcept;

    // === Send ===
    [[nodiscard]] LoRaError send(const std::uint8_t* data, std::size_t len,
                                 std::uint32_t timeout_ms = 2000) noexcept;
    [[nodiscard]] bool      send_async(const std::uint8_t* data,
                                       std::uint8_t len) noexcept;

    // === Receive control (optional — auto by default) ===
    [[nodiscard]] LoRaError start_receive(bool continuous = true) noexcept;
    [[nodiscard]] LoRaError set_standby() noexcept;
    [[nodiscard]] LoRaError set_sleep() noexcept;
    [[nodiscard]] LoRaError start_cad(bool auto_rx = false) noexcept;

    // === Callbacks ===
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

    // === Escape hatches to direct API ===
    [[nodiscard]] LoRaTransceiver&     transceiver() noexcept;
    [[nodiscard]] chips::SX127xDriver& driver() noexcept;
#ifdef ARDUINO_ARCH_ESP32
    [[nodiscard]] platform::esp32::RadioPumpTask& pump() noexcept;
#endif

    LoRa() noexcept;
    ~LoRa();
    LoRa(const LoRa&) = delete;
    LoRa& operator=(const LoRa&) = delete;

private:
    friend void IRAM_ATTR loradriver_isr_dio0_trampoline();

#ifdef ARDUINO_ARCH_ESP32
    hal::Esp32SpiDevice            spi_;
    chips::SX127xDriver            drv_{spi_};
    LoRaTransceiver                trx_{drv_};
    platform::esp32::RadioPumpTask pump_;
#else
    hal::ArduinoSpiDevice          spi_;
    chips::SX127xDriver            drv_{spi_};
    LoRaTransceiver                trx_{drv_};
#endif

    bool        running_ = false;
    std::int8_t attached_dio0_ = -1;

    static LoRa* instance_;
};

/// Global facade instance. Use this directly for the common case;
/// drop down to LoRaTransceiver for multi-instance or custom HAL.
extern LoRa lora;

} // namespace loradriver
```

## `begin(cfg)` sequence

Abort-on-error. State left clean on failure (no half-init).

1. `if (running_)` → return `AlreadyInitialized`.
2. Validate `cfg.validate()`. If fail → return `InvalidConfig`.
3. `instance_ = this`.
4. SPI bus init:
   - If `cfg.spi_pins.sck >= 0 || .miso >= 0 || .mosi >= 0`:
     `SPI.begin(spi_pins.sck, spi_pins.miso, spi_pins.mosi)`.
   - Else: `SPI.begin()`.
5. `spi_.bind(SPI, cfg.pin_ss, cfg.spi_frequency_hz)`
   (deferred init — see "HAL changes" below).
6. `err = trx_.begin(cfg)`. If err → `instance_ = nullptr`; return err.
7. If `cfg.pin_dio0 >= 0`:
   `attachInterrupt(digitalPinToInterrupt(cfg.pin_dio0),
                    &loradriver_isr_dio0_trampoline, RISING)`.
   Set `attached_dio0_ = cfg.pin_dio0`.
8. On ESP32, if `!cfg.auto_pump_disabled`:
   `pump_.start(trx_, cfg.pump.period_ms, cfg.pump.priority,
                cfg.pump.stack_words, cfg.pump.core_id,
                cfg.pump.tx_queue_depth, cfg.pump.stop_timeout_ms)`.
9. If `!cfg.auto_start_receive_disabled`:
   `trx_.start_receive(true)`.
10. `running_ = true`. Return `OK`.

## `end()` sequence

Idempotent. Safe to call without prior `begin()`.

1. If `!running_`, return.
2. On ESP32, if pump was started, `pump_.stop()`.
3. `trx_.end()` (clears callbacks, puts chip to sleep, FSM → `Uninit`).
4. If `attached_dio0_ >= 0`:
   `detachInterrupt(digitalPinToInterrupt(attached_dio0_))`;
   `attached_dio0_ = -1`.
5. `instance_ = nullptr`.
6. `running_ = false`.

The destructor calls `end()`. Static destruction order is fine because
the global `lora` instance is the only one and outlives all references.

## ISR trampoline

```cpp
void IRAM_ATTR loradriver_isr_dio0_trampoline() {
    if (LoRa::instance_) {
        LoRa::instance_->drv_.handle_interrupt();
#ifdef ARDUINO_ARCH_ESP32
        LoRa::instance_->pump_.notify_from_isr();
#endif
    }
}
```

- Lives in IRAM (`IRAM_ATTR`).
- Single null check; the body is identical to what users currently write
  by hand. After inlining: ~6 asm instructions.
- No-op if `begin()` not yet called or after `end()` — no UB, no crash.

## HAL changes

`Esp32SpiDevice` and `ArduinoSpiDevice` gain a `bind()` method for
deferred init, because the global `LoRa lora` is constructed before
`SPI.begin()` runs in user code.

```cpp
class Esp32SpiDevice : public ISpiDevice {
public:
    Esp32SpiDevice() noexcept = default;  // NEW: default-constructible
    Esp32SpiDevice(SPIClass& spi, std::int8_t cs,
                   std::uint32_t freq_hz = 8'000'000u) noexcept;  // existing

    void bind(SPIClass& spi, std::int8_t cs,
              std::uint32_t freq_hz) noexcept;  // NEW

    // ... rest unchanged
};
```

The existing constructor calls `bind()` internally. Existing user code
keeps working without changes.

## New `LoRaConfig` presets — implementation sketch

```cpp
constexpr LoRaConfig LoRaConfig::esp32_sx1276_868mhz(
    std::int8_t cs, std::int8_t rst, std::int8_t dio0) noexcept
{
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
// ... three more similar
```

`constexpr` lets the compiler fold the preset call into a constant initialization
when used at file scope, leaving zero runtime cost.

## Testing

### Host tests (`tests/host/test_facade_lora.cpp`, new)

A `LoRa` constructor variant injecting `IRadioDriver&` is exposed behind
`LORADRIVER_TESTING` so host tests can replace the inner stack with
`FakeSpiDevice` + `SX127xDriver`. Not compiled for end users.

| Test | Asserts |
|---|---|
| `TestFacadeBeginReturnsOk` | `begin(cfg)` → `OK`, `is_running()` → `true` |
| `TestFacadeBeginTwiceReturnsAlreadyInitialized` | Re-`begin()` returns `AlreadyInitialized` |
| `TestFacadeEndIsIdempotent` | `end()` × 2, then `end()` without prior `begin()`, no crash |
| `TestFacadeAutoStartReceiveDefault` | After `begin()`, transceiver state is `RxContinuous` |
| `TestFacadeAutoStartReceiveDisabled` | With `auto_start_receive_disabled=true`, state stays `Standby` |
| `TestFacadeSendForwardsToTransceiver` | `send(buf, len)` reaches FakeSpi as a FIFO write + Tx mode |
| `TestFacadePresetEsp32Sx1276Has868Mhz` | Preset frequency_hz matches table |
| `TestFacadePresetArduinoSx1278Has433Mhz` | Preset frequency_hz matches table |

What is NOT host-testable and goes to embedded smoke:
- The auto-attach of `attachInterrupt` (no Arduino runtime in host).
- The auto-`pump_.start()` (no FreeRTOS in host).

### Embedded smoke (`tests/embedded/test_smoke/test_main.cpp`, extend)

Add `test_facade_begin_then_send_loopback`:
- Calls `lora.begin(LoRaConfig::esp32_sx1276_868mhz(...))`.
- Registers an `on_tx_done` callback that sets a flag.
- Calls `lora.send_async(...)`.
- Waits up to 1 s; asserts the flag was set.

Validates the full chain: preset → SPI init → reset → register config →
auto-ISR → auto-pump → send_async path → TxDone callback delivery.

## Documentation updates

| File | Action |
|---|---|
| `README.md` | Quick-start switches to the facade (`lora.begin(cfg)`). "Why this driver" gains "Facade API for simple usage, full DI for advanced cases" as a feature. |
| `docs/api.md` | New section "Facade API (`loradriver::lora`)" before "Class hierarchy". Documents singleton, presets, `spi_pins`, escape hatches. Explicitly notes that the direct DI API remains first-class. |
| `USAGE.md` (new) | Task-oriented guide. Facade examples first (~10 lines). Section "Advanced: direct DI" mirrors current usage and links to `MultiInstance/`. |
| `examples/BasicSender/BasicSender.ino` | Rewritten with facade. ~12 lines. |
| `examples/BasicReceiver/BasicReceiver.ino` | Rewritten with facade. ~12 lines. |
| `examples/Esp32WithPumpTask/` → `examples/Esp32Async/` | Renamed + rewritten with facade. ~14 lines. The pump task becomes an implementation detail of the facade. |
| `examples/MultiInstance/MultiInstance.ino` | Unchanged. Header comment added: "This example uses the direct DI API because two radios on one MCU need two distinct object trees; the facade is single-instance." |
| `examples/AdvancedDirectDi/` (optional, new) | Verbatim copy of the old `BasicSender` for users who want the explicit DI pattern (host-testable, HAL-swappable). May be skipped if `MultiInstance` is enough as a reference. |
| `CHANGELOG.md` | Section `## 1.3.0`. See full draft below. |

### CHANGELOG draft

```
## 1.3.0 — YYYY-MM-DD

### Added

- Facade API: `loradriver::LoRa` class + global `loradriver::lora`
  instance. Wraps SPI device + driver + transceiver + (ESP32) pump task
  in a single object. begin() handles SPI.begin(), attachInterrupt for
  DIO0, RadioPumpTask startup, and start_receive(true) automatically.
  Reduces minimal ESP32 setup from ~40 lines to ~12 lines.
- LoRaConfig presets: esp32_sx1276_868mhz(), esp32_sx1278_433mhz(),
  arduino_sx1276_868mhz(), arduino_sx1278_433mhz().
- LoRaConfig::SpiPins for custom SPI bus pinouts.
- LoRaConfig::PumpConfig for pump task tuning, with defaults matching
  the prior hardcoded values.
- LoRaConfig flags auto_start_receive_disabled and auto_pump_disabled.
- src/loradriver/lora.hpp + src/api/lora_facade.cpp.

### Changed

- Esp32SpiDevice / ArduinoSpiDevice: added default constructor and
  bind() for deferred init. Existing constructors continue to work.
- BasicSender, BasicReceiver, Esp32WithPumpTask (renamed Esp32Async)
  rewritten with the facade. MultiInstance unchanged.

### Notes

- The direct DI API remains first-class and unchanged. The facade is
  strictly additive. Multi-instance, host tests with FakeSpiDevice,
  and custom HAL injection still go through the direct API.
```

## Release plan

1. Branch `feat/facade-api` off `main`.
2. Granular commits:
   1. `feat(hal): add bind() and default ctor to Esp32SpiDevice/ArduinoSpiDevice`
   2. `feat(config): add LoRaConfig::SpiPins, PumpConfig, auto flags`
   3. `feat(config): add 4 named presets (esp32/arduino × sx1276/sx1278)`
   4. `feat: introduce loradriver::LoRa facade + singleton + ISR trampoline`
   5. `test(host): facade lifecycle, presets, callback forwarding`
   6. `test(embedded): facade begin → send_async → TxDone smoke`
   7. `docs: facade API in README, docs/api.md, new USAGE.md`
   8. `examples: rewrite BasicSender/BasicReceiver/Esp32Async with facade`
   9. `chore: bump version to 1.3.0`
3. PR to `main`, all 6 CI workflows must go green.
4. Tag `v1.3.0` → release workflow publishes the GitHub Release.

## Trade-offs (recap)

| Aspect | Effect |
|---|---|
| User boilerplate (ESP32 + pump) | ~40 → ~12 lines |
| `.bss` cost | ~440 B for the single global instance |
| Flash cost | ~1.5 KB added (facade methods + presets) |
| ISR latency | unchanged (trampoline inlines to the same asm) |
| Multi-instance | unchanged — direct API still supports it |
| Host-testability | unchanged — facade has an injectable test ctor |
| ABI / API back-compat | 100 %, additive only |

## Out of scope (separate specs)

- Stream-like API (`LoRa.beginPacket()` / `write()` / `endPacket()` /
  `available()` / `read()` / `peek()` / `print()`).
- `peek()` on the receive metadata buffer.
- Additional preset boards (TTGO, Heltec specifics) — first see if
  users ask for them.
