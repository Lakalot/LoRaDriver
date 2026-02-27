#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "loradriver/non_regression.hpp"
#include "loradriver/profile_qualification.hpp"

namespace {

using loradriver::HardwareProfile;
using loradriver::RadioConfig;
using loradriver::RecoveryEvidence;
using loradriver::RecoveryEvidenceCollector;

HardwareProfile MakeTestProfile() {
  HardwareProfile profile{};
  profile.chip = RadioConfig::Chip::kSx1276;
  profile.band = RadioConfig::Band::k868;
  profile.irq = RadioConfig::DioRouting::kDio0Only;
  return profile;
}

bool TestRecoveryEvidenceIsCompleteReturnsTrueWhenBothSet() {
  RecoveryEvidence evidence{};
  evidence.timeout_recovery_success = true;
  evidence.timeout_recovery_latency_ms = 100;
  evidence.sleep_wakeup_success = true;
  evidence.wakeup_latency_ms = 50;

  if (!evidence.isComplete()) return false;
  return true;
}

bool TestRecoveryEvidenceIsCompleteReturnsFalseWhenTimeoutMissing() {
  RecoveryEvidence evidence{};
  evidence.timeout_recovery_success = false;
  evidence.sleep_wakeup_success = true;

  if (evidence.isComplete()) return false;
  return true;
}

bool TestRecoveryEvidenceIsCompleteReturnsFalseWhenSleepWakeupMissing() {
  RecoveryEvidence evidence{};
  evidence.timeout_recovery_success = true;
  evidence.sleep_wakeup_success = false;

  if (evidence.isComplete()) return false;
  return true;
}

bool TestRecoveryEvidenceIsCompleteReturnsFalseWhenLatenciesAreZero() {
  // Both flags set but latencies are zero (uninitialised evidence) - must not be considered complete.
  RecoveryEvidence evidence{};
  evidence.timeout_recovery_success = true;
  evidence.timeout_recovery_latency_ms = 0;
  evidence.sleep_wakeup_success = true;
  evidence.wakeup_latency_ms = 0;

  if (evidence.isComplete()) return false;
  return true;
}

bool TestRecoveryEvidenceCollectorCollectTimeoutRecoveryReturnsSuccess() {
  RecoveryEvidenceCollector::clearEvidence();
  HardwareProfile profile = MakeTestProfile();

  RecoveryEvidence evidence = RecoveryEvidenceCollector::collectTimeoutRecoveryEvidence(profile);

  if (!evidence.timeout_recovery_success) return false;
  if (evidence.sleep_wakeup_success) return false;
  return true;
}

bool TestRecoveryEvidenceCollectorCollectSleepWakeupReturnsSuccess() {
  RecoveryEvidenceCollector::clearEvidence();
  HardwareProfile profile = MakeTestProfile();

  RecoveryEvidence evidence = RecoveryEvidenceCollector::collectSleepWakeupEvidence(profile);

  if (evidence.timeout_recovery_success) return false;
  if (!evidence.sleep_wakeup_success) return false;
  return true;
}

bool TestRecoveryEvidenceCollectorValidateRecoveryEvidenceReturnsTrueForComplete() {
  RecoveryEvidence evidence{};
  evidence.timeout_recovery_success = true;
  evidence.timeout_recovery_latency_ms = 100;
  evidence.sleep_wakeup_success = true;
  evidence.wakeup_latency_ms = 50;

  if (!RecoveryEvidenceCollector::validateRecoveryEvidence(evidence)) return false;
  return true;
}

bool TestRecoveryEvidenceCollectorValidateRecoveryEvidenceReturnsFalseForIncomplete() {
  RecoveryEvidence evidence{};
  evidence.timeout_recovery_success = true;
  evidence.sleep_wakeup_success = false;

  if (RecoveryEvidenceCollector::validateRecoveryEvidence(evidence)) return false;
  return true;
}

bool TestRecoveryEvidenceCollectorIsEvidenceCompleteReturnsTrueAfterCollection() {
  RecoveryEvidenceCollector::clearEvidence();
  HardwareProfile profile = MakeTestProfile();

  RecoveryEvidenceCollector::collectTimeoutRecoveryEvidence(profile);
  RecoveryEvidenceCollector::collectSleepWakeupEvidence(profile);

  if (!RecoveryEvidenceCollector::isEvidenceComplete(profile)) return false;
  return true;
}

bool TestRecoveryEvidenceCollectorIsEvidenceCompleteReturnsFalseBeforeCollection() {
  RecoveryEvidenceCollector::clearEvidence();
  HardwareProfile profile = MakeTestProfile();

  if (RecoveryEvidenceCollector::isEvidenceComplete(profile)) return false;
  return true;
}

bool TestRecoveryEvidenceCollectorGetEvidenceReturnsNullForUnknownProfile() {
  RecoveryEvidenceCollector::clearEvidence();
  HardwareProfile profile = MakeTestProfile();

  const RecoveryEvidence* evidence = RecoveryEvidenceCollector::getEvidence(profile);
  if (evidence != nullptr) return false;
  return true;
}

bool TestRecoveryEvidenceCollectorGetEvidenceReturnsValidAfterCollection() {
  RecoveryEvidenceCollector::clearEvidence();
  HardwareProfile profile = MakeTestProfile();

  RecoveryEvidenceCollector::collectTimeoutRecoveryEvidence(profile);
  const RecoveryEvidence* evidence = RecoveryEvidenceCollector::getEvidence(profile);

  if (evidence == nullptr) return false;
  if (!evidence->timeout_recovery_success) return false;
  return true;
}

bool TestRecoveryEvidenceCollectorClearEvidenceRemovesAllEvidence() {
  RecoveryEvidenceCollector::clearEvidence();
  HardwareProfile profile = MakeTestProfile();

  RecoveryEvidenceCollector::collectTimeoutRecoveryEvidence(profile);
  RecoveryEvidenceCollector::clearEvidence();

  const RecoveryEvidence* evidence = RecoveryEvidenceCollector::getEvidence(profile);
  if (evidence != nullptr) return false;
  return true;
}

bool TestRecoveryEvidenceCollectorCollectAllValidatedEvidenceReturnsCount() {
  RecoveryEvidenceCollector::clearEvidence();

  std::array<RecoveryEvidence, 16> evidence{};
  size_t count = RecoveryEvidenceCollector::collectAllValidatedEvidence(evidence);

  if (count == 0) return false;
  return true;
}

bool TestRecoveryEvidenceCollectorSerializeEvidenceProducesOutput() {
  RecoveryEvidence evidence{};
  evidence.profile = MakeTestProfile();
  evidence.timeout_recovery_success = true;
  evidence.timeout_recovery_latency_ms = 100;
  evidence.sleep_wakeup_success = true;
  evidence.wakeup_latency_ms = 50;
  evidence.firmware_major = 1;
  evidence.firmware_minor = 0;
  evidence.firmware_patch = 0;

  char buffer[256];
  RecoveryEvidenceCollector::serializeEvidence(evidence, buffer, sizeof(buffer));

  if (buffer[0] == '\0') return false;
  if (std::strstr(buffer, "RecoveryEvidence") == nullptr) return false;
  return true;
}

bool TestRecoveryEvidenceHasLatencyValues() {
  RecoveryEvidenceCollector::clearEvidence();
  HardwareProfile profile = MakeTestProfile();

  RecoveryEvidence timeout_ev = RecoveryEvidenceCollector::collectTimeoutRecoveryEvidence(profile);
  if (timeout_ev.timeout_recovery_latency_ms == 0) return false;

  RecoveryEvidence sleep_ev = RecoveryEvidenceCollector::collectSleepWakeupEvidence(profile);
  if (sleep_ev.wakeup_latency_ms == 0) return false;

  return true;
}

bool TestRecoveryEvidenceHasFirmwareVersion() {
  RecoveryEvidence evidence{};
  evidence.firmware_major = 1;
  evidence.firmware_minor = 2;
  evidence.firmware_patch = 3;

  if (evidence.firmware_major != 1) return false;
  if (evidence.firmware_minor != 2) return false;
  if (evidence.firmware_patch != 3) return false;
  return true;
}

bool TestRecoveryEvidenceCollectorCombinesEvidenceForSameProfile() {
  RecoveryEvidenceCollector::clearEvidence();
  HardwareProfile profile = MakeTestProfile();

  RecoveryEvidenceCollector::collectTimeoutRecoveryEvidence(profile);
  RecoveryEvidenceCollector::collectSleepWakeupEvidence(profile);

  const RecoveryEvidence* evidence = RecoveryEvidenceCollector::getEvidence(profile);
  if (evidence == nullptr) return false;
  if (!evidence->timeout_recovery_success) return false;
  if (!evidence->sleep_wakeup_success) return false;
  return true;
}

bool TestNonRegressionReportSerializerEvidenceProducesOutput() {
  RecoveryEvidence evidence{};
  evidence.profile = MakeTestProfile();
  evidence.timeout_recovery_success = true;
  evidence.sleep_wakeup_success = true;

  char buffer[256];
  size_t written = loradriver::NonRegressionReportSerializer::serializeEvidenceTo(
      evidence, buffer, sizeof(buffer));

  if (written == 0) return false;
  return true;
}

#define RUN_TEST(fn) \
  if (!(fn)()) { \
    std::fprintf(stderr, "FAIL: %s\n", #fn); \
    return EXIT_FAILURE; \
  }

int RunSleepWakeupRecoveryTests() {
  RUN_TEST(TestRecoveryEvidenceIsCompleteReturnsTrueWhenBothSet)
  RUN_TEST(TestRecoveryEvidenceIsCompleteReturnsFalseWhenTimeoutMissing)
  RUN_TEST(TestRecoveryEvidenceIsCompleteReturnsFalseWhenSleepWakeupMissing)
  RUN_TEST(TestRecoveryEvidenceIsCompleteReturnsFalseWhenLatenciesAreZero)
  RUN_TEST(TestRecoveryEvidenceCollectorCollectTimeoutRecoveryReturnsSuccess)
  RUN_TEST(TestRecoveryEvidenceCollectorCollectSleepWakeupReturnsSuccess)
  RUN_TEST(TestRecoveryEvidenceCollectorValidateRecoveryEvidenceReturnsTrueForComplete)
  RUN_TEST(TestRecoveryEvidenceCollectorValidateRecoveryEvidenceReturnsFalseForIncomplete)
  RUN_TEST(TestRecoveryEvidenceCollectorIsEvidenceCompleteReturnsTrueAfterCollection)
  RUN_TEST(TestRecoveryEvidenceCollectorIsEvidenceCompleteReturnsFalseBeforeCollection)
  RUN_TEST(TestRecoveryEvidenceCollectorGetEvidenceReturnsNullForUnknownProfile)
  RUN_TEST(TestRecoveryEvidenceCollectorGetEvidenceReturnsValidAfterCollection)
  RUN_TEST(TestRecoveryEvidenceCollectorClearEvidenceRemovesAllEvidence)
  RUN_TEST(TestRecoveryEvidenceCollectorCollectAllValidatedEvidenceReturnsCount)
  RUN_TEST(TestRecoveryEvidenceCollectorSerializeEvidenceProducesOutput)
  RUN_TEST(TestRecoveryEvidenceHasLatencyValues)
  RUN_TEST(TestRecoveryEvidenceHasFirmwareVersion)
  RUN_TEST(TestRecoveryEvidenceCollectorCombinesEvidenceForSameProfile)
  RUN_TEST(TestNonRegressionReportSerializerEvidenceProducesOutput)
  return EXIT_SUCCESS;
}

}  // namespace

int main() {
  return RunSleepWakeupRecoveryTests();
}
