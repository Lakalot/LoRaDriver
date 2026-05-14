// Blocking LoRa sender using the facade. Sends "hello N" every second.

#include <Arduino.h>
#include <SPI.h>
#include <LoRaDriver.h>

using namespace loradriver;

constexpr int8_t kCS = 5;
constexpr int8_t kRST = 14;
constexpr int8_t kDIO0 = 26;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {}

    LoRaConfig cfg = LoRaConfig::esp32_sx1276_868mhz(kCS, kRST, kDIO0);
    cfg.tx_power_dbm = 14;
    // cfg.spi_pins = {18, 19, 22}; // uncomment for boards with custom MOSI

    const LoRaError e = lora.begin(cfg);
    Serial.printf("[lora] begin: %s\n", to_string(e));
    if (e != LoRaError::OK) {
        while (true) { delay(1000); }
    }
}

void loop() {
    static uint32_t counter = 0;
    char msg[32];
    const int n = snprintf(msg, sizeof(msg), "hello %lu",
                           static_cast<unsigned long>(counter++));
    const LoRaError e = lora.send(reinterpret_cast<const uint8_t*>(msg),
                                  static_cast<size_t>(n),
                                  /*timeout_ms=*/1000);
    Serial.printf("[lora] send #%lu rc=%s\n",
                  static_cast<unsigned long>(counter), to_string(e));
    delay(1000);
}
