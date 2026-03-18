#include "changelog_manager.hpp"
#include <cstdio>
#include <cstring>

namespace loradriver {

namespace {
std::array<ChangelogEntry, ChangelogManagerImpl::kMaxEntries> entries_{};
size_t entry_count_ = 0;
}  // namespace

std::array<ChangelogEntry, ChangelogManagerImpl::kMaxEntries>& ChangelogManagerImpl::getEntries() noexcept {
  return entries_;
}

size_t& ChangelogManagerImpl::getEntryCount() noexcept {
  return entry_count_;
}

size_t ChangelogManager::entry_count_ = 0;

LoRaError ChangelogManager::addEntry(const ChangelogEntry& entry) noexcept {
  if (entry_count_ >= kMaxEntries) {
    return LoRaError::kRegistryFull;
  }

  if (!entry.version.isValid()) {
    return LoRaError::kInvalidVersion;
  }

  if (!entry.isValid()) {
    return LoRaError::kChangelogValidationFailed;
  }

  auto& entries = ChangelogManagerImpl::getEntries();

  for (size_t i = 0; i < entry_count_; ++i) {
    if (entries[i].version == entry.version) {
      return LoRaError::kInvalidConfig;
    }
  }

  entries[entry_count_] = entry;
  entry_count_++;
  ChangelogManagerImpl::getEntryCount() = entry_count_;

  return LoRaError::kOk;
}

bool ChangelogManager::validateChangelog() noexcept {
  auto& entries = ChangelogManagerImpl::getEntries();

  for (size_t i = 0; i < entry_count_; ++i) {
    if (!entries[i].isValid()) {
      return false;
    }
  }

  for (size_t i = 1; i < entry_count_; ++i) {
    for (size_t j = 0; j < i; ++j) {
      if (entries[i].version == entries[j].version) {
        return false;
      }
    }
  }

  return true;
}

size_t ChangelogManager::getChangesSince(const SemVerVersion& version,
                                          std::array<ChangelogEntry, 32>& out) noexcept {
  auto& entries = ChangelogManagerImpl::getEntries();
  size_t count = 0;

  for (size_t i = 0; i < entry_count_ && count < 32; ++i) {
    if (entries[i].version > version) {
      out[count++] = entries[i];
    }
  }

  return count;
}

size_t ChangelogManager::getBreakingChanges(const SemVerVersion& version,
                                             std::array<ChangelogEntry, 16>& out) noexcept {
  auto& entries = ChangelogManagerImpl::getEntries();
  size_t count = 0;

  for (size_t i = 0; i < entry_count_ && count < 16; ++i) {
    if (entries[i].version > version &&
        (entries[i].category == ChangeCategory::kBreaking ||
         entries[i].category == ChangeCategory::kDeprecation)) {
      out[count++] = entries[i];
    }
  }

  return count;
}

LoRaError ChangelogManager::formatChangelog(char* buffer, size_t buffer_size) noexcept {
  if (!buffer || buffer_size == 0) {
    return LoRaError::kInvalidConfig;
  }

  auto& entries = ChangelogManagerImpl::getEntries();
  size_t offset = 0;

  offset += static_cast<size_t>(std::snprintf(buffer + offset, buffer_size - offset, "# Changelog\n\n"));

  for (size_t i = 0; i < entry_count_ && offset < buffer_size - 1; ++i) {
    const ChangelogEntry& entry = entries[i];
    offset += static_cast<size_t>(std::snprintf(
      buffer + offset, buffer_size - offset,
      "## v%u.%u.%u (%u)\n- %s\n\n",
      entry.version.major, entry.version.minor, entry.version.patch,
      entry.date,
      entry.description));
  }

  return LoRaError::kOk;
}

const ChangelogEntry* ChangelogManager::getEntry(size_t index) noexcept {
  if (index >= entry_count_) {
    return nullptr;
  }
  return &ChangelogManagerImpl::getEntries()[index];
}

void ChangelogManager::clear() noexcept {
  entry_count_ = 0;
  ChangelogManagerImpl::getEntryCount() = 0;
}

size_t ChangelogManager::getEntryCount() noexcept {
  return entry_count_;
}

bool SemVerParser::parse(const char* version_str, SemVerVersion& out) noexcept {
  if (!version_str || version_str[0] == '\0') {
    return false;
  }

  out = SemVerVersion{};

  const char* ptr = version_str;

  auto parse_uint = [](const char*& p, unsigned int& value) noexcept -> bool {
    if (*p < '0' || *p > '9') {
      return false;
    }
    unsigned int v = 0;
    while (*p >= '0' && *p <= '9') {
      v = v * 10U + static_cast<unsigned int>(*p - '0');
      if (v > 255U) {
        return false;
      }
      ++p;
    }
    value = v;
    return true;
  };

  unsigned int major = 0;
  unsigned int minor = 0;
  unsigned int patch = 0;

  if (!parse_uint(ptr, major) || *ptr != '.') {
    return false;
  }
  ++ptr;
  if (!parse_uint(ptr, minor) || *ptr != '.') {
    return false;
  }
  ++ptr;
  if (!parse_uint(ptr, patch)) {
    return false;
  }

  out.major = static_cast<uint8_t>(major);
  out.minor = static_cast<uint8_t>(minor);
  out.patch = static_cast<uint8_t>(patch);

  if (*ptr == '-') {
    ++ptr;
    if (*ptr == '\0') return false;
    size_t len = 0;
    while (*ptr && *ptr != '+') {
      if (len >= 15) return false;
      out.prerelease[len++] = *ptr++;
    }
    out.prerelease[len] = '\0';
  }

  if (*ptr == '+') {
    ++ptr;
    if (*ptr == '\0') return false;
    size_t len = 0;
    while (*ptr) {
      if (len >= 15) return false;
      out.build_metadata[len++] = *ptr++;
    }
    out.build_metadata[len] = '\0';
  }

  return *ptr == '\0';
}

size_t SemVerParser::formatTo(const SemVerVersion& version,
                               char* buffer, size_t buffer_size) noexcept {
  if (!buffer || buffer_size == 0) return 0;

  if (version.prerelease[0] != '\0' && version.build_metadata[0] != '\0') {
    return static_cast<size_t>(std::snprintf(buffer, buffer_size,
      "%u.%u.%u-%s+%s",
      version.major, version.minor, version.patch,
      version.prerelease, version.build_metadata));
  } else if (version.prerelease[0] != '\0') {
    return static_cast<size_t>(std::snprintf(buffer, buffer_size,
      "%u.%u.%u-%s",
      version.major, version.minor, version.patch,
      version.prerelease));
  } else if (version.build_metadata[0] != '\0') {
    return static_cast<size_t>(std::snprintf(buffer, buffer_size,
      "%u.%u.%u+%s",
      version.major, version.minor, version.patch,
      version.build_metadata));
  }

  return static_cast<size_t>(std::snprintf(buffer, buffer_size,
    "%u.%u.%u",
    version.major, version.minor, version.patch));
}

int SemVerParser::compareVersions(const SemVerVersion& v1,
                                   const SemVerVersion& v2) noexcept {
  return v1.compare(v2);
}

size_t ChangelogSerializer::serializeEntryTo(const ChangelogEntry& entry,
                                              char* buffer, size_t buffer_size) noexcept {
  if (!buffer || buffer_size == 0) return 0;

  char version_str[32] = {};
  const size_t version_len = SemVerParser::formatTo(entry.version, version_str, sizeof(version_str));
  (void)version_len;

  return static_cast<size_t>(std::snprintf(buffer, buffer_size,
    "ChangelogEntry{version=%s, date=%u, category=%u, desc=%s}",
    version_str,
    entry.date,
    static_cast<unsigned>(entry.category),
    entry.description));
}

}  // namespace loradriver
