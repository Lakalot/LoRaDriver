#include <Arduino.h>
#include <SPI.h>
#include <unity.h>

#include "loradriver/chips/sx127x_driver.hpp"
#include "loradriver/hal/esp32_spi_device.hpp"
#include "loradriver/lora_config.hpp"
#include "loradriver/lora_transceiver.hpp"

using namespace loradriver;

namespace {

constexpr std::int8_t kPinSS    = 5;
constexpr std::int8_t kPinReset = 14;
constexpr std::int8_t kPinDio0  = 26;

void hard_reset() {
    pinMode(kPinReset, OUTPUT);
    digitalWrite(kPinReset, LOW);
    delay(2);
    digitalWrite(kPinReset, HIGH);
    delay(10);
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_chip_version_is_0x12() {
    SPI.begin();
    hard_reset();
    static hal::Esp32SpiDevice spi(SPI, kPinSS);
    static chips::SX127xDriver drv(spi);

    LoRaConfig cfg;
    cfg.chip = ChipModel::SX1276;
    cfg.frequency_hz = 868'000'000u;
    cfg.pin_ss = kPinSS; cfg.pin_reset = kPinReset; cfg.pin_dio0 = kPinDio0;

    TEST_ASSERT_EQUAL(static_cast<int>(LoRaError::OK), static_cast<int>(drv.begin(cfg)));
    TEST_ASSERT_EQUAL_HEX8(0x12, drv.chip_version());
}

void test_transceiver_begin_reaches_standby() {
    static hal::Esp32SpiDevice spi(SPI, kPinSS);
    static chips::SX127xDriver drv(spi);
    static LoRaTransceiver trx(drv);

    LoRaConfig cfg;
    cfg.chip = ChipModel::SX1276;
    cfg.frequency_hz = 868'000'000u;
    cfg.pin_ss = kPinSS; cfg.pin_reset = kPinReset; cfg.pin_dio0 = kPinDio0;

    TEST_ASSERT_EQUAL(static_cast<int>(LoRaError::OK), static_cast<int>(trx.begin(cfg)));
    TEST_ASSERT_EQUAL(static_cast<int>(LoRaTransceiver::State::Standby),
                      static_cast<int>(trx.state()));
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_chip_version_is_0x12);
    RUN_TEST(test_transceiver_begin_reaches_standby);
    UNITY_END();
}

void loop() {}
