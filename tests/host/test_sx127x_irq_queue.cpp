#include "fake_spi_device.hpp"
#include "loradriver/chips/sx127x_driver.hpp"
#include "loradriver/lora_config.hpp"
#include "loradriver/radio_event.hpp"
#include "test_runner.hpp"

#include "../../src/chips/sx127x/sx127x_registers.hpp"

#ifdef _WIN32
#include <windows.h>
static void sleep_ms(int ms) {
    Sleep(ms);
}
#else
#include <unistd.h>
static void sleep_ms(int ms) {
    usleep(ms * 1000);
}
#endif

using loradriver::ChipModel;
using loradriver::LoRaConfig;
using loradriver::LoRaError;
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
    c.spreading_factor = 9;
    c.bandwidth_hz = 125'000u;
    c.coding_rate = 5;
    c.preamble_length = 8;
    c.sync_word = 0x12;
    c.crc_enabled = true;
    c.tx_power_dbm = 14;
    c.pa_output = PaOutput::PaBoost;
    c.ocp_ma = 100;
    c.pin_ss = 5;
    c.pin_reset = 14;
    c.pin_dio0 = 26;
    return c;
}

bool TestHandleInterruptEnqueuesEvent() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    drv.handle_interrupt();
    int count = 0;
    drv.set_event_callback([&count](RadioEvent, int) { ++count; });
    spi.set_register(reg::kIrqFlags, irq::kRxDone);
    drv.process_events();
    LD_EXPECT_EQ(count, 1);
    return true;
}

bool TestProcessEventsEmitsRxDone() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    bool saw_rxdone = false;
    drv.set_event_callback([&](RadioEvent ev, int) {
        if (ev == RadioEvent::RxDone)
            saw_rxdone = true;
    });
    spi.set_register(reg::kIrqFlags, irq::kRxDone);
    drv.handle_interrupt();
    drv.process_events();
    LD_EXPECT(saw_rxdone);
    return true;
}

bool TestProcessEventsEmitsTxDone() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    bool saw_txdone = false;
    drv.set_event_callback([&](RadioEvent ev, int) {
        if (ev == RadioEvent::TxDone)
            saw_txdone = true;
    });
    spi.set_register(reg::kIrqFlags, irq::kTxDone);
    drv.handle_interrupt();
    drv.process_events();
    LD_EXPECT(saw_txdone);
    LD_EXPECT(!drv.is_transmitting());
    return true;
}

bool TestProcessEventsEmitsRxCrcError() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    bool saw_crc = false;
    drv.set_event_callback([&](RadioEvent ev, int) {
        if (ev == RadioEvent::RxCrcError)
            saw_crc = true;
    });
    spi.set_register(reg::kIrqFlags, irq::kRxDone | irq::kPayloadCrcError);
    drv.handle_interrupt();
    drv.process_events();
    LD_EXPECT(saw_crc);
    LD_EXPECT_EQ(drv.get_stats().rx_crc_errors, std::uint32_t{1});
    return true;
}

bool TestProcessEventsClearsIrqFlags() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    spi.clear_writes();
    spi.set_register(reg::kIrqFlags, irq::kRxDone);
    drv.handle_interrupt();
    drv.process_events();
    bool saw_clear = false;
    for (const auto& w : spi.writes()) {
        if (w.reg == reg::kIrqFlags && w.value == irq::kClearAll)
            saw_clear = true;
    }
    LD_EXPECT(saw_clear);
    return true;
}

bool TestIrqOverflowDetected() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    for (int i = 0; i < 20; ++i)
        drv.handle_interrupt();
    LD_EXPECT(drv.get_stats().irq_overflows > 0u);
    return true;
}

bool TestRandomByteReadsWidebandRssi() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    spi.set_register(reg::kRssiWideband, 0x5A);
    LD_EXPECT_EQ(drv.random_byte(), std::uint8_t{0x5A});
    return true;
}

