#include "loradriver/incident_classification.hpp"

namespace loradriver {

namespace {

struct CategoryMapping {
  LoRaError error;
  IncidentCategory category;
  IncidentSeverity severity;
  EscalationPath escalation;
  const char* playbook;
};

constexpr CategoryMapping kCategoryMappings[] = {
    {LoRaError::kTimeoutRecovered, IncidentCategory::kTimeoutRelated, IncidentSeverity::kWarning,
     EscalationPath::kSupportL1, "timeout-recovery"},
    {LoRaError::kTimeoutRecoveryFailure, IncidentCategory::kTimeoutRelated, IncidentSeverity::kCritical,
     EscalationPath::kEngineering, "timeout-recovery-failure"},
    {LoRaError::kInvalidConfig, IncidentCategory::kConfigError, IncidentSeverity::kWarning,
     EscalationPath::kSupportL2, "config-validation"},
    {LoRaError::kUnsupportedProfile, IncidentCategory::kConfigError, IncidentSeverity::kWarning,
     EscalationPath::kSupportL2, "profile-validation"},
    {LoRaError::kHardwareInitFailure, IncidentCategory::kHardwareFault, IncidentSeverity::kCritical,
     EscalationPath::kHardwareTeam, "hardware-init"},
    {LoRaError::kTransitionGuardFailure, IncidentCategory::kRuntimeTransition, IncidentSeverity::kWarning,
     EscalationPath::kEngineering, "state-transition"},
    {LoRaError::kAlreadyInitialized, IncidentCategory::kConfigError, IncidentSeverity::kInfo,
     EscalationPath::kSupportL1, "init-check"},
    {LoRaError::kNotInitialized, IncidentCategory::kConfigError, IncidentSeverity::kWarning,
     EscalationPath::kSupportL1, "init-required"},
    {LoRaError::kNotImplemented, IncidentCategory::kRuntimeTransition, IncidentSeverity::kWarning,
     EscalationPath::kEngineering, "feature-stub"},
};

void copyPlaybookName(char* dest, const char* src, std::size_t size) noexcept {
  std::size_t i = 0;
  while (i < size - 1 && src[i] != '\0') {
    dest[i] = src[i];
    ++i;
  }
  dest[i] = '\0';
}

}  // namespace

const char* getIncidentCategoryName(IncidentCategory category) noexcept {
  switch (category) {
    case IncidentCategory::kTimeoutRelated:
      return "TimeoutRelated";
    case IncidentCategory::kIrqAnomaly:
      return "IrqAnomaly";
    case IncidentCategory::kConfigError:
      return "ConfigError";
    case IncidentCategory::kRuntimeTransition:
      return "RuntimeTransition";
    case IncidentCategory::kHardwareFault:
      return "HardwareFault";
    case IncidentCategory::kUnknown:
    default:
      return "Unknown";
  }
}

const char* getIncidentSeverityName(IncidentSeverity severity) noexcept {
  switch (severity) {
    case IncidentSeverity::kInfo:
      return "Info";
    case IncidentSeverity::kWarning:
      return "Warning";
    case IncidentSeverity::kCritical:
      return "Critical";
    default:
      return "Unknown";
  }
}

const char* getEscalationPathName(EscalationPath path) noexcept {
  switch (path) {
    case EscalationPath::kSupportL1:
      return "SupportL1";
    case EscalationPath::kSupportL2:
      return "SupportL2";
    case EscalationPath::kEngineering:
      return "Engineering";
    case EscalationPath::kHardwareTeam:
      return "HardwareTeam";
    default:
      return "Unknown";
  }
}

const char* IncidentClassification::categoryToString() const noexcept {
  return getIncidentCategoryName(category);
}

const char* IncidentClassification::severityToString() const noexcept {
  return getIncidentSeverityName(severity);
}

const char* IncidentClassification::escalationToString() const noexcept {
  return getEscalationPathName(escalation_path);
}

IncidentClassification classifyIncident(const IncidentSnapshot& snapshot) noexcept {
  IncidentClassification result;
  result.taxonomy_version_major = 1;
  result.taxonomy_version_minor = 0;

  if (snapshot.error == LoRaError::kOk) {
    result.category = IncidentCategory::kUnknown;
    result.severity = IncidentSeverity::kInfo;
    result.escalation_path = EscalationPath::kSupportL1;
    copyPlaybookName(result.suggested_playbook, "no-incident", IncidentClassification::kPlaybookNameSize);
    return result;
  }

  for (const auto& mapping : kCategoryMappings) {
    if (mapping.error == snapshot.error) {
      result.category = mapping.category;
      result.severity = mapping.severity;
      result.escalation_path = mapping.escalation;
      copyPlaybookName(result.suggested_playbook, mapping.playbook, IncidentClassification::kPlaybookNameSize);
      return result;
    }
  }

  result.category = IncidentCategory::kUnknown;
  result.severity = IncidentSeverity::kWarning;
  result.escalation_path = EscalationPath::kSupportL1;
  copyPlaybookName(result.suggested_playbook, "unknown-error", IncidentClassification::kPlaybookNameSize);
  return result;
}

}  // namespace loradriver
