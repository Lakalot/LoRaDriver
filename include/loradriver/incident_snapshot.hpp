#pragma once

#include <cstddef>
#include <cstdint>

#include "loradriver/lora_error.hpp"
#include "loradriver/radio_config.hpp"
#include "loradriver/version.hpp"

namespace loradriver {

/// Comprehensive incident capture for support handoff.
///
/// Fields intentionally mirror DiagnosticContext for completeness, but
/// IncidentSnapshot is a standalone value type for serialization and
/// cross-boundary transport. DiagnosticContext is a lightweight internal
/// struct for last-operation tracking within the driver.
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
  std::uint32_t spi_frequency_hz = 8000000;

  // Active LoRa modulation parameters (required for deterministic reproduction)
  std::uint8_t spreading_factor = 9;
  std::uint32_t bandwidth_khz = 125;
  std::uint8_t coding_rate_denominator = 5;
  std::uint8_t sync_word = 0x12;
  std::int8_t tx_power_dbm = 14;
  bool crc_enabled = true;
  std::uint16_t preamble_length = 8;

  static constexpr std::size_t kFormatBufferSize = 256;

  std::size_t formatTo(char* buffer, std::size_t buffer_size) const noexcept;
};

}  // namespace loradriver