bool TestTxWatchdogTimeout() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    const std::uint8_t buf[2] = {1, 2};
    LD_EXPECT_EQ(drv.start_transmit(buf, 2, 0), LoRaError::OK);
    bool saw_timeout = false;
    drv.set_event_callback([&](RadioEvent ev, int) {
        if (ev == RadioEvent::TxTimeout)
            saw_timeout = true;
    });
    drv.process_events();
    LD_EXPECT(saw_timeout);
    LD_EXPECT(!drv.is_transmitting());
    LD_EXPECT_EQ(drv.get_stats().tx_timeout, std::uint32_t{1});
    return true;
}

bool TestRxSilenceWatchdogFiresWhenIdle() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.rx_silence_timeout_ms = 0;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    LD_EXPECT_EQ(drv.start_receive(true), LoRaError::OK);
    int rx_timeouts = 0;
    drv.set_event_callback([&rx_timeouts](RadioEvent ev, int) {
        if (ev == RadioEvent::RxTimeout)
            ++rx_timeouts;
    });
    for (int i = 0; i < 100; ++i)
        drv.process_events();
    LD_EXPECT_EQ(rx_timeouts, 0);
    return true;
}

bool TestRxSilenceWatchdogTriggersAfterTimeout() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.rx_silence_timeout_ms = 1;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    LD_EXPECT_EQ(drv.start_receive(true), LoRaError::OK);
    int rx_timeouts = 0;
    drv.set_event_callback([&rx_timeouts](RadioEvent ev, int) {
        if (ev == RadioEvent::RxTimeout)
            ++rx_timeouts;
    });
    sleep_ms(5);
    drv.process_events();
    LD_EXPECT(rx_timeouts >= 1);
    return true;
}

bool TestHeartbeatDetectsDeadChip() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    spi.set_chip_version(0xFF);
    LD_EXPECT_EQ(drv.check_alive(), LoRaError::UnsupportedChip);
    return true;
}

bool TestHeartbeatPassesOnLiveChip() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(drv.check_alive(), LoRaError::OK);
    return true;
}

bool TestPollingModeReadsIrqFlagsWithoutInterrupt() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.polling_mode = true;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);

    int rxdone_count = 0;
    drv.set_event_callback([&rxdone_count](RadioEvent ev, int) {
        if (ev == RadioEvent::RxDone)
            ++rxdone_count;
    });
    spi.set_register(reg::kIrqFlags, irq::kRxDone);
    drv.process_events();
    LD_EXPECT_EQ(rxdone_count, 1);
    return true;
}

bool TestNonPollingModeIgnoresIrqFlagsWithoutInterrupt() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.polling_mode = false;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);

    int rxdone_count = 0;
    drv.set_event_callback([&rxdone_count](RadioEvent ev, int) {
        if (ev == RadioEvent::RxDone)
            ++rxdone_count;
    });
    spi.set_register(reg::kIrqFlags, irq::kRxDone);
    drv.process_events();
    LD_EXPECT_EQ(rxdone_count, 0);
    return true;
}

int main() {
    LD_RUN(TestHandleInterruptEnqueuesEvent);
    LD_RUN(TestProcessEventsEmitsRxDone);
    LD_RUN(TestProcessEventsEmitsTxDone);
    LD_RUN(TestProcessEventsEmitsRxCrcError);
    LD_RUN(TestProcessEventsClearsIrqFlags);
    LD_RUN(TestIrqOverflowDetected);
    LD_RUN(TestRandomByteReadsWidebandRssi);
    LD_RUN(TestTxWatchdogTimeout);
    LD_RUN(TestPollingModeReadsIrqFlagsWithoutInterrupt);
    LD_RUN(TestNonPollingModeIgnoresIrqFlagsWithoutInterrupt);
    LD_RUN(TestHeartbeatDetectsDeadChip);
    LD_RUN(TestHeartbeatPassesOnLiveChip);
    LD_RUN(TestRxSilenceWatchdogFiresWhenIdle);
    LD_RUN(TestRxSilenceWatchdogTriggersAfterTimeout);
    return loradriver::test::report();
}
