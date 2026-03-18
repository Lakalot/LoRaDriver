#pragma once

#include <loradriver/versioning.hpp>
#include <cstring>

namespace loradriver {

class ChangelogManagerImpl {
 public:
  static constexpr size_t kMaxEntries = ChangelogManager::kMaxEntries;

  static std::array<ChangelogEntry, kMaxEntries>& getEntries() noexcept;
  static size_t& getEntryCount() noexcept;
};

}  // namespace loradriver
