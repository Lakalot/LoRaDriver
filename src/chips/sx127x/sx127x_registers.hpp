#pragma once

#include <cstdint>

// Datasheet references: SX1276/77/78/79 rev 7 (Semtech DS_SX1276-7-8-9_W_APP_V7.pdf)
// Errata note: SX1276_77_8_ErrataNote_1.1_STD.pdf

namespace loradriver::chips::sx127x::reg {

constexpr std::uint8_t kFifo               = 0x00;
constexpr std::uint8_t kOpMode             = 0x01;
constexpr std::uint8_t kFrMsb              = 0x06;
constexpr std::uint8_t kFrMid              = 0x07;
constexpr std::uint8_t kFrLsb              = 0x08;
constexpr std::uint8_t kPaConfig           = 0x09;
constexpr std::uint8_t kOcp                = 0x0B;
constexpr std::uint8_t kLna                = 0x0C;
constexpr std::uint8_t kFifoAddrPtr        = 0x0D;
constexpr std::uint8_t kFifoTxBaseAddr     = 0x0E;
constexpr std::uint8_t kFifoRxBaseAddr     = 0x0F;
constexpr std::uint8_t kFifoRxCurrentAddr  = 0x10;
constexpr std::uint8_t kIrqFlagsMask       = 0x11;
constexpr std::uint8_t kIrqFlags           = 0x12;
constexpr std::uint8_t kRxNbBytes          = 0x13;
constexpr std::uint8_t kPktSnrValue        = 0x19;
constexpr std::uint8_t kPktRssiValue       = 0x1A;
constexpr std::uint8_t kRssiValue          = 0x1B;
constexpr std::uint8_t kModemConfig1       = 0x1D;
constexpr std::uint8_t kModemConfig2       = 0x1E;
constexpr std::uint8_t kSymbTimeoutLsb     = 0x1F;
constexpr std::uint8_t kPreambleMsb        = 0x20;
constexpr std::uint8_t kPreambleLsb        = 0x21;
constexpr std::uint8_t kPayloadLength      = 0x22;
constexpr std::uint8_t kModemConfig3       = 0x26;
constexpr std::uint8_t kFeiMsb             = 0x28;
constexpr std::uint8_t kFeiMid             = 0x29;
constexpr std::uint8_t kFeiLsb             = 0x2A;
constexpr std::uint8_t kRssiWideband       = 0x2C;
constexpr std::uint8_t kDetectionOptimize  = 0x31;
constexpr std::uint8_t kInvertIq           = 0x33;
constexpr std::uint8_t kHighBwOptimize1    = 0x36;
constexpr std::uint8_t kDetectionThreshold = 0x37;
constexpr std::uint8_t kSyncWord           = 0x39;
constexpr std::uint8_t kHighBwOptimize2    = 0x3A;
constexpr std::uint8_t kImageCal           = 0x3B;  // FSK mode access
constexpr std::uint8_t kInvertIq2          = kImageCal;  // LoRa mode alias
constexpr std::uint8_t kDioMapping1        = 0x40;
constexpr std::uint8_t kVersion            = 0x42;
constexpr std::uint8_t kPaDac              = 0x4D;

}  // namespace loradriver::chips::sx127x::reg

namespace loradriver::chips::sx127x::opmode {

constexpr std::uint8_t kLoRaModeBit  = 0x80;
constexpr std::uint8_t kFskSleep     = 0x00;
constexpr std::uint8_t kLoRaSleep    = 0x80;
constexpr std::uint8_t kLoRaStandby  = 0x81;
constexpr std::uint8_t kLoRaTx       = 0x83;
constexpr std::uint8_t kLoRaRxCont   = 0x85;
constexpr std::uint8_t kLoRaRxSingle = 0x86;
constexpr std::uint8_t kLoRaCad      = 0x87;

}  // namespace loradriver::chips::sx127x::opmode

namespace loradriver::chips::sx127x::irq {

constexpr std::uint8_t kRxTimeout       = 0x80;
constexpr std::uint8_t kRxDone          = 0x40;
constexpr std::uint8_t kPayloadCrcError = 0x20;
constexpr std::uint8_t kValidHeader     = 0x10;
constexpr std::uint8_t kTxDone          = 0x08;
constexpr std::uint8_t kCadDone         = 0x04;
constexpr std::uint8_t kFhssChangeChan  = 0x02;
constexpr std::uint8_t kCadDetected     = 0x01;
constexpr std::uint8_t kClearAll        = 0xFF;

}  // namespace loradriver::chips::sx127x::irq

namespace loradriver::chips::sx127x::dio {

// RegDioMapping1: bits[7:6] = DIO0, [5:4] = DIO1
constexpr std::uint8_t kDio0RxDone = 0x00;
constexpr std::uint8_t kDio0TxDone = 0x40;
constexpr std::uint8_t kDio0CadDone = 0x80;
constexpr std::uint8_t kDio1RxTimeout = 0x00;
constexpr std::uint8_t kDio1FhssChange = 0x10;

}  // namespace loradriver::chips::sx127x::dio
