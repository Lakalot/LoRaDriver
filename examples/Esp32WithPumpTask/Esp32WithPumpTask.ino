// ESP32 example using the FreeRTOS pump task — async TX, automatic RX restore.
#include <Arduino.h>
#include <SPI.h>

#include "loradriver/chips/sx127x_driver.hpp"
#include "loradriver/hal/esp32_spi_device.hpp"
#include "loradriver/lora_transceiver.hpp"
#include "loradriver/platform/esp32/radio_pump_task.hpp"

using namespace loradriver;

constexpr std::int8_t kSS = 5, kRst = 14, kDio0 = 26;

hal::Esp32SpiDevice           g_spi(SPI, kSS);
chips::SX127xDriver           g_drv(g_spi);
LoRaTransceiver               g_trx(g_drv);
platform::esp32::RadioPumpTask g_pump;

void IRAM_ATTR isr_dio0() {
    g_trx.handle_interrupt();
    g_pump.notify_from_isr();
}

void setup() {
    Serial.begin(115200);
    SPI.begin();
    pinMode(kRst, OUTPUT); digitalWrite(kRst, LOW); delay(2); digitalWrite(kRst, HIGH); delay(10);

    LoRaConfig cfg;
    cfg.chip = ChipModel::SX1276;
    cfg.frequency_hz = 868'000'000u;
    cfg.pin_ss = kSS; cfg.pin_reset = kRst; cfg.pin_dio0 = kDio0;
    (void)g_trx.begin(cfg);

    g_trx.on_receive([](const LoRaPacket& m, const std::uint8_t* d, std::size_t n) {
        Serial.printf("RX %u rssi=%d  ", static_cast<unsigned>(n), m.rssi_dbm);
        for (std::size_t i = 0; i < n; ++i) Serial.write(d[i]);
        Serial.println();
    });

    (void)g_trx.start_receive(true);
    attachInterrupt(digitalPinToInterrupt(kDio0), isr_dio0, RISING);
    g_pump.start(g_trx, /*period_ms=*/2, /*priority=*/2, /*stack_words=*/2048, /*core_id=*/1);
}

void loop() {
    static std::uint32_t i = 0;
    char msg[16];
    const int n = snprintf(msg, sizeof(msg), "tx %lu", static_cast<unsigned long>(i++));
    g_pump.enqueue_packet(reinterpret_cast<const std::uint8_t*>(msg),
                          static_cast<std::uint8_t>(n));
    delay(2000);
}
