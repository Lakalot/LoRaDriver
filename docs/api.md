# LoRaDriver Public API Reference

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

Opt out of steps 5 and 6 via `cfg.facade_auto_pump = false` and
`cfg.facade_auto_start_receive = false`.

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

The sync word in every preset is `0x12` (private LoRa P2P). Use `0x34`
for LoRaWAN public networks.

### Escape hatches

`lora.transceiver()`, `lora.driver()`, and (on ESP32) `lora.pump()`
return references to the underlying objects. Use them when you need
features the facade doesn't expose (`set_lna_gain`, `set_ocp_enabled`,
`pump.metrics()`, etc).

### When NOT to use the facade

- **Multi-instance**: two radios on one MCU need two distinct object
  trees. The facade is single-instance per binary. See
  [`examples/arduino/MultiInstance`](../examples/arduino/MultiInstance/).
- **Custom HAL**: if you need a non-Arduino SPI implementation or
  `FakeSpiDevice` for tests, instantiate the direct DI stack. See
  [`examples/arduino/AdvancedDirectDi`](../examples/arduino/AdvancedDirectDi/).

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
`LoRaError::AlreadyInitialized` — the config is **not** re-applied. To change
config, call `end()` first, then `begin(new_cfg)`.

`end()` cleanly tears down: detaches the driver event callback, clears all
user callbacks, puts the chip to sleep, returns the FSM to `Uninit`. The
caller is still responsible for `detachInterrupt(pin)` if they had wired the
ISR — the driver does not own the GPIO line.

## lib_deps (PlatformIO consumers)

Recommended (pin to a release tag — reproducible, no clone required):

```
lib_deps = https://github.com/Lakalot/LoRaDriver.git#v1.3.0
```

Track the rolling main branch (gets fixes faster, but builds aren't
reproducible):

```
lib_deps = https://github.com/Lakalot/LoRaDriver.git
```

Local development against an in-progress checkout (optional — use only
when actively editing the driver):

```
lib_deps = symlink://D:/DEV/C++/LoRaDriver         ; Windows
lib_deps = symlink:///home/you/dev/LoRaDriver      ; Linux / macOS
```

## Random bytes

`IRadioDriver::random_byte()` reads `RegRssiWideband` once. The returned byte
contains a few bits of entropy from the wideband noise floor — useful as a
seed but NOT cryptographically secure. For higher-quality randomness, read
multiple bytes spaced across receive idle periods and feed them into a hash
or CSPRNG.

## Callback contract

User-provided callbacks (`on_receive`, `on_event`, `on_tx_done`,
`on_header`) MUST be noexcept. The driver is built with `-fno-exceptions` on
Clang/GCC; a throwing callback is undefined behaviour. If your callback can
fail, store the error in a flag and handle it from the main loop instead.

## ISR responsibilities

The DIO0 ISR must:
1. Call `driver_.handle_interrupt()` (cheap: pushes a ring entry).
2. Optionally call `pump.notify_from_isr()` if using `RadioPumpTask`.
3. Do nothing else SPI-related — the driver reads `RegIrqFlags` from
   `process_events()`, not from the ISR.

Mark the ISR with `IRAM_ATTR` on ESP32 so it lives in IRAM and stays callable
while flash is busy.

## Polling mode (no DIO0 ISR)

If you don't wire DIO0, set `LoRaConfig::polling_mode = true`. `process_events()`
will then read `RegIrqFlags` every call regardless of whether `handle_interrupt`
was invoked. Higher SPI traffic but works without a free GPIO.

## Heartbeat

`check_alive()` re-reads `RegVersion` (~5 µs at 8 MHz SPI) and returns
`UnsupportedChip` if the chip has gone away (brown-out, ESD reset). Call
periodically from your main loop or pump task and re-init if it fails.

## CI scope

GitHub Actions runs the following workflows on every push to main /
rewrite / hardening / finishing / ci branches and every PR to main:

- **Host tests** (`host-tests.yml`) — matrix of `{ubuntu, windows, macos}
  × {Debug, Release}` plus a dedicated MSVC `/EHs-c- /GR-` job
  (`LORADRIVER_NO_EXCEPTIONS_MSVC=ON`) and an Ubuntu ASan+UBSan job, and
  clang-format lint.
- **PlatformIO build** (`platformio.yml`) — `pio run -e esp32dev` and a
  no-upload smoke test build to validate the embedded toolchain.
- **Arduino IDE compile** (`arduino-compile.yml`) — `arduino/compile-sketches`
  builds every example in `examples/` against `esp32:esp32:esp32`.
- **clang-tidy** (`clang-tidy.yml`) — runs the checks declared in
  `.clang-tidy` against `src/*.cpp` using a Ninja-generated
  `compile_commands.json`.
- **CodeQL** (`codeql.yml`) — GitHub's C++ security/quality scan, also
  runs weekly on a schedule.
- **Docs** (`docs.yml`) — Doxygen HTML deployed to GitHub Pages on every
  push to `main`.
- **Release** (`release.yml`) — tag-triggered (`v*.*.*`). Re-runs host
  tests, packages a versioned `LoRaDriver-X.Y.Z.{zip,tar.gz}` containing
  `src/`, `include/`, `examples/`, manifests and docs, then publishes a
  GitHub Release with the matching `CHANGELOG.md` section as the body.

## Out of scope for v1.1

The following datasheet features are intentionally not exposed:

- **FHSS (Frequency Hopping Spread Spectrum)** — `RegHopPeriod` + IRQ
  `FhssChangeChannel`. Useful for high-bitrate links over noisy bands but
  adds significant state machine complexity. Open an issue if you need it.
- **RxDutyCycle (battery-economising RX)** — datasheet §4.1.4.4. Trades
  latency for power. Implementable in user code via a timer that alternates
  `set_sleep()` / `start_receive(false)`.
- **SX126x family** — different chip family (command-based protocol instead
  of register-based). Out of scope for the SX127x driver; a separate
  `loradriver::chips::SX126xDriver` could be added in a future major version.
