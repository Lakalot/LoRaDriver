#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "loradriver/non_regression.hpp"
#include "loradriver/profile_qualification.hpp"

namespace {

using loradriver::HardwareProfile;
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

HardwareProfile MakeDio01Profile() {
  HardwareProfile profile{};
  profile.chip = RadioConfig::Chip::kSx1276;
  profile.band = RadioConfig::Band::k868;
  profile.irq = RadioConfig::DioRouting::kDio0Dio1;
  return profile;
}

bool TestIrqRaceCase001HasCorrectProperties() {
  const NonRegressionCase* c = NonRegressionExecutor::getCase("NR-IRQ-001");
  if (c == nullptr) return false;
  if (c->category != NonRegressionCategory::kIrqRace) return false;
  if (c->applies_to_all_profiles) return false;
  return true;
}

bool TestIrqRaceCase002HasCorrectProperties() {
  const NonRegressionCase* c = NonRegressionExecutor::getCase("NR-IRQ-002");
  if (c == nullptr) return false;
  if (c->category != NonRegressionCategory::kIrqRace) return false;
  if (!c->applies_to_all_profiles) return false;
  return true;
}

bool TestIrqRaceCase001MatchesOnlyDio01Profiles() {
  const NonRegressionCase* c = NonRegressionExecutor::getCase("NR-IRQ-001");
  if (c == nullptr) return false;

  HardwareProfile dio0_only = MakeTestProfile();
  HardwareProfile dio01 = MakeDio01Profile();

  if (c->matchesProfile(dio0_only)) return false;
  if (!c->matchesProfile(dio01)) return false;
  return true;
}

bool TestIrqRaceCase002MatchesAllProfiles() {
  const NonRegressionCase* c = NonRegressionExecutor::getCase("NR-IRQ-002");
  if (c == nullptr) return false;

  HardwareProfile dio0_only = MakeTestProfile();
  HardwareProfile dio01 = MakeDio01Profile();

  if (!c->matchesProfile(dio0_only)) return false;
  if (!c->matchesProfile(dio01)) return false;
  return true;
}

bool TestIrqRaceCase001ExecutionSkipsForNonMatchingProfile() {
  HardwareProfile dio0_only = MakeTestProfile();
  loradriver::CaseExecutionResult result =
      NonRegressionExecutor::executeCase("NR-IRQ-001", dio0_only);

  if (!result.passed) return false;
  if (!result.baseline_matched) return false;
  if (std::strstr(result.delta_detail, "Skipped") == nullptr) return false;
  return true;
}

bool TestIrqRaceCase001ExecutionPassesForMatchingProfile() {
  HardwareProfile dio01 = MakeDio01Profile();
  loradriver::CaseExecutionResult result =
      NonRegressionExecutor::executeCase("NR-IRQ-001", dio01);

  if (!result.passed) return false;
  if (!result.baseline_matched) return false;
  return true;
}

bool TestIrqRaceCase002ExecutionPassesForAllProfiles() {
  HardwareProfile dio0_only = MakeTestProfile();
  HardwareProfile dio01 = MakeDio01Profile();

  loradriver::CaseExecutionResult result1 =
      NonRegressionExecutor::executeCase("NR-IRQ-002", dio0_only);
  loradriver::CaseExecutionResult result2 =
      NonRegressionExecutor::executeCase("NR-IRQ-002", dio01);

  if (!result1.passed || !result1.baseline_matched) return false;
  if (!result2.passed || !result2.baseline_matched) return false;
  return true;
}

bool TestIrqRaceCasesHaveCorrectPatternIds() {
  const NonRegressionCase* c1 = NonRegressionExecutor::getCase("NR-IRQ-001");
  const NonRegressionCase* c2 = NonRegressionExecutor::getCase("NR-IRQ-002");

  if (c1 == nullptr || c2 == nullptr) return false;
  if (std::strstr(c1->incident_pattern_id, "IRQ") == nullptr) return false;
  if (std::strstr(c2->incident_pattern_id, "IRQ") == nullptr) return false;
  return true;
}

bool TestIrqRaceCasesHaveBaselineLatency() {
  const NonRegressionCase* c1 = NonRegressionExecutor::getCase("NR-IRQ-001");
  const NonRegressionCase* c2 = NonRegressionExecutor::getCase("NR-IRQ-002");

  if (c1 == nullptr || c2 == nullptr) return false;
  if (c1->baseline_result.expected_latency_ms == 0) return false;
  if (c2->baseline_result.expected_latency_ms == 0) return false;
  return true;
}

bool TestIrqRaceCasesAreEnabled() {
  const NonRegressionCase* c1 = NonRegressionExecutor::getCase("NR-IRQ-001");
  const NonRegressionCase* c2 = NonRegressionExecutor::getCase("NR-IRQ-002");

  if (c1 == nullptr || c2 == nullptr) return false;
  if (!c1->enabled) return false;
  if (!c2->enabled) return false;
  return true;
}

bool TestIrqRaceCasesExpectSuccess() {
  const NonRegressionCase* c1 = NonRegressionExecutor::getCase("NR-IRQ-001");
  const NonRegressionCase* c2 = NonRegressionExecutor::getCase("NR-IRQ-002");

  if (c1 == nullptr || c2 == nullptr) return false;
  if (!c1->baseline_result.expected_success) return false;
  if (!c2->baseline_result.expected_success) return false;
  return true;
}

bool TestSuiteExecutionIncludesIrqRaceCases() {
  HardwareProfile profile = MakeDio01Profile();
  loradriver::SuiteExecutionReport report{};
  NonRegressionExecutor::executeSuite("V1-CRITICAL", profile, report);

  bool found_irq001 = false;
  bool found_irq002 = false;
  for (uint8_t i = 0; i < report.results_count; ++i) {
    if (std::strcmp(report.results[i].case_id, "NR-IRQ-001") == 0) found_irq001 = true;
    if (std::strcmp(report.results[i].case_id, "NR-IRQ-002") == 0) found_irq002 = true;
  }

  if (!found_irq001) return false;
  if (!found_irq002) return false;
  return true;
}

#define RUN_TEST(fn) \
  if (!(fn)()) { \
    std::fprintf(stderr, "FAIL: %s\n", #fn); \
    return EXIT_FAILURE; \
  }

int RunIrqRaceScenarioTests() {
  RUN_TEST(TestIrqRaceCase001HasCorrectProperties)
  RUN_TEST(TestIrqRaceCase002HasCorrectProperties)
  RUN_TEST(TestIrqRaceCase001MatchesOnlyDio01Profiles)
  RUN_TEST(TestIrqRaceCase002MatchesAllProfiles)
  RUN_TEST(TestIrqRaceCase001ExecutionSkipsForNonMatchingProfile)
  RUN_TEST(TestIrqRaceCase001ExecutionPassesForMatchingProfile)
  RUN_TEST(TestIrqRaceCase002ExecutionPassesForAllProfiles)
  RUN_TEST(TestIrqRaceCasesHaveCorrectPatternIds)
  RUN_TEST(TestIrqRaceCasesHaveBaselineLatency)
  RUN_TEST(TestIrqRaceCasesAreEnabled)
  RUN_TEST(TestIrqRaceCasesExpectSuccess)
  RUN_TEST(TestSuiteExecutionIncludesIrqRaceCases)
  return EXIT_SUCCESS;
}

}  // namespace

int main() {
  return RunIrqRaceScenarioTests();
}
