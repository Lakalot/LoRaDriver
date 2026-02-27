#pragma once

#include <cstddef>
#include <cstdint>

#include "loradriver/incident_snapshot.hpp"

namespace loradriver {

/// Stable incident category codes for field classification.
/// Code ranges are reserved as follows:
///   1000-1999: Timeout-related incidents
///   2000-2999: IRQ anomaly incidents (reserved - no LoRaError maps here in V1)
///   3000-3999: Configuration errors
///   4000-4999: Runtime/FSM transition incidents
///   5000-5999: Hardware faults
///   6000-8999: Reserved for future categories
///   9000-9999: Unknown/uncategorized
/// New categories MUST use a new range; existing codes MUST NOT be reused.
enum class IncidentCategory : std::uint16_t {
  kTimeoutRelated = 1000,
  kIrqAnomaly = 2000,      ///< Reserved for V1-bis when IRQ-specific errors are added.
  kConfigError = 3000,
  kRuntimeTransition = 4000,
  kHardwareFault = 5000,
  kUnknown = 9000
};

enum class IncidentSeverity : std::uint8_t {
  kInfo = 0,
  kWarning = 1,
  kCritical = 2
};

enum class EscalationPath : std::uint8_t {
  kSupportL1 = 0,
  kSupportL2 = 1,
  kEngineering = 2,
  kHardwareTeam = 3
};

struct IncidentClassification {
  static constexpr std::size_t kPlaybookNameSize = 32;

  std::uint8_t taxonomy_version_major = 1;
  std::uint8_t taxonomy_version_minor = 0;
  IncidentCategory category = IncidentCategory::kUnknown;
  IncidentSeverity severity = IncidentSeverity::kInfo;
  EscalationPath escalation_path = EscalationPath::kSupportL1;

  /// Fixed-size playbook name buffer. Always null-terminated.
  /// Default-initialized to all zeros (empty string).
  /// Maximum playbook name length is kPlaybookNameSize - 1 characters.
  char suggested_playbook[kPlaybookNameSize] = {};

  [[nodiscard]] const char* categoryToString() const noexcept;
  [[nodiscard]] const char* severityToString() const noexcept;
  [[nodiscard]] const char* escalationToString() const noexcept;
};

[[nodiscard]] IncidentClassification classifyIncident(const IncidentSnapshot& snapshot) noexcept;

[[nodiscard]] const char* getIncidentCategoryName(IncidentCategory category) noexcept;
[[nodiscard]] const char* getIncidentSeverityName(IncidentSeverity severity) noexcept;
[[nodiscard]] const char* getEscalationPathName(EscalationPath path) noexcept;

}  // namespace loradriver
