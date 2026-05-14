// Polling LoRa receiver using the facade.
//
// lora.begin() enters RX continuous automatically and auto-attaches the
// DIO0 ISR. The on_receive callback fires from the pump task (ESP32) —
// keep it fast: copy what you need and signal your main task.

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
    // cfg.spi_pins = {18, 19, 22}; // uncomment for boards with custom MOSI

    const LoRaError e = lora.begin(cfg);
    Serial.printf("[lora] begin: %s\n", to_string(e));
    if (e != LoRaError::OK) {
        while (true) { delay(1000); }
    }

    lora.on_receive([](const LoRaPacket& meta, const uint8_t* data, size_t len) {
        Serial.printf("[lora] RX %u bytes  rssi=%d  snr=%.1f: ",
                      static_cast<unsigned>(len), meta.rssi_dbm, meta.snr_db());
        for (size_t i = 0; i < len; ++i) Serial.write(data[i]);
        Serial.println();
    });
}

void loop() {
    // Nothing to do here — the RX callback runs on the pump task (core 1).
    delay(100);
}
