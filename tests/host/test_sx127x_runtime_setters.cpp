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
    c.pin_ss = 5; c.pin_reset = 14; c.pin_dio0 = 26;
    return c;
}

bool TestSetFrequencyChangesFrfRegisters() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    spi.clear_writes();
    LD_EXPECT_EQ(drv.set_frequency(915'000'000u), LoRaError::OK);
    const std::uint64_t frf = (915'000'000ull << 19) / 32'000'000ull;
    LD_EXPECT_EQ(spi.reg(reg::kFrMsb), static_cast<std::uint8_t>((frf >> 16) & 0xFF));
    return true;
}

bool TestSetSpreadingFactorRejectsOutOfRange() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(drv.set_spreading_factor(5), LoRaError::InvalidConfig);
    LD_EXPECT_EQ(drv.set_spreading_factor(13), LoRaError::InvalidConfig);
    return true;
}

bool TestSetSpreadingFactorUpdatesModemConfig2() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(drv.set_spreading_factor(11), LoRaError::OK);
    LD_EXPECT_EQ(static_cast<std::uint8_t>((spi.reg(reg::kModemConfig2) >> 4) & 0x0Fu), std::uint8_t{11});
    return true;
}

bool TestSetBandwidthUpdatesModemConfig1() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(drv.set_bandwidth(250'000u), LoRaError::OK);
    LD_EXPECT_EQ(static_cast<std::uint8_t>((spi.reg(reg::kModemConfig1) >> 4) & 0x0Fu), std::uint8_t{8});
    return true;
}

bool TestSetTxPowerSwitchesPaDac() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(drv.set_tx_power(20, PaOutput::PaBoost), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kPaDac), std::uint8_t{0x87});
    LD_EXPECT_EQ(drv.set_tx_power(10, PaOutput::PaBoost), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kPaDac), std::uint8_t{0x84});
    return true;
}

bool TestSettersRejectedBeforeBegin() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.set_frequency(868'000'000u), LoRaError::NotInitialized);
    LD_EXPECT_EQ(drv.set_spreading_factor(9), LoRaError::NotInitialized);
    LD_EXPECT_EQ(drv.set_bandwidth(125'000u), LoRaError::NotInitialized);
    LD_EXPECT_EQ(drv.set_tx_power(14, PaOutput::PaBoost), LoRaError::NotInitialized);
    return true;
}

bool TestSetStandbyAndSleep() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(drv.set_sleep(), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kOpMode), std::uint8_t{0x80});  // LoRaSleep
    LD_EXPECT_EQ(drv.set_standby(), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kOpMode), std::uint8_t{0x81});  // LoRaStandby
    return true;
}

bool TestSetLnaGainRejectsOutOfRange() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(drv.set_lna_gain(7), LoRaError::InvalidConfig);
    return true;
}

bool TestSetLnaGainZeroEnablesAgc() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(drv.set_lna_gain(3), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kModemConfig3) & 0x04u, std::uint8_t{0});
    LD_EXPECT_EQ(drv.set_lna_gain(0), LoRaError::OK);
    LD_EXPECT_EQ(static_cast<std::uint8_t>(spi.reg(reg::kModemConfig3) & 0x04u),
                 std::uint8_t{0x04});
    return true;
}

bool TestSetLnaGainSpecificValueDisablesAgc() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(drv.set_lna_gain(2), LoRaError::OK);
    LD_EXPECT_EQ(static_cast<std::uint8_t>((spi.reg(reg::kLna) >> 5) & 0x07u),
                 std::uint8_t{2});
    LD_EXPECT_EQ(spi.reg(reg::kModemConfig3) & 0x04u, std::uint8_t{0});
    return true;
}

bool TestSetOcpEnabledTogglesBit5() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(drv.set_ocp_enabled(false), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kOcp) & 0x20u, std::uint8_t{0});
    LD_EXPECT_EQ(drv.set_ocp_enabled(true), LoRaError::OK);
    LD_EXPECT_EQ(static_cast<std::uint8_t>(spi.reg(reg::kOcp) & 0x20u),
                 std::uint8_t{0x20});
    return true;
}

int main() {
    LD_RUN(TestSetFrequencyChangesFrfRegisters);
    LD_RUN(TestSetSpreadingFactorRejectsOutOfRange);
    LD_RUN(TestSetSpreadingFactorUpdatesModemConfig2);
    LD_RUN(TestSetBandwidthUpdatesModemConfig1);
    LD_RUN(TestSetTxPowerSwitchesPaDac);
    LD_RUN(TestSettersRejectedBeforeBegin);
    LD_RUN(TestSetStandbyAndSleep);
    LD_RUN(TestSetLnaGainRejectsOutOfRange);
    LD_RUN(TestSetLnaGainZeroEnablesAgc);
    LD_RUN(TestSetLnaGainSpecificValueDisablesAgc);
    LD_RUN(TestSetOcpEnabledTogglesBit5);
    return loradriver::test::report();
}
