#pragma once

#include <array>
#include <cstdint>

#include <loradriver/profile_qualification.hpp>
#include <loradriver/incident_classification.hpp>

namespace loradriver {

enum class NonRegressionCategory : uint8_t {
  kTimeout = 0,
  kSleepWakeup = 1,
  kIrqRace = 2,
  kFsmTransition = 3,
  kConfigValidation = 4
};

struct BaselineResult {
  static constexpr size_t kMaxDetailLength = 64;

  bool expected_success;
  uint32_t expected_latency_ms;
  char expected_detail[kMaxDetailLength];

  [[nodiscard]] constexpr bool operator==(const BaselineResult& other) const noexcept {
    return expected_success == other.expected_success &&
           expected_latency_ms == other.expected_latency_ms;
  }
};

struct NonRegressionCase {
  static constexpr size_t kMaxIdLength = 16;
  static constexpr size_t kMaxPatternIdLength = 32;

  char id[kMaxIdLength];
  NonRegressionCategory category;
  BaselineResult baseline_result;
  char incident_pattern_id[kMaxPatternIdLength];
  HardwareProfile profile_constraint;
  bool enabled;
  bool applies_to_all_profiles;
  /// When true, only the irq field of profile_constraint is compared (e.g. DIO0+DIO1-only cases).
  bool match_irq_only;

  [[nodiscard]] constexpr bool matchesProfile(const HardwareProfile& profile) const noexcept {
    if (applies_to_all_profiles) {
      return true;
    }
    if (match_irq_only) {
      return profile_constraint.irq == profile.irq;
    }
    return profile_constraint == profile;
  }
};

struct NonRegressionSuite {
  static constexpr size_t kMaxCases = 32;
  static constexpr size_t kMaxSuiteIdLength = 16;

  char suite_id[kMaxSuiteIdLength];
  std::array<NonRegressionCase, kMaxCases> cases;
  uint8_t cases_count;
  uint8_t suite_version_major;
  uint8_t suite_version_minor;
  uint8_t suite_version_patch;

  [[nodiscard]] constexpr size_t getEnabledCasesCount() const noexcept {
    size_t count = 0;
    for (uint8_t i = 0; i < cases_count; ++i) {
      if (cases[i].enabled) {
        ++count;
      }
    }
    return count;
  }
};

struct RecoveryEvidence {
  HardwareProfile profile;
  bool timeout_recovery_success;
  uint32_t timeout_recovery_latency_ms;
  bool sleep_wakeup_success;
  uint32_t wakeup_latency_ms;
  uint32_t collected_timestamp;
  uint8_t firmware_major;
  uint8_t firmware_minor;
  uint8_t firmware_patch;

  /// Returns true only when both recovery paths succeeded with non-zero measured latencies.
  /// A latency of 0 ms indicates an uninitialised/uncollected measurement and is not valid evidence.
  [[nodiscard]] constexpr bool isComplete() const noexcept {
    return timeout_recovery_success && sleep_wakeup_success &&
           timeout_recovery_latency_ms > 0 && wakeup_latency_ms > 0;
  }
};

struct CaseExecutionResult {
  char case_id[NonRegressionCase::kMaxIdLength];
  bool passed;
  bool baseline_matched;
  uint32_t actual_latency_ms;
  char delta_detail[128];
};

struct SuiteExecutionReport {
  static constexpr size_t kMaxResults = NonRegressionSuite::kMaxCases;

  char suite_id[NonRegressionSuite::kMaxSuiteIdLength];
  std::array<CaseExecutionResult, kMaxResults> results;
  uint8_t results_count;
  uint8_t passed_count;
  uint8_t failed_count;
  uint8_t baseline_mismatch_count;
  uint32_t execution_timestamp;

  [[nodiscard]] constexpr bool allPassed() const noexcept {
    return failed_count == 0 && baseline_mismatch_count == 0;
  }

  [[nodiscard]] constexpr uint8_t passRate() const noexcept {
    if (results_count == 0) return 0;
    return static_cast<uint8_t>((passed_count * 100) / results_count);
  }
};

class NonRegressionExecutor {
 public:
  static constexpr size_t kMaxSuites = 8;

