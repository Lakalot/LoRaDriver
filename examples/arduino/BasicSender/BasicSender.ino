// Minimal blocking sender — sends a packet every second.
//
// Uses the v1.3.0 facade API. For the explicit DI pattern, see
// examples/arduino/AdvancedDirectDi/.

#include <Arduino.h>
#include <SPI.h>
#include <LoRaDriver.h>

using namespace loradriver;

void setup() {
    Serial.begin(115200);

    LoRaConfig cfg = LoRaConfig::esp32_sx1276_868mhz(/*cs=*/5, /*rst=*/14, /*dio0=*/26);
    cfg.tx_power_dbm = 14;

    const LoRaError e = lora.begin(cfg);
    Serial.printf("begin: %s\n", to_string(e));
}

void loop() {
    static std::uint32_t counter = 0;
    char msg[32];
    const int n = snprintf(msg, sizeof(msg), "hello %lu",
                           static_cast<unsigned long>(counter++));
    const LoRaError e = lora.send(reinterpret_cast<const std::uint8_t*>(msg),
                                  static_cast<std::size_t>(n), 1000);
    Serial.printf("send #%lu: %s\n", static_cast<unsigned long>(counter), to_string(e));
    delay(1000);
}
