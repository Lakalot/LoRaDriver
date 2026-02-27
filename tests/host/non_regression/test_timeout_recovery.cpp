#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "loradriver/non_regression.hpp"
#include "loradriver/profile_qualification.hpp"

namespace {

using loradriver::BaselineResult;
using loradriver::CaseExecutionResult;
using loradriver::HardwareProfile;
using loradriver::NonRegressionCase;
using loradriver::NonRegressionCategory;
using loradriver::NonRegressionExecutor;
using loradriver::NonRegressionReportSerializer;
using loradriver::NonRegressionSuite;
using loradriver::ProfileQualificationMatrix;
using loradriver::RadioConfig;
using loradriver::SuiteExecutionReport;

HardwareProfile MakeTestProfile() {
  HardwareProfile profile{};
  profile.chip = RadioConfig::Chip::kSx1276;
  profile.band = RadioConfig::Band::k868;
  profile.irq = RadioConfig::DioRouting::kDio0Only;
  return profile;
}

bool TestNonRegressionCategoryEnumValuesAreStable() {
  if (static_cast<uint8_t>(NonRegressionCategory::kTimeout) != 0) return false;
  if (static_cast<uint8_t>(NonRegressionCategory::kSleepWakeup) != 1) return false;
  if (static_cast<uint8_t>(NonRegressionCategory::kIrqRace) != 2) return false;
  if (static_cast<uint8_t>(NonRegressionCategory::kFsmTransition) != 3) return false;
  if (static_cast<uint8_t>(NonRegressionCategory::kConfigValidation) != 4) return false;
  return true;
}

bool TestBaselineResultEqualityOperator() {
  BaselineResult a{};
  a.expected_success = true;
  a.expected_latency_ms = 100;

  BaselineResult b{};
  b.expected_success = true;
  b.expected_latency_ms = 100;

  BaselineResult c{};
  c.expected_success = false;
  c.expected_latency_ms = 100;

  if (!(a == b)) return false;
  if (a == c) return false;
  return true;
}

bool TestNonRegressionCaseMatchesProfileWhenAppliesToAll() {
  NonRegressionCase c{};
  c.applies_to_all_profiles = true;
  c.enabled = true;

  HardwareProfile p1 = MakeTestProfile();
  HardwareProfile p2{};
  p2.chip = RadioConfig::Chip::kSx1278;

  if (!c.matchesProfile(p1)) return false;
  if (!c.matchesProfile(p2)) return false;
  return true;
}

bool TestNonRegressionCaseMatchesProfileWhenSpecificConstraint() {
  NonRegressionCase c{};
  c.applies_to_all_profiles = false;
  c.profile_constraint = MakeTestProfile();
  c.enabled = true;

  HardwareProfile matching = MakeTestProfile();
  HardwareProfile different{};
  different.chip = RadioConfig::Chip::kSx1278;

  if (!c.matchesProfile(matching)) return false;
  if (c.matchesProfile(different)) return false;
  return true;
}

bool TestNonRegressionSuiteGetEnabledCasesCount() {
  NonRegressionSuite suite{};
  suite.cases_count = 5;
  for (uint8_t i = 0; i < 5; ++i) {
    suite.cases[i].enabled = (i < 3);
  }

  if (suite.getEnabledCasesCount() != 3) return false;
  return true;
}

bool TestNonRegressionExecutorGetSuiteReturnsValidSuite() {
  const NonRegressionSuite* suite = NonRegressionExecutor::getSuite("V1-CRITICAL");
  if (suite == nullptr) return false;
  if (std::strcmp(suite->suite_id, "V1-CRITICAL") != 0) return false;
  return true;
}

bool TestNonRegressionExecutorGetSuiteReturnsNullForUnknown() {
  const NonRegressionSuite* suite = NonRegressionExecutor::getSuite("UNKNOWN");
  return suite == nullptr;
}

bool TestNonRegressionExecutorGetCaseReturnsValidCase() {
  const NonRegressionCase* c = NonRegressionExecutor::getCase("NR-TIMEOUT-001");
  if (c == nullptr) return false;
  if (std::strcmp(c->id, "NR-TIMEOUT-001") != 0) return false;
  if (c->category != NonRegressionCategory::kTimeout) return false;
  return true;
}

bool TestNonRegressionExecutorGetCaseReturnsNullForUnknown() {
  const NonRegressionCase* c = NonRegressionExecutor::getCase("NR-UNKNOWN-999");
  return c == nullptr;
}

bool TestNonRegressionExecutorExecuteCaseReturnsPassForMatchingBaseline() {
  HardwareProfile profile = MakeTestProfile();
  CaseExecutionResult result = NonRegressionExecutor::executeCase("NR-TIMEOUT-001", profile);

  if (!result.passed) return false;
  if (!result.baseline_matched) return false;
  return true;
}

bool TestNonRegressionExecutorExecuteCaseReturnsFailForUnknownCase() {
  HardwareProfile profile = MakeTestProfile();
  CaseExecutionResult result = NonRegressionExecutor::executeCase("NR-UNKNOWN", profile);

  if (result.passed) return false;
  return true;
}

bool TestNonRegressionExecutorExecuteSuitePopulatesReport() {
  HardwareProfile profile = MakeTestProfile();
  SuiteExecutionReport report{};
  NonRegressionExecutor::executeSuite("V1-CRITICAL", profile, report);

  if (std::strcmp(report.suite_id, "V1-CRITICAL") != 0) return false;
  if (report.results_count == 0) return false;
  if (report.failed_count != 0) return false;
  return true;
}

bool TestNonRegressionExecutorCompareWithBaselineReturnsTrueWhenMatched() {
  CaseExecutionResult result{};
  result.baseline_matched = true;
  result.actual_latency_ms = 100;

  BaselineResult baseline{};
  baseline.expected_latency_ms = 100;

  if (!NonRegressionExecutor::compareWithBaseline(result, baseline)) return false;
  return true;
}

bool TestNonRegressionExecutorCompareWithBaselineReturnsFalseWhenMismatched() {
  CaseExecutionResult result{};
  result.baseline_matched = true;
  result.actual_latency_ms = 150;

  BaselineResult baseline{};
  baseline.expected_latency_ms = 100;

  if (NonRegressionExecutor::compareWithBaseline(result, baseline)) return false;
  return true;
}

bool TestNonRegressionExecutorGenerateRegressionReportProducesOutput() {
  HardwareProfile profile = MakeTestProfile();
  SuiteExecutionReport report{};
  NonRegressionExecutor::executeSuite("V1-CRITICAL", profile, report);

  char buffer[4096];
  NonRegressionExecutor::generateRegressionReport(report, buffer, sizeof(buffer));

  if (buffer[0] == '\0') return false;
  if (std::strstr(buffer, "V1-CRITICAL") == nullptr) return false;
  return true;
}

bool TestNonRegressionExecutorGetAllSuitesReturnsCorrectCount() {
  std::array<NonRegressionSuite, NonRegressionExecutor::kMaxSuites> suites{};
  size_t count = NonRegressionExecutor::getAllSuites(suites);

  if (count != NonRegressionExecutor::kDefaultSuiteCount) return false;
  return true;
}

bool TestSuiteExecutionReportAllPassedReturnsTrueWhenNoFailures() {
  SuiteExecutionReport report{};
  report.results_count = 5;
  report.passed_count = 5;
  report.failed_count = 0;
  report.baseline_mismatch_count = 0;

  if (!report.allPassed()) return false;
  return true;
}

bool TestSuiteExecutionReportAllPassedReturnsFalseWhenFailures() {
  SuiteExecutionReport report{};
  report.results_count = 5;
  report.passed_count = 4;
  report.failed_count = 1;
  report.baseline_mismatch_count = 0;

  if (report.allPassed()) return false;
  return true;
}

bool TestSuiteExecutionReportPassRateCalculatesCorrectly() {
  SuiteExecutionReport report{};
  report.results_count = 10;
  report.passed_count = 8;

  if (report.passRate() != 80) return false;
  return true;
}

bool TestAllV1NonRegressionCasesAreDefined() {
  const char* expected_cases[] = {
      "NR-TIMEOUT-001", "NR-TIMEOUT-002", "NR-TIMEOUT-003",
      "NR-SLEEP-001", "NR-SLEEP-002", "NR-SLEEP-003",
      "NR-IRQ-001", "NR-IRQ-002",
      "NR-FSM-001", "NR-FSM-002"
  };

  for (const char* case_id : expected_cases) {
    const NonRegressionCase* c = NonRegressionExecutor::getCase(case_id);
    if (c == nullptr) return false;
    if (!c->enabled) return false;
  }
  return true;
}

bool TestTimeoutCasesHaveCorrectCategory() {
  const NonRegressionCase* c1 = NonRegressionExecutor::getCase("NR-TIMEOUT-001");
  const NonRegressionCase* c2 = NonRegressionExecutor::getCase("NR-TIMEOUT-002");
  const NonRegressionCase* c3 = NonRegressionExecutor::getCase("NR-TIMEOUT-003");

  if (c1->category != NonRegressionCategory::kTimeout) return false;
  if (c2->category != NonRegressionCategory::kTimeout) return false;
  if (c3->category != NonRegressionCategory::kTimeout) return false;
  return true;
}

bool TestSleepWakeupCasesHaveCorrectCategory() {
  const NonRegressionCase* c1 = NonRegressionExecutor::getCase("NR-SLEEP-001");
  const NonRegressionCase* c2 = NonRegressionExecutor::getCase("NR-SLEEP-002");
  const NonRegressionCase* c3 = NonRegressionExecutor::getCase("NR-SLEEP-003");

  if (c1->category != NonRegressionCategory::kSleepWakeup) return false;
  if (c2->category != NonRegressionCategory::kSleepWakeup) return false;
  if (c3->category != NonRegressionCategory::kSleepWakeup) return false;
  return true;
}

bool TestIrqRaceCasesHaveCorrectCategory() {
  const NonRegressionCase* c1 = NonRegressionExecutor::getCase("NR-IRQ-001");
  const NonRegressionCase* c2 = NonRegressionExecutor::getCase("NR-IRQ-002");

  if (c1->category != NonRegressionCategory::kIrqRace) return false;
  if (c2->category != NonRegressionCategory::kIrqRace) return false;
  return true;
}

bool TestFsmCasesHaveCorrectCategory() {
  const NonRegressionCase* c1 = NonRegressionExecutor::getCase("NR-FSM-001");
  const NonRegressionCase* c2 = NonRegressionExecutor::getCase("NR-FSM-002");

  if (c1->category != NonRegressionCategory::kFsmTransition) return false;
  if (c2->category != NonRegressionCategory::kFsmTransition) return false;
  return true;
}

bool TestNonRegressionReportSerializerSuiteReportProducesOutput() {
  HardwareProfile profile = MakeTestProfile();
  SuiteExecutionReport report{};
  NonRegressionExecutor::executeSuite("V1-CRITICAL", profile, report);

  char buffer[4096];
  size_t written = NonRegressionReportSerializer::serializeSuiteReportTo(report, buffer, sizeof(buffer));

  if (written == 0) return false;
  return true;
}

bool TestNonRegressionReportSerializerCaseResultProducesOutput() {
  CaseExecutionResult result{};
  std::strcpy(result.case_id, "NR-TEST-001");
  result.passed = true;
  result.baseline_matched = true;
  result.actual_latency_ms = 100;
  std::strcpy(result.delta_detail, "OK");

  char buffer[256];
  size_t written = NonRegressionReportSerializer::serializeCaseResultTo(result, buffer, sizeof(buffer));

  if (written == 0) return false;
  if (std::strstr(buffer, "NR-TEST-001") == nullptr) return false;
  return true;
}

#define RUN_TEST(fn) \
  if (!(fn)()) { \
    std::fprintf(stderr, "FAIL: %s\n", #fn); \
    return EXIT_FAILURE; \
  }

int RunNonRegressionTests() {
  RUN_TEST(TestNonRegressionCategoryEnumValuesAreStable)
  RUN_TEST(TestBaselineResultEqualityOperator)
  RUN_TEST(TestNonRegressionCaseMatchesProfileWhenAppliesToAll)
  RUN_TEST(TestNonRegressionCaseMatchesProfileWhenSpecificConstraint)
  RUN_TEST(TestNonRegressionSuiteGetEnabledCasesCount)
  RUN_TEST(TestNonRegressionExecutorGetSuiteReturnsValidSuite)
  RUN_TEST(TestNonRegressionExecutorGetSuiteReturnsNullForUnknown)
  RUN_TEST(TestNonRegressionExecutorGetCaseReturnsValidCase)
  RUN_TEST(TestNonRegressionExecutorGetCaseReturnsNullForUnknown)
  RUN_TEST(TestNonRegressionExecutorExecuteCaseReturnsPassForMatchingBaseline)
  RUN_TEST(TestNonRegressionExecutorExecuteCaseReturnsFailForUnknownCase)
  RUN_TEST(TestNonRegressionExecutorExecuteSuitePopulatesReport)
  RUN_TEST(TestNonRegressionExecutorCompareWithBaselineReturnsTrueWhenMatched)
  RUN_TEST(TestNonRegressionExecutorCompareWithBaselineReturnsFalseWhenMismatched)
  RUN_TEST(TestNonRegressionExecutorGenerateRegressionReportProducesOutput)
  RUN_TEST(TestNonRegressionExecutorGetAllSuitesReturnsCorrectCount)
  RUN_TEST(TestSuiteExecutionReportAllPassedReturnsTrueWhenNoFailures)
  RUN_TEST(TestSuiteExecutionReportAllPassedReturnsFalseWhenFailures)
  RUN_TEST(TestSuiteExecutionReportPassRateCalculatesCorrectly)
  RUN_TEST(TestAllV1NonRegressionCasesAreDefined)
  RUN_TEST(TestTimeoutCasesHaveCorrectCategory)
  RUN_TEST(TestSleepWakeupCasesHaveCorrectCategory)
  RUN_TEST(TestIrqRaceCasesHaveCorrectCategory)
  RUN_TEST(TestFsmCasesHaveCorrectCategory)
  RUN_TEST(TestNonRegressionReportSerializerSuiteReportProducesOutput)
  RUN_TEST(TestNonRegressionReportSerializerCaseResultProducesOutput)
  return EXIT_SUCCESS;
}

}  // namespace

int main() {
  return RunNonRegressionTests();
}
