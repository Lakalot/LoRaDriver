#include "loradriver/lora_config.hpp"
#include "loradriver/lora_error.hpp"
#include "test_runner.hpp"

using loradriver::ChipModel;
using loradriver::LoRaConfig;
using loradriver::LoRaError;
using loradriver::PaOutput;

static LoRaConfig MakeValidSx1276() {
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

static LoRaConfig MakeValidSx1278() {
    LoRaConfig c = MakeValidSx1276();
    c.chip = ChipModel::SX1278;
    c.frequency_hz = 433'920'000u;
    return c;
}

bool TestDefaultIsValid() {
    LoRaConfig c = MakeValidSx1276();
    LD_EXPECT_EQ(c.validate(), LoRaError::OK);
    return true;
}

bool TestRejectsSx1278WithHighBand() {
    LoRaConfig c = MakeValidSx1278();
    c.frequency_hz = 868'000'000u;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    return true;
}

bool TestAcceptsSx1276WithHighBand() {
    LoRaConfig c = MakeValidSx1276();
    c.frequency_hz = 915'000'000u;
    LD_EXPECT_EQ(c.validate(), LoRaError::OK);
    return true;
}

bool TestRejectsBwNotInList() {
    LoRaConfig c = MakeValidSx1276();
    c.bandwidth_hz = 100'000u;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    return true;
}

bool TestRejectsBw500OnSx1278LowBand() {
    LoRaConfig c = MakeValidSx1278();
    c.bandwidth_hz = 500'000u;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    return true;
}

bool TestRejectsSfOutOfRange() {
    LoRaConfig c = MakeValidSx1276();
    c.spreading_factor = 5;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    c.spreading_factor = 13;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    return true;
}

bool TestSf6RequiresImplicitHeader() {
    LoRaConfig c = MakeValidSx1276();
    c.spreading_factor = 6;
    c.implicit_header = false;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    c.implicit_header = true;
    LD_EXPECT_EQ(c.validate(), LoRaError::OK);
    return true;
}

bool TestRejectsCodingRateOutOfRange() {
    LoRaConfig c = MakeValidSx1276();
    c.coding_rate = 4;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    c.coding_rate = 9;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    return true;
}

bool TestRejectsPreambleTooShort() {
    LoRaConfig c = MakeValidSx1276();
    c.preamble_length = 5;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    return true;
}

bool TestRejectsOcpOutOfRange() {
    LoRaConfig c = MakeValidSx1276();
    c.ocp_ma = 30;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    c.ocp_ma = 250;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    return true;
}

bool TestRejectsTxPowerForPaBoost() {
    LoRaConfig c = MakeValidSx1276();
    c.pa_output = PaOutput::PaBoost;
    c.tx_power_dbm = 1;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    c.tx_power_dbm = 21;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    return true;
}

bool TestAcceptsTxPowerPaDacRange() {
    LoRaConfig c = MakeValidSx1276();
    c.pa_output = PaOutput::PaBoost;
    c.tx_power_dbm = 20;
    LD_EXPECT_EQ(c.validate(), LoRaError::OK);
    return true;
}

bool TestRejectsTxPowerForRfo() {
    LoRaConfig c = MakeValidSx1276();
    c.pa_output = PaOutput::Rfo;
    c.tx_power_dbm = -1;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    c.tx_power_dbm = 15;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    return true;
}

bool TestRejectsMissingPins() {
    LoRaConfig c = MakeValidSx1276();
    c.pin_ss = -1;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    c.pin_ss = 5; c.pin_reset = -1;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    c.pin_reset = 14; c.pin_dio0 = -1;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    return true;
}

bool TestLdroRequiredForSlowSymbols() {
    LoRaConfig c = MakeValidSx1276();
    c.spreading_factor = 12; c.bandwidth_hz = 125'000u;
    LD_EXPECT(c.ldro_required());
    c.spreading_factor = 11; c.bandwidth_hz = 125'000u;
    LD_EXPECT(c.ldro_required());
    c.spreading_factor = 7;  c.bandwidth_hz = 125'000u;
    LD_EXPECT(!c.ldro_required());
    return true;
}

int main() {
    LD_RUN(TestDefaultIsValid);
    LD_RUN(TestRejectsSx1278WithHighBand);
    LD_RUN(TestAcceptsSx1276WithHighBand);
    LD_RUN(TestRejectsBwNotInList);
    LD_RUN(TestRejectsBw500OnSx1278LowBand);
    LD_RUN(TestRejectsSfOutOfRange);
    LD_RUN(TestSf6RequiresImplicitHeader);
    LD_RUN(TestRejectsCodingRateOutOfRange);
    LD_RUN(TestRejectsPreambleTooShort);
    LD_RUN(TestRejectsOcpOutOfRange);
    LD_RUN(TestRejectsTxPowerForPaBoost);
    LD_RUN(TestAcceptsTxPowerPaDacRange);
    LD_RUN(TestRejectsTxPowerForRfo);
    LD_RUN(TestRejectsMissingPins);
    LD_RUN(TestLdroRequiredForSlowSymbols);
    return loradriver::test::report();
}
