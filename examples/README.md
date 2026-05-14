# LoRaDriver examples

Two flavours, organised by the framework they target.

| Folder | Format | Use case |
|---|---|---|
| [`arduino/`](arduino/) | `.ino` sketches | Arduino IDE users. Open the `.ino`, install the library, compile. |
| [`pio/`](pio/) | Full PlatformIO projects (each with its own `platformio.ini`) | PlatformIO users. Copy a subfolder, `pio run --target upload`. |

Both sets cover the same patterns:

- **`BasicSender` / `pio/sender`** — blocking `lora.send()` every second.
- **`BasicReceiver` / `pio/receiver`** — callback-driven `on_receive`.
- **`Esp32Async` / `pio/async-pump`** — non-blocking `lora.send_async()` with the FreeRTOS pump task.

Arduino-only:

- **`arduino/MultiInstance`** — two SX1276 modules on independent SPI buses (direct DI; not facade-supported).
- **`arduino/AdvancedDirectDi`** — explicit DI pattern for custom HAL, host tests, or tighter lifetime control.

See [`../USAGE.md`](../USAGE.md) for a task-oriented walkthrough and
[`../docs/api.md`](../docs/api.md) for the full API reference.
