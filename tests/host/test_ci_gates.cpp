#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "loradriver/ci_gates.hpp"
#include "loradriver/profile_qualification.hpp"

namespace {

using loradriver::ChannelPolicy;
using loradriver::CiGateEngine;
using loradriver::GateCategory;
using loradriver::GateEvaluation;
using loradriver::GateReport;
using loradriver::GateReportSerializer;
using loradriver::GateResult;
using loradriver::GateRule;
using loradriver::GateSeverity;
using loradriver::GateWaiver;
using loradriver::GoNoGoThreshold;
using loradriver::HardwareProfile;
using loradriver::ProfileQualificationMatrix;
using loradriver::RadioConfig;
using loradriver::ReleaseChannel;
using loradriver::ThresholdOperator;

bool TestGateSeverityEnumValuesAreStable() {
  if (static_cast<uint8_t>(GateSeverity::kBlocking) != 0) {
    return false;
  }
  if (static_cast<uint8_t>(GateSeverity::kWarning) != 1) {
    return false;
  }
  if (static_cast<uint8_t>(GateSeverity::kAdvisory) != 2) {
    return false;
  }
  return true;
}

bool TestGateCategoryEnumValuesAreStable() {
  if (static_cast<uint8_t>(GateCategory::kInit) != 0) {
    return false;
  }
  if (static_cast<uint8_t>(GateCategory::kTxRx) != 1) {
    return false;
  }
  if (static_cast<uint8_t>(GateCategory::kIrq) != 2) {
    return false;
  }
  if (static_cast<uint8_t>(GateCategory::kTimeout) != 3) {
    return false;
  }
  if (static_cast<uint8_t>(GateCategory::kRecovery) != 4) {
    return false;
  }
  if (static_cast<uint8_t>(GateCategory::kIntegration) != 5) {
    return false;
  }
  return true;
}

bool TestThresholdOperatorEnumValuesAreStable() {
  if (static_cast<uint8_t>(ThresholdOperator::kGreaterOrEqual) != 0) {
    return false;
  }
  if (static_cast<uint8_t>(ThresholdOperator::kLessOrEqual) != 1) {
    return false;
  }
  if (static_cast<uint8_t>(ThresholdOperator::kEqual) != 2) {
    return false;
  }
  if (static_cast<uint8_t>(ThresholdOperator::kLess) != 3) {
    return false;
  }
  if (static_cast<uint8_t>(ThresholdOperator::kGreater) != 4) {
    return false;
  }
  return true;
}

bool TestGoNoGoThresholdEvaluateGreaterOrEqual() {
  GoNoGoThreshold threshold{};
  std::strcpy(threshold.metric_name, "init_success_rate");
  threshold.op = ThresholdOperator::kGreaterOrEqual;
  threshold.value = 99.0f;
  std::strcpy(threshold.unit, "percent");

  if (!threshold.evaluate(99.0f)) {
    return false;
  }
  if (!threshold.evaluate(99.5f)) {
    return false;
  }
  if (!threshold.evaluate(100.0f)) {
    return false;
  }
  if (threshold.evaluate(98.9f)) {
    return false;
  }
  return true;
}

bool TestGoNoGoThresholdEvaluateLessOrEqual() {
  GoNoGoThreshold threshold{};
  std::strcpy(threshold.metric_name, "init_time_p99");
  threshold.op = ThresholdOperator::kLessOrEqual;
  threshold.value = 500.0f;
  std::strcpy(threshold.unit, "ms");

  if (!threshold.evaluate(500.0f)) {
    return false;
  }
  if (!threshold.evaluate(400.0f)) {
    return false;
  }
  if (!threshold.evaluate(0.0f)) {
    return false;
  }
  if (threshold.evaluate(501.0f)) {
    return false;
  }
  return true;
}

bool TestGoNoGoThresholdEvaluateEqual() {
  GoNoGoThreshold threshold{};
  std::strcpy(threshold.metric_name, "irq_overflow_count");
  threshold.op = ThresholdOperator::kEqual;
  threshold.value = 0.0f;
  std::strcpy(threshold.unit, "count");

  if (!threshold.evaluate(0.0f)) {
    return false;
  }
  if (threshold.evaluate(1.0f)) {
    return false;
  }
  if (threshold.evaluate(-1.0f)) {
    return false;
  }
  return true;
}

bool TestGoNoGoThresholdEvaluateLess() {
  GoNoGoThreshold threshold{};
  std::strcpy(threshold.metric_name, "test_metric");
  threshold.op = ThresholdOperator::kLess;
  threshold.value = 100.0f;
  std::strcpy(threshold.unit, "ms");

  if (!threshold.evaluate(99.0f)) {
    return false;
  }
  if (!threshold.evaluate(0.0f)) {
    return false;
  }
  if (threshold.evaluate(100.0f)) {
    return false;
  }
  if (threshold.evaluate(101.0f)) {
    return false;
  }
  return true;
}

bool TestGoNoGoThresholdEvaluateGreater() {
  GoNoGoThreshold threshold{};
  std::strcpy(threshold.metric_name, "test_metric");
  threshold.op = ThresholdOperator::kGreater;
  threshold.value = 50.0f;
  std::strcpy(threshold.unit, "count");

  if (!threshold.evaluate(51.0f)) {
    return false;
  }
  if (!threshold.evaluate(100.0f)) {
    return false;
  }
  if (threshold.evaluate(50.0f)) {
    return false;
  }
  if (threshold.evaluate(49.0f)) {
    return false;
  }
  return true;
}

bool TestGoNoGoThresholdEqualUsesEpsilon() {
  GoNoGoThreshold threshold{};
  threshold.op = ThresholdOperator::kEqual;
  threshold.value = 100.0f;

  if (!threshold.evaluate(100.0f)) {
    return false;
  }
  if (!threshold.evaluate(100.0f + 1e-7f)) {
    return false;
  }
  if (!threshold.evaluate(100.0f - 1e-7f)) {
    return false;
  }
  if (threshold.evaluate(100.1f)) {
    return false;
  }
  return true;
}

bool TestGateRuleIsBlockingReturnsTrueForBlockingEnabled() {
  GateRule rule{};
  std::strcpy(rule.id, "INIT-001");
  rule.category = GateCategory::kInit;
  rule.severity = GateSeverity::kBlocking;
  rule.enabled = true;

  if (!rule.isBlocking()) {
    return false;
  }
  return true;
}

bool TestGateRuleIsBlockingReturnsFalseForWarning() {
  GateRule rule{};
  std::strcpy(rule.id, "TEST-001");
  rule.category = GateCategory::kInit;
  rule.severity = GateSeverity::kWarning;
  rule.enabled = true;

  if (rule.isBlocking()) {
    return false;
  }
  return true;
}

bool TestGateRuleIsBlockingReturnsFalseForDisabled() {
  GateRule rule{};
  std::strcpy(rule.id, "INIT-001");
  rule.category = GateCategory::kInit;
  rule.severity = GateSeverity::kBlocking;
  rule.enabled = false;

  if (rule.isBlocking()) {
    return false;
  }
  return true;
}

bool TestCiGateEngineGetGateRuleReturnsValidRule() {
  const GateRule* rule = CiGateEngine::getGateRule("INIT-001");
  if (rule == nullptr) {
    return false;
  }
  if (std::strcmp(rule->id, "INIT-001") != 0) {
    return false;
  }
  if (rule->category != GateCategory::kInit) {
    return false;
  }
  if (rule->severity != GateSeverity::kBlocking) {
    return false;
  }
  return true;
}

bool TestCiGateEngineGetGateRuleReturnsErrorForUnknown() {
  const GateRule* rule = CiGateEngine::getGateRule("UNKNOWN-999");
  return rule == nullptr;
}

bool TestCiGateEngineEvaluateGatePassesWhenThresholdMet() {
  GateResult result = CiGateEngine::evaluateGate("INIT-001", 99.5f);
  if (result != GateResult::kPass) {
    return false;
  }
  return true;
}

bool TestCiGateEngineEvaluateGateFailsWhenThresholdNotMet() {
  GateResult result = CiGateEngine::evaluateGate("INIT-001", 98.0f);
  if (result != GateResult::kFail) {
    return false;
  }
  return true;
}

bool TestCiGateEngineEvaluateGateReturnsErrorForUnknownGateId() {
  GateResult result = CiGateEngine::evaluateGate("UNKNOWN-999", 100.0f);
  if (result != GateResult::kError) {
    return false;
  }
  return true;
}

bool TestCiGateEngineEvaluateAllGatesPopulatesReport() {
  float values[CiGateEngine::kGateRulesCount] = {
      99.5f,
      450.0f,
      99.5f,
      98.5f,
      99.95f,
      0.0f,
      99.5f,
      99.5f,
      100.0f,
      0.0f,
      100.0f
  };

  GateReport report{};
  CiGateEngine::evaluateAllGates(values, CiGateEngine::kGateRulesCount, report);

  if (report.evaluations_count != CiGateEngine::kGateRulesCount) {
    return false;
  }
  if (report.release_blocked) {
    return false;
  }
  return true;
}

bool TestCiGateEngineIsReleaseBlockedReturnsFalseWhenAllPass() {
  GateReport report{};
  report.blocking_failed = 0;
  report.blocking_passed = 11;

  if (CiGateEngine::isReleaseBlocked(report)) {
    return false;
  }
  return true;
}

bool TestCiGateEngineIsReleaseBlockedReturnsTrueWhenAnyFail() {
  GateReport report{};
  report.blocking_failed = 1;
  report.blocking_passed = 10;

  if (!CiGateEngine::isReleaseBlocked(report)) {
    return false;
  }
  return true;
}

bool TestCiGateEngineGetFailedBlockingGatesReturnsCorrectCount() {
  GateReport report{};
  report.evaluations[0] = {"INIT-001", GateResult::kFail, 98.0f, 99.0f, false};
  report.evaluations[1] = {"INIT-002", GateResult::kPass, 450.0f, 500.0f, true};
  report.evaluations_count = 2;
  report.blocking_failed = 1;
  report.blocking_passed = 1;

  std::array<GateEvaluation, 16> failed{};
  uint8_t count = 0;
  CiGateEngine::getFailedBlockingGates(report, failed, count);

  if (count != 1) {
    return false;
  }
  if (std::strcmp(failed[0].gate_id, "INIT-001") != 0) {
    return false;
  }
  return true;
}

bool TestWaiverRequestReturnsWaiverId() {
  CiGateEngine::clearWaivers();

  uint32_t waiver_id = CiGateEngine::requestWaiver("INIT-001", "Test justification");
  if (waiver_id == 0) {
    return false;
  }
  return true;
}

bool TestWaiverApproveSetsApprovalStatus() {
  CiGateEngine::clearWaivers();

  uint32_t waiver_id = CiGateEngine::requestWaiver("INIT-002", "Approval test");
  if (!CiGateEngine::approveWaiver(waiver_id, "release-owner", 42)) {
    return false;
  }

  const GateWaiver* waiver = CiGateEngine::getWaiver(waiver_id);
  if (waiver == nullptr) {
    return false;
  }
  if (!waiver->is_approved) {
    return false;
  }
  if (waiver->approved_by_id != 42) {
    return false;
  }
  return true;
}

bool TestWaiverIsValidReturnsTrueForApprovedWaiver() {
  CiGateEngine::clearWaivers();

  uint32_t waiver_id = CiGateEngine::requestWaiver("TXRX-001", "Valid waiver test");
  CiGateEngine::approveWaiver(waiver_id, "tech-lead", 99);

  if (!CiGateEngine::isWaiverValid(waiver_id)) {
    return false;
  }
  return true;
}

bool TestWaiverIsValidReturnsFalseForUnapprovedWaiver() {
  CiGateEngine::clearWaivers();

  uint32_t waiver_id = CiGateEngine::requestWaiver("IRQ-001", "Unapproved test");

  if (CiGateEngine::isWaiverValid(waiver_id)) {
    return false;
  }
  return true;
}

bool TestWaiverIsValidReturnsFalseForExpiredWaiver() {
  CiGateEngine::clearWaivers();

  uint32_t waiver_id = CiGateEngine::requestWaiver("TIMEOUT-001", "Expired test");
  CiGateEngine::approveWaiver(waiver_id, "release-owner", 1);

  if (!CiGateEngine::setWaiverExpired(waiver_id)) {
    return false;
  }

  if (CiGateEngine::isWaiverValid(waiver_id)) {
    return false;
  }
  return true;
}

bool TestWaiverSetExpiredReturnsFalseForUnknownId() {
  CiGateEngine::clearWaivers();

  if (CiGateEngine::setWaiverExpired(99999)) {
    return false;
  }
  return true;
}

bool TestChannelPolicyGetChannelPolicyReturnsRegularPolicy() {
  const ChannelPolicy* policy = CiGateEngine::getChannelPolicy(ReleaseChannel::kRegular);
  if (policy == nullptr) {
    return false;
  }
  if (policy->channel != ReleaseChannel::kRegular) {
    return false;
  }
  if (!policy->allow_waivers) {
    return false;
  }
  return true;
}

bool TestChannelPolicyGetChannelPolicyReturnsHotfixPolicy() {
  const ChannelPolicy* policy = CiGateEngine::getChannelPolicy(ReleaseChannel::kHotfix);
  if (policy == nullptr) {
    return false;
  }
  if (policy->channel != ReleaseChannel::kHotfix) {
    return false;
  }
  if (!policy->allow_waivers) {
    return false;
  }
  return true;
}

bool TestChannelPolicyBothChannelsHaveSameBlockingGates() {
  const ChannelPolicy* regular = CiGateEngine::getChannelPolicy(ReleaseChannel::kRegular);
  const ChannelPolicy* hotfix = CiGateEngine::getChannelPolicy(ReleaseChannel::kHotfix);

  if (regular == nullptr || hotfix == nullptr) {
    return false;
  }

  if (regular->required_gates_count != hotfix->required_gates_count) {
    return false;
  }

  for (uint8_t i = 0; i < regular->required_gates_count; ++i) {
    if (std::strcmp(regular->required_gates[i], hotfix->required_gates[i]) != 0) {
      return false;
    }
  }
  return true;
}

bool TestChannelPolicyGetRequiredGatesReturnsCorrectCount() {
  std::array<char[GateRule::kMaxIdLength], 16> gates{};
  size_t count = CiGateEngine::getRequiredGates(ReleaseChannel::kRegular, gates);

  if (count != 11) {
    return false;
  }
  return true;
}

bool TestGateReportGenerationPopulatesAllFields() {
  GateReport report{};
  CiGateEngine::generateGateReport(report, 1, 2, 3);

  if (report.report_major != 1 || report.report_minor != 2 || report.report_patch != 3) {
    return false;
  }
  return true;
}

bool TestGateReportIsReproducible() {
  GateReport report1{};
  GateReport report2{};

  CiGateEngine::generateGateReport(report1, 1, 0, 0);
  CiGateEngine::generateGateReport(report2, 1, 0, 0);

  if (report1.evaluations_count != report2.evaluations_count) {
    return false;
  }
  if (report1.blocking_passed != report2.blocking_passed) {
    return false;
  }
  return true;
}

bool TestGateReportSerializerProducesOutput() {
  GateReport report{};
  CiGateEngine::generateGateReport(report, 1, 0, 0);

  char buffer[8192];
  const size_t written = GateReportSerializer::serializeTo(report, buffer, sizeof(buffer));

  if (written == 0) {
    return false;
  }
  if (buffer[0] == '\0') {
    return false;
  }
  return true;
}

bool TestGateReportSerializerContainsExpectedFields() {
  GateReport report{};
  CiGateEngine::generateGateReport(report, 2, 1, 3);

  char buffer[8192];
  const size_t written = GateReportSerializer::serializeTo(report, buffer, sizeof(buffer));

  if (written == 0) {
    return false;
  }

  const std::string output(buffer, written);
  if (output.find("GATE_REPORT") == std::string::npos) {
    return false;
  }
  if (output.find("v=2.1.3") == std::string::npos) {
    return false;
  }
  return true;
}

bool TestGateReportSerializerRejectsNullBuffer() {
  GateReport report{};
  const size_t written = GateReportSerializer::serializeTo(report, nullptr, 1024);
  return written == 0;
}

bool TestGateReportSerializerRejectsZeroBufferSize() {
  GateReport report{};
  char buffer[1];
  const size_t written = GateReportSerializer::serializeTo(report, buffer, 0);
  return written == 0;
}

bool TestAllV1GateRulesAreDefined() {
  const char* expected_gates[] = {
      "INIT-001", "INIT-002", "TXRX-001", "TXRX-002",
      "IRQ-001", "IRQ-002", "TIMEOUT-001", "RECOVERY-001",
      "RECOVERY-002", "INTEGRATION-001", "NONREG-001"
  };

  for (const char* gate_id : expected_gates) {
    const GateRule* rule = CiGateEngine::getGateRule(gate_id);
    if (rule == nullptr) {
      return false;
    }
    if (rule->severity != GateSeverity::kBlocking) {
      return false;
    }
    if (!rule->enabled) {
      return false;
    }
  }
  return true;
}

bool TestInitGateRulesHaveCorrectThresholds() {
  const GateRule* init001 = CiGateEngine::getGateRule("INIT-001");
  if (init001 == nullptr) {
    return false;
  }
  if (init001->threshold.value < 99.0f) {
    return false;
  }
  if (init001->threshold.op != ThresholdOperator::kGreaterOrEqual) {
    return false;
  }

  const GateRule* init002 = CiGateEngine::getGateRule("INIT-002");
  if (init002 == nullptr) {
    return false;
  }
  if (init002->threshold.value > 500.0f) {
    return false;
  }
  if (init002->threshold.op != ThresholdOperator::kLessOrEqual) {
    return false;
  }
  return true;
}

bool TestTxRxGateRulesHaveCorrectThresholds() {
  const GateRule* txrx001 = CiGateEngine::getGateRule("TXRX-001");
  if (txrx001 == nullptr) {
    return false;
  }
  if (txrx001->threshold.value < 99.0f) {
    return false;
  }

  const GateRule* txrx002 = CiGateEngine::getGateRule("TXRX-002");
  if (txrx002 == nullptr) {
    return false;
  }
  if (txrx002->threshold.value < 98.0f) {
    return false;
  }
  return true;
}

bool TestIrqGateRulesHaveCorrectThresholds() {
  const GateRule* irq001 = CiGateEngine::getGateRule("IRQ-001");
  if (irq001 == nullptr) {
    return false;
  }
  if (irq001->threshold.value < 99.9f) {
    return false;
  }

  const GateRule* irq002 = CiGateEngine::getGateRule("IRQ-002");
  if (irq002 == nullptr) {
    return false;
  }
  if (irq002->threshold.value != 0.0f) {
    return false;
  }
  if (irq002->threshold.op != ThresholdOperator::kEqual) {
    return false;
  }
  return true;
}

bool TestTimeoutAndRecoveryGateRulesHaveCorrectThresholds() {
  const GateRule* timeout001 = CiGateEngine::getGateRule("TIMEOUT-001");
  if (timeout001 == nullptr) {
    return false;
  }
  if (timeout001->threshold.value < 99.0f) {
    return false;
  }

  const GateRule* recovery001 = CiGateEngine::getGateRule("RECOVERY-001");
  if (recovery001 == nullptr) {
    return false;
  }
  if (recovery001->threshold.value < 99.0f) {
    return false;
  }
  return true;
}

bool TestIntegrationGateRuleHasCorrectThreshold() {
  const GateRule* integration001 = CiGateEngine::getGateRule("INTEGRATION-001");
  if (integration001 == nullptr) {
    return false;
  }
  if (integration001->threshold.value != 0.0f) {
    return false;
  }
  if (integration001->threshold.op != ThresholdOperator::kEqual) {
    return false;
  }
  return true;
}

bool TestNewGatesRecovery002AndNonreg001() {
  const GateRule* recovery002 = CiGateEngine::getGateRule("RECOVERY-002");
  if (recovery002 == nullptr) {
    return false;
  }
  if (recovery002->threshold.value != 100.0f) {
    return false;
  }

  const GateRule* nonreg001 = CiGateEngine::getGateRule("NONREG-001");
  if (nonreg001 == nullptr) {
    return false;
  }
  if (nonreg001->threshold.value != 100.0f) {
    return false;
  }
  return true;
}

bool TestEvaluateForProfileWithValidatedProfile() {
  HardwareProfile profile{RadioConfig::Chip::kSx1276, RadioConfig::Band::k868, RadioConfig::DioRouting::kDio0Only};

  float values[CiGateEngine::kGateRulesCount] = {
      99.5f, 450.0f, 99.5f, 98.5f, 99.95f, 0.0f, 99.5f, 99.5f, 100.0f, 0.0f, 100.0f
  };

  GateReport report{};
  GateResult result = CiGateEngine::evaluateForProfile(profile, values, CiGateEngine::kGateRulesCount, report);

  if (result != GateResult::kPass) {
    return false;
  }
  if (report.release_blocked) {
    return false;
  }
  return true;
}

bool TestEvaluateForProfileWithNonValidatedProfile() {
  HardwareProfile profile{RadioConfig::Chip::kSx126xStub, RadioConfig::Band::k868, RadioConfig::DioRouting::kDio0Only};

  float values[CiGateEngine::kGateRulesCount] = {
      99.5f, 450.0f, 99.5f, 98.5f, 99.95f, 0.0f, 99.5f, 99.5f, 100.0f, 0.0f, 100.0f
  };

  GateReport report{};
  GateResult result = CiGateEngine::evaluateForProfile(profile, values, CiGateEngine::kGateRulesCount, report);

  if (result != GateResult::kError) {
    return false;
  }
  return true;
}

bool TestGateRulesCountIsCorrect() {
  if (CiGateEngine::kGateRulesCount != 11) {
    return false;
  }
  return true;
}

#define RUN_TEST(fn) \
  if (!(fn)()) { \
    std::fprintf(stderr, "FAIL: %s\n", #fn); \
    return EXIT_FAILURE; \
  }

int RunCiGatesTests() {
  RUN_TEST(TestGateSeverityEnumValuesAreStable)
  RUN_TEST(TestGateCategoryEnumValuesAreStable)
  RUN_TEST(TestThresholdOperatorEnumValuesAreStable)
  RUN_TEST(TestGoNoGoThresholdEvaluateGreaterOrEqual)
  RUN_TEST(TestGoNoGoThresholdEvaluateLessOrEqual)
  RUN_TEST(TestGoNoGoThresholdEvaluateEqual)
  RUN_TEST(TestGoNoGoThresholdEvaluateLess)
  RUN_TEST(TestGoNoGoThresholdEvaluateGreater)
  RUN_TEST(TestGoNoGoThresholdEqualUsesEpsilon)
  RUN_TEST(TestGateRuleIsBlockingReturnsTrueForBlockingEnabled)
  RUN_TEST(TestGateRuleIsBlockingReturnsFalseForWarning)
  RUN_TEST(TestGateRuleIsBlockingReturnsFalseForDisabled)
  RUN_TEST(TestCiGateEngineGetGateRuleReturnsValidRule)
  RUN_TEST(TestCiGateEngineGetGateRuleReturnsErrorForUnknown)
  RUN_TEST(TestCiGateEngineEvaluateGatePassesWhenThresholdMet)
  RUN_TEST(TestCiGateEngineEvaluateGateFailsWhenThresholdNotMet)
  RUN_TEST(TestCiGateEngineEvaluateGateReturnsErrorForUnknownGateId)
  RUN_TEST(TestCiGateEngineEvaluateAllGatesPopulatesReport)
  RUN_TEST(TestCiGateEngineIsReleaseBlockedReturnsFalseWhenAllPass)
  RUN_TEST(TestCiGateEngineIsReleaseBlockedReturnsTrueWhenAnyFail)
  RUN_TEST(TestCiGateEngineGetFailedBlockingGatesReturnsCorrectCount)
  RUN_TEST(TestWaiverRequestReturnsWaiverId)
  RUN_TEST(TestWaiverApproveSetsApprovalStatus)
  RUN_TEST(TestWaiverIsValidReturnsTrueForApprovedWaiver)
  RUN_TEST(TestWaiverIsValidReturnsFalseForUnapprovedWaiver)
  RUN_TEST(TestWaiverIsValidReturnsFalseForExpiredWaiver)
  RUN_TEST(TestWaiverSetExpiredReturnsFalseForUnknownId)
  RUN_TEST(TestChannelPolicyGetChannelPolicyReturnsRegularPolicy)
  RUN_TEST(TestChannelPolicyGetChannelPolicyReturnsHotfixPolicy)
  RUN_TEST(TestChannelPolicyBothChannelsHaveSameBlockingGates)
  RUN_TEST(TestChannelPolicyGetRequiredGatesReturnsCorrectCount)
  RUN_TEST(TestGateReportGenerationPopulatesAllFields)
  RUN_TEST(TestGateReportIsReproducible)
  RUN_TEST(TestGateReportSerializerProducesOutput)
  RUN_TEST(TestGateReportSerializerContainsExpectedFields)
  RUN_TEST(TestGateReportSerializerRejectsNullBuffer)
  RUN_TEST(TestGateReportSerializerRejectsZeroBufferSize)
  RUN_TEST(TestAllV1GateRulesAreDefined)
  RUN_TEST(TestInitGateRulesHaveCorrectThresholds)
  RUN_TEST(TestTxRxGateRulesHaveCorrectThresholds)
  RUN_TEST(TestIrqGateRulesHaveCorrectThresholds)
  RUN_TEST(TestTimeoutAndRecoveryGateRulesHaveCorrectThresholds)
  RUN_TEST(TestIntegrationGateRuleHasCorrectThreshold)
  RUN_TEST(TestNewGatesRecovery002AndNonreg001)
  RUN_TEST(TestEvaluateForProfileWithValidatedProfile)
  RUN_TEST(TestEvaluateForProfileWithNonValidatedProfile)
  RUN_TEST(TestGateRulesCountIsCorrect)
  return EXIT_SUCCESS;
}

}  // namespace

int main() {
  return RunCiGatesTests();
}
