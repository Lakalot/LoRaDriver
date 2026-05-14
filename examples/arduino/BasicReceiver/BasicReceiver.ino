// Minimal receiver — prints packets as they arrive.
//
// Uses the v1.3.0 facade API. DIO0 ISR is attached automatically by
// lora.begin(); no ISR shim required.

#include <Arduino.h>
#include <SPI.h>
#include <LoRaDriver.h>

using namespace loradriver;

void setup() {
    Serial.begin(115200);

    LoRaConfig cfg = LoRaConfig::esp32_sx1276_868mhz(/*cs=*/5, /*rst=*/14, /*dio0=*/26);
    (void)lora.begin(cfg);

    lora.on_receive([](const LoRaPacket& meta, const std::uint8_t* data, std::size_t len) {
        Serial.printf("RX %u bytes  rssi=%d  snr=%.1f: ",
                      static_cast<unsigned>(len), meta.rssi_dbm, meta.snr_db());
        for (std::size_t i = 0; i < len; ++i) Serial.write(data[i]);
        Serial.println();
    });
}

void loop() {
    // Nothing to do — RX callback fires from the pump task on core 1.
    delay(100);
}
