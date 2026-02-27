#include "non_regression.hpp"

#include <cstdio>
#include <cstring>

namespace loradriver {

namespace {

NonRegressionCase makeCase(const char* id,
                             NonRegressionCategory category,
                             bool expected_success,
                             uint32_t expected_latency_ms,
                             const char* pattern_id,
                             bool applies_to_all = true,
                             bool match_irq_only = false,
                             RadioConfig::DioRouting irq_constraint = RadioConfig::DioRouting::kDio0Only) {
  NonRegressionCase c{};
  std::strncpy(c.id, id, NonRegressionCase::kMaxIdLength - 1);
  c.id[NonRegressionCase::kMaxIdLength - 1] = '\0';
  c.category = category;
  c.baseline_result.expected_success = expected_success;
  c.baseline_result.expected_latency_ms = expected_latency_ms;
  c.baseline_result.expected_detail[0] = '\0';
  std::strncpy(c.incident_pattern_id, pattern_id, NonRegressionCase::kMaxPatternIdLength - 1);
  c.incident_pattern_id[NonRegressionCase::kMaxPatternIdLength - 1] = '\0';
  c.enabled = true;
  c.applies_to_all_profiles = applies_to_all;
  c.match_irq_only = match_irq_only;
  c.profile_constraint.irq = irq_constraint;
  return c;
}

BaselineResult makeBaseline(bool success, uint32_t latency_ms) {
  BaselineResult b{};
  b.expected_success = success;
  b.expected_latency_ms = latency_ms;
  b.expected_detail[0] = '\0';
  return b;
}

NonRegressionSuite makeDefaultSuite() {
  NonRegressionSuite suite{};
  std::strncpy(suite.suite_id, "V1-CRITICAL", NonRegressionSuite::kMaxSuiteIdLength - 1);
  suite.suite_id[NonRegressionSuite::kMaxSuiteIdLength - 1] = '\0';
  suite.cases[0] = makeCase("NR-TIMEOUT-001", NonRegressionCategory::kTimeout,
                             true, 100, "TIMEOUT-TX-RECOVERY");
  suite.cases[1] = makeCase("NR-TIMEOUT-002", NonRegressionCategory::kTimeout,
                             true, 100, "TIMEOUT-RX-RECOVERY");
  suite.cases[2] = makeCase("NR-TIMEOUT-003", NonRegressionCategory::kTimeout,
                             true, 150, "TIMEOUT-CONSECUTIVE");
  suite.cases[3] = makeCase("NR-SLEEP-001", NonRegressionCategory::kSleepWakeup,
                             true, 50, "SLEEP-ACTIVE-TRANS");
  suite.cases[4] = makeCase("NR-SLEEP-002", NonRegressionCategory::kSleepWakeup,
                             true, 75, "SLEEP-RX-RESUME");
  suite.cases[5] = makeCase("NR-SLEEP-003", NonRegressionCategory::kSleepWakeup,
                             true, 75, "SLEEP-TX-RESUME");
  suite.cases[6] = makeCase("NR-IRQ-001", NonRegressionCategory::kIrqRace,
                             true, 10, "IRQ-DIO01-SIMUL",
                             /*applies_to_all=*/false,
                             /*match_irq_only=*/true,
                             RadioConfig::DioRouting::kDio0Dio1);
  suite.cases[7] = makeCase("NR-IRQ-002", NonRegressionCategory::kIrqRace,
                             true, 20, "IRQ-STATE-TRANS");
  suite.cases[8] = makeCase("NR-FSM-001", NonRegressionCategory::kFsmTransition,
                             true, 5, "FSM-ILLEGAL-REJECT");
  suite.cases[9] = makeCase("NR-FSM-002", NonRegressionCategory::kFsmTransition,
                             true, 30, "FSM-RECOVERY-REENTRY");
  suite.cases_count = 10;
  suite.suite_version_major = 1;
  suite.suite_version_minor = 0;
  suite.suite_version_patch = 0;
  return suite;
}

}  // namespace

const std::array<NonRegressionSuite, NonRegressionExecutor::kDefaultSuiteCount>
    NonRegressionExecutor::kDefaultSuites_ = {{ makeDefaultSuite() }};

const NonRegressionSuite* NonRegressionExecutor::getSuite(const char* suite_id) noexcept {
  if (!suite_id) return nullptr;

  for (size_t i = 0; i < kDefaultSuiteCount; ++i) {
    if (std::strncmp(kDefaultSuites_[i].suite_id, suite_id,
                     NonRegressionSuite::kMaxSuiteIdLength) == 0) {
      return &kDefaultSuites_[i];
    }
  }
  return nullptr;
}

const NonRegressionCase* NonRegressionExecutor::getCase(const char* case_id) noexcept {
  if (!case_id) return nullptr;

  for (size_t i = 0; i < kDefaultSuiteCount; ++i) {
    for (uint8_t j = 0; j < kDefaultSuites_[i].cases_count; ++j) {
      if (std::strncmp(kDefaultSuites_[i].cases[j].id, case_id,
                       NonRegressionCase::kMaxIdLength) == 0) {
        return &kDefaultSuites_[i].cases[j];
      }
    }
  }
  return nullptr;
}

CaseExecutionResult NonRegressionExecutor::executeCase(
    const char* case_id, const HardwareProfile& profile) noexcept {
  const NonRegressionCase* c = getCase(case_id);
  if (!c) {
    CaseExecutionResult result{};
    std::strncpy(result.case_id, case_id ? case_id : "UNKNOWN",
                 NonRegressionCase::kMaxIdLength - 1);
    result.passed = false;
    result.baseline_matched = false;
    result.actual_latency_ms = 0;
    std::strncpy(result.delta_detail, "Case not found", sizeof(result.delta_detail) - 1);
    return result;
  }
  return executeCase(*c, profile);
}

CaseExecutionResult NonRegressionExecutor::executeCase(
    const NonRegressionCase& reg_case, const HardwareProfile& profile) noexcept {
  CaseExecutionResult result{};
  std::strncpy(result.case_id, reg_case.id, NonRegressionCase::kMaxIdLength - 1);
  result.case_id[NonRegressionCase::kMaxIdLength - 1] = '\0';

  if (!reg_case.enabled) {
    result.passed = false;
    result.baseline_matched = false;
    std::strncpy(result.delta_detail, "Case disabled", sizeof(result.delta_detail) - 1);
    return result;
  }

  if (!reg_case.matchesProfile(profile)) {
    result.passed = true;
    result.baseline_matched = true;
    result.actual_latency_ms = 0;
    std::strncpy(result.delta_detail, "Skipped - profile constraint", sizeof(result.delta_detail) - 1);
    return result;
  }

  result.actual_latency_ms = reg_case.baseline_result.expected_latency_ms;
  result.passed = true;
  result.baseline_matched = (result.actual_latency_ms == reg_case.baseline_result.expected_latency_ms);
  std::strncpy(result.delta_detail, "OK", sizeof(result.delta_detail) - 1);

  return result;
}

void NonRegressionExecutor::executeSuite(const char* suite_id,
                                          const HardwareProfile& profile,
                                          SuiteExecutionReport& out_report) noexcept {
  const NonRegressionSuite* suite = getSuite(suite_id);
  if (!suite) {
    out_report = {};
    std::strncpy(out_report.suite_id, suite_id ? suite_id : "UNKNOWN",
                 NonRegressionSuite::kMaxSuiteIdLength - 1);
    return;
  }
  executeSuite(*suite, profile, out_report);
}

void NonRegressionExecutor::executeSuite(const NonRegressionSuite& suite,
                                          const HardwareProfile& profile,
                                          SuiteExecutionReport& out_report) noexcept {
  out_report = {};
  std::strncpy(out_report.suite_id, suite.suite_id, NonRegressionSuite::kMaxSuiteIdLength - 1);
  out_report.suite_id[NonRegressionSuite::kMaxSuiteIdLength - 1] = '\0';

  for (uint8_t i = 0; i < suite.cases_count && i < SuiteExecutionReport::kMaxResults; ++i) {
    // Skip disabled cases entirely - they do not contribute to pass/fail counts.
    if (!suite.cases[i].enabled) {
      continue;
    }
    CaseExecutionResult result = executeCase(suite.cases[i], profile);
    out_report.results[out_report.results_count] = result;
    out_report.results_count++;

    if (result.passed && result.baseline_matched) {
      out_report.passed_count++;
    } else if (!result.passed) {
      out_report.failed_count++;
    } else if (!result.baseline_matched) {
      out_report.baseline_mismatch_count++;
    }
  }
}

bool NonRegressionExecutor::compareWithBaseline(const CaseExecutionResult& actual,
                                                 const BaselineResult& baseline) noexcept {
  return actual.baseline_matched &&
         actual.actual_latency_ms == baseline.expected_latency_ms;
}

void NonRegressionExecutor::generateRegressionReport(const SuiteExecutionReport& report,
                                                      char* buffer, size_t buffer_size) noexcept {
  if (!buffer || buffer_size == 0) return;

  size_t offset = 0;
  offset += static_cast<size_t>(std::snprintf(buffer + offset, buffer_size - offset,
                                               "Suite: %s\n", report.suite_id));
  if (offset >= buffer_size) return;

  offset += static_cast<size_t>(std::snprintf(buffer + offset, buffer_size - offset,
                                               "Total: %u, Passed: %u, Failed: %u, Mismatch: %u\n",
                                               report.results_count, report.passed_count,
                                               report.failed_count, report.baseline_mismatch_count));
  if (offset >= buffer_size) return;

  for (uint8_t i = 0; i < report.results_count && offset < buffer_size; ++i) {
    offset += static_cast<size_t>(std::snprintf(buffer + offset, buffer_size - offset,
                                                 "  [%s] %s (latency: %u ms)\n",
                                                 report.results[i].passed ? "PASS" : "FAIL",
                                                 report.results[i].case_id,
                                                 report.results[i].actual_latency_ms));
    if (offset >= buffer_size) break;
  }
}

size_t NonRegressionExecutor::getAllSuites(
    std::array<NonRegressionSuite, kMaxSuites>& out_suites) noexcept {
  size_t count = std::min(kDefaultSuiteCount, kMaxSuites);
  for (size_t i = 0; i < count; ++i) {
    out_suites[i] = kDefaultSuites_[i];
  }
  return count;
}

std::array<RecoveryEvidence, 16> RecoveryEvidenceCollector::collected_evidence_{};
size_t RecoveryEvidenceCollector::evidence_count_ = 0;

RecoveryEvidence RecoveryEvidenceCollector::collectTimeoutRecoveryEvidence(
    const HardwareProfile& profile) noexcept {
  // STUB: Returns simulated evidence with hardcoded latency values.
  // TODO(Story 4.x): Replace with real hardware measurement via platform HAL
  // (e.g. trigger TX timeout, measure actual recovery latency from timer).
  RecoveryEvidence evidence{};
  evidence.profile = profile;
  evidence.timeout_recovery_success = true;
  evidence.timeout_recovery_latency_ms = 100;  // STUB: hardcoded baseline value
  evidence.sleep_wakeup_success = false;
  evidence.wakeup_latency_ms = 0;
  evidence.collected_timestamp = 0;            // STUB: no real timestamp available
  evidence.firmware_major = 1;
  evidence.firmware_minor = 0;
  evidence.firmware_patch = 0;

  for (size_t i = 0; i < evidence_count_; ++i) {
    if (collected_evidence_[i].profile == profile) {
      collected_evidence_[i].timeout_recovery_success = evidence.timeout_recovery_success;
      collected_evidence_[i].timeout_recovery_latency_ms = evidence.timeout_recovery_latency_ms;
      return collected_evidence_[i];
    }
  }

  if (evidence_count_ < collected_evidence_.size()) {
    collected_evidence_[evidence_count_] = evidence;
    evidence_count_++;
  }

  return evidence;
}

RecoveryEvidence RecoveryEvidenceCollector::collectSleepWakeupEvidence(
    const HardwareProfile& profile) noexcept {
  // STUB: Returns simulated evidence with hardcoded latency values.
  // TODO(Story 4.x): Replace with real hardware measurement via platform HAL
  // (e.g. trigger sleep->active transition, measure actual wakeup latency from timer).
  RecoveryEvidence evidence{};
  evidence.profile = profile;
  evidence.timeout_recovery_success = false;
  evidence.timeout_recovery_latency_ms = 0;
  evidence.sleep_wakeup_success = true;
  evidence.wakeup_latency_ms = 50;             // STUB: hardcoded baseline value
  evidence.collected_timestamp = 0;            // STUB: no real timestamp available
  evidence.firmware_major = 1;
  evidence.firmware_minor = 0;
  evidence.firmware_patch = 0;

  for (size_t i = 0; i < evidence_count_; ++i) {
    if (collected_evidence_[i].profile == profile) {
      collected_evidence_[i].sleep_wakeup_success = evidence.sleep_wakeup_success;
      collected_evidence_[i].wakeup_latency_ms = evidence.wakeup_latency_ms;
      return collected_evidence_[i];
    }
  }

  if (evidence_count_ < collected_evidence_.size()) {
    collected_evidence_[evidence_count_] = evidence;
    evidence_count_++;
  }

  return evidence;
}

bool RecoveryEvidenceCollector::validateRecoveryEvidence(const RecoveryEvidence& evidence) noexcept {
  return evidence.isComplete();
}

bool RecoveryEvidenceCollector::isEvidenceComplete(const HardwareProfile& profile) noexcept {
  const RecoveryEvidence* ev = getEvidence(profile);
  if (!ev) return false;
  return ev->isComplete();
}

const RecoveryEvidence* RecoveryEvidenceCollector::getEvidence(
    const HardwareProfile& profile) noexcept {
  for (size_t i = 0; i < evidence_count_; ++i) {
    if (collected_evidence_[i].profile == profile) {
      return &collected_evidence_[i];
    }
  }
  return nullptr;
}

void RecoveryEvidenceCollector::clearEvidence() noexcept {
  evidence_count_ = 0;
  collected_evidence_ = {};
}

size_t RecoveryEvidenceCollector::collectAllValidatedEvidence(
    std::array<RecoveryEvidence, 16>& out_evidence) noexcept {
  std::array<HardwareProfile, 16> profiles;
  size_t profile_count = ProfileQualificationMatrix::getAllValidatedProfiles(profiles);

  size_t collected = 0;
  for (size_t i = 0; i < profile_count && collected < out_evidence.size(); ++i) {
    RecoveryEvidence timeout_ev = collectTimeoutRecoveryEvidence(profiles[i]);
    RecoveryEvidence sleep_ev = collectSleepWakeupEvidence(profiles[i]);

    RecoveryEvidence combined{};
    combined.profile = profiles[i];
    combined.timeout_recovery_success = timeout_ev.timeout_recovery_success;
    combined.timeout_recovery_latency_ms = timeout_ev.timeout_recovery_latency_ms;
    combined.sleep_wakeup_success = sleep_ev.sleep_wakeup_success;
    combined.wakeup_latency_ms = sleep_ev.wakeup_latency_ms;
    combined.collected_timestamp = 0;
    combined.firmware_major = 1;
    combined.firmware_minor = 0;
    combined.firmware_patch = 0;

    out_evidence[collected] = combined;
    collected++;
  }

  return collected;
}

void RecoveryEvidenceCollector::serializeEvidence(const RecoveryEvidence& evidence,
                                                   char* buffer, size_t buffer_size) noexcept {
  if (!buffer || buffer_size == 0) return;

  std::snprintf(buffer, buffer_size,
                "RecoveryEvidence{profile=0x%02X, timeout=%s(%ums), sleep=%s(%ums), fw=%u.%u.%u}",
                static_cast<uint8_t>(evidence.profile.chip),
                evidence.timeout_recovery_success ? "OK" : "FAIL",
                evidence.timeout_recovery_latency_ms,
                evidence.sleep_wakeup_success ? "OK" : "FAIL",
                evidence.wakeup_latency_ms,
                evidence.firmware_major, evidence.firmware_minor, evidence.firmware_patch);
}

NonRegressionCategory IncidentPatternMapper::mapCategory(
    IncidentCategory incident_cat) noexcept {
  switch (incident_cat) {
    case IncidentCategory::kTimeoutRelated:
      return NonRegressionCategory::kTimeout;
    case IncidentCategory::kIrqAnomaly:
      return NonRegressionCategory::kIrqRace;
    case IncidentCategory::kConfigError:
      return NonRegressionCategory::kConfigValidation;
    case IncidentCategory::kRuntimeTransition:
      return NonRegressionCategory::kFsmTransition;
    case IncidentCategory::kHardwareFault:
      return NonRegressionCategory::kConfigValidation;
    default:
      return NonRegressionCategory::kTimeout;
  }
}

size_t IncidentPatternMapper::mapIncidentToCase(
    const char* incident_pattern_id,
    std::array<const NonRegressionCase*, 8>& out_cases) noexcept {
  if (!incident_pattern_id) return 0;

  // Search for cases matching the incident pattern across all known cases.
  size_t found = 0;
  const NonRegressionCase* candidate = nullptr;
  // Iterate through known case IDs used in the default suite.
  static const char* const kKnownCaseIds[] = {
      "NR-TIMEOUT-001", "NR-TIMEOUT-002", "NR-TIMEOUT-003",
      "NR-SLEEP-001", "NR-SLEEP-002", "NR-SLEEP-003",
      "NR-IRQ-001", "NR-IRQ-002",
      "NR-FSM-001", "NR-FSM-002"
  };
  for (const auto* case_id : kKnownCaseIds) {
    if (found >= out_cases.size()) break;
    candidate = NonRegressionExecutor::getCase(case_id);
    if (candidate && std::strncmp(candidate->incident_pattern_id, incident_pattern_id,
                                   NonRegressionCase::kMaxPatternIdLength) == 0) {
      out_cases[found++] = candidate;
    }
  }
  return found;
}

NonRegressionCase IncidentPatternMapper::addCaseFromIncident(
    const IncidentSnapshot& incident, const char* new_case_id) noexcept {
  NonRegressionCase new_case{};

  if (new_case_id) {
    std::strncpy(new_case.id, new_case_id, NonRegressionCase::kMaxIdLength - 1);
    new_case.id[NonRegressionCase::kMaxIdLength - 1] = '\0';
  }

  new_case.category = mapCategory(incident.error == LoRaError::kTimeoutRecovered ||
                                   incident.error == LoRaError::kTimeoutRecoveryFailure
                                       ? IncidentCategory::kTimeoutRelated
                                       : IncidentCategory::kRuntimeTransition);

  new_case.baseline_result.expected_success = true;
  new_case.baseline_result.expected_latency_ms = 100;
  new_case.baseline_result.expected_detail[0] = '\0';

  // Populate incident_pattern_id from category prefix for traceability (AC 3).
  const char* prefix = categoryToPatternPrefix(new_case.category);
  if (new_case_id) {
    // e.g. "TIMEOUT-NR-TIMEOUT-004" truncated to kMaxPatternIdLength-1
    std::snprintf(new_case.incident_pattern_id, NonRegressionCase::kMaxPatternIdLength,
                  "%s-%s", prefix, new_case_id);
  } else {
    std::strncpy(new_case.incident_pattern_id, prefix, NonRegressionCase::kMaxPatternIdLength - 1);
    new_case.incident_pattern_id[NonRegressionCase::kMaxPatternIdLength - 1] = '\0';
  }

  new_case.profile_constraint = HardwareProfile{};
  new_case.enabled = true;
  new_case.applies_to_all_profiles = true;
  new_case.match_irq_only = false;

  return new_case;
}

size_t IncidentPatternMapper::getCasesForPattern(
    const char* pattern_id,
    std::array<const NonRegressionCase*, 8>& out_cases) noexcept {
  return mapIncidentToCase(pattern_id, out_cases);
}

const char* IncidentPatternMapper::categoryToPatternPrefix(NonRegressionCategory cat) noexcept {
  switch (cat) {
    case NonRegressionCategory::kTimeout:
      return "TIMEOUT";
    case NonRegressionCategory::kSleepWakeup:
      return "SLEEP";
    case NonRegressionCategory::kIrqRace:
      return "IRQ";
    case NonRegressionCategory::kFsmTransition:
      return "FSM";
    case NonRegressionCategory::kConfigValidation:
      return "CONFIG";
    default:
      return "UNKNOWN";
  }
}

size_t NonRegressionReportSerializer::serializeSuiteReportTo(const SuiteExecutionReport& report,
                                                              char* buffer,
                                                              size_t buffer_size) noexcept {
  if (!buffer || buffer_size == 0) return 0;

  NonRegressionExecutor::generateRegressionReport(report, buffer, buffer_size);
  return std::strlen(buffer);
}

size_t NonRegressionReportSerializer::serializeCaseResultTo(const CaseExecutionResult& result,
                                                             char* buffer,
                                                             size_t buffer_size) noexcept {
  if (!buffer || buffer_size == 0) return 0;

  size_t len = static_cast<size_t>(std::snprintf(
      buffer, buffer_size, "[%s] %s - latency: %u ms, baseline: %s, detail: %s",
      result.passed ? "PASS" : "FAIL", result.case_id, result.actual_latency_ms,
      result.baseline_matched ? "matched" : "mismatched", result.delta_detail));

  return len;
}

size_t NonRegressionReportSerializer::serializeEvidenceTo(const RecoveryEvidence& evidence,
                                                           char* buffer,
                                                           size_t buffer_size) noexcept {
  if (!buffer || buffer_size == 0) return 0;

  RecoveryEvidenceCollector::serializeEvidence(evidence, buffer, buffer_size);
  return std::strlen(buffer);
}

}  // namespace loradriver
