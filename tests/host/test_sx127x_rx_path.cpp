#include "fake_spi_device.hpp"
#include "loradriver/chips/sx127x_driver.hpp"
#include "loradriver/lora_config.hpp"
#include "test_runner.hpp"

#include "../../src/chips/sx127x/sx127x_registers.hpp"

using loradriver::ChipModel;
using loradriver::LoRaConfig;
using loradriver::LoRaError;
using loradriver::PaOutput;
using loradriver::chips::SX127xDriver;
using loradriver::test::FakeSpiDevice;
namespace reg    = loradriver::chips::sx127x::reg;
namespace opmode = loradriver::chips::sx127x::opmode;
namespace dio    = loradriver::chips::sx127x::dio;

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

bool TestStartReceiveContinuous() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(drv.start_receive(true), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kOpMode), opmode::kLoRaRxCont);
    LD_EXPECT_EQ(static_cast<std::uint8_t>(spi.reg(reg::kDioMapping1) & 0xC0u), dio::kDio0RxDone);
    LD_EXPECT_EQ(spi.reg(reg::kFifoRxBaseAddr), std::uint8_t{0});
    return true;
}

bool TestStartReceiveSingle() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(drv.start_receive(false), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kOpMode), opmode::kLoRaRxSingle);
    return true;
}

bool TestStartReceiveRejectedBeforeBegin() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.start_receive(true), LoRaError::NotInitialized);
    return true;
}

bool TestReadPacketRejectsNull() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(drv.read_packet(nullptr, 10), 0);
    return true;
}

bool TestReadPacketCopiesFromFifo() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    const std::uint8_t payload[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    for (std::size_t i = 0; i < 4; ++i) {
        spi.set_register(static_cast<std::uint8_t>(reg::kFifo + i), payload[i]);
    }
    spi.set_register(reg::kFifoRxCurrentAddr, 0);
    spi.set_register(reg::kRxNbBytes, 4);

    std::uint8_t out[8]{};
    const int n = drv.read_packet(out, sizeof(out));
    LD_EXPECT_EQ(n, 4);
    for (int i = 0; i < 4; ++i) LD_EXPECT_EQ(out[i], payload[i]);
    return true;
}

bool TestReadPacketClampsToMaxLen() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    for (std::size_t i = 0; i < 10; ++i) {
        spi.set_register(static_cast<std::uint8_t>(reg::kFifo + i), static_cast<std::uint8_t>(i));
    }
    spi.set_register(reg::kFifoRxCurrentAddr, 0);
    spi.set_register(reg::kRxNbBytes, 10);

    std::uint8_t out[4]{};
    const int n = drv.read_packet(out, sizeof(out));
    LD_EXPECT_EQ(n, 4);
    return true;
}

int main() {
    LD_RUN(TestStartReceiveContinuous);
    LD_RUN(TestStartReceiveSingle);
    LD_RUN(TestStartReceiveRejectedBeforeBegin);
    LD_RUN(TestReadPacketRejectsNull);
    LD_RUN(TestReadPacketCopiesFromFifo);
    LD_RUN(TestReadPacketClampsToMaxLen);
    return loradriver::test::report();
}
