# LoRaDriver — PlatformIO example projects

Three standalone PlatformIO projects you can copy verbatim into your own
workspace. Each pulls LoRaDriver via `lib_deps` from the pinned tag —
no clone or symlink required.

| Project | What it shows |
|---|---|
| [`sender/`](sender/) | Blocking `lora.send()` every second. Minimal facade init. |
| [`receiver/`](receiver/) | Polling receiver. Callback-driven RX, no manual ISR. |
| [`async-pump/`](async-pump/) | `lora.send_async()` + auto-pump-task. Non-blocking TX. |

The `.ino` sketches under [`examples/arduino/`](../arduino/) are the
Arduino IDE equivalents — same code, just the PlatformIO project
scaffold around them.

## How to use one of these projects

```bash
cp -r examples/pio/sender ~/my-lora-project
cd ~/my-lora-project
pio run --target upload --upload-port /dev/ttyUSB0
pio device monitor --port /dev/ttyUSB0 --baud 115200
```

Each `platformio.ini` pins `LoRaDriver` to a release tag for
reproducibility. Bump the `#v1.3.0` suffix to track newer releases.

## Custom SPI pins

If your board uses non-default SPI pins (TTGO LoRa32, SYNC-SIGNAL-LORA,
etc.), set `cfg.spi_pins` in `main.cpp` before `lora.begin(cfg)` —
see the `async-pump` example for an explicit annotated setup.

## Pin assignments

All three examples assume:

| Function | GPIO |
|---|---|
| LoRa NSS / CS | 5 |
| LoRa RST | 14 |
| LoRa DIO0 | 26 |
| SPI SCK / MISO / MOSI | board defaults (18 / 19 / 23 on ESP32 VSPI) |

Edit the three pin numbers at the top of each `main.cpp` to match your
hardware.
