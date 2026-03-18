#pragma once

#include <loradriver/artifact_governance.hpp>
#include <cstring>

namespace loradriver {

class ArtifactRegistryImpl {
 public:
  static constexpr size_t kMaxArtifacts = ArtifactRegistry::kMaxArtifacts;
  static constexpr size_t kMaxLinks = ArtifactRegistry::kMaxLinks;

  static std::array<ArtifactMetadata, kMaxArtifacts>& getArtifacts() noexcept;
  static std::array<ArtifactLink, kMaxLinks>& getLinks() noexcept;
  static size_t& getArtifactCount() noexcept;
  static size_t& getLinkCount() noexcept;
  static uint32_t& getNextArtifactCounter() noexcept;
  static std::array<RetentionPolicy, ArtifactRegistry::kRetentionPolicyCount>& getPolicies() noexcept;

  static void initializeDefaultPolicies() noexcept;
  static void generateArtifactId(char* buffer, size_t buffer_size) noexcept;
  static size_t findArtifactIndex(const char* id) noexcept;
};

}  // namespace loradriver
