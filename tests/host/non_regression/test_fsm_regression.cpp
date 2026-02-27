#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "loradriver/non_regression.hpp"
#include "loradriver/incident_classification.hpp"
#include "loradriver/incident_snapshot.hpp"
#include "loradriver/profile_qualification.hpp"

namespace {

using loradriver::HardwareProfile;
using loradriver::IncidentCategory;
using loradriver::IncidentPatternMapper;
using loradriver::IncidentSnapshot;
using loradriver::LoRaError;
using loradriver::NonRegressionCase;
using loradriver::NonRegressionCategory;
using loradriver::NonRegressionExecutor;
using loradriver::RadioConfig;

HardwareProfile MakeTestProfile() {
  HardwareProfile profile{};
  profile.chip = RadioConfig::Chip::kSx1276;
  profile.band = RadioConfig::Band::k868;
  profile.irq = RadioConfig::DioRouting::kDio0Only;
  return profile;
}

bool TestFsmCase001HasCorrectProperties() {
  const NonRegressionCase* c = NonRegressionExecutor::getCase("NR-FSM-001");
  if (c == nullptr) return false;
  if (c->category != NonRegressionCategory::kFsmTransition) return false;
  if (!c->applies_to_all_profiles) return false;
  return true;
}

bool TestFsmCase002HasCorrectProperties() {
  const NonRegressionCase* c = NonRegressionExecutor::getCase("NR-FSM-002");
  if (c == nullptr) return false;
  if (c->category != NonRegressionCategory::kFsmTransition) return false;
  if (!c->applies_to_all_profiles) return false;
  return true;
}

bool TestFsmCasesAreEnabled() {
  const NonRegressionCase* c1 = NonRegressionExecutor::getCase("NR-FSM-001");
  const NonRegressionCase* c2 = NonRegressionExecutor::getCase("NR-FSM-002");

  if (c1 == nullptr || c2 == nullptr) return false;
  if (!c1->enabled) return false;
  if (!c2->enabled) return false;
  return true;
}

bool TestFsmCasesHaveCorrectPatternIds() {
  const NonRegressionCase* c1 = NonRegressionExecutor::getCase("NR-FSM-001");
  const NonRegressionCase* c2 = NonRegressionExecutor::getCase("NR-FSM-002");

  if (c1 == nullptr || c2 == nullptr) return false;
  if (std::strstr(c1->incident_pattern_id, "FSM") == nullptr) return false;
  if (std::strstr(c2->incident_pattern_id, "FSM") == nullptr) return false;
  return true;
}

bool TestFsmCaseExecutionPassesForMatchingBaseline() {
  HardwareProfile profile = MakeTestProfile();

  loradriver::CaseExecutionResult result1 =
      NonRegressionExecutor::executeCase("NR-FSM-001", profile);
  loradriver::CaseExecutionResult result2 =
      NonRegressionExecutor::executeCase("NR-FSM-002", profile);

  if (!result1.passed || !result1.baseline_matched) return false;
  if (!result2.passed || !result2.baseline_matched) return false;
  return true;
}

bool TestIncidentPatternMapperMapsTimeoutToNonRegressionCategory() {
  NonRegressionCategory cat =
      IncidentPatternMapper::mapCategory(IncidentCategory::kTimeoutRelated);
  if (cat != NonRegressionCategory::kTimeout) return false;
  return true;
}

bool TestIncidentPatternMapperMapsRuntimeTransitionToNonRegressionCategory() {
  NonRegressionCategory cat =
      IncidentPatternMapper::mapCategory(IncidentCategory::kRuntimeTransition);
  if (cat != NonRegressionCategory::kFsmTransition) return false;
  return true;
}

bool TestIncidentPatternMapperMapsConfigErrorToNonRegressionCategory() {
  NonRegressionCategory cat =
      IncidentPatternMapper::mapCategory(IncidentCategory::kConfigError);
  if (cat != NonRegressionCategory::kConfigValidation) return false;
  return true;
}

bool TestIncidentPatternMapperMapsIrqAnomalyToNonRegressionCategory() {
  NonRegressionCategory cat =
      IncidentPatternMapper::mapCategory(IncidentCategory::kIrqAnomaly);
  if (cat != NonRegressionCategory::kIrqRace) return false;
  return true;
}

bool TestIncidentPatternMapperMapsHardwareFaultToNonRegressionCategory() {
  NonRegressionCategory cat =
      IncidentPatternMapper::mapCategory(IncidentCategory::kHardwareFault);
  if (cat != NonRegressionCategory::kConfigValidation) return false;
  return true;
}

bool TestIncidentPatternMapperMapIncidentToCaseReturnsMatchingCases() {
  std::array<const NonRegressionCase*, 8> cases{};
  size_t count = IncidentPatternMapper::mapIncidentToCase("TIMEOUT-TX-RECOVERY", cases);

  if (count == 0) return false;
  return true;
}

bool TestIncidentPatternMapperMapIncidentToCaseReturnsZeroForUnknown() {
  std::array<const NonRegressionCase*, 8> cases{};
  size_t count = IncidentPatternMapper::mapIncidentToCase("UNKNOWN-PATTERN", cases);

  if (count != 0) return false;
  return true;
}

bool TestIncidentPatternMapperGetCasesForPatternReturnsMatchingCases() {
  std::array<const NonRegressionCase*, 8> cases{};
  size_t count = IncidentPatternMapper::getCasesForPattern("FSM-ILLEGAL-REJECT", cases);

  if (count == 0) return false;
  return true;
}

bool TestIncidentPatternMapperAddCaseFromIncidentCreatesNewCase() {
  IncidentSnapshot snapshot{};
  snapshot.error = LoRaError::kTimeoutRecovered;

  NonRegressionCase new_case =
      IncidentPatternMapper::addCaseFromIncident(snapshot, "NR-NEW-001");

  if (std::strcmp(new_case.id, "NR-NEW-001") != 0) return false;
  if (!new_case.enabled) return false;
  if (!new_case.applies_to_all_profiles) return false;
  // Verify incident_pattern_id is populated for AC 3 traceability.
  if (new_case.incident_pattern_id[0] == '\0') return false;
  return true;
}

bool TestIncidentPatternMapperAddCaseFromIncidentSetsCorrectCategory() {
  IncidentSnapshot timeout_snapshot{};
  timeout_snapshot.error = LoRaError::kTimeoutRecovered;

  NonRegressionCase timeout_case =
      IncidentPatternMapper::addCaseFromIncident(timeout_snapshot, "NR-TIMEOUT-NEW");

  if (timeout_case.category != NonRegressionCategory::kTimeout) return false;
  return true;
}

bool TestSuiteExecutionIncludesFsmCases() {
  HardwareProfile profile = MakeTestProfile();
  loradriver::SuiteExecutionReport report{};
  NonRegressionExecutor::executeSuite("V1-CRITICAL", profile, report);

  bool found_fsm001 = false;
  bool found_fsm002 = false;
  for (uint8_t i = 0; i < report.results_count; ++i) {
    if (std::strcmp(report.results[i].case_id, "NR-FSM-001") == 0) found_fsm001 = true;
    if (std::strcmp(report.results[i].case_id, "NR-FSM-002") == 0) found_fsm002 = true;
  }

  if (!found_fsm001) return false;
  if (!found_fsm002) return false;
  return true;
}

bool TestFsmCasesHaveBaselineLatency() {
  const NonRegressionCase* c1 = NonRegressionExecutor::getCase("NR-FSM-001");
  const NonRegressionCase* c2 = NonRegressionExecutor::getCase("NR-FSM-002");

  if (c1 == nullptr || c2 == nullptr) return false;
  if (c1->baseline_result.expected_latency_ms == 0) return false;
  if (c2->baseline_result.expected_latency_ms == 0) return false;
  return true;
}

bool TestFsmCasesExpectSuccess() {
  const NonRegressionCase* c1 = NonRegressionExecutor::getCase("NR-FSM-001");
  const NonRegressionCase* c2 = NonRegressionExecutor::getCase("NR-FSM-002");

  if (c1 == nullptr || c2 == nullptr) return false;
  if (!c1->baseline_result.expected_success) return false;
  if (!c2->baseline_result.expected_success) return false;
  return true;
}

#define RUN_TEST(fn) \
  if (!(fn)()) { \
    std::fprintf(stderr, "FAIL: %s\n", #fn); \
    return EXIT_FAILURE; \
  }

int RunFsmRegressionTests() {
  RUN_TEST(TestFsmCase001HasCorrectProperties)
  RUN_TEST(TestFsmCase002HasCorrectProperties)
  RUN_TEST(TestFsmCasesAreEnabled)
  RUN_TEST(TestFsmCasesHaveCorrectPatternIds)
  RUN_TEST(TestFsmCaseExecutionPassesForMatchingBaseline)
  RUN_TEST(TestIncidentPatternMapperMapsTimeoutToNonRegressionCategory)
  RUN_TEST(TestIncidentPatternMapperMapsRuntimeTransitionToNonRegressionCategory)
  RUN_TEST(TestIncidentPatternMapperMapsConfigErrorToNonRegressionCategory)
  RUN_TEST(TestIncidentPatternMapperMapsIrqAnomalyToNonRegressionCategory)
  RUN_TEST(TestIncidentPatternMapperMapsHardwareFaultToNonRegressionCategory)
  RUN_TEST(TestIncidentPatternMapperMapIncidentToCaseReturnsMatchingCases)
  RUN_TEST(TestIncidentPatternMapperMapIncidentToCaseReturnsZeroForUnknown)
  RUN_TEST(TestIncidentPatternMapperGetCasesForPatternReturnsMatchingCases)
  RUN_TEST(TestIncidentPatternMapperAddCaseFromIncidentCreatesNewCase)
  RUN_TEST(TestIncidentPatternMapperAddCaseFromIncidentSetsCorrectCategory)
  RUN_TEST(TestSuiteExecutionIncludesFsmCases)
  RUN_TEST(TestFsmCasesHaveBaselineLatency)
  RUN_TEST(TestFsmCasesExpectSuccess)
  return EXIT_SUCCESS;
}

}  // namespace

int main() {
  return RunFsmRegressionTests();
}
