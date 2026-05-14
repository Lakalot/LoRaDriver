# LoRaDriver

[![Host tests](https://github.com/Lakalot/LoRaDriver/actions/workflows/host-tests.yml/badge.svg)](https://github.com/Lakalot/LoRaDriver/actions/workflows/host-tests.yml)
[![PlatformIO build](https://github.com/Lakalot/LoRaDriver/actions/workflows/platformio.yml/badge.svg)](https://github.com/Lakalot/LoRaDriver/actions/workflows/platformio.yml)
[![Arduino IDE compile](https://github.com/Lakalot/LoRaDriver/actions/workflows/arduino-compile.yml/badge.svg)](https://github.com/Lakalot/LoRaDriver/actions/workflows/arduino-compile.yml)
[![clang-tidy](https://github.com/Lakalot/LoRaDriver/actions/workflows/clang-tidy.yml/badge.svg)](https://github.com/Lakalot/LoRaDriver/actions/workflows/clang-tidy.yml)
[![CodeQL](https://github.com/Lakalot/LoRaDriver/actions/workflows/codeql.yml/badge.svg)](https://github.com/Lakalot/LoRaDriver/actions/workflows/codeql.yml)
[![Docs](https://github.com/Lakalot/LoRaDriver/actions/workflows/docs.yml/badge.svg)](https://github.com/Lakalot/LoRaDriver/actions/workflows/docs.yml)
[![Release](https://img.shields.io/github/v/release/Lakalot/LoRaDriver?display_name=tag&sort=semver)](https://github.com/Lakalot/LoRaDriver/releases/latest)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A production-quality C++17 driver for Semtech **SX1276** and **SX1278** LoRa
transceivers, designed for embedded targets (ESP32, AVR, STM32, …) with a
clean dependency-injection architecture and a zero-allocation runtime path.

---

## Why this driver?

Most open-source SX127x drivers tangle the SPI HAL, the chip register layer
and the application FSM into one class — making them hard to test, hard to
port, and easy to break with a sloppy callback. LoRaDriver separates those
concerns:

```
┌──────────────────┐    ┌──────────────────┐    ┌──────────────────┐
│   ISpiDevice     │ →  │  SX127xDriver    │ →  │ LoRaTransceiver  │
│   (HAL)          │    │  (chip layer)    │    │ (FSM + dispatch) │
└──────────────────┘    └──────────────────┘    └──────────────────┘
                                                          │
                                                          ▼
                                              ┌──────────────────────┐
                                              │ RadioPumpTask        │
                                              │ (optional, ESP32)    │
                                              │ FreeRTOS pump task   │
                                              └──────────────────────┘
```

Each layer has a single responsibility, can be swapped in tests, and exposes
a `[[nodiscard]] noexcept` API.

## Features

- **Layered DI** — `ISpiDevice` (HAL) → `SX127xDriver` (chip) →
  `LoRaTransceiver` (FSM) → optional `RadioPumpTask` (ESP32 FreeRTOS).
- **Zero-allocation runtime** — no heap after `begin()`, no exceptions,
  built with `-fno-exceptions -fno-rtti` on Clang/GCC.
- **Semtech errata applied** — 2.1 (BW 500 kHz high-band) and the full
  2.3 IfFreq table per BW.
- **Auto-tuned RF path** — LDRO auto-selection (AN1200.24),
  PA_BOOST/RFO + PaDac high-power 20 dBm path, OCP auto-bump to 130 mA
  on `dBm > 17`, LNA boost for weak signals.
- **Robust under stress** — ISR-safe ring buffer with overflow stat,
  watchdog TX timeout, mode-transition read-back verify, runtime RX
  image recalibration on > 5 % frequency change.
- **ESP32-friendly** — DMA-capable SPI via `transferBytes`, FreeRTOS
  pump task with ISR notify and async TX queue.
- **Multi-instance** — two SX127x modules on independent SPI buses
  (VSPI + HSPI on ESP32).
- **Tested** — 50+ host tests + 1 on-target embedded smoke test;
  matrix CI across Linux / Windows / macOS, ASan + UBSan, clang-tidy,
  CodeQL security scan.

## Supported hardware

| Chip   | Frequency band     | Status |
|--------|--------------------|--------|
| SX1276 | 862 – 1020 MHz     | ✅ Tested |
| SX1277 | 862 – 1020 MHz     | ✅ Compile-checked (SF cap enforced) |
| SX1278 | 137 – 525 MHz      | ✅ Tested |
| SX1279 | 137 – 960 MHz      | ✅ Compile-checked |

Tested on TTGO LoRa32 (ESP32 + SX1276), Heltec WiFi LoRa 32, and
generic DOIT ESP32 DevKit + bare SX127x module. Other Arduino-compatible
MCUs work through the `ArduinoSpiDevice` HAL — only the ESP32 has a
DMA-optimized HAL out of the box.

## Installation

### PlatformIO

Pin to a release tag (reproducible builds):

```ini
[env:esp32dev]
platform   = espressif32
board      = esp32dev
framework  = arduino
lib_deps   = https://github.com/Lakalot/LoRaDriver.git#v1.3.0
```

Or track `main` (rolling, gets fixes first):

```ini
lib_deps = https://github.com/Lakalot/LoRaDriver.git
```

### Arduino IDE

1. **Sketch** → **Include Library** → **Add .ZIP Library…**
2. Pick a release archive from
   [Releases](https://github.com/Lakalot/LoRaDriver/releases).

Or clone directly into your `~/Arduino/libraries/` folder:

```bash
git clone https://github.com/Lakalot/LoRaDriver.git ~/Arduino/libraries/LoRaDriver
```

### CMake (host-only, non-Arduino)

```cmake
add_subdirectory(third_party/LoRaDriver)
target_link_libraries(my_target PRIVATE loradriver)
```

## Quick start

A minimal blocking sender on ESP32 + SX1276:

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

> Arduino IDE sketches: [`examples/arduino/`](examples/arduino/) — blocking sender, polling receiver, ESP32 async with auto-pump-task, multi-instance, advanced direct-DI.
>
> PlatformIO standalone projects: [`examples/pio/`](examples/pio/) — copy any subfolder verbatim, `pio run --target upload`, done. Three flavours: `sender/`, `receiver/`, `async-pump/`.
>
> Read [`USAGE.md`](USAGE.md) for a task-oriented guide.

## Documentation

| Document                                  | Contents                                      |
|-------------------------------------------|-----------------------------------------------|
| [`USAGE.md`](USAGE.md)                    | Task-oriented usage guide (TX, RX, tuning, troubleshooting) |
| [`docs/api.md`](docs/api.md)              | API reference: lifecycle, ISR contract, lib_deps, CI scope |
| [`docs/hardware-smoke.md`](docs/hardware-smoke.md) | How to validate on a real ESP32 + SX127x module |
| [`CHANGELOG.md`](CHANGELOG.md)            | Versioned release notes                       |
| [Doxygen API docs](https://lakalot.github.io/LoRaDriver/) | Generated HTML (published on every push to `main`) |

## Building host tests

```bash
cmake -S . -B build/host
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

Sanitizer build (ASan + UBSan, Clang/GCC):

```bash
cmake -S . -B build/asan -DLORADRIVER_SANITIZERS=ON
cmake --build build/asan
ctest --test-dir build/asan --output-on-failure
```

The matrix CI also exercises the MSVC `/EHs-c- /GR-` (no exceptions, no
RTTI) configuration via `-DLORADRIVER_NO_EXCEPTIONS_MSVC=ON`.

## Versioning and stability

LoRaDriver follows [Semantic Versioning](https://semver.org/). The public
API is the set of types and functions exported from
[`src/loradriver/`](src/loradriver/) — namespace `loradriver::*`.

Breaking changes bump the **major** version. Adding new optional fields
to `LoRaConfig` is treated as a minor change. Patch releases are
ABI-compatible — safe to upgrade `lib_deps` to a newer patch within the
same minor series.

## Contributing

Issues and pull requests are welcome. Before opening a PR:

1. Run `./tools/lint.sh` (clang-format check).
2. Build and run the host tests (`ctest --output-on-failure`).
3. If you touched ISR-path code, verify the ASan + UBSan job locally
   (`-DLORADRIVER_SANITIZERS=ON`).
4. Add or update a test under `tests/host/` for any new behavior.

## License

MIT. See [`library.properties`](library.properties) for author and
maintainer info.
