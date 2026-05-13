#include "fake_spi_device.hpp"
#include "test_runner.hpp"

using loradriver::LoRaError;
using loradriver::test::FakeSpiDevice;

bool TestDefaultVersionIs0x12() {
    FakeSpiDevice s;
    std::uint8_t v = 0;
    LD_EXPECT_EQ(s.read_register(0x42, v), LoRaError::OK);
    LD_EXPECT_EQ(v, std::uint8_t{0x12});
    return true;
}

bool TestWriteThenRead() {
    FakeSpiDevice s;
    LD_EXPECT_EQ(s.write_register(0x10, 0xAB), LoRaError::OK);
    std::uint8_t v = 0;
    LD_EXPECT_EQ(s.read_register(0x10, v), LoRaError::OK);
    LD_EXPECT_EQ(v, std::uint8_t{0xAB});
    return true;
}

bool TestBurstWriteThenRead() {
    FakeSpiDevice s;
    const std::uint8_t in[4] = {1, 2, 3, 4};
    LD_EXPECT_EQ(s.burst_write(0x20, in, 4), LoRaError::OK);
    std::uint8_t out[4] = {};
    LD_EXPECT_EQ(s.burst_read(0x20, out, 4), LoRaError::OK);
    for (int i = 0; i < 4; ++i)
        LD_EXPECT_EQ(out[i], in[i]);
    return true;
}

bool TestFailWritesReturnsSpiFailure() {
    FakeSpiDevice s;
    s.fail_writes(true);
    LD_EXPECT_EQ(s.write_register(0x10, 0x55), LoRaError::SpiFailure);
    return true;
}

bool TestFailReadsReturnsSpiFailure() {
    FakeSpiDevice s;
    s.fail_reads(true);
    std::uint8_t v = 0;
    LD_EXPECT_EQ(s.read_register(0x42, v), LoRaError::SpiFailure);
    return true;
}

bool TestNullBuffersRejected() {
    FakeSpiDevice s;
    LD_EXPECT_EQ(s.burst_write(0x10, nullptr, 4), LoRaError::NullArgument);
    LD_EXPECT_EQ(s.burst_read(0x10, nullptr, 4), LoRaError::NullArgument);
    return true;
}

bool TestWritesLogRecorded() {
    FakeSpiDevice s;
    (void)s.write_register(0x01, 0xAA);
    (void)s.write_register(0x02, 0xBB);
    LD_EXPECT_EQ(s.writes().size(), std::size_t{2});
    LD_EXPECT_EQ(s.writes()[0].reg, std::uint8_t{0x01});
    LD_EXPECT_EQ(s.writes()[0].value, std::uint8_t{0xAA});
    return true;
}

int main() {
    LD_RUN(TestDefaultVersionIs0x12);
    LD_RUN(TestWriteThenRead);
    LD_RUN(TestBurstWriteThenRead);
    LD_RUN(TestFailWritesReturnsSpiFailure);
    LD_RUN(TestFailReadsReturnsSpiFailure);
    LD_RUN(TestNullBuffersRejected);
    LD_RUN(TestWritesLogRecorded);
    return loradriver::test::report();
}
