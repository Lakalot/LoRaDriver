// Minimal polling receiver — print packets as they arrive.
#include <Arduino.h>
#include <SPI.h>

#include <LoRaDriver.h>

#include "loradriver/chips/sx127x_driver.hpp"
#include "loradriver/hal/arduino_spi_device.hpp"
#include "loradriver/lora_transceiver.hpp"

using namespace loradriver;

constexpr std::int8_t kSS = 5, kRst = 14, kDio0 = 26;

hal::ArduinoSpiDevice g_spi(SPI, kSS);
chips::SX127xDriver   g_drv(g_spi);
LoRaTransceiver       g_trx(g_drv);

void IRAM_ATTR isr_dio0() { g_trx.handle_interrupt(); }

void setup() {
    Serial.begin(115200);
    SPI.begin();
    pinMode(kRst, OUTPUT); digitalWrite(kRst, LOW); delay(2); digitalWrite(kRst, HIGH); delay(10);

    LoRaConfig cfg;
    cfg.chip = ChipModel::SX1276;
    cfg.frequency_hz = 868'000'000u;
    cfg.pin_ss = kSS; cfg.pin_reset = kRst; cfg.pin_dio0 = kDio0;
    (void)g_trx.begin(cfg);

    g_trx.on_receive([](const LoRaPacket& meta, const std::uint8_t* data, std::size_t len) {
        Serial.printf("RX %u bytes  rssi=%d  snr=%.1f: ",
                      static_cast<unsigned>(len), meta.rssi_dbm, meta.snr_db());
        for (std::size_t i = 0; i < len; ++i) Serial.write(data[i]);
        Serial.println();
    });

    attachInterrupt(digitalPinToInterrupt(kDio0), isr_dio0, RISING);
    (void)g_trx.start_receive(true);
}

void loop() { g_trx.poll(); delay(2); }
