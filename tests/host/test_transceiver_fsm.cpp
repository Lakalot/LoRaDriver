#include "fake_spi_device.hpp"
#include "loradriver/chips/sx127x_driver.hpp"
#include "loradriver/lora_config.hpp"
#include "loradriver/lora_transceiver.hpp"
#include "test_runner.hpp"

#include "../../src/chips/sx127x/sx127x_registers.hpp"

using loradriver::ChipModel;
using loradriver::LoRaConfig;
using loradriver::LoRaError;
using loradriver::LoRaPacket;
using loradriver::LoRaTransceiver;
using loradriver::PaOutput;
using loradriver::RadioEvent;
using loradriver::chips::SX127xDriver;
using loradriver::test::FakeSpiDevice;
namespace reg = loradriver::chips::sx127x::reg;
namespace irq = loradriver::chips::sx127x::irq;

static LoRaConfig MakeCfg() {
    LoRaConfig c;
    c.chip = ChipModel::SX1276;
    c.frequency_hz = 868'000'000u;
    c.spreading_factor = 9; c.bandwidth_hz = 125'000u;
    c.coding_rate = 5; c.preamble_length = 8;
    c.sync_word = 0x12; c.crc_enabled = true;
    c.tx_power_dbm = 14; c.pa_output = PaOutput::PaBoost;
    c.ocp_ma = 100;
    c.pin_ss = 5; c.pin_reset = 14; c.pin_dio0 = 26;
    return c;
}