  [[nodiscard]] static const NonRegressionSuite* getSuite(const char* suite_id) noexcept;
  [[nodiscard]] static const NonRegressionCase* getCase(const char* case_id) noexcept;

  [[nodiscard]] static CaseExecutionResult executeCase(const char* case_id,
                                                        const HardwareProfile& profile) noexcept;

  [[nodiscard]] static CaseExecutionResult executeCase(const NonRegressionCase& reg_case,
                                                        const HardwareProfile& profile) noexcept;

  static void executeSuite(const char* suite_id,
                           const HardwareProfile& profile,
                           SuiteExecutionReport& out_report) noexcept;

  static void executeSuite(const NonRegressionSuite& suite,
                           const HardwareProfile& profile,
                           SuiteExecutionReport& out_report) noexcept;

  [[nodiscard]] static bool compareWithBaseline(const CaseExecutionResult& actual,
                                                 const BaselineResult& baseline) noexcept;

  static void generateRegressionReport(const SuiteExecutionReport& report,
                                        char* buffer, size_t buffer_size) noexcept;

  [[nodiscard]] static size_t getAllSuites(
      std::array<NonRegressionSuite, kMaxSuites>& out_suites) noexcept;

  static constexpr size_t kDefaultSuiteCount = 1;

 private:
  static const std::array<NonRegressionSuite, kDefaultSuiteCount> kDefaultSuites_;
};

class RecoveryEvidenceCollector {
 public:
  static constexpr size_t kMaxEvidencePerProfile = 16;

  [[nodiscard]] static RecoveryEvidence collectTimeoutRecoveryEvidence(
      const HardwareProfile& profile) noexcept;

  [[nodiscard]] static RecoveryEvidence collectSleepWakeupEvidence(
      const HardwareProfile& profile) noexcept;

  [[nodiscard]] static bool validateRecoveryEvidence(const RecoveryEvidence& evidence) noexcept;

  [[nodiscard]] static bool isEvidenceComplete(const HardwareProfile& profile) noexcept;

  [[nodiscard]] static const RecoveryEvidence* getEvidence(const HardwareProfile& profile) noexcept;

  static void clearEvidence() noexcept;

  [[nodiscard]] static size_t collectAllValidatedEvidence(
      std::array<RecoveryEvidence, 16>& out_evidence) noexcept;

  static void serializeEvidence(const RecoveryEvidence& evidence,
                                 char* buffer, size_t buffer_size) noexcept;

 private:
  static std::array<RecoveryEvidence, 16> collected_evidence_;
  static size_t evidence_count_;
};

class IncidentPatternMapper {
 public:
  static constexpr size_t kMaxMappings = 32;

  [[nodiscard]] static NonRegressionCategory mapCategory(
      IncidentCategory incident_cat) noexcept;

  [[nodiscard]] static size_t mapIncidentToCase(
      const char* incident_pattern_id,
      std::array<const NonRegressionCase*, 8>& out_cases) noexcept;

  [[nodiscard]] static NonRegressionCase addCaseFromIncident(
      const IncidentSnapshot& incident,
      const char* new_case_id) noexcept;

  [[nodiscard]] static size_t getCasesForPattern(
      const char* pattern_id,
      std::array<const NonRegressionCase*, 8>& out_cases) noexcept;

 private:
  static const char* categoryToPatternPrefix(NonRegressionCategory cat) noexcept;
};

struct NonRegressionReportSerializer {
  [[nodiscard]] static size_t serializeSuiteReportTo(const SuiteExecutionReport& report,
                                                      char* buffer, size_t buffer_size) noexcept;

  [[nodiscard]] static size_t serializeCaseResultTo(const CaseExecutionResult& result,
                                                     char* buffer, size_t buffer_size) noexcept;

  [[nodiscard]] static size_t serializeEvidenceTo(const RecoveryEvidence& evidence,
                                                   char* buffer, size_t buffer_size) noexcept;
};

}  // namespace loradriver
