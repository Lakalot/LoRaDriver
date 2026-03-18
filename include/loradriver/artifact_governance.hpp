#pragma once

#include <array>
#include <cstdint>

#include <loradriver/profile_qualification.hpp>
#include <loradriver/lora_error.hpp>

namespace loradriver {

enum class ArtifactType : uint8_t {
  kValidationReport = 0,
  kIncidentEvidence = 1,
  kRecoveryProof = 2,
  kTestMatrix = 3,
  kGateReport = 4,
  kTelemetryBaseline = 5,
  kBuildLog = 6,
  kReleaseManifest = 7
};

struct ArtifactMetadata {
  static constexpr size_t kMaxIdLength = 32;
  static constexpr size_t kMaxVersionLength = 16;
  static constexpr size_t kMaxModuleLength = 32;
  static constexpr size_t kMaxLinkedProfiles = 4;

  char id[kMaxIdLength];
  ArtifactType type;
  uint32_t created_timestamp;
  uint32_t expires_timestamp;
  uint16_t retention_days;
  char linked_version[kMaxVersionLength];
  uint8_t linked_profile_count;
  std::array<HardwareProfile, kMaxLinkedProfiles> linked_profiles;
  char source_module[kMaxModuleLength];

  [[nodiscard]] constexpr bool isExpired(uint32_t current_timestamp) const noexcept {
    return current_timestamp >= expires_timestamp;
  }

  [[nodiscard]] constexpr bool isValid() const noexcept {
    return id[0] != '\0' && retention_days > 0;
  }
};

struct RetentionPolicy {
  static constexpr uint16_t kV1MinRetentionDays = 90;
  static constexpr uint16_t kV1MaxRetentionDays = 180;

  ArtifactType artifact_type;
  uint16_t min_retention_days;
  uint16_t max_retention_days;
  bool auto_delete_expired;
  bool compress_after_days;
  uint16_t compress_threshold;

  [[nodiscard]] constexpr bool isValidRetention(uint16_t days) const noexcept {
    return days >= min_retention_days && days <= max_retention_days;
  }

  [[nodiscard]] constexpr bool shouldCompress(uint16_t age_days) const noexcept {
    return compress_after_days && age_days >= compress_threshold;
  }
};

struct ArtifactLink {
  static constexpr size_t kMaxIdLength = ArtifactMetadata::kMaxIdLength;

  char source_id[kMaxIdLength];
  char target_id[kMaxIdLength];
  uint32_t created_timestamp;

  [[nodiscard]] constexpr bool isValid() const noexcept {
    return source_id[0] != '\0' && target_id[0] != '\0';
  }
};

class ArtifactRegistry {
 public:
  static constexpr size_t kMaxArtifacts = 64;
  static constexpr size_t kMaxLinks = 128;

  [[nodiscard]] static const char* registerArtifact(const ArtifactMetadata& metadata) noexcept;
  [[nodiscard]] static size_t getArtifactsByType(ArtifactType type,
                                                  std::array<ArtifactMetadata, kMaxArtifacts>& out) noexcept;
  [[nodiscard]] static size_t getArtifactsByVersion(const char* version,
                                                     std::array<ArtifactMetadata, kMaxArtifacts>& out) noexcept;
  [[nodiscard]] static size_t getExpiredArtifacts(std::array<ArtifactMetadata, 32>& out,
                                                   uint32_t current_timestamp) noexcept;
  [[nodiscard]] static LoRaError linkArtifacts(const char* source_id, const char* target_id) noexcept;
  [[nodiscard]] static bool purgeExpired(uint32_t current_timestamp) noexcept;
  [[nodiscard]] static const ArtifactMetadata* getArtifact(const char* id) noexcept;
  static void clear() noexcept;

  [[nodiscard]] static const RetentionPolicy* getRetentionPolicy(ArtifactType type) noexcept;
  static void setRetentionPolicy(ArtifactType type, const RetentionPolicy& policy) noexcept;

  static constexpr size_t kRetentionPolicyCount = 8;

 private:
  static size_t artifact_count_;
  static size_t link_count_;
  static uint32_t next_artifact_counter_;
};

struct ArtifactRegistrySerializer {
  [[nodiscard]] static size_t serializeArtifactTo(const ArtifactMetadata& metadata,
                                                   char* buffer, size_t buffer_size) noexcept;
  [[nodiscard]] static size_t serializePolicyTo(const RetentionPolicy& policy,
                                                 char* buffer, size_t buffer_size) noexcept;
};

}  // namespace loradriver
