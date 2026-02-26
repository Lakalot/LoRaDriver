#include "loradriver/incident_snapshot.hpp"

#include <cstdio>

namespace loradriver {

// Format field mapping (stable contract - do not reorder):
//   v  = driver version (major.minor.patch)
//   e  = error code (LoRaError enum value as int)
//   c  = chip (RadioConfig::Chip enum value as int)
//   b  = band (RadioConfig::Band enum value as int)
//   d  = DIO routing (RadioConfig::DioRouting enum value as int)
//   dc = detail diagnostic code (numeric)
//   seq = operation sequence counter
//   ts  = timestamp in milliseconds (0 if no TimestampSource configured)
std::size_t IncidentSnapshot::formatTo(char* buffer, std::size_t buffer_size) const noexcept {
  if (buffer == nullptr || buffer_size == 0) {
    return 0;
  }

  const int chip_val = static_cast<int>(chip);
  const int band_val = static_cast<int>(band);
  const int dio_val = static_cast<int>(dio_routing);
  const int error_val = static_cast<int>(error);

  const int written = std::snprintf(
      buffer, buffer_size,
      "LORADRIVER_INCIDENT:v=%u.%u.%u;e=%d;c=%d;b=%d;d=%d;dc=%d;seq=%u;ts=%u;",
      static_cast<unsigned>(version_major), static_cast<unsigned>(version_minor),
      static_cast<unsigned>(version_patch), error_val, chip_val, band_val, dio_val,
      detail_code, static_cast<unsigned>(sequence), static_cast<unsigned>(timestamp_ms));

  if (written < 0 || static_cast<std::size_t>(written) >= buffer_size) {
    return 0;
  }

  return static_cast<std::size_t>(written);
}

}  // namespace loradriver
