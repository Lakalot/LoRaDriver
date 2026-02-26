#pragma once

#include <cstddef>
#include <cstdint>

#include "loradriver/incident_snapshot.hpp"

namespace loradriver {

enum class IncidentCategory : std::uint16_t {
  kTimeoutRelated = 1000,
  kIrqAnomaly = 2000,
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
