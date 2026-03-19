#include "config_validation.hpp"

namespace loradriver {

namespace {

constexpr int kDiagInvalidSpreadingFactor = 2010;
constexpr int kDiagInvalidBandwidth = 2011;
constexpr int kDiagInvalidCodingRate = 2012;
constexpr int kDiagInvalidPreambleLength = 2013;

constexpr std::uint8_t kMinSpreadingFactor = 7;
constexpr std::uint8_t kMaxSpreadingFactor = 12;
constexpr std::uint8_t kMinCodingRateDenominator = 5;
constexpr std::uint8_t kMaxCodingRateDenominator = 8;
constexpr std::uint16_t kMinPreambleLength = 6;

}  // namespace

int ValidateLoRaParams(const RadioConfig& config) noexcept {
  if (config.spreading_factor < kMinSpreadingFactor ||
      config.spreading_factor > kMaxSpreadingFactor) {
    return kDiagInvalidSpreadingFactor;
  }

  if (config.bandwidth_khz != 125 && config.bandwidth_khz != 250 &&
      config.bandwidth_khz != 500) {
    return kDiagInvalidBandwidth;
  }

  if (config.coding_rate_denominator < kMinCodingRateDenominator ||
      config.coding_rate_denominator > kMaxCodingRateDenominator) {
    return kDiagInvalidCodingRate;
  }

  if (config.preamble_length < kMinPreambleLength) {
    return kDiagInvalidPreambleLength;
  }

  return 0;
}

}  // namespace loradriver
