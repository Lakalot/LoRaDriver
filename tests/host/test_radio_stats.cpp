#include "loradriver/lora_packet.hpp"
#include "loradriver/radio_event.hpp"
#include "loradriver/radio_stats.hpp"
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
    p.snr_q4 = -20;  // -5.0 dB
    LD_EXPECT(p.snr_db() < -4.9f && p.snr_db() > -5.1f);
    return true;
}

bool TestEventEnumDistinct() {
    LD_EXPECT(static_cast<int>(RadioEvent::TxDone)
              != static_cast<int>(RadioEvent::RxDone));
    return true;
}

int main() {
    LD_RUN(TestStatsDefaultZeroed);
    LD_RUN(TestStatsSnapshotByValue);
    LD_RUN(TestPacketSnrConversion);
    LD_RUN(TestEventEnumDistinct);
    return loradriver::test::report();
}
