// Minimal blocking sender — sends a packet every second.
#include <Arduino.h>
#include <SPI.h>

#include "loradriver/chips/sx127x_driver.hpp"
#include "loradriver/hal/arduino_spi_device.hpp"
#include "loradriver/lora_transceiver.hpp"

using namespace loradriver;

constexpr std::int8_t kSS = 5, kRst = 14, kDio0 = 26;

hal::ArduinoSpiDevice g_spi(SPI, kSS);
chips::SX127xDriver   g_drv(g_spi);
LoRaTransceiver       g_trx(g_drv);

void setup() {
    Serial.begin(115200);
    SPI.begin();

    pinMode(kRst, OUTPUT);
    digitalWrite(kRst, LOW);  delay(2);
    digitalWrite(kRst, HIGH); delay(10);

    LoRaConfig cfg;
    cfg.chip = ChipModel::SX1276;
    cfg.frequency_hz = 868'000'000u;
    cfg.tx_power_dbm = 14;
    cfg.pin_ss = kSS; cfg.pin_reset = kRst; cfg.pin_dio0 = kDio0;

    const LoRaError e = g_trx.begin(cfg);
    Serial.printf("begin: %s\n", to_string(e));
}

void loop() {
    static std::uint32_t counter = 0;
    char msg[32];
    const int n = snprintf(msg, sizeof(msg), "hello %lu", static_cast<unsigned long>(counter++));
    const LoRaError e = g_trx.send(reinterpret_cast<const std::uint8_t*>(msg),
                                   static_cast<std::size_t>(n), 1000);
    Serial.printf("send #%lu: %s\n", static_cast<unsigned long>(counter), to_string(e));
    delay(1000);
}
