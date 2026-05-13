#include "fake_spi_device.hpp"
#include "loradriver/chips/sx127x_driver.hpp"
#include "loradriver/lora_config.hpp"
#include "loradriver/lora_error.hpp"
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

bool TestBeginRejectsInvalidConfig() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig bad = MakeCfg();
    bad.pin_ss = -1;
    LD_EXPECT_EQ(drv.begin(bad), LoRaError::InvalidConfig);
    return true;
}

bool TestBeginRejectsMissingChip() {
    FakeSpiDevice spi;
    spi.set_chip_version(0xFF);
    SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::UnsupportedChip);
    return true;
}

bool TestBeginSucceedsAndEntersStandby() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kOpMode), opmode::kLoRaStandby);
    return true;
}

bool TestBeginWritesFrequencyRegisters868() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.frequency_hz = 868'100'000u;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);

    const std::uint64_t frf = (static_cast<std::uint64_t>(868'100'000u) << 19) / 32'000'000ull;
    LD_EXPECT_EQ(spi.reg(reg::kFrMsb), static_cast<std::uint8_t>((frf >> 16) & 0xFF));
    LD_EXPECT_EQ(spi.reg(reg::kFrMid), static_cast<std::uint8_t>((frf >> 8) & 0xFF));
    LD_EXPECT_EQ(spi.reg(reg::kFrLsb), static_cast<std::uint8_t>(frf & 0xFF));
    return true;
}

bool TestBeginAppliesSyncWord() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.sync_word = 0x34;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kSyncWord), std::uint8_t{0x34});
    return true;
}

bool TestBeginAppliesLdroForSf12() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.spreading_factor = 12;
    c.bandwidth_hz = 125'000u;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    LD_EXPECT(((spi.reg(reg::kModemConfig3) >> 3) & 0x01u) == 1u);
    return true;
}

bool TestBeginAppliesAgcAuto() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.agc_auto = true;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    LD_EXPECT(((spi.reg(reg::kModemConfig3) >> 2) & 0x01u) == 1u);
    return true;
}

bool TestBeginAppliesPaBoost14dBm() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.tx_power_dbm = 14;
    c.pa_output = PaOutput::PaBoost;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    LD_EXPECT((spi.reg(reg::kPaConfig) & 0x80u) != 0u);
    LD_EXPECT_EQ(static_cast<std::uint8_t>(spi.reg(reg::kPaConfig) & 0x0Fu), std::uint8_t{0x0C});
    return true;
}

bool TestBeginEnablesPaDacForHighPower() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.tx_power_dbm = 20;
    c.pa_output = PaOutput::PaBoost;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kPaDac), std::uint8_t{0x87});
    return true;
}

bool TestBeginAppliesOcp100mA() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.ocp_ma = 100;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    LD_EXPECT((spi.reg(reg::kOcp) & 0x20u) != 0u);
    LD_EXPECT_EQ(static_cast<std::uint8_t>(spi.reg(reg::kOcp) & 0x1Fu), std::uint8_t{0x0B});
    return true;
}

bool TestBeginClearsIrqFlags() {
    FakeSpiDevice spi;
    spi.set_register(reg::kIrqFlags, 0xFF);
    SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    bool saw_clear = false;
    for (const auto& w : spi.writes()) {
        if (w.reg == reg::kIrqFlags && w.value == 0xFFu) saw_clear = true;
    }
    LD_EXPECT(saw_clear);
    return true;
}

bool TestBeginRejectsSpiFailure() {
    FakeSpiDevice spi;
    spi.fail_writes(true);
    SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::SpiFailure);
    return true;
}

// Fake that drops every write to RegOpMode (chip is dead but bus says OK).
class DeadOpModeFakeSpi : public FakeSpiDevice {
public:
    [[nodiscard]] LoRaError transfer(std::uint8_t addr,
                                     const std::uint8_t* tx,
                                     std::uint8_t* rx,
                                     std::size_t len) noexcept override {
        const bool is_write = (addr & 0x80u) != 0u;
        const std::uint8_t r = addr & 0x7Fu;
        if (is_write && r == 0x01 /*OpMode*/) {
            // Silently swallow the write, then let read see the old value.
            return LoRaError::OK;
        }
        return FakeSpiDevice::transfer(addr, tx, rx, len);
    }
};

