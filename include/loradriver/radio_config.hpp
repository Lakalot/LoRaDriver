#pragma once

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

  [[nodiscard]] bool isV1Supported() const noexcept {
    const bool chip_supported = chip == Chip::kSx1276 || chip == Chip::kSx1278;
    const bool band_supported = band == Band::k433 || band == Band::k868;
    const bool dio_supported = dio_routing == DioRouting::kDio0Only || dio_routing == DioRouting::kDio0Dio1;
    return chip_supported && band_supported && dio_supported;
  }
};

}  // namespace loradriver
