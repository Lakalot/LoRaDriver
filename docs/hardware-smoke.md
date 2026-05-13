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
- `test_start_receive_then_self_send_loopback` — PASS on the firmware side; whether a
  packet is actually received depends on whether a second device is paired and
  transmitting.

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

and re-flash. The driver applies the same init sequence; SX1278 is distinguished
by `LoRaConfig::validate()` (rejects 868 MHz) and by the RSSI offset
(-157 dBm low-band vs -164 high-band).
