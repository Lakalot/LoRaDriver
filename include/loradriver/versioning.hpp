#pragma once

#include <array>
#include <cstdint>

#include <loradriver/lora_error.hpp>

namespace loradriver {

struct SemVerVersion {
  uint8_t major = 0;
  uint8_t minor = 0;
  uint8_t patch = 0;
  char prerelease[16] = {};
  char build_metadata[16] = {};

  [[nodiscard]] constexpr bool isRelease() const noexcept {
    return prerelease[0] == '\0';
  }

  [[nodiscard]] constexpr bool isPrerelease() const noexcept {
    return prerelease[0] != '\0';
  }

  [[nodiscard]] constexpr int compare(const SemVerVersion& other) const noexcept {
    if (major != other.major) return major > other.major ? 1 : -1;
    if (minor != other.minor) return minor > other.minor ? 1 : -1;
    if (patch != other.patch) return patch > other.patch ? 1 : -1;

    const bool this_pre = prerelease[0] != '\0';
    const bool other_pre = other.prerelease[0] != '\0';
    if (this_pre != other_pre) {
      return this_pre ? -1 : 1;
    }
    if (!this_pre) {
      return 0;
    }

    size_t i = 0;
    while (prerelease[i] != '\0' && other.prerelease[i] != '\0') {
      if (prerelease[i] != other.prerelease[i]) {
        return prerelease[i] > other.prerelease[i] ? 1 : -1;
      }
      ++i;
    }
    if (prerelease[i] == '\0' && other.prerelease[i] == '\0') {
      return 0;
    }
    return prerelease[i] == '\0' ? -1 : 1;
  }

  [[nodiscard]] constexpr bool operator==(const SemVerVersion& other) const noexcept {
    return compare(other) == 0;
  }

  [[nodiscard]] constexpr bool operator!=(const SemVerVersion& other) const noexcept {
    return compare(other) != 0;
  }

  [[nodiscard]] constexpr bool operator<(const SemVerVersion& other) const noexcept {
    return compare(other) < 0;
  }

  [[nodiscard]] constexpr bool operator<=(const SemVerVersion& other) const noexcept {
    return compare(other) <= 0;
  }

  [[nodiscard]] constexpr bool operator>(const SemVerVersion& other) const noexcept {
    return compare(other) > 0;
  }

  [[nodiscard]] constexpr bool operator>=(const SemVerVersion& other) const noexcept {
    return compare(other) >= 0;
  }

  [[nodiscard]] constexpr bool isValid() const noexcept {
    if (major > 99 || minor > 99 || patch > 99) return false;

    for (size_t i = 0; prerelease[i] != '\0'; ++i) {
      const char c = prerelease[i];
      const bool valid = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
                         (c >= 'A' && c <= 'Z') || c == '-' || c == '.';
      if (!valid) return false;
    }

    for (size_t i = 0; build_metadata[i] != '\0'; ++i) {
      const char c = build_metadata[i];
      const bool valid = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
                         (c >= 'A' && c <= 'Z') || c == '-' || c == '.';
      if (!valid) return false;
    }

    return true;
  }
};

enum class ChangeCategory : uint8_t {
  kBreaking = 0,
  kFeature = 1,
  kFix = 2,
  kInternal = 3,
  kDeprecation = 4,
  kSecurity = 5
};

enum class VersionCompatibility : uint8_t {
  kCompatible = 0,
  kMigrationRequired = 1,
  kBreaking = 2
};

struct ChangelogEntry {
  static constexpr size_t kMaxDescriptionLength = 256;
  static constexpr size_t kMaxNotesLength = 256;
  static constexpr size_t kMaxGuideLength = 128;
  static constexpr size_t kMaxRefsLength = 64;
  static constexpr size_t kMaxContributorLength = 32;

  SemVerVersion version;
  uint32_t date;
  ChangeCategory category;
  char description[kMaxDescriptionLength] = {};
  char breaking_notes[kMaxNotesLength] = {};
  char migration_guide[kMaxGuideLength] = {};
  char issue_refs[kMaxRefsLength] = {};
  char contributor[kMaxContributorLength] = {};

  [[nodiscard]] constexpr bool isValid() const noexcept {
    if (!version.isValid()) return false;
    if (description[0] == '\0') return false;
    if (category == ChangeCategory::kBreaking &&
        (breaking_notes[0] == '\0' || migration_guide[0] == '\0')) return false;
    if (category == ChangeCategory::kSecurity && issue_refs[0] == '\0') return false;
    return true;
  }

  [[nodiscard]] constexpr bool requiresMigration() const noexcept {
    return category == ChangeCategory::kBreaking || category == ChangeCategory::kDeprecation;
  }
};

class ChangelogManager {
 public:
  static constexpr size_t kMaxEntries = 64;

  [[nodiscard]] static LoRaError addEntry(const ChangelogEntry& entry) noexcept;
  [[nodiscard]] static bool validateChangelog() noexcept;
  [[nodiscard]] static size_t getChangesSince(const SemVerVersion& version,
                                               std::array<ChangelogEntry, 32>& out) noexcept;
  [[nodiscard]] static size_t getBreakingChanges(const SemVerVersion& version,
                                                  std::array<ChangelogEntry, 16>& out) noexcept;
  [[nodiscard]] static LoRaError formatChangelog(char* buffer, size_t buffer_size) noexcept;
  [[nodiscard]] static const ChangelogEntry* getEntry(size_t index) noexcept;
  static void clear() noexcept;
  [[nodiscard]] static size_t getEntryCount() noexcept;

 private:
  static size_t entry_count_;
};

struct SemVerParser {
  [[nodiscard]] static bool parse(const char* version_str, SemVerVersion& out) noexcept;
  [[nodiscard]] static size_t formatTo(const SemVerVersion& version,
                                        char* buffer, size_t buffer_size) noexcept;
  [[nodiscard]] static int compareVersions(const SemVerVersion& v1,
                                            const SemVerVersion& v2) noexcept;
};

struct TraceabilityLink {
  static constexpr size_t kMaxIdLength = 32;

  char source_artifact[kMaxIdLength] = {};
  char target_artifact[kMaxIdLength] = {};
  char link_type[16] = {};
  uint32_t timestamp = 0;

  [[nodiscard]] constexpr bool isValid() const noexcept {
    return source_artifact[0] != '\0' && target_artifact[0] != '\0';
  }
};

class TraceabilityEngine {
 public:
  static constexpr size_t kMaxLinks = 128;

  [[nodiscard]] static LoRaError linkBuildToTest(const char* build_id, const char* test_id) noexcept;
  [[nodiscard]] static LoRaError linkTestToRelease(const char* test_id, const char* release_id) noexcept;
  [[nodiscard]] static LoRaError linkIncidentToArtifact(const char* incident_id,
                                                         const char* artifact_id) noexcept;
  [[nodiscard]] static size_t getFullTraceChain(const char* artifact_id,
                                                 std::array<TraceabilityLink, 16>& out) noexcept;
  [[nodiscard]] static bool validateTraceIntegrity(const char* version) noexcept;
  static void clear() noexcept;

 private:
  static size_t link_count_;
};

struct ChangelogSerializer {
  [[nodiscard]] static size_t serializeEntryTo(const ChangelogEntry& entry,
                                                char* buffer, size_t buffer_size) noexcept;
};

struct TraceabilitySerializer {
  [[nodiscard]] static size_t serializeLinkTo(const TraceabilityLink& link,
                                               char* buffer, size_t buffer_size) noexcept;
};

}  // namespace loradriver
