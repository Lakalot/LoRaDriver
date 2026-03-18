#include "loradriver/profile_qualification.hpp"
#include "loradriver/artifact_governance.hpp"
#include <cstdio>
#include <cstring>

namespace loradriver {

namespace {
HardwareProfile makeProfile(RadioConfig::Chip chip, RadioConfig::Band band, RadioConfig::DioRouting irq) {
  return HardwareProfile{chip, band, irq};
}

ProfileQualificationEntry makeValidatedEntry(RadioConfig::Chip chip, RadioConfig::Band band,
                                              RadioConfig::DioRouting irq, const char* criteria,
                                              std::initializer_list<uint32_t> test_ids) {
  ProfileQualificationEntry entry{};
  entry.profile = makeProfile(chip, band, irq);
  entry.status = ProfileStatus::kValidated;
  std::strncpy(entry.validation_criteria, criteria, ProfileQualificationEntry::kMaxCriteriaLength - 1);
  entry.validation_criteria[ProfileQualificationEntry::kMaxCriteriaLength - 1] = '\0';

  // Explicitly zero-initialize full array before partial fill (L2 fix).
  entry.required_test_ids.fill(0);

  size_t i = 0;
  for (uint32_t tid : test_ids) {
    if (i < entry.required_test_ids.size()) {
      entry.required_test_ids[i++] = tid;
    }
  }
  entry.required_tests_count = static_cast<uint8_t>(i);
  entry.last_tested_major = 0;
  entry.last_tested_minor = 0;
  entry.last_tested_patch = 0;
  entry.pass_rate_threshold = 100;
  return entry;
}

/// Deferred entry: chip + band + irq are specified so the entry can be found via
/// getQualificationEntry. For SX126x "any band / any IRQ", we store a representative
/// combination. Lookups for non-matching SX126x combinations use the dedicated
/// deferred-chip check in computeV1Index / getQualificationEntry.
ProfileQualificationEntry makeDeferredEntry(RadioConfig::Chip chip, RadioConfig::Band band,
                                            RadioConfig::DioRouting irq, const char* reason) {
  ProfileQualificationEntry entry{};
  entry.profile = makeProfile(chip, band, irq);
  entry.status = ProfileStatus::kDeferred;
  std::strncpy(entry.validation_criteria, reason, ProfileQualificationEntry::kMaxCriteriaLength - 1);
  entry.validation_criteria[ProfileQualificationEntry::kMaxCriteriaLength - 1] = '\0';
  entry.required_test_ids.fill(0);
  entry.required_tests_count = 0;
  entry.last_tested_major = 0;
  entry.last_tested_minor = 0;
  entry.last_tested_patch = 0;
  entry.pass_rate_threshold = 0;
  return entry;
}
}  // namespace

const std::array<ProfileQualificationEntry, ProfileQualificationMatrix::kMatrixSize>
    ProfileQualificationMatrix::kQualificationMatrix_ = {{
        makeValidatedEntry(RadioConfig::Chip::kSx1276, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Only,
                           "Primary V1 target - SX1276 433MHz DIO0", {1001, 1002, 1003, 1004}),
        makeValidatedEntry(RadioConfig::Chip::kSx1276, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Dio1,
                           "Primary V1 target - SX1276 433MHz DIO0+DIO1", {1011, 1012, 1013, 1014}),
        makeValidatedEntry(RadioConfig::Chip::kSx1276, RadioConfig::Band::k868, RadioConfig::DioRouting::kDio0Only,
                           "Primary V1 target - SX1276 868MHz DIO0", {1021, 1022, 1023, 1024}),
        makeValidatedEntry(RadioConfig::Chip::kSx1276, RadioConfig::Band::k868, RadioConfig::DioRouting::kDio0Dio1,
                           "Primary V1 target - SX1276 868MHz DIO0+DIO1", {1031, 1032, 1033, 1034}),
        makeValidatedEntry(RadioConfig::Chip::kSx1278, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Only,
                           "Primary V1 target - SX1278 433MHz DIO0", {2001, 2002, 2003, 2004}),
        makeValidatedEntry(RadioConfig::Chip::kSx1278, RadioConfig::Band::k433, RadioConfig::DioRouting::kDio0Dio1,
                           "Primary V1 target - SX1278 433MHz DIO0+DIO1", {2011, 2012, 2013, 2014}),
        makeValidatedEntry(RadioConfig::Chip::kSx1278, RadioConfig::Band::k868, RadioConfig::DioRouting::kDio0Only,
                           "Primary V1 target - SX1278 868MHz DIO0", {2021, 2022, 2023, 2024}),
        makeValidatedEntry(RadioConfig::Chip::kSx1278, RadioConfig::Band::k868, RadioConfig::DioRouting::kDio0Dio1,
                           "Primary V1 target - SX1278 868MHz DIO0+DIO1", {2031, 2032, 2033, 2034}),
        makeDeferredEntry(RadioConfig::Chip::kSx126xStub, RadioConfig::Band::k433,
                         RadioConfig::DioRouting::kDio0Only, "V1-bis scope - deferred until V2"),
    }};

int ProfileQualificationMatrix::computeV1Index(RadioConfig::Chip chip, RadioConfig::Band band,
                                                RadioConfig::DioRouting irq) noexcept {
  const auto chip_val = static_cast<int>(chip);
  const auto band_val = static_cast<int>(band);
  const auto irq_val = static_cast<int>(irq);

  // V1 validated profiles: kSx1276(0), kSx1278(1) only.
  if (chip_val < 0 || chip_val >= static_cast<int>(kV1ChipCount)) {
    return -1;
  }
  if (band_val < 0 || band_val >= static_cast<int>(kV1BandCount)) {
    return -1;
  }
  if (irq_val < 0 || irq_val >= static_cast<int>(kV1IrqCount)) {
    return -1;
  }

  // Flat index: chip * (bands * irqs) + band * irqs + irq
  return chip_val * static_cast<int>(kV1BandCount * kV1IrqCount)
       + band_val * static_cast<int>(kV1IrqCount)
       + irq_val;
}

ProfileStatus ProfileQualificationMatrix::getProfileStatus(RadioConfig::Chip chip, RadioConfig::Band band,
                                                            RadioConfig::DioRouting irq) noexcept {
  const auto* entry = getQualificationEntry(chip, band, irq);
  return entry ? entry->status : ProfileStatus::kDeferred;
}

ProfileStatus ProfileQualificationMatrix::getProfileStatus(const HardwareProfile& profile) noexcept {
  return getProfileStatus(profile.chip, profile.band, profile.irq);
}

const ProfileQualificationEntry* ProfileQualificationMatrix::getQualificationEntry(
    RadioConfig::Chip chip, RadioConfig::Band band, RadioConfig::DioRouting irq) noexcept {
  // O(1) lookup for V1 validated profiles via flat index.
  const int idx = computeV1Index(chip, band, irq);
  if (idx >= 0 && idx < static_cast<int>(kV1ValidatedCount)) {
    return &kQualificationMatrix_[static_cast<size_t>(idx)];
  }

  // Deferred chip (SX126x) - stored at index kV1ValidatedCount (position 8).
  if (chip == RadioConfig::Chip::kSx126xStub) {
    return &kQualificationMatrix_[kV1ValidatedCount];
  }

  return nullptr;
}

const ProfileQualificationEntry* ProfileQualificationMatrix::getQualificationEntry(
    const HardwareProfile& profile) noexcept {
  return getQualificationEntry(profile.chip, profile.band, profile.irq);
}

bool ProfileQualificationMatrix::isProfileValidated(RadioConfig::Chip chip, RadioConfig::Band band,
                                                     RadioConfig::DioRouting irq) noexcept {
  return getProfileStatus(chip, band, irq) == ProfileStatus::kValidated;
}

bool ProfileQualificationMatrix::isProfileValidated(const HardwareProfile& profile) noexcept {
  return isProfileValidated(profile.chip, profile.band, profile.irq);
}

bool ProfileQualificationMatrix::isReleaseBlocking(RadioConfig::Chip chip, RadioConfig::Band band,
                                                   RadioConfig::DioRouting irq) noexcept {
  const auto* entry = getQualificationEntry(chip, band, irq);
  return entry ? entry->isReleaseBlocking() : false;
}

bool ProfileQualificationMatrix::isReleaseBlocking(const HardwareProfile& profile) noexcept {
  return isReleaseBlocking(profile.chip, profile.band, profile.irq);
}

size_t ProfileQualificationMatrix::getAllValidatedProfiles(
    std::array<HardwareProfile, 16>& out_profiles) noexcept {
  size_t count = 0;
  for (const auto& entry : kQualificationMatrix_) {
    if (entry.status == ProfileStatus::kValidated && count < out_profiles.size()) {
      out_profiles[count++] = entry.profile;
    }
  }
  return count;
}

size_t ProfileQualificationMatrix::getTotalProfileCount() noexcept {
  return kQualificationMatrix_.size();
}

void ProfileQualificationMatrix::generateQualificationReport(QualificationReport& out_report, uint8_t major,
                                                              uint8_t minor, uint8_t patch) noexcept {
  out_report.validated_count = 0;
  out_report.secondary_count = 0;
  out_report.deferred_count = 0;
  out_report.experimental_count = 0;
  out_report.total_profiles = static_cast<uint8_t>(kQualificationMatrix_.size());
  out_report.results_count = 0;
  out_report.report_major = major;
  out_report.report_minor = minor;
  out_report.report_patch = patch;

  for (const auto& entry : kQualificationMatrix_) {
    if (out_report.results_count >= out_report.profile_results.size()) break;

    auto& result = out_report.profile_results[out_report.results_count++];
    result.profile = entry.profile;
    result.status = entry.status;
    result.pass_rate = entry.pass_rate_threshold;

    switch (entry.status) {
      case ProfileStatus::kValidated:
        result.status_code = 'V';
        out_report.validated_count++;
        break;
      case ProfileStatus::kSecondary:
        result.status_code = 'S';
        out_report.secondary_count++;
        break;
      case ProfileStatus::kDeferred:
        result.status_code = 'D';
        out_report.deferred_count++;
        break;
      case ProfileStatus::kExperimental:
        result.status_code = 'E';
        out_report.experimental_count++;
        break;
    }
  }

  ArtifactMetadata metadata{};
  metadata.type = ArtifactType::kValidationReport;
  metadata.created_timestamp = 1;
  metadata.retention_days = 90;
  std::snprintf(metadata.linked_version, sizeof(metadata.linked_version), "%u.%u.%u", major, minor, patch);
  std::strncpy(metadata.source_module, "ProfileQualificationMatrix", sizeof(metadata.source_module) - 1);
  metadata.source_module[sizeof(metadata.source_module) - 1] = '\0';
  (void)ArtifactRegistry::registerArtifact(metadata);
}

uint32_t ProfileGovernance::next_change_id_ = 1;
std::array<ProfileStatusChange, ProfileGovernance::kMaxPendingChanges> ProfileGovernance::pending_changes_{};
size_t ProfileGovernance::pending_count_ = 0;
std::array<ProfileStatusChange, ProfileGovernance::kMaxAuditLogSize> ProfileGovernance::audit_log_{};
size_t ProfileGovernance::audit_log_count_ = 0;

uint32_t ProfileGovernance::proposeStatusChange(const HardwareProfile& profile, ProfileStatus new_status,
                                                 const char* justification, uint8_t version_major,
                                                 uint8_t version_minor, uint8_t version_patch) noexcept {
  if (pending_count_ >= kMaxPendingChanges) {
    return 0;
  }

  ProfileStatusChange change{};
  change.profile = profile;
  change.old_status = ProfileQualificationMatrix::getProfileStatus(profile);
  change.new_status = new_status;
  change.change_id = next_change_id_++;
  change.approver_id = 0;
  change.approved = 0;
  change.change_major = version_major;
  change.change_minor = version_minor;
  change.change_patch = version_patch;

  if (justification != nullptr) {
    std::strncpy(change.justification, justification, ProfileStatusChange::kMaxJustificationLength - 1);
    change.justification[ProfileStatusChange::kMaxJustificationLength - 1] = '\0';
  } else {
    change.justification[0] = '\0';
  }

  pending_changes_[pending_count_++] = change;
  return change.change_id;
}

bool ProfileGovernance::approveStatusChange(uint32_t change_id, uint32_t approver_id) noexcept {
  for (size_t i = 0; i < pending_count_; ++i) {
    if (pending_changes_[i].change_id == change_id && pending_changes_[i].approved == 0) {
      pending_changes_[i].approved = 1;
      pending_changes_[i].approver_id = approver_id;

      if (audit_log_count_ < kMaxAuditLogSize) {
        audit_log_[audit_log_count_++] = pending_changes_[i];
      }

      pending_changes_[i] = pending_changes_[--pending_count_];
      return true;
    }
  }
  return false;
}

bool ProfileGovernance::rejectStatusChange(uint32_t change_id, uint32_t approver_id) noexcept {
  for (size_t i = 0; i < pending_count_; ++i) {
    if (pending_changes_[i].change_id == change_id && pending_changes_[i].approved == 0) {
      pending_changes_[i].approved = 2;
      pending_changes_[i].approver_id = approver_id;

      if (audit_log_count_ < kMaxAuditLogSize) {
        audit_log_[audit_log_count_++] = pending_changes_[i];
      }

      pending_changes_[i] = pending_changes_[--pending_count_];
      return true;
    }
  }
  return false;
}

const ProfileStatusChange* ProfileGovernance::getPendingChange(uint32_t change_id) noexcept {
  for (size_t i = 0; i < pending_count_; ++i) {
    if (pending_changes_[i].change_id == change_id) {
      return &pending_changes_[i];
    }
  }
  return nullptr;
}

size_t ProfileGovernance::getPendingChangesCount() noexcept {
  return pending_count_;
}

size_t ProfileGovernance::getAllPendingChanges(
    std::array<ProfileStatusChange, kMaxPendingChanges>& out_changes) noexcept {
  for (size_t i = 0; i < pending_count_; ++i) {
    out_changes[i] = pending_changes_[i];
  }
  return pending_count_;
}

size_t ProfileGovernance::getAuditLogSize() noexcept {
  return audit_log_count_;
}

void ProfileGovernance::getAuditLogEntry(size_t index, ProfileStatusChange& out_entry) noexcept {
  if (index < audit_log_count_) {
    out_entry = audit_log_[index];
  }
}

void ProfileGovernance::clearPendingChanges() noexcept {
  pending_count_ = 0;
}

void ProfileGovernance::clearAuditLog() noexcept {
  audit_log_count_ = 0;
}

void ProfileGovernance::resetAll() noexcept {
  pending_count_ = 0;
  audit_log_count_ = 0;
  next_change_id_ = 1;
}

size_t QualificationReportSerializer::serializeTo(const QualificationReport& report, char* buffer,
                                                   size_t buffer_size) noexcept {
  if (buffer == nullptr || buffer_size == 0) {
    return 0;
  }

  size_t written = 0;
  int result = std::snprintf(buffer + written, buffer_size - written,
                              "QUALIFICATION_REPORT:v=%u.%u.%u:total=%u:V=%u:S=%u:D=%u:E=%u\n",
                              report.report_major, report.report_minor, report.report_patch,
                              report.total_profiles, report.validated_count, report.secondary_count,
                              report.deferred_count, report.experimental_count);

  if (result < 0 || static_cast<size_t>(result) >= buffer_size - written) {
    return 0;
  }
  written += static_cast<size_t>(result);

  for (uint8_t i = 0; i < report.results_count && written < buffer_size - 1; ++i) {
    const auto& pr = report.profile_results[i];
    result = std::snprintf(buffer + written, buffer_size - written, "PROFILE:c=%d:b=%d:i=%d:s=%c:r=%u\n",
                           static_cast<int>(pr.profile.chip), static_cast<int>(pr.profile.band),
                           static_cast<int>(pr.profile.irq), pr.status_code, pr.pass_rate);

    if (result < 0 || static_cast<size_t>(result) >= buffer_size - written) {
      return 0;
    }
    written += static_cast<size_t>(result);
  }

  return written;
}

size_t QualificationReportSerializer::serializeEntryTo(const ProfileQualificationEntry& entry, char* buffer,
                                                        size_t buffer_size) noexcept {
  if (buffer == nullptr || buffer_size == 0) {
    return 0;
  }

  int result = std::snprintf(buffer, buffer_size,
                              "ENTRY:c=%d:b=%d:i=%d:status=%u:criteria=%s:tests=%u:threshold=%u\n",
                              static_cast<int>(entry.profile.chip), static_cast<int>(entry.profile.band),
                              static_cast<int>(entry.profile.irq), static_cast<unsigned>(entry.status),
                              entry.validation_criteria, static_cast<unsigned>(entry.required_tests_count),
                              entry.pass_rate_threshold);

  return (result > 0 && static_cast<size_t>(result) < buffer_size) ? static_cast<size_t>(result) : 0;
}

size_t QualificationReportSerializer::serializeStatusChangeTo(const ProfileStatusChange& change, char* buffer,
                                                               size_t buffer_size) noexcept {
  if (buffer == nullptr || buffer_size == 0) {
    return 0;
  }

  int result = std::snprintf(buffer, buffer_size,
                              "CHANGE:id=%u:c=%d:b=%d:i=%d:old=%u:new=%u:approved=%u:approver=%u:why=%s\n",
                              change.change_id, static_cast<int>(change.profile.chip),
                              static_cast<int>(change.profile.band), static_cast<int>(change.profile.irq),
                              static_cast<unsigned>(change.old_status), static_cast<unsigned>(change.new_status),
                              static_cast<unsigned>(change.approved), change.approver_id, change.justification);

  return (result > 0 && static_cast<size_t>(result) < buffer_size) ? static_cast<size_t>(result) : 0;
}

}  // namespace loradriver