bool TestBeginDetectsDeadOpModeRegister() {
    DeadOpModeFakeSpi spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.auto_reset = false;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::SpiVerifyMismatch);
    return true;
}

bool TestSf6ImplicitHeaderInitOk() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.spreading_factor = 6;
    c.implicit_header = true;
    c.crc_enabled = true;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    LD_EXPECT_EQ(static_cast<std::uint8_t>(spi.reg(reg::kModemConfig1) & 0x01u),
                 std::uint8_t{0x01});
    LD_EXPECT_EQ(spi.reg(reg::kDetectionOptimize), std::uint8_t{0x05});
    LD_EXPECT_EQ(spi.reg(reg::kDetectionThreshold), std::uint8_t{0x0C});
    return true;
}

bool TestTcxoEnabledSetsTcxoInputBit() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.tcxo_enabled = true;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    LD_EXPECT_EQ(static_cast<std::uint8_t>(spi.reg(reg::kTcxo) & 0x10u),
                 std::uint8_t{0x10});
    return true;
}

bool TestTcxoDisabledLeavesXtalDefault() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.tcxo_enabled = false;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kTcxo) & 0x10u, std::uint8_t{0});
    return true;
}

bool TestInvertIqWritesBothRegisters() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.invert_iq = true;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    LD_EXPECT_EQ(static_cast<std::uint8_t>(spi.reg(reg::kInvertIq) & 0x40u),
                 std::uint8_t{0x40});
    LD_EXPECT_EQ(spi.reg(reg::kInvertIq2), std::uint8_t{0x19});
    return true;
}

bool TestInvertIqDisabledKeepsDefaults() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.invert_iq = false;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kInvertIq) & 0x40u, std::uint8_t{0});
    return true;
}

bool TestBeginCalibratesRxImage() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    bool saw_cal = false;
    for (const auto& w : spi.writes()) {
        if (w.reg == reg::kImageCal && (w.value & 0x40u) != 0u) saw_cal = true;
    }
    LD_EXPECT(saw_cal);
    return true;
}

bool TestSymbolTimeoutUsesConfigValueDirectly() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.symbol_timeout = 0x140;  // 320, fits in 10 bits
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kSymbTimeoutLsb), std::uint8_t{0x40});
    LD_EXPECT_EQ(static_cast<std::uint8_t>(spi.reg(reg::kModemConfig2) & 0x03u),
                 std::uint8_t{0x01});
    return true;
}

bool TestInitSetsTxBaseTo0AndRxBaseTo128() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kFifoTxBaseAddr), std::uint8_t{0});
    LD_EXPECT_EQ(spi.reg(reg::kFifoRxBaseAddr), std::uint8_t{128});
    return true;
}

bool TestBeginSx1278At433MHzWritesCorrectFrf() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.chip = ChipModel::SX1278;
    c.frequency_hz = 433'920'000u;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);

    const std::uint64_t frf = (static_cast<std::uint64_t>(433'920'000u) << 19) / 32'000'000ull;
    LD_EXPECT_EQ(spi.reg(reg::kFrMsb), static_cast<std::uint8_t>((frf >> 16) & 0xFF));
    LD_EXPECT_EQ(spi.reg(reg::kFrMid), static_cast<std::uint8_t>((frf >> 8) & 0xFF));
    LD_EXPECT_EQ(spi.reg(reg::kFrLsb), static_cast<std::uint8_t>(frf & 0xFF));
    return true;
}

bool TestBeginSx1278RejectsHighBandFrequency() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.chip = ChipModel::SX1278;
    c.frequency_hz = 868'000'000u;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::InvalidConfig);
    return true;
}

bool TestStandbyToTxVerifiesOpMode() {
    // Simulate a chip that has gone fully dead: writes silently swallowed AND
    // reads return 0x00 (no LoRa bit set). The relaxed verify only checks the
    // LoRa-mode bit, so we must clear it explicitly to trigger the mismatch.
    FakeSpiDevice good;
    SX127xDriver drv_ok(good);
    LoRaConfig c = MakeCfg();
    c.auto_reset = false;
    LD_EXPECT_EQ(drv_ok.begin(c), LoRaError::OK);
    good.set_register(reg::kOpMode, 0x00);  // chip "died" — LoRa bit cleared
    good.set_dead_after_writes(reg::kOpMode, 0);
    const std::uint8_t buf[2] = {0xAA, 0x55};
    LD_EXPECT_EQ(drv_ok.start_transmit(buf, 2, 1000), LoRaError::SpiVerifyMismatch);
    return true;
}

