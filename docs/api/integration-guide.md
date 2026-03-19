# LoRaDriver V1 Integration Guide

This guide covers all steps required to integrate LoRaDriver into a product firmware project using
the standard V1 path. No fork of the driver source is required for any supported V1 use case.

---

## 1. Dependency Setup

### PlatformIO

**Minimum tool version:** PlatformIO Core `6.1.19` (current latest stable).

Add the library to `platformio.ini`:

```ini
[env:your_board]
platform  = espressif32          ; or atmelavr, etc.
framework = arduino
lib_deps  =
    file://../LoRaDriver         ; local path — adjust to your directory layout
```

If LoRaDriver is consumed as a registered library or a git submodule, replace `file://..` with the
appropriate `lib_deps` entry (remote URI or local path).

**Minimum board / framework requirements:**
- C++17 support enabled (`build_flags = -std=gnu++17`)
- `std::function` available (present in all supported Arduino-framework SDKs)
- SPI peripheral accessible to the host MCU

### CMake

**Minimum CMake version:** `3.16` (recommended `4.3.0` or later).

```cmake
# In your top-level CMakeLists.txt:
add_subdirectory(path/to/LoRaDriver)

target_link_libraries(your_firmware PRIVATE loradriver)
target_include_directories(your_firmware PRIVATE path/to/LoRaDriver/include)
```

The library target exposes only `include/loradriver/` as a public include path. Never add
`src/chips/` or `src/platform/` to your include directories; those paths are internal adapters.

---

## 2. Public API Quickstart

All runtime operations are methods on `loradriver::LoRaDriver`. Include the single entry-point
header:

```cpp
#include "loradriver/lora_driver.hpp"
```

### Initialization sequence

```cpp
#include "loradriver/lora_driver.hpp"

loradriver::LoRaDriver driver;

// 1. Build a hardware profile
loradriver::RadioConfig config;
config.chip        = loradriver::RadioConfig::Chip::kSx1276;
config.band        = loradriver::RadioConfig::Band::k868;
config.dio_routing = loradriver::RadioConfig::DioRouting::kDio0Only;  // IRQ_MINIMAL

// 2. Optional: inject a platform timestamp source
driver.setTimestampSource([]() -> std::uint32_t { return millis(); });

// 3. Optional: register an event callback before begin()
driver.setEventCallback([](loradriver::RadioEvent event, int detail) {
    // Handle events — see RadioEvent enum in radio_event.hpp
});

// 4. Initialize
loradriver::LoRaError err = driver.begin(config);
if (err != loradriver::LoRaError::kOk) {
    // Profile rejected or hardware not detected — err carries typed failure
    return;
}
```

### Core runtime operations

```cpp
// Transmit
const uint8_t payload[] = { 0xDE, 0xAD, 0xBE, 0xEF };
loradriver::LoRaError tx = driver.send(payload, sizeof(payload));

// Start continuous receive
loradriver::LoRaError rx = driver.startReceive();

// Recover from a timeout condition (call after RadioEvent::kTimeout)
loradriver::LoRaError rec = driver.recoverFromTimeout();

// Enter low power
driver.sleep();

// Return to active state before further data operations
driver.standby();
```

> **All `LoRaDriver` methods return `LoRaError`.** Always check the return value. Refer to
> `include/loradriver/lora_error.hpp` for the complete typed error enumeration.

---

## 3. Callback Contract

The event callback signature is:

```cpp
std::function<void(loradriver::RadioEvent, int)>
```

- First argument: `RadioEvent` — the event type (see `include/loradriver/radio_event.hpp`)
- Second argument: `int` — a detail code providing additional context (diagnostic detail)

The callback is invoked synchronously from within driver method calls. It must be non-blocking
and must not call back into `LoRaDriver` methods that acquire internal state (i.e., no re-entrant
`begin()` / `send()` calls from within the callback).

**Key events to handle:**

| `RadioEvent` | Meaning | Recommended action |
|---|---|---|
| `kInitialized` | `begin()` succeeded | Mark driver ready |
| `kTxCompleted` | TX burst finished | Queue next payload or enter RX |
| `kTxFailed` | TX failed | Log error, retry or alert |
| `kRxDone` | Packet received | Read payload from application buffer |
| `kTimeout` | RX or TX timed out | Call `recoverFromTimeout()` |
| `kRecoveryCompleted` | Timeout recovery done | Resume normal operation |
| `kError` | Runtime error | Inspect detail code, capture snapshot |
| `kSleep` | Sleep entered | Gate further operations until `standby()` |

