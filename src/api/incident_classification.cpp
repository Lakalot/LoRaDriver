#include "loradriver/incident_classification.hpp"

namespace loradriver {

namespace {

// Manual null-terminated copy instead of std::strncpy to avoid pulling in
// <cstring> and to guarantee deterministic null-termination without
// platform-dependent padding behavior. Safe for embedded fixed-size buffers.
void copyPlaybookName(char* dest, const char* src, std::size_t size) noexcept {
  if (dest == nullptr || size == 0) {
    return;
  }
  if (src == nullptr) {
    dest[0] = '\0';
    return;
  }
  std::size_t i = 0;
  while (i < size - 1 && src[i] != '\0') {
    dest[i] = src[i];
    ++i;
  }
  dest[i] = '\0';
}

void applyClassification(IncidentClassification& result, IncidentCategory category,
                         IncidentSeverity severity, EscalationPath escalation,
                         const char* playbook) noexcept {
  result.category = category;
  result.severity = severity;
  result.escalation_path = escalation;
  copyPlaybookName(result.suggested_playbook, playbook, IncidentClassification::kPlaybookNameSize);
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

  // Deterministic switch-based lookup table (not linear scan) per architecture spec.
  switch (snapshot.error) {
    case LoRaError::kOk:
      applyClassification(result, IncidentCategory::kUnknown, IncidentSeverity::kInfo,
                          EscalationPath::kSupportL1, "no-incident");
      return result;

    case LoRaError::kTimeoutRecovered:
      applyClassification(result, IncidentCategory::kTimeoutRelated, IncidentSeverity::kWarning,
                          EscalationPath::kSupportL1, "timeout-recovery");
      return result;

    case LoRaError::kTimeoutRecoveryFailure:
      applyClassification(result, IncidentCategory::kTimeoutRelated, IncidentSeverity::kCritical,
                          EscalationPath::kEngineering, "timeout-recovery-failure");
      return result;

    case LoRaError::kInvalidConfig:
      applyClassification(result, IncidentCategory::kConfigError, IncidentSeverity::kWarning,
                          EscalationPath::kSupportL2, "config-validation");
      return result;

    case LoRaError::kUnsupportedProfile:
      applyClassification(result, IncidentCategory::kConfigError, IncidentSeverity::kWarning,
                          EscalationPath::kSupportL2, "profile-validation");
      return result;

    case LoRaError::kHardwareInitFailure:
      applyClassification(result, IncidentCategory::kHardwareFault, IncidentSeverity::kCritical,
                          EscalationPath::kHardwareTeam, "hardware-init");
      return result;

    case LoRaError::kTransitionGuardFailure:
      applyClassification(result, IncidentCategory::kRuntimeTransition, IncidentSeverity::kWarning,
                          EscalationPath::kEngineering, "state-transition");
      return result;

    case LoRaError::kAlreadyInitialized:
      applyClassification(result, IncidentCategory::kConfigError, IncidentSeverity::kInfo,
                          EscalationPath::kSupportL1, "init-check");
      return result;

    case LoRaError::kNotInitialized:
      applyClassification(result, IncidentCategory::kConfigError, IncidentSeverity::kWarning,
                          EscalationPath::kSupportL1, "init-required");
      return result;

    case LoRaError::kNotImplemented:
      applyClassification(result, IncidentCategory::kRuntimeTransition, IncidentSeverity::kWarning,
                          EscalationPath::kEngineering, "feature-stub");
      return result;

    default:
      // Unmapped error codes fall to unknown with warning severity.
      applyClassification(result, IncidentCategory::kUnknown, IncidentSeverity::kWarning,
                          EscalationPath::kSupportL1, "unknown-error");
      return result;
  }
}

}  // namespace loradriver
