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
namespace reg = loradriver::chips::sx127x::reg;
namespace opmode = loradriver::chips::sx127x::opmode;
namespace dio = loradriver::chips::sx127x::dio;

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

bool TestTransmitRejectsBeforeBegin() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    const std::uint8_t buf[1] = {0xAA};
    LD_EXPECT_EQ(drv.start_transmit(buf, 1, 1000), LoRaError::NotInitialized);
    return true;
}

bool TestTransmitRejectsNullBuffer() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(drv.start_transmit(nullptr, 4, 1000), LoRaError::NullArgument);
    return true;
}

bool TestTransmitRejectsZeroLength() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    const std::uint8_t buf[1] = {0xAA};
    LD_EXPECT_EQ(drv.start_transmit(buf, 0, 1000), LoRaError::InvalidConfig);
    return true;
}

bool TestTransmitRejectsOversizedPayload() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    std::uint8_t buf[256]{};
    LD_EXPECT_EQ(drv.start_transmit(buf, 256, 1000), LoRaError::TxBufferTooLarge);
    return true;
}

bool TestTransmitWritesFifoAndPayloadLength() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    spi.clear_writes();
    const std::uint8_t buf[5] = {1, 2, 3, 4, 5};
    LD_EXPECT_EQ(drv.start_transmit(buf, 5, 1000), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kFifoAddrPtr), std::uint8_t{0});
    LD_EXPECT_EQ(spi.reg(reg::kFifoTxBaseAddr), std::uint8_t{0});
    LD_EXPECT_EQ(spi.reg(reg::kPayloadLength), std::uint8_t{5});
    // Verify the 5 payload bytes were written to FIFO via the burst_write log.
    // (State assertion is unreliable because later writes — OpMode at 0x01,
    //  ModemConfig1 at 0x1D, etc. — overlap the FIFO splay range 0..4.)
    int payload_writes_seen = 0;
    for (const auto& w : spi.writes()) {
        if (payload_writes_seen < 5 && w.reg == static_cast<std::uint8_t>(payload_writes_seen) &&
            w.value == buf[payload_writes_seen]) {
            ++payload_writes_seen;
        }
    }
    LD_EXPECT_EQ(payload_writes_seen, 5);
    return true;
}

bool TestTransmitSetsTxOpModeAndDio0TxDone() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    const std::uint8_t buf[2] = {0xAA, 0x55};
    LD_EXPECT_EQ(drv.start_transmit(buf, 2, 1000), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kOpMode), opmode::kLoRaTx);
    LD_EXPECT_EQ(static_cast<std::uint8_t>(spi.reg(reg::kDioMapping1) & 0xC0u), dio::kDio0TxDone);
    LD_EXPECT(drv.is_transmitting());
    return true;
}

bool TestTransmitFailsOnSpiError() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    spi.fail_writes(true);
    const std::uint8_t buf[2] = {0xAA, 0x55};
    LD_EXPECT_EQ(drv.start_transmit(buf, 2, 1000), LoRaError::SpiFailure);
    LD_EXPECT(!drv.is_transmitting());
    return true;
}

bool TestStartContinuousWaveSetsTxContBit() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(drv.start_continuous_wave(), LoRaError::OK);
    LD_EXPECT_EQ(static_cast<std::uint8_t>(spi.reg(reg::kModemConfig2) & 0x08u),
                 std::uint8_t{0x08});
    LD_EXPECT_EQ(spi.reg(reg::kOpMode), opmode::kLoRaTx);
    return true;
}

int main() {
    LD_RUN(TestTransmitRejectsBeforeBegin);
    LD_RUN(TestTransmitRejectsNullBuffer);
    LD_RUN(TestTransmitRejectsZeroLength);
    LD_RUN(TestTransmitRejectsOversizedPayload);
    LD_RUN(TestTransmitWritesFifoAndPayloadLength);
    LD_RUN(TestTransmitSetsTxOpModeAndDio0TxDone);
    LD_RUN(TestTransmitFailsOnSpiError);
    LD_RUN(TestStartContinuousWaveSetsTxContBit);
    return loradriver::test::report();
}