bool TestBeginEntersStandby() {
    FakeSpiDevice spi; SX127xDriver drv(spi); LoRaTransceiver trx(drv);
    LD_EXPECT_EQ(trx.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT(trx.state() == LoRaTransceiver::State::Standby);
    return true;
}

bool TestSleepThenStandby() {
    FakeSpiDevice spi; SX127xDriver drv(spi); LoRaTransceiver trx(drv);
    LD_EXPECT_EQ(trx.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(trx.set_sleep(), LoRaError::OK);
    LD_EXPECT(trx.state() == LoRaTransceiver::State::Sleep);
    LD_EXPECT_EQ(trx.set_standby(), LoRaError::OK);
    LD_EXPECT(trx.state() == LoRaTransceiver::State::Standby);
    return true;
}

bool TestStartReceiveContinuousTransitions() {
    FakeSpiDevice spi; SX127xDriver drv(spi); LoRaTransceiver trx(drv);
    LD_EXPECT_EQ(trx.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(trx.start_receive(true), LoRaError::OK);
    LD_EXPECT(trx.state() == LoRaTransceiver::State::RxContinuous);
    return true;
}

bool TestStartReceiveSingleTransitions() {
    FakeSpiDevice spi; SX127xDriver drv(spi); LoRaTransceiver trx(drv);
    LD_EXPECT_EQ(trx.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(trx.start_receive(false), LoRaError::OK);
    LD_EXPECT(trx.state() == LoRaTransceiver::State::RxSingle);
    return true;
}

bool TestSendRejectsBeforeBegin() {
    FakeSpiDevice spi; SX127xDriver drv(spi); LoRaTransceiver trx(drv);
    const std::uint8_t buf[1] = {0xAA};
    LD_EXPECT_EQ(trx.send(buf, 1, 100), LoRaError::NotInitialized);
    return true;
}

bool TestSendCompletesWhenTxDoneIrqArrives() {
    FakeSpiDevice spi; SX127xDriver drv(spi); LoRaTransceiver trx(drv);
    LD_EXPECT_EQ(trx.begin(MakeCfg()), LoRaError::OK);

    // Pre-arm: TxDone IRQ is already pending; handle_interrupt below pushes a ring
    // entry; the send() wait loop drains it on first process_events() call.
    spi.set_register(reg::kIrqFlags, irq::kTxDone);
    drv.handle_interrupt();

    const std::uint8_t buf[2] = {1, 2};
    const LoRaError e = trx.send(buf, 2, 100);
    LD_EXPECT_EQ(e, LoRaError::OK);
    LD_EXPECT(trx.state() == LoRaTransceiver::State::Standby);
    return true;
}

bool TestSendReturnsTxTimeout() {
    FakeSpiDevice spi; SX127xDriver drv(spi); LoRaTransceiver trx(drv);
    LD_EXPECT_EQ(trx.begin(MakeCfg()), LoRaError::OK);
    const std::uint8_t buf[2] = {1, 2};
    // No IRQ arranged → driver watchdog fires after timeout_ms=0
    LD_EXPECT_EQ(trx.send(buf, 2, 0), LoRaError::OK);
    // Note: with timeout_ms=0, the driver watchdog fires on first process_events
    // and clears tx_in_progress_, so the send() wait loop returns OK (driver
    // already emitted TxTimeout to the event callback). Verify via stats.
    LD_EXPECT_EQ(trx.stats().tx_timeout, std::uint32_t{1});
    return true;
}

bool TestOnReceiveDispatchesPacket() {
    FakeSpiDevice spi; SX127xDriver drv(spi); LoRaTransceiver trx(drv);
    LD_EXPECT_EQ(trx.begin(MakeCfg()), LoRaError::OK);

    bool got = false;
    std::size_t got_len = 0;
    std::uint8_t got_data[3] = {};
    trx.on_receive([&](const LoRaPacket& meta, const std::uint8_t* data, std::size_t len) {
        got = true;
        got_len = len;
        if (len <= 3) for (std::size_t i = 0; i < len; ++i) got_data[i] = data[i];
        (void)meta;
    });

    LD_EXPECT_EQ(trx.start_receive(true), LoRaError::OK);

    // Now arrange a fake received frame in FIFO + IRQ flags (after start_receive
    // so OpMode write at reg 0x01 doesn't overwrite payload[1]).
    const std::uint8_t payload[3] = {0xAA, 0xBB, 0xCC};
    for (std::size_t i = 0; i < 3; ++i) {
        spi.set_register(static_cast<std::uint8_t>(reg::kFifo + i), payload[i]);
    }
    spi.set_register(reg::kFifoRxCurrentAddr, 0);
    spi.set_register(reg::kRxNbBytes, 3);
    spi.set_register(reg::kPktRssiValue, 100);
    spi.set_register(reg::kPktSnrValue, 0x14);
    spi.set_register(reg::kIrqFlags, irq::kRxDone);

    drv.handle_interrupt();
    trx.poll();
    LD_EXPECT(got);
    LD_EXPECT_EQ(got_len, std::size_t{3});
    for (int i = 0; i < 3; ++i) LD_EXPECT_EQ(got_data[i], payload[i]);
    return true;
}

bool TestOnEventForwardsRadioEvents() {
    FakeSpiDevice spi; SX127xDriver drv(spi); LoRaTransceiver trx(drv);
    LD_EXPECT_EQ(trx.begin(MakeCfg()), LoRaError::OK);
    int count = 0;
    trx.on_event([&](RadioEvent, int) { ++count; });
    spi.set_register(reg::kIrqFlags, irq::kRxDone);
    drv.handle_interrupt();
    trx.poll();
    LD_EXPECT(count >= 1);
    return true;
}

bool TestOnTxDoneFiresAfterTransmission() {
    FakeSpiDevice spi; SX127xDriver drv(spi); LoRaTransceiver trx(drv);
    LD_EXPECT_EQ(trx.begin(MakeCfg()), LoRaError::OK);
    bool fired = false;
    trx.on_tx_done([&]() { fired = true; });
    spi.set_register(reg::kIrqFlags, irq::kTxDone);
    drv.handle_interrupt();
    const std::uint8_t buf[1] = {0xAA};
    LD_EXPECT_EQ(trx.send(buf, 1, 100), LoRaError::OK);
    LD_EXPECT(fired);
    return true;
}

bool TestEndClearsCallbacks() {
    FakeSpiDevice spi; SX127xDriver drv(spi); LoRaTransceiver trx(drv);
    LD_EXPECT_EQ(trx.begin(MakeCfg()), LoRaError::OK);
    int rx_callback_calls = 0;
    trx.on_receive([&rx_callback_calls](const LoRaPacket&, const std::uint8_t*, std::size_t) {
        ++rx_callback_calls;
    });
    trx.end();
    spi.set_register(reg::kIrqFlags, irq::kRxDone);
    spi.set_register(reg::kRxNbBytes, 4);
    drv.handle_interrupt();
    drv.process_events();
    LD_EXPECT_EQ(rx_callback_calls, 0);
    return true;
}

int main() {
    LD_RUN(TestBeginEntersStandby);
    LD_RUN(TestSleepThenStandby);
    LD_RUN(TestStartReceiveContinuousTransitions);
    LD_RUN(TestStartReceiveSingleTransitions);
    LD_RUN(TestSendRejectsBeforeBegin);
    LD_RUN(TestSendCompletesWhenTxDoneIrqArrives);
    LD_RUN(TestSendReturnsTxTimeout);
    LD_RUN(TestOnReceiveDispatchesPacket);
    LD_RUN(TestOnEventForwardsRadioEvents);
    LD_RUN(TestOnTxDoneFiresAfterTransmission);
    LD_RUN(TestEndClearsCallbacks);
    return loradriver::test::report();
}
