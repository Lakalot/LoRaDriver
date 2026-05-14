// Async LoRa TX with the FreeRTOS pump task. Shows custom SPI pins and
// pump tuning. send_async() returns immediately; the pump task drives
// the radio from RX -> TX -> RX without blocking the main loop.

#include <Arduino.h>
#include <SPI.h>
#include <LoRaDriver.h>

using namespace loradriver;

constexpr int8_t kCS = 5;
constexpr int8_t kRST = 14;
constexpr int8_t kDIO0 = 26;

// Custom SPI bus pins (uncomment + edit if your board diverges from the
// ESP32 VSPI defaults of SCK=18 / MISO=19 / MOSI=23). All three or none.
constexpr int8_t kSCK = -1;
constexpr int8_t kMISO = -1;
constexpr int8_t kMOSI = -1;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {}

    LoRaConfig cfg = LoRaConfig::esp32_sx1276_868mhz(kCS, kRST, kDIO0);

    // Optional: custom SPI pins (only set if you need to override).
    if (kSCK >= 0 && kMISO >= 0 && kMOSI >= 0) {
        cfg.spi_pins = {kSCK, kMISO, kMOSI};
    }

    // Optional: pump task tuning. Defaults shown.
    cfg.pump.period_ms = 2;        // poll interval
    cfg.pump.priority = 2;         // FreeRTOS task priority
    cfg.pump.stack_words = 2048;   // 8 KB stack
    cfg.pump.core_id = 1;          // pin to APP core
    cfg.pump.tx_queue_depth = 8;   // up to 8 outgoing packets queued

    const LoRaError e = lora.begin(cfg);
    Serial.printf("[lora] begin: %s\n", to_string(e));
    if (e != LoRaError::OK) {
        while (true) { delay(1000); }
    }

    lora.on_receive([](const LoRaPacket& m, const uint8_t* d, size_t n) {
        Serial.printf("[lora] RX %u rssi=%d  ", static_cast<unsigned>(n), m.rssi_dbm);
        for (size_t i = 0; i < n; ++i) Serial.write(d[i]);
        Serial.println();
    });

    lora.on_tx_done([]() noexcept {
        Serial.println("[lora] TX done");
    });
}

void loop() {
    static uint32_t i = 0;
    char msg[16];
    const int n = snprintf(msg, sizeof(msg), "tx %lu",
                           static_cast<unsigned long>(i++));
    const bool ok = lora.send_async(reinterpret_cast<const uint8_t*>(msg),
                                    static_cast<uint8_t>(n));
    if (!ok) {
        Serial.println("[lora] TX queue full — dropping");
    }
    delay(2000);
}
