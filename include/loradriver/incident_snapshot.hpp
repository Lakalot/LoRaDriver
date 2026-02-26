#pragma once

#include <cstddef>
#include <cstdint>

#include "loradriver/lora_error.hpp"
#include "loradriver/radio_config.hpp"
#include "loradriver/version.hpp"

namespace loradriver {

struct IncidentSnapshot {
  std::uint8_t version_major = LORADRIVER_VERSION_MAJOR;
  std::uint8_t version_minor = LORADRIVER_VERSION_MINOR;
  std::uint8_t version_patch = LORADRIVER_VERSION_PATCH;
  LoRaError error = LoRaError::kOk;
  int detail_code = 0;
  RadioConfig::Chip chip = RadioConfig::Chip{};
  RadioConfig::Band band = RadioConfig::Band{};
  RadioConfig::DioRouting dio_routing = RadioConfig::DioRouting{};
  std::uint32_t sequence = 0;
  std::uint32_t timestamp_ms = 0;

  static constexpr std::size_t kFormatBufferSize = 256;

  std::size_t formatTo(char* buffer, std::size_t buffer_size) const noexcept;
};

}  // namespace loradriver
