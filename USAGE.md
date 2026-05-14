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

Either all three SpiPins fields must be set together (≥ 0), or all left
at `-1` (default — use board defaults). Partial overrides are rejected
by `LoRaConfig::validate()`.

If you want to call `SPI.begin(...)` yourself before `lora.begin()`,
leave `cfg.spi_pins` at its defaults (all `-1`) — the facade will then
just call `SPI.begin()` with no args, which is idempotent.

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
cfg.pump.period_ms     = 5;     // less polling = lower CPU
cfg.pump.tx_queue_depth = 16;   // deeper queue for bursty senders
cfg.pump.priority      = 5;     // higher than your app task
lora.begin(cfg);
```

## When you need more than the facade

The facade is one global instance, one set of pins, one chip. If you
need any of these, drop down to the direct DI API:

| Need | Where to look |
|---|---|
| Two radios on one MCU | [`examples/arduino/MultiInstance`](examples/arduino/MultiInstance/) |
| Custom HAL (non-Arduino SPI) | [`examples/arduino/AdvancedDirectDi`](examples/arduino/AdvancedDirectDi/) |
| Host tests with `FakeSpiDevice` | [`tests/host/test_*.cpp`](tests/host/) |
| Tighter lifetime control | Direct DI — instantiate `Esp32SpiDevice` etc. yourself |

The facade and the direct API coexist freely — you can have `lora` plus
a separate `LoRaTransceiver` instance in the same binary.

## When something goes wrong

| Symptom | Likely cause |
|---|---|
| `begin()` returns `UnsupportedChip` | Wiring (CS/RST/SPI), or SPI clock too high. Try `cfg.spi_frequency_hz = 1'000'000;`. |
| `begin()` returns `InvalidConfig` | Validate the config: frequency/chip combination, or partial `spi_pins`. |
| `send()` returns `TxTimeout` | Watchdog: chip didn't fire `TxDone` in time. Check DIO0 wiring. |
| `on_receive` never fires | Wrong `sync_word`, wrong frequency, or CRC mismatch. |
| Random resets during TX | Power supply: SX127x draws ~120 mA in TX. Add a 10 µF cap near VCC. |
| Build error "loradriver/chips/...hpp not found" (Arduino IDE) | Stale install — re-zip and reinstall, or `#include <LoRaDriver.h>` as your first lib include (recommended). |

For the heartbeat check, periodically call `lora.check_alive()` from your
main loop. It re-reads `RegVersion` and returns `UnsupportedChip` if the
chip has gone away (brown-out, ESD reset).
