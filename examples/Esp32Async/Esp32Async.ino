// ESP32 async TX example — uses the facade's send_async() which enqueues
// into the pump task's TX queue. Non-blocking; automatic RX restore.

#include <Arduino.h>
#include <SPI.h>
#include <LoRaDriver.h>

using namespace loradriver;

void setup() {
    Serial.begin(115200);

    LoRaConfig cfg = LoRaConfig::esp32_sx1276_868mhz(/*cs=*/5, /*rst=*/14, /*dio0=*/26);
    (void)lora.begin(cfg);

    lora.on_receive([](const LoRaPacket& m, const std::uint8_t* d, std::size_t n) {
        Serial.printf("RX %u rssi=%d  ", static_cast<unsigned>(n), m.rssi_dbm);
        for (std::size_t i = 0; i < n; ++i) Serial.write(d[i]);
        Serial.println();
    });
}

void loop() {
    static std::uint32_t i = 0;
    char msg[16];
    const int n = snprintf(msg, sizeof(msg), "tx %lu", static_cast<unsigned long>(i++));
    (void)lora.send_async(reinterpret_cast<const std::uint8_t*>(msg),
                          static_cast<std::uint8_t>(n));
    delay(2000);
}