bool TestBeginPulsesResetWhenAutoResetTrue() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.auto_reset = true;
    int reset_calls = 0;
    SX127xDriver::s_reset_hook_ = [&reset_calls]() { ++reset_calls; };
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    SX127xDriver::s_reset_hook_ = nullptr;
    LD_EXPECT_EQ(reset_calls, 1);
    return true;
}

bool TestBeginSkipsResetWhenAutoResetFalse() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.auto_reset = false;
    int reset_calls = 0;
    SX127xDriver::s_reset_hook_ = [&reset_calls]() { ++reset_calls; };
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    SX127xDriver::s_reset_hook_ = nullptr;
    LD_EXPECT_EQ(reset_calls, 0);
    return true;
}

bool TestSkipImageCalibrationOmitsImageCalWrite() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.skip_image_calibration = true;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    // No write with bit 6 set on RegImageCal should appear in the log.
    for (const auto& w : spi.writes()) {
        if (w.reg == reg::kImageCal && (w.value & 0x40u) != 0u) {
            return false;
        }
    }
    return true;
}

bool TestErrata23WritesIfFreqRegisters() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.bandwidth_hz = 125'000u;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kIfFreq1), std::uint8_t{0x40});
    LD_EXPECT_EQ(spi.reg(reg::kIfFreq2), std::uint8_t{0x00});
    return true;
}

bool TestBeginEndBeginCycleSucceeds() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.auto_reset = false;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    drv.end();
    // Second begin must re-run the full init sequence, not return AlreadyInitialized.
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    return true;
}

bool TestBeginEndBeginAppliesNewConfig() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c1 = MakeCfg();
    c1.auto_reset = false;
    c1.sync_word = 0x12;
    LD_EXPECT_EQ(drv.begin(c1), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kSyncWord), std::uint8_t{0x12});
    drv.end();

    LoRaConfig c2 = c1;
    c2.sync_word = 0x34;
    LD_EXPECT_EQ(drv.begin(c2), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kSyncWord), std::uint8_t{0x34});
    return true;
}

int main() {
    LD_RUN(TestBeginRejectsInvalidConfig);
    LD_RUN(TestBeginRejectsMissingChip);
    LD_RUN(TestBeginSucceedsAndEntersStandby);
    LD_RUN(TestBeginWritesFrequencyRegisters868);
    LD_RUN(TestBeginAppliesSyncWord);
    LD_RUN(TestBeginAppliesLdroForSf12);
    LD_RUN(TestBeginAppliesAgcAuto);
    LD_RUN(TestBeginAppliesPaBoost14dBm);
    LD_RUN(TestBeginEnablesPaDacForHighPower);
    LD_RUN(TestBeginAppliesOcp100mA);
    LD_RUN(TestBeginClearsIrqFlags);
    LD_RUN(TestBeginRejectsSpiFailure);
    LD_RUN(TestBeginPulsesResetWhenAutoResetTrue);
    LD_RUN(TestBeginSkipsResetWhenAutoResetFalse);
    LD_RUN(TestBeginDetectsDeadOpModeRegister);
    LD_RUN(TestStandbyToTxVerifiesOpMode);
    LD_RUN(TestSf6ImplicitHeaderInitOk);
    LD_RUN(TestTcxoEnabledSetsTcxoInputBit);
    LD_RUN(TestTcxoDisabledLeavesXtalDefault);
    LD_RUN(TestInvertIqWritesBothRegisters);
    LD_RUN(TestInvertIqDisabledKeepsDefaults);
    LD_RUN(TestBeginCalibratesRxImage);
    LD_RUN(TestSymbolTimeoutUsesConfigValueDirectly);
    LD_RUN(TestInitSetsTxBaseTo0AndRxBaseTo128);
    LD_RUN(TestBeginSx1278At433MHzWritesCorrectFrf);
    LD_RUN(TestBeginSx1278RejectsHighBandFrequency);
    LD_RUN(TestSkipImageCalibrationOmitsImageCalWrite);
    LD_RUN(TestErrata23WritesIfFreqRegisters);
    LD_RUN(TestBeginEndBeginCycleSucceeds);
    LD_RUN(TestBeginEndBeginAppliesNewConfig);
    return loradriver::test::report();
}
