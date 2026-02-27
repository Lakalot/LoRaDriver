#pragma once

#include <loradriver/radio_config.hpp>
#include <array>
#include <cstdint>

namespace loradriver {

enum class ProfileStatus : uint8_t {
  kValidated = 0,
  kSecondary = 1,
  kDeferred = 2,
  kExperimental = 3
};

struct HardwareProfile {
  RadioConfig::Chip chip;
  RadioConfig::Band band;
  RadioConfig::DioRouting irq;

  [[nodiscard]] constexpr bool operator==(const HardwareProfile& other) const noexcept {
    return chip == other.chip && band == other.band && irq == other.irq;
  }

  [[nodiscard]] constexpr bool operator!=(const HardwareProfile& other) const noexcept {
    return !(*this == other);
  }
};

struct ProfileQualificationEntry {
  HardwareProfile profile;
  ProfileStatus status;
  static constexpr size_t kMaxCriteriaLength = 64;
  static constexpr size_t kMaxTestsCount = 8;

  char validation_criteria[kMaxCriteriaLength];
  std::array<uint32_t, kMaxTestsCount> required_test_ids;
  uint8_t required_tests_count;

  uint8_t last_tested_major;
  uint8_t last_tested_minor;
  uint8_t last_tested_patch;
  uint8_t pass_rate_threshold;

  [[nodiscard]] constexpr bool isReleaseBlocking() const noexcept {
    return status == ProfileStatus::kValidated;
  }
};

struct ProfileStatusChange {
  static constexpr size_t kMaxJustificationLength = 128;

  HardwareProfile profile;
  ProfileStatus old_status;
  ProfileStatus new_status;
  char justification[kMaxJustificationLength];
  uint32_t change_id;
  uint32_t approver_id;
  uint8_t approved;
  uint8_t change_major;
  uint8_t change_minor;
  uint8_t change_patch;
};

struct QualificationReport {
  /// Maximum number of profiles tracked in a single report.
  /// Shared with ProfileQualificationMatrix::kMaxProfiles.
  static constexpr size_t kMaxProfiles = 16;

  uint8_t validated_count;
  uint8_t secondary_count;
  uint8_t deferred_count;
  uint8_t experimental_count;
  uint8_t total_profiles;

  struct ProfileResult {
    HardwareProfile profile;
    ProfileStatus status;
    uint8_t pass_rate;
    char status_code;
  };

  std::array<ProfileResult, kMaxProfiles> profile_results;
  uint8_t results_count;

  uint8_t report_major;
  uint8_t report_minor;
  uint8_t report_patch;
};

class ProfileQualificationMatrix {
 public:
  static constexpr size_t kMaxProfiles = 16;
  static constexpr size_t kMatrixSize = 9;

  /// V1 validated profile count (SX1276/SX1278 x 433/868 x DIO0/DIO0+DIO1).
  static constexpr size_t kV1ValidatedCount = 8;

  /// Number of chips in the V1 matrix index (kSx1276=0, kSx1278=1).
  static constexpr size_t kV1ChipCount = 2;
  /// Number of bands (k433=0, k868=1).
  static constexpr size_t kV1BandCount = 2;
  /// Number of DIO routing modes (kDio0Only=0, kDio0Dio1=1).
  static constexpr size_t kV1IrqCount = 2;

  [[nodiscard]] static ProfileStatus getProfileStatus(RadioConfig::Chip chip,
                                                       RadioConfig::Band band,
                                                       RadioConfig::DioRouting irq) noexcept;

  [[nodiscard]] static ProfileStatus getProfileStatus(const HardwareProfile& profile) noexcept;

  [[nodiscard]] static const ProfileQualificationEntry* getQualificationEntry(
      RadioConfig::Chip chip, RadioConfig::Band band, RadioConfig::DioRouting irq) noexcept;

  [[nodiscard]] static const ProfileQualificationEntry* getQualificationEntry(
      const HardwareProfile& profile) noexcept;

  [[nodiscard]] static bool isProfileValidated(RadioConfig::Chip chip,
                                                RadioConfig::Band band,
                                                RadioConfig::DioRouting irq) noexcept;

  [[nodiscard]] static bool isProfileValidated(const HardwareProfile& profile) noexcept;

  [[nodiscard]] static bool isReleaseBlocking(RadioConfig::Chip chip,
                                               RadioConfig::Band band,
                                               RadioConfig::DioRouting irq) noexcept;

  [[nodiscard]] static bool isReleaseBlocking(const HardwareProfile& profile) noexcept;

  [[nodiscard]] static size_t getAllValidatedProfiles(
      std::array<HardwareProfile, kMaxProfiles>& out_profiles) noexcept;

  [[nodiscard]] static size_t getTotalProfileCount() noexcept;

  static void generateQualificationReport(QualificationReport& out_report,
                                          uint8_t major, uint8_t minor, uint8_t patch) noexcept;

 private:
  static const std::array<ProfileQualificationEntry, kMatrixSize> kQualificationMatrix_;

  /// Compute flat index for V1 validated profiles. Returns -1 for non-V1 profiles.
  [[nodiscard]] static int computeV1Index(RadioConfig::Chip chip,
                                           RadioConfig::Band band,
                                           RadioConfig::DioRouting irq) noexcept;
};

class ProfileGovernance {
 public:
  static constexpr size_t kMaxPendingChanges = 16;
  static constexpr size_t kMaxAuditLogSize = 64;

  [[nodiscard]] static uint32_t proposeStatusChange(
      const HardwareProfile& profile,
      ProfileStatus new_status,
      const char* justification,
      uint8_t version_major, uint8_t version_minor, uint8_t version_patch) noexcept;

  [[nodiscard]] static bool approveStatusChange(uint32_t change_id, uint32_t approver_id) noexcept;

  [[nodiscard]] static bool rejectStatusChange(uint32_t change_id, uint32_t approver_id) noexcept;

  [[nodiscard]] static const ProfileStatusChange* getPendingChange(uint32_t change_id) noexcept;

  [[nodiscard]] static size_t getPendingChangesCount() noexcept;

  [[nodiscard]] static size_t getAllPendingChanges(
      std::array<ProfileStatusChange, kMaxPendingChanges>& out_changes) noexcept;

  [[nodiscard]] static size_t getAuditLogSize() noexcept;

  static void getAuditLogEntry(size_t index, ProfileStatusChange& out_entry) noexcept;

  static void clearPendingChanges() noexcept;

  /// Reset audit log (for test isolation).
  static void clearAuditLog() noexcept;

  /// Full reset: pending changes, audit log, and change ID counter (for test isolation).
  static void resetAll() noexcept;

 private:
  static uint32_t next_change_id_;
  static std::array<ProfileStatusChange, kMaxPendingChanges> pending_changes_;
  static size_t pending_count_;
  static std::array<ProfileStatusChange, kMaxAuditLogSize> audit_log_;
  static size_t audit_log_count_;
};

struct QualificationReportSerializer {
  [[nodiscard]] static size_t serializeTo(const QualificationReport& report,
                                          char* buffer, size_t buffer_size) noexcept;

  [[nodiscard]] static size_t serializeEntryTo(const ProfileQualificationEntry& entry,
                                               char* buffer, size_t buffer_size) noexcept;

  [[nodiscard]] static size_t serializeStatusChangeTo(const ProfileStatusChange& change,
                                                      char* buffer, size_t buffer_size) noexcept;
};

}  // namespace loradriver
