# LoRaDriver

LoRaDriver is a deterministic, architecture-first LoRa radio driver baseline for ESP32 (Arduino framework) with host-side CMake/CTest validation.

## V1 Scope

- Supported chips: SX1276, SX1278
- Supported bands: 433 MHz, 868 MHz
- Supported IRQ routing: DIO0 only, DIO0 + DIO1
- Protocol scope: LoRa P2P only
- Out of scope in V1: LoRaWAN and SX126x runtime support (stub-only)

## Build and Test

### Target lane (PlatformIO)

```bash
python -m platformio run -e esp32dev
python -m platformio test -e esp32dev --without-uploading --without-testing
```

To execute embedded Unity tests with attached hardware:

```bash
python -m platformio test -e esp32dev --test-port <serial-port>
```

### Host lane (CMake/CTest)

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
```
