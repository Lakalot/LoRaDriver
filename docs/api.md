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
`LoRaError::AlreadyInitialized` — the config is **not** re-applied. To change
config, call `end()` first, then `begin(new_cfg)`.

`end()` cleanly tears down: detaches the driver event callback, clears all
user callbacks, puts the chip to sleep, returns the FSM to `Uninit`. The
caller is still responsible for `detachInterrupt(pin)` if they had wired the
ISR — the driver does not own the GPIO line.

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