---

## 4. V1 Profile Configuration Examples

All four V1-supported profile combinations are illustrated below.

### SX1276 · 868 MHz · IRQ_MINIMAL (DIO0 only)

```cpp
loradriver::RadioConfig config;
config.chip        = loradriver::RadioConfig::Chip::kSx1276;
config.band        = loradriver::RadioConfig::Band::k868;
config.dio_routing = loradriver::RadioConfig::DioRouting::kDio0Only;
// LoRa modulation defaults: SF9, BW125, CR4/5, sync 0x12, 14 dBm, CRC on
driver.begin(config);
```

### SX1276 · 868 MHz · IRQ_EXTENDED (DIO0 + DIO1)

```cpp
loradriver::RadioConfig config;
config.chip        = loradriver::RadioConfig::Chip::kSx1276;
config.band        = loradriver::RadioConfig::Band::k868;
config.dio_routing = loradriver::RadioConfig::DioRouting::kDio0Dio1;
driver.begin(config);
```

### SX1278 · 433 MHz · IRQ_MINIMAL (DIO0 only)

```cpp
loradriver::RadioConfig config;
config.chip        = loradriver::RadioConfig::Chip::kSx1278;
config.band        = loradriver::RadioConfig::Band::k433;
config.dio_routing = loradriver::RadioConfig::DioRouting::kDio0Only;
driver.begin(config);
```

### SX1278 · 433 MHz · IRQ_EXTENDED (DIO0 + DIO1)

```cpp
loradriver::RadioConfig config;
config.chip        = loradriver::RadioConfig::Chip::kSx1278;
config.band        = loradriver::RadioConfig::Band::k433;
config.dio_routing = loradriver::RadioConfig::DioRouting::kDio0Dio1;
driver.begin(config);
```

### Overriding LoRa modulation parameters

Default V1 modulation parameters are suitable for most applications. To override:

```cpp
config.spreading_factor      = 10;   // SF7–SF12
config.bandwidth_khz         = 250;  // 125 / 250 / 500 kHz
config.coding_rate_denominator = 6;  // 5=CR4/5 .. 8=CR4/8
config.tx_power_dbm          = 20;   // dBm
config.crc_enabled           = true;
config.preamble_length       = 8;    // symbols
config.sync_word             = 0x12; // LoRa public sync word
```

---

## 5. No Source Fork Required

Standard V1 use cases (SX1276 / SX1278, 433 / 868 MHz, DIO0-only or DIO0+DIO1) are fully
addressable via `RadioConfig` alone. The driver's hardware adapters (`src/chips/`,
`src/platform/`) are compiled as part of the library and do not need to be modified.

If a platform abstraction is unavailable for your target, open an issue in the project repository
— do not fork internal adapter paths.

---

## 6. Diagnostic and Incident Capture

For field triage and support handoff, the driver exposes a snapshot API:

```cpp
// Capture a point-in-time incident snapshot
auto snapshot = driver.captureIncidentSnapshot();

// Serialize to a fixed-size buffer
char buffer[loradriver::IncidentSnapshot::kFormatBufferSize];
snapshot.formatTo(buffer, sizeof(buffer));
// buffer: "LORADRIVER_INCIDENT:v=...;e=...;c=...;b=...;d=...;dc=...;seq=...;ts=...;"
```

Attach the formatted string to support tickets to provide complete operational context.

---

## Cross-references

- V1 support scope and deferred items: [`docs/scope/v1-support-boundaries.md`](../scope/v1-support-boundaries.md)
- Power profile and DIO wiring trade-offs: [`docs/scope/power-profile-comparison.md`](../scope/power-profile-comparison.md)
- SX126x onboarding prerequisites: [`docs/scope/v1-bis-entry-criteria.md`](../scope/v1-bis-entry-criteria.md)
- Existing integration conventions: [`docs/api/integration.md`](integration.md)
- `RadioConfig` definition: `include/loradriver/radio_config.hpp`
- `RadioEvent` enumeration: `include/loradriver/radio_event.hpp`
- `LoRaError` enumeration: `include/loradriver/lora_error.hpp`
