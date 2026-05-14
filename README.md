# LoRaDriver

[![Host tests](https://github.com/Lakalot/LoRaDriver/actions/workflows/host-tests.yml/badge.svg)](https://github.com/Lakalot/LoRaDriver/actions/workflows/host-tests.yml)
[![PlatformIO build](https://github.com/Lakalot/LoRaDriver/actions/workflows/platformio.yml/badge.svg)](https://github.com/Lakalot/LoRaDriver/actions/workflows/platformio.yml)
[![Arduino IDE compile](https://github.com/Lakalot/LoRaDriver/actions/workflows/arduino-compile.yml/badge.svg)](https://github.com/Lakalot/LoRaDriver/actions/workflows/arduino-compile.yml)
[![clang-tidy](https://github.com/Lakalot/LoRaDriver/actions/workflows/clang-tidy.yml/badge.svg)](https://github.com/Lakalot/LoRaDriver/actions/workflows/clang-tidy.yml)
[![CodeQL](https://github.com/Lakalot/LoRaDriver/actions/workflows/codeql.yml/badge.svg)](https://github.com/Lakalot/LoRaDriver/actions/workflows/codeql.yml)
[![Docs](https://github.com/Lakalot/LoRaDriver/actions/workflows/docs.yml/badge.svg)](https://github.com/Lakalot/LoRaDriver/actions/workflows/docs.yml)

Clean C++17 driver for Semtech SX1276 and SX1278 LoRa transceivers.

## Highlights

- **Layered DI**: `ISpiDevice` (HAL) → `SX127xDriver` (chip) → `LoRaTransceiver` (FSM) → optional `RadioPumpTask` (ESP32 FreeRTOS).
- No singletons, no heap after `begin()`, no exceptions, `[[nodiscard]] noexcept` everywhere on the radio path.
- Semtech errata 2.1 (BW 500 kHz high-band) applied. LDRO auto, OCP, PA_BOOST/RFO + PaDac high-power, LNA boost.
- ISR-safe ring buffer + watchdog TX timeout.
- DMA-capable SPI on ESP32 via `transferBytes`.
- ~50 host tests + 1 embedded smoke.

## Quick start (Arduino + ESP32)

```cpp
#include <SPI.h>
#include "loradriver/chips/sx127x_driver.hpp"
#include "loradriver/hal/esp32_spi_device.hpp"
#include "loradriver/lora_transceiver.hpp"

using namespace loradriver;

hal::Esp32SpiDevice spi(SPI, /*cs=*/5);
chips::SX127xDriver drv(spi);
LoRaTransceiver     trx(drv);

void setup() {
    LoRaConfig cfg;
    cfg.chip = ChipModel::SX1276;
    cfg.frequency_hz = 868'000'000u;
    cfg.pin_ss = 5; cfg.pin_reset = 14; cfg.pin_dio0 = 26;
    trx.begin(cfg);
    trx.start_receive(true);
}
```

See `examples/` for sender, receiver, and ESP32-with-pump-task templates.

## Build host tests

```bash
cmake -S . -B build/host
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```
