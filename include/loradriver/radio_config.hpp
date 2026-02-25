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

  Chip chip = Chip::kSx1276;
  Band band = Band::k868;
  DioRouting dio_routing = DioRouting::kDio0Only;
  std::uint32_t spi_frequency_hz = 8000000;

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
