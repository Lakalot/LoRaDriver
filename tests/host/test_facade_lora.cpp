// Host tests for the loradriver::LoRa facade.
//
// The facade's Arduino-side members (SPI device, driver, transceiver,
// pump task) are excluded on host builds. Tests use the
// LORADRIVER_FACADE_HOST_TEST injection constructor to wrap a
// LoRaTransceiver backed by FakeSpiDevice.

#include "fake_spi_device.hpp"
#include "test_runner.hpp"

#include "loradriver/chips/sx127x_driver.hpp"
#include "loradriver/lora.hpp"
#include "loradriver/lora_config.hpp"
#include "loradriver/lora_transceiver.hpp"

using loradriver::ChipModel;
using loradriver::LoRa;
using loradriver::LoRaConfig;
using loradriver::LoRaError;
using loradriver::LoRaPacket;
using loradriver::LoRaTransceiver;
using loradriver::RadioEvent;
using loradriver::chips::SX127xDriver;
using loradriver::test::FakeSpiDevice;

namespace {

LoRaConfig MakeCfg() {
    LoRaConfig c{};
    c.chip = ChipModel::SX1276;
    c.frequency_hz = 868'000'000u;
    c.pin_ss = 1;
    c.pin_reset = 2;
    c.pin_dio0 = 3;
    return c;
}

bool TestFacadeEscapeReturnsInjectedTransceiver() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaTransceiver trx(drv);
    LoRa facade(trx);

    LD_EXPECT(&facade.transceiver() == &trx);
    return true;
}

bool TestFacadeOnEventDeliversTxDone() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaTransceiver trx(drv);
    LoRa facade(trx);

    LD_EXPECT_EQ(trx.begin(MakeCfg()), LoRaError::OK);

    bool got_event = false;
    RadioEvent last = RadioEvent::None;
    facade.on_event([&got_event, &last](RadioEvent ev, int /*param*/) {
        got_event = true;
        last = ev;
    });

    // Simulate a TxDone IRQ: set bit 3 (0x08) of RegIrqFlags then trigger
    // a poll. Same approach as test_sx127x_irq_queue / test_transceiver_fsm.
    spi.set_register(0x12 /* RegIrqFlags */, 0x08);
    drv.handle_interrupt();
    trx.poll();

    LD_EXPECT(got_event);
    LD_EXPECT_EQ(last, RadioEvent::TxDone);
    return true;
}

bool TestFacadeOnReceiveRegistrationDoesNotCrash() {
    // We can't easily simulate a full RxDone packet path here (would
    // require setting up FIFO contents and several register flags).
    // For host testing, the value is in confirming that on_receive
    // does not crash when called through the facade — the actual RX
    // wiring is covered by test_transceiver_fsm.cpp::TestOnReceiveDispatchesPacket.
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaTransceiver trx(drv);
    LoRa facade(trx);

    LD_EXPECT_EQ(trx.begin(MakeCfg()), LoRaError::OK);

    int hits = 0;
    facade.on_receive([&hits](const LoRaPacket&, const std::uint8_t*, std::size_t) { ++hits; });

    // The escape hatch must still return the same trx instance.
    LD_EXPECT(&facade.transceiver() == &trx);
    return true;
}

bool TestFacadePresetsAreConstexpr() {
    // Force compile-time evaluation by binding to constexpr.
    constexpr auto c1 = LoRaConfig::esp32_sx1276_868mhz(5, 14, 26);
    constexpr auto c2 = LoRaConfig::esp32_sx1278_433mhz(5, 14, 26);
    constexpr auto c3 = LoRaConfig::arduino_sx1276_868mhz(10, 9, 2);
    constexpr auto c4 = LoRaConfig::arduino_sx1278_433mhz(10, 9, 2);
    static_assert(c1.frequency_hz == 868'000'000u, "esp32_sx1276_868mhz freq");
    static_assert(c2.frequency_hz == 433'920'000u, "esp32_sx1278_433mhz freq");
    static_assert(c3.frequency_hz == 868'000'000u, "arduino_sx1276_868mhz freq");
    static_assert(c4.frequency_hz == 433'920'000u, "arduino_sx1278_433mhz freq");
    (void)c1;
    (void)c2;
    (void)c3;
    (void)c4;
    return true;
}

} // namespace

int main() {
    LD_RUN(TestFacadeEscapeReturnsInjectedTransceiver);
    LD_RUN(TestFacadeOnEventDeliversTxDone);
    LD_RUN(TestFacadeOnReceiveRegistrationDoesNotCrash);
    LD_RUN(TestFacadePresetsAreConstexpr);
    return loradriver::test::report();
}
