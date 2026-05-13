#include <Arduino.h>
#include <SPI.h>
#include <unity.h>

#include "loradriver/chips/sx127x_driver.hpp"
#include "loradriver/hal/esp32_spi_device.hpp"
#include "loradriver/lora_config.hpp"
#include "loradriver/lora_transceiver.hpp"

using namespace loradriver;

namespace {

// Pin wiring matches the SYNC-SIGNAL-LORA project (the consumer that drives
// this smoke test). If your board differs, override these constants and the
// SPI.begin(sck, miso, mosi) call below.
constexpr std::int8_t kPinSck   = 18;
constexpr std::int8_t kPinMiso  = 19;
constexpr std::int8_t kPinMosi  = 22;  // non-default — MOSI on GPIO22, not 23
constexpr std::int8_t kPinSS    = 5;
constexpr std::int8_t kPinReset = 17;  // swapped with DIO0: hardware wired backwards
constexpr std::int8_t kPinDio0  = 4;

hal::Esp32SpiDevice g_spi(SPI, kPinSS);
chips::SX127xDriver g_drv(g_spi);
LoRaTransceiver     g_trx(g_drv);

LoRaConfig make_cfg() {
    LoRaConfig c;
    c.chip = ChipModel::SX1276;
    c.frequency_hz = 868'100'000u;
    c.spreading_factor = 7;
    c.bandwidth_hz = 125'000u;
    c.tx_power_dbm = 14;
    c.pin_ss = kPinSS; c.pin_reset = kPinReset; c.pin_dio0 = kPinDio0;
    return c;
}

void IRAM_ATTR isr_dio0() { g_trx.handle_interrupt(); }

}  // namespace

void setUp() {}
void tearDown() {}

void test_chip_version_is_0x12() {
    // SPI.begin(sck, miso, mosi) — explicit pins because MOSI is non-default.
    SPI.begin(kPinSck, kPinMiso, kPinMosi);
    TEST_ASSERT_EQUAL(static_cast<int>(LoRaError::OK),
                      static_cast<int>(g_trx.begin(make_cfg())));
    TEST_ASSERT_EQUAL_HEX8(0x12, g_trx.chip_version());
}

void test_check_alive() {
    TEST_ASSERT_EQUAL(static_cast<int>(LoRaError::OK),
                      static_cast<int>(g_trx.check_alive()));
}

void test_tx_blocking_returns_ok() {
    const std::uint8_t payload[] = {'p','i','n','g'};
    TEST_ASSERT_EQUAL(static_cast<int>(LoRaError::OK),
                      static_cast<int>(g_trx.send(payload, 4, 2000)));
}

void test_start_receive_then_self_send_loopback() {
    attachInterrupt(digitalPinToInterrupt(kPinDio0), isr_dio0, RISING);

    volatile bool got_packet = false;
    static std::uint8_t got[16];
    static std::size_t got_len = 0;
    g_trx.on_receive([&](const LoRaPacket&, const std::uint8_t* d, std::size_t n) {
        got_len = (n < sizeof(got)) ? n : sizeof(got);
        memcpy(got, d, got_len);
        got_packet = true;
    });

    TEST_ASSERT_EQUAL(static_cast<int>(LoRaError::OK),
                      static_cast<int>(g_trx.start_receive(true)));

    const std::uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
    TEST_ASSERT_EQUAL(static_cast<int>(LoRaError::OK),
                      static_cast<int>(g_trx.send(payload, 4, 2000)));
    TEST_ASSERT_EQUAL(static_cast<int>(LoRaError::OK),
                      static_cast<int>(g_trx.start_receive(true)));

    const std::uint32_t t0 = millis();
    while (!got_packet && (millis() - t0) < 5000) {
        g_trx.poll();
        delay(10);
    }
    Serial.printf("[smoke] rx_received=%s len=%u\n",
                  got_packet ? "yes" : "no", static_cast<unsigned>(got_len));
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_chip_version_is_0x12);
    RUN_TEST(test_check_alive);
    RUN_TEST(test_tx_blocking_returns_ok);
    RUN_TEST(test_start_receive_then_self_send_loopback);
    UNITY_END();
}

void loop() {}
