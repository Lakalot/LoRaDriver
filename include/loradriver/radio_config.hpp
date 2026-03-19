#pragma once

#include <cstdint>

namespace loradriver {

struct RadioConfig {
  enum class Chip {
    kSx1276,
    kSx1278,
    kSx126xStub
  };

  enum class Band {
    k433,
    k868
  };

  enum class DioRouting {
    kDio0Only,
    kDio0Dio1
  };

  // Hardware profile fields
  Chip chip = Chip::kSx1276;
  Band band = Band::k868;
  DioRouting dio_routing = DioRouting::kDio0Only;
  std::uint32_t spi_frequency_hz = 8000000;

  // LoRa modulation parameter fields (V1 defaults)
  std::uint8_t spreading_factor = 9;         // SF7-SF12; default SF9
  std::uint32_t bandwidth_khz = 125;         // 125 / 250 / 500 kHz; default 125
  std::uint8_t coding_rate_denominator = 5;  // 5=CR4/5 .. 8=CR4/8; default 5
  std::uint8_t sync_word = 0x12;             // LoRa sync word; default 0x12
  std::int8_t tx_power_dbm = 14;             // TX power in dBm; default 14
  bool crc_enabled = true;                   // Payload CRC; default on
  std::uint16_t preamble_length = 8;         // Preamble symbols; default 8

  static constexpr std::uint32_t kMinSpiFrequencyHz = 4000000;
  static constexpr std::uint32_t kMaxSpiFrequencyHz = 8000000;

  [[nodiscard]] bool isV1Supported() const noexcept {
    const bool chip_supported = chip == Chip::kSx1276 || chip == Chip::kSx1278;
    const bool band_supported = band == Band::k433 || band == Band::k868;
    const bool dio_supported = dio_routing == DioRouting::kDio0Only || dio_routing == DioRouting::kDio0Dio1;
    return chip_supported && band_supported && dio_supported;
  }

  [[nodiscard]] bool isSpiFrequencyInRange() const noexcept {
    return spi_frequency_hz >= kMinSpiFrequencyHz && spi_frequency_hz <= kMaxSpiFrequencyHz;
  }
};

}  // namespace loradriver
