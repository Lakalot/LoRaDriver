#include "loradriver/lora_packet.hpp"
#include "loradriver/radio_event.hpp"
#include "loradriver/radio_stats.hpp"
#include "loradriver/version.hpp"
#include "test_runner.hpp"

using loradriver::LoRaPacket;
using loradriver::RadioEvent;
using loradriver::RadioStats;

bool TestStatsDefaultZeroed() {
    RadioStats s{};
    LD_EXPECT_EQ(s.tx_done, 0u);
    LD_EXPECT_EQ(s.rx_done, 0u);
    LD_EXPECT_EQ(s.irq_overflows, 0u);
    LD_EXPECT_EQ(s.last_rssi_dbm, 0);
    return true;
}

bool TestStatsSnapshotByValue() {
    RadioStats s{};
    s.tx_done = 7;
    RadioStats copy = s;
    s.tx_done = 99;
    LD_EXPECT_EQ(copy.tx_done, 7u);
    return true;
}

bool TestPacketSnrConversion() {
    LoRaPacket p{};
    p.snr_q4 = -20; // -5.0 dB
    LD_EXPECT(p.snr_db() < -4.9f && p.snr_db() > -5.1f);
    return true;
}

bool TestEventEnumDistinct() {
    LD_EXPECT(static_cast<int>(RadioEvent::TxDone) != static_cast<int>(RadioEvent::RxDone));
    return true;
}

bool TestVersionAccessors() {
    LD_EXPECT_EQ(loradriver::version_major(), std::uint8_t{1});
    LD_EXPECT_EQ(loradriver::version_minor(), std::uint8_t{3});
    LD_EXPECT_EQ(loradriver::version_patch(), std::uint8_t{0});
    const char* s = loradriver::version_string();
    LD_EXPECT(s[0] == '1' && s[1] == '.' && s[2] == '3');
    return true;
}

int main() {
    LD_RUN(TestStatsDefaultZeroed);
    LD_RUN(TestStatsSnapshotByValue);
    LD_RUN(TestPacketSnrConversion);
    LD_RUN(TestEventEnumDistinct);
    LD_RUN(TestVersionAccessors);
    return loradriver::test::report();
}
