#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <tuple>

#include "loradriver/profile_qualification.hpp"
#include "loradriver/radio_config.hpp"

namespace {

using loradriver::HardwareProfile;
using loradriver::ProfileGovernance;
using loradriver::ProfileQualificationEntry;
using loradriver::ProfileQualificationMatrix;
using loradriver::ProfileStatus;
using loradriver::ProfileStatusChange;
using loradriver::QualificationReport;
using loradriver::QualificationReportSerializer;
using loradriver::RadioConfig;

bool TestProfileStatusEnumValuesAreStable() {
  if (static_cast<uint8_t>(ProfileStatus::kValidated) != 0) {
    return false;
  }
  if (static_cast<uint8_t>(ProfileStatus::kSecondary) != 1) {
    return false;
  }
  if (static_cast<uint8_t>(ProfileStatus::kDeferred) != 2) {
    return false;
  }
  if (static_cast<uint8_t>(ProfileStatus::kExperimental) != 3) {
    return false;
  }
  return true;
}

bool TestHardwareProfileEqualityWorks() {
  HardwareProfile p1{RadioConfig::Chip::kSx1276, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Only};
  HardwareProfile p2{RadioConfig::Chip::kSx1276, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Only};
  HardwareProfile p3{RadioConfig::Chip::kSx1278, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Only};

  if (!(p1 == p2)) {
    return false;
  }
  if (p1 == p3) {
    return false;
  }
  if (!(p1 != p3)) {
    return false;
  }
  return true;
}

bool TestGetProfileStatusReturnsValidatedForV1Profiles() {
  const std::array<std::tuple<RadioConfig::Chip, RadioConfig::Band, RadioConfig::DioRouting>, 8> v1_profiles = {{
      {RadioConfig::Chip::kSx1276, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Only},
      {RadioConfig::Chip::kSx1276, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Dio1},
      {RadioConfig::Chip::kSx1276, RadioConfig::Band::k868, RadioConfig::DioRouting::kDio0Only},
      {RadioConfig::Chip::kSx1276, RadioConfig::Band::k868, RadioConfig::DioRouting::kDio0Dio1},
      {RadioConfig::Chip::kSx1278, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Only},
      {RadioConfig::Chip::kSx1278, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Dio1},
      {RadioConfig::Chip::kSx1278, RadioConfig::Band::k868, RadioConfig::DioRouting::kDio0Only},
      {RadioConfig::Chip::kSx1278, RadioConfig::Band::k868, RadioConfig::DioRouting::kDio0Dio1},
  }};

  for (const auto& [chip, band, irq] : v1_profiles) {
    const ProfileStatus status = ProfileQualificationMatrix::getProfileStatus(chip, band, irq);
    if (status != ProfileStatus::kValidated) {
      return false;
    }
  }
  return true;
}

bool TestGetProfileStatusReturnsDeferredForSx126x() {
  const ProfileStatus status = ProfileQualificationMatrix::getProfileStatus(
      RadioConfig::Chip::kSx126xStub, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Only);
  return status == ProfileStatus::kDeferred;
}

bool TestGetProfileStatusWithHardwareProfileStruct() {
  HardwareProfile profile{RadioConfig::Chip::kSx1276, RadioConfig::Band::k868, RadioConfig::DioRouting::kDio0Dio1};
  const ProfileStatus status = ProfileQualificationMatrix::getProfileStatus(profile);
  return status == ProfileStatus::kValidated;
}

bool TestGetQualificationEntryReturnsValidEntry() {
  const ProfileQualificationEntry* entry = ProfileQualificationMatrix::getQualificationEntry(
      RadioConfig::Chip::kSx1276, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Only);

  if (entry == nullptr) {
    return false;
  }
  if (entry->status != ProfileStatus::kValidated) {
    return false;
  }
  if (entry->profile.chip != RadioConfig::Chip::kSx1276) {
    return false;
  }
  if (entry->profile.band != RadioConfig::Band::k433) {
    return false;
  }
  if (entry->profile.irq != RadioConfig::DioRouting::kDio0Only) {
    return false;
  }
  return true;
}

bool TestGetQualificationEntryReturnsNullForUnknownProfile() {
  HardwareProfile unknown{static_cast<RadioConfig::Chip>(99), RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Only};
  const ProfileQualificationEntry* entry = ProfileQualificationMatrix::getQualificationEntry(unknown);
  return entry == nullptr;
}

bool TestIsProfileValidatedReturnsTrueForV1Profiles() {
  if (!ProfileQualificationMatrix::isProfileValidated(
          RadioConfig::Chip::kSx1276, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Only)) {
    return false;
  }
  if (!ProfileQualificationMatrix::isProfileValidated(
          RadioConfig::Chip::kSx1278, RadioConfig::Band::k868, RadioConfig::DioRouting::kDio0Dio1)) {
    return false;
  }
  return true;
}

bool TestIsProfileValidatedReturnsFalseForDeferred() {
  if (ProfileQualificationMatrix::isProfileValidated(
          RadioConfig::Chip::kSx126xStub, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Only)) {
    return false;
  }
  return true;
}

bool TestIsReleaseBlockingReturnsTrueOnlyForValidated() {
  if (!ProfileQualificationMatrix::isReleaseBlocking(
          RadioConfig::Chip::kSx1276, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Only)) {
    return false;
  }
  if (ProfileQualificationMatrix::isReleaseBlocking(
          RadioConfig::Chip::kSx126xStub, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Only)) {
    return false;
  }
  return true;
}

bool TestGetAllValidatedProfilesReturnsCorrectCount() {
  std::array<HardwareProfile, 16> profiles{};
  const size_t count = ProfileQualificationMatrix::getAllValidatedProfiles(profiles);
  return count == 8;
}

bool TestGetAllValidatedProfilesPopulatesAllV1Profiles() {
  std::array<HardwareProfile, 16> profiles{};
  const size_t count = ProfileQualificationMatrix::getAllValidatedProfiles(profiles);

  bool found_1276_433_dio0 = false;
  bool found_1278_868_dio1 = false;

  for (size_t i = 0; i < count; ++i) {
    if (profiles[i].chip == RadioConfig::Chip::kSx1276 && profiles[i].band == RadioConfig::Band::k433 &&
        profiles[i].irq == RadioConfig::DioRouting::kDio0Only) {
      found_1276_433_dio0 = true;
    }
    if (profiles[i].chip == RadioConfig::Chip::kSx1278 && profiles[i].band == RadioConfig::Band::k868 &&
        profiles[i].irq == RadioConfig::DioRouting::kDio0Dio1) {
      found_1278_868_dio1 = true;
    }
  }

  return found_1276_433_dio0 && found_1278_868_dio1;
}

bool TestGetTotalProfileCountReturnsCorrectValue() {
  return ProfileQualificationMatrix::getTotalProfileCount() == 9;
}

bool TestGenerateQualificationReportPopulatesAllFields() {
  QualificationReport report{};
  ProfileQualificationMatrix::generateQualificationReport(report, 1, 2, 3);

  if (report.report_major != 1 || report.report_minor != 2 || report.report_patch != 3) {
    return false;
  }
  if (report.validated_count != 8) {
    return false;
  }
  if (report.deferred_count != 1) {
    return false;
  }
  if (report.secondary_count != 0) {
    return false;
  }
  if (report.experimental_count != 0) {
    return false;
  }
  if (report.total_profiles != 9) {
    return false;
  }
  if (report.results_count != 9) {
    return false;
  }
  return true;
}

bool TestGenerateQualificationReportStatusCodesAreCorrect() {
  QualificationReport report{};
  ProfileQualificationMatrix::generateQualificationReport(report, 0, 0, 0);

  int validated_with_v = 0;
  int deferred_with_d = 0;

  for (uint8_t i = 0; i < report.results_count; ++i) {
    if (report.profile_results[i].status == ProfileStatus::kValidated) {
      if (report.profile_results[i].status_code == 'V') {
        validated_with_v++;
      }
    }
    if (report.profile_results[i].status == ProfileStatus::kDeferred) {
      if (report.profile_results[i].status_code == 'D') {
        deferred_with_d++;
      }
    }
  }

  return validated_with_v == 8 && deferred_with_d == 1;
}

bool TestQualificationEntryIsReleaseBlockingMethod() {
  const ProfileQualificationEntry* validated = ProfileQualificationMatrix::getQualificationEntry(
      RadioConfig::Chip::kSx1276, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Only);
  const ProfileQualificationEntry* deferred = ProfileQualificationMatrix::getQualificationEntry(
      RadioConfig::Chip::kSx126xStub, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Only);

  if (!validated->isReleaseBlocking()) {
    return false;
  }
  if (deferred->isReleaseBlocking()) {
    return false;
  }
  return true;
}

bool TestProfileQualificationEntryHasValidationCriteria() {
  const ProfileQualificationEntry* entry = ProfileQualificationMatrix::getQualificationEntry(
      RadioConfig::Chip::kSx1276, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Only);

  if (entry == nullptr) {
    return false;
  }
  if (entry->validation_criteria[0] == '\0') {
    return false;
  }
  return true;
}

bool TestProfileQualificationEntryHasRequiredTestIds() {
  const ProfileQualificationEntry* entry = ProfileQualificationMatrix::getQualificationEntry(
      RadioConfig::Chip::kSx1276, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Only);

  if (entry == nullptr) {
    return false;
  }
  if (entry->required_tests_count == 0) {
    return false;
  }
  if (entry->required_test_ids[0] == 0) {
    return false;
  }
  return true;
}

bool TestMatrixCompletenessForV1Scope() {
  const std::array<RadioConfig::Chip, 2> chips = {RadioConfig::Chip::kSx1276, RadioConfig::Chip::kSx1278};
  const std::array<RadioConfig::Band, 2> bands = {RadioConfig::Band::k433, RadioConfig::Band::k868};
  const std::array<RadioConfig::DioRouting, 2> irqs = {RadioConfig::DioRouting::kDio0Only, RadioConfig::DioRouting::kDio0Dio1};

  for (const auto chip : chips) {
    for (const auto band : bands) {
      for (const auto irq : irqs) {
        const ProfileQualificationEntry* entry = ProfileQualificationMatrix::getQualificationEntry(chip, band, irq);
        if (entry == nullptr) {
          return false;
        }
        if (entry->status != ProfileStatus::kValidated) {
          return false;
        }
      }
    }
  }
  return true;
}

bool TestProfileStatusLookupDeterminism() {
  for (int i = 0; i < 10; ++i) {
    const ProfileStatus s1 = ProfileQualificationMatrix::getProfileStatus(
        RadioConfig::Chip::kSx1276, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Only);
    const ProfileStatus s2 = ProfileQualificationMatrix::getProfileStatus(
        RadioConfig::Chip::kSx1276, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Only);
    if (s1 != s2) {
      return false;
    }
  }
  return true;
}

bool TestReportGenerationIsReproducible() {
  QualificationReport report1{};
  QualificationReport report2{};

  ProfileQualificationMatrix::generateQualificationReport(report1, 1, 0, 0);
  ProfileQualificationMatrix::generateQualificationReport(report2, 1, 0, 0);

  if (report1.validated_count != report2.validated_count) {
    return false;
  }
  if (report1.deferred_count != report2.deferred_count) {
    return false;
  }
  if (report1.results_count != report2.results_count) {
    return false;
  }

  for (uint8_t i = 0; i < report1.results_count; ++i) {
    if (report1.profile_results[i].status != report2.profile_results[i].status) {
      return false;
    }
    if (report1.profile_results[i].status_code != report2.profile_results[i].status_code) {
      return false;
    }
  }
  return true;
}

bool TestGovernanceProposeStatusChangeReturnsChangeId() {
  ProfileGovernance::resetAll();

  HardwareProfile profile{RadioConfig::Chip::kSx1276, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Only};
  const uint32_t change_id = ProfileGovernance::proposeStatusChange(
      profile, ProfileStatus::kSecondary, "Demoting to secondary", 1, 0, 0);

  if (change_id == 0) {
    return false;
  }
  return true;
}

bool TestGovernanceGetPendingChangeReturnsCorrectChange() {
  ProfileGovernance::resetAll();

  HardwareProfile profile{RadioConfig::Chip::kSx1276, RadioConfig::Band::k868, RadioConfig::DioRouting::kDio0Dio1};
  const uint32_t change_id = ProfileGovernance::proposeStatusChange(
      profile, ProfileStatus::kExperimental, "Testing new mode", 1, 1, 0);

  const ProfileStatusChange* change = ProfileGovernance::getPendingChange(change_id);
  if (change == nullptr) {
    return false;
  }
  if (change->change_id != change_id) {
    return false;
  }
  if (change->new_status != ProfileStatus::kExperimental) {
    return false;
  }
  if (change->old_status != ProfileStatus::kValidated) {
    return false;
  }
  return true;
}

bool TestGovernanceApproveStatusChangeUpdatesApprovalStatus() {
  ProfileGovernance::resetAll();

  HardwareProfile profile{RadioConfig::Chip::kSx1278, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Only};
  const uint32_t change_id = ProfileGovernance::proposeStatusChange(
      profile, ProfileStatus::kSecondary, "Approval test", 1, 2, 0);

  if (!ProfileGovernance::approveStatusChange(change_id, 42)) {
    return false;
  }

  const ProfileStatusChange* change = ProfileGovernance::getPendingChange(change_id);
  if (change != nullptr) {
    return false;
  }

  if (ProfileGovernance::getAuditLogSize() == 0) {
    return false;
  }

  ProfileStatusChange audit_entry;
  ProfileGovernance::getAuditLogEntry(0, audit_entry);
  if (audit_entry.change_id != change_id) {
    return false;
  }
  if (audit_entry.approved != 1) {
    return false;
  }
  if (audit_entry.approver_id != 42) {
    return false;
  }
  return true;
}

bool TestGovernanceRejectStatusChangeUpdatesRejectionStatus() {
  ProfileGovernance::resetAll();

  HardwareProfile profile{RadioConfig::Chip::kSx1278, RadioConfig::Band::k868, RadioConfig::DioRouting::kDio0Dio1};
  const uint32_t change_id = ProfileGovernance::proposeStatusChange(
      profile, ProfileStatus::kDeferred, "Rejection test", 1, 3, 0);

  if (!ProfileGovernance::rejectStatusChange(change_id, 99)) {
    return false;
  }

  const ProfileStatusChange* change = ProfileGovernance::getPendingChange(change_id);
  if (change != nullptr) {
    return false;
  }
  return true;
}

bool TestGovernanceGetPendingChangesCount() {
  ProfileGovernance::resetAll();

  if (ProfileGovernance::getPendingChangesCount() != 0) {
    return false;
  }

  HardwareProfile profile{RadioConfig::Chip::kSx1276, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Only};
  ProfileGovernance::proposeStatusChange(profile, ProfileStatus::kSecondary, "Test 1", 1, 0, 0);
  ProfileGovernance::proposeStatusChange(profile, ProfileStatus::kExperimental, "Test 2", 1, 0, 0);

  if (ProfileGovernance::getPendingChangesCount() != 2) {
    return false;
  }
  return true;
}

bool TestGovernanceGetAllPendingChanges() {
  ProfileGovernance::resetAll();

  HardwareProfile profile1{RadioConfig::Chip::kSx1276, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Only};
  HardwareProfile profile2{RadioConfig::Chip::kSx1278, RadioConfig::Band::k868, RadioConfig::DioRouting::kDio0Dio1};

  ProfileGovernance::proposeStatusChange(profile1, ProfileStatus::kSecondary, "Change 1", 1, 0, 0);
  ProfileGovernance::proposeStatusChange(profile2, ProfileStatus::kExperimental, "Change 2", 1, 0, 0);

  std::array<ProfileStatusChange, 16> pending{};
  const size_t count = ProfileGovernance::getAllPendingChanges(pending);

  if (count != 2) {
    return false;
  }
  return true;
}

bool TestGovernanceClearPendingChanges() {
  ProfileGovernance::resetAll();
  HardwareProfile profile{RadioConfig::Chip::kSx1276, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Only};
  ProfileGovernance::proposeStatusChange(profile, ProfileStatus::kSecondary, "Test", 1, 0, 0);

  ProfileGovernance::clearPendingChanges();

  if (ProfileGovernance::getPendingChangesCount() != 0) {
    return false;
  }
  return true;
}

bool TestGovernanceStatusChangeWorkflowIsDeterministic() {
  ProfileGovernance::resetAll();

  HardwareProfile profile{RadioConfig::Chip::kSx1276, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Only};
  const uint32_t id1 = ProfileGovernance::proposeStatusChange(
      profile, ProfileStatus::kSecondary, "Determinism test", 1, 0, 0);
  const uint32_t id2 = ProfileGovernance::proposeStatusChange(
      profile, ProfileStatus::kSecondary, "Determinism test", 1, 0, 0);

  if (id1 == id2) {
    return false;
  }
  if (id2 != id1 + 1) {
    return false;
  }
  return true;
}

bool TestSerializerReportToBufferProducesOutput() {
  QualificationReport report{};
  ProfileQualificationMatrix::generateQualificationReport(report, 1, 0, 0);

  char buffer[4096];
  const size_t written = QualificationReportSerializer::serializeTo(report, buffer, sizeof(buffer));

  if (written == 0) {
    return false;
  }
  if (buffer[0] == '\0') {
    return false;
  }
  return true;
}

bool TestSerializerReportContainsExpectedFields() {
  QualificationReport report{};
  ProfileQualificationMatrix::generateQualificationReport(report, 2, 1, 3);

  char buffer[4096];
  const size_t written = QualificationReportSerializer::serializeTo(report, buffer, sizeof(buffer));

  if (written == 0) {
    return false;
  }

  const std::string output(buffer, written);
  if (output.find("QUALIFICATION_REPORT") == std::string::npos) {
    return false;
  }
  if (output.find("v=2.1.3") == std::string::npos) {
    return false;
  }
  if (output.find("PROFILE:") == std::string::npos) {
    return false;
  }
  return true;
}

bool TestSerializerEntryToBufferProducesOutput() {
  const ProfileQualificationEntry* entry = ProfileQualificationMatrix::getQualificationEntry(
      RadioConfig::Chip::kSx1276, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Only);

  if (entry == nullptr) {
    return false;
  }

  char buffer[1024];
  const size_t written = QualificationReportSerializer::serializeEntryTo(*entry, buffer, sizeof(buffer));

  if (written == 0) {
    return false;
  }
  if (buffer[0] == '\0') {
    return false;
  }
  return true;
}

bool TestSerializerStatusChangeToBufferProducesOutput() {
  ProfileGovernance::resetAll();

  HardwareProfile profile{RadioConfig::Chip::kSx1276, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Only};
  const uint32_t change_id = ProfileGovernance::proposeStatusChange(
      profile, ProfileStatus::kSecondary, "Serialization test", 1, 0, 0);

  const ProfileStatusChange* change = ProfileGovernance::getPendingChange(change_id);
  if (change == nullptr) {
    return false;
  }

  char buffer[1024];
  const size_t written = QualificationReportSerializer::serializeStatusChangeTo(*change, buffer, sizeof(buffer));

  if (written == 0) {
    return false;
  }
  if (buffer[0] == '\0') {
    return false;
  }
  return true;
}

bool TestSerializerRejectsNullBuffer() {
  QualificationReport report{};
  const size_t written = QualificationReportSerializer::serializeTo(report, nullptr, 1024);
  return written == 0;
}

bool TestSerializerRejectsZeroBufferSize() {
  QualificationReport report{};
  char buffer[1];
  const size_t written = QualificationReportSerializer::serializeTo(report, buffer, 0);
  return written == 0;
}

bool TestDeferredChipReturnsDeferredForAnyBandAndIrq() {
  // SX126x should return kDeferred regardless of band/IRQ combination (M5 fix).
  const ProfileStatus s1 = ProfileQualificationMatrix::getProfileStatus(
      RadioConfig::Chip::kSx126xStub, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Only);
  const ProfileStatus s2 = ProfileQualificationMatrix::getProfileStatus(
      RadioConfig::Chip::kSx126xStub, RadioConfig::Band::k868, RadioConfig::DioRouting::kDio0Dio1);
  const ProfileStatus s3 = ProfileQualificationMatrix::getProfileStatus(
      RadioConfig::Chip::kSx126xStub, RadioConfig::Band::k868, RadioConfig::DioRouting::kDio0Only);
  const ProfileStatus s4 = ProfileQualificationMatrix::getProfileStatus(
      RadioConfig::Chip::kSx126xStub, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Dio1);

  if (s1 != ProfileStatus::kDeferred) return false;
  if (s2 != ProfileStatus::kDeferred) return false;
  if (s3 != ProfileStatus::kDeferred) return false;
  if (s4 != ProfileStatus::kDeferred) return false;

  // getQualificationEntry should also return non-null for any SX126x combo.
  const auto* e1 = ProfileQualificationMatrix::getQualificationEntry(
      RadioConfig::Chip::kSx126xStub, RadioConfig::Band::k868, RadioConfig::DioRouting::kDio0Dio1);
  if (e1 == nullptr) return false;
  if (e1->status != ProfileStatus::kDeferred) return false;

  return true;
}

#define RUN_TEST(fn) \
  if (!(fn)()) { \
    std::fprintf(stderr, "FAIL: %s\n", #fn); \
    return EXIT_FAILURE; \
  }

int RunProfileQualificationTests() {
  RUN_TEST(TestProfileStatusEnumValuesAreStable)
  RUN_TEST(TestHardwareProfileEqualityWorks)
  RUN_TEST(TestGetProfileStatusReturnsValidatedForV1Profiles)
  RUN_TEST(TestGetProfileStatusReturnsDeferredForSx126x)
  RUN_TEST(TestGetProfileStatusWithHardwareProfileStruct)
  RUN_TEST(TestGetQualificationEntryReturnsValidEntry)
  RUN_TEST(TestGetQualificationEntryReturnsNullForUnknownProfile)
  RUN_TEST(TestIsProfileValidatedReturnsTrueForV1Profiles)
  RUN_TEST(TestIsProfileValidatedReturnsFalseForDeferred)
  RUN_TEST(TestIsReleaseBlockingReturnsTrueOnlyForValidated)
  RUN_TEST(TestGetAllValidatedProfilesReturnsCorrectCount)
  RUN_TEST(TestGetAllValidatedProfilesPopulatesAllV1Profiles)
  RUN_TEST(TestGetTotalProfileCountReturnsCorrectValue)
  RUN_TEST(TestGenerateQualificationReportPopulatesAllFields)
  RUN_TEST(TestGenerateQualificationReportStatusCodesAreCorrect)
  RUN_TEST(TestQualificationEntryIsReleaseBlockingMethod)
  RUN_TEST(TestProfileQualificationEntryHasValidationCriteria)
  RUN_TEST(TestProfileQualificationEntryHasRequiredTestIds)
  RUN_TEST(TestMatrixCompletenessForV1Scope)
  RUN_TEST(TestProfileStatusLookupDeterminism)
  RUN_TEST(TestReportGenerationIsReproducible)
  RUN_TEST(TestGovernanceProposeStatusChangeReturnsChangeId)
  RUN_TEST(TestGovernanceGetPendingChangeReturnsCorrectChange)
  RUN_TEST(TestGovernanceApproveStatusChangeUpdatesApprovalStatus)
  RUN_TEST(TestGovernanceRejectStatusChangeUpdatesRejectionStatus)
  RUN_TEST(TestGovernanceGetPendingChangesCount)
  RUN_TEST(TestGovernanceGetAllPendingChanges)
  RUN_TEST(TestGovernanceClearPendingChanges)
  RUN_TEST(TestGovernanceStatusChangeWorkflowIsDeterministic)
  RUN_TEST(TestSerializerReportToBufferProducesOutput)
  RUN_TEST(TestSerializerReportContainsExpectedFields)
  RUN_TEST(TestSerializerEntryToBufferProducesOutput)
  RUN_TEST(TestSerializerStatusChangeToBufferProducesOutput)
  RUN_TEST(TestSerializerRejectsNullBuffer)
  RUN_TEST(TestSerializerRejectsZeroBufferSize)
  RUN_TEST(TestDeferredChipReturnsDeferredForAnyBandAndIrq)
  return EXIT_SUCCESS;
}

}  // namespace

int main() {
  return RunProfileQualificationTests();
}
