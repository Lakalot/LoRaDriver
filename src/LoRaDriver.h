// Top-level umbrella header for Arduino IDE compatibility.
// Use this if you don't want to spell the full namespaced paths.
// Otherwise include the granular headers directly (recommended for
// PlatformIO and CMake consumers):
//     #include "loradriver/lora_transceiver.hpp"
//     #include "loradriver/chips/sx127x_driver.hpp"
//     #include "loradriver/hal/esp32_spi_device.hpp"  // ESP32 only
//     #include "loradriver/platform/esp32/radio_pump_task.hpp"  // ESP32 only

#pragma once

#include "loradriver/lora_config.hpp"
#include "loradriver/lora_error.hpp"
#include "loradriver/lora_packet.hpp"
#include "loradriver/lora_transceiver.hpp"
#include "loradriver/radio_driver.hpp"
#include "loradriver/radio_event.hpp"
#include "loradriver/radio_stats.hpp"
#include "loradriver/version.hpp"

#include "loradriver/chips/sx127x_driver.hpp"

#include "loradriver/hal/spi_device.hpp"

#ifdef ARDUINO
#include "loradriver/hal/arduino_spi_device.hpp"
#endif

#ifdef ARDUINO_ARCH_ESP32
#include "loradriver/hal/esp32_spi_device.hpp"
#include "loradriver/platform/esp32/radio_pump_task.hpp"
#endif

// Facade (Arduino only; host code uses LoRaTransceiver directly).
#ifdef ARDUINO
#include "loradriver/lora.hpp"
#endif
