#include "artifact_registry.hpp"
#include <cstdio>
#include <cstring>

namespace loradriver {

namespace {
std::array<ArtifactMetadata, ArtifactRegistryImpl::kMaxArtifacts> artifacts_{};
std::array<ArtifactLink, ArtifactRegistryImpl::kMaxLinks> links_{};
std::array<RetentionPolicy, ArtifactRegistry::kRetentionPolicyCount> policies_{};
size_t artifact_count_ = 0;
size_t link_count_ = 0;
uint32_t next_artifact_counter_ = 1;
bool policies_initialized_ = false;
}  // namespace

std::array<ArtifactMetadata, ArtifactRegistryImpl::kMaxArtifacts>& ArtifactRegistryImpl::getArtifacts() noexcept {
  return artifacts_;
}

std::array<ArtifactLink, ArtifactRegistryImpl::kMaxLinks>& ArtifactRegistryImpl::getLinks() noexcept {
  return links_;
}

size_t& ArtifactRegistryImpl::getArtifactCount() noexcept {
  return artifact_count_;
}

size_t& ArtifactRegistryImpl::getLinkCount() noexcept {
  return link_count_;
}

uint32_t& ArtifactRegistryImpl::getNextArtifactCounter() noexcept {
  return next_artifact_counter_;
}

std::array<RetentionPolicy, ArtifactRegistry::kRetentionPolicyCount>& ArtifactRegistryImpl::getPolicies() noexcept {
  return policies_;
}

void ArtifactRegistryImpl::initializeDefaultPolicies() noexcept {
  if (policies_initialized_) return;

  policies_[static_cast<size_t>(ArtifactType::kValidationReport)] = {
    ArtifactType::kValidationReport, 90, 180, true, true, 60
  };
  policies_[static_cast<size_t>(ArtifactType::kIncidentEvidence)] = {
    ArtifactType::kIncidentEvidence, 90, 180, true, true, 60
  };
  policies_[static_cast<size_t>(ArtifactType::kRecoveryProof)] = {
    ArtifactType::kRecoveryProof, 90, 180, true, true, 60
  };
  policies_[static_cast<size_t>(ArtifactType::kTestMatrix)] = {
    ArtifactType::kTestMatrix, 90, 180, true, true, 30
  };
  policies_[static_cast<size_t>(ArtifactType::kGateReport)] = {
    ArtifactType::kGateReport, 90, 180, true, true, 30
  };
  policies_[static_cast<size_t>(ArtifactType::kTelemetryBaseline)] = {
    ArtifactType::kTelemetryBaseline, 180, 365, false, true, 90
  };
  policies_[static_cast<size_t>(ArtifactType::kBuildLog)] = {
    ArtifactType::kBuildLog, 30, 90, true, true, 14
  };
  policies_[static_cast<size_t>(ArtifactType::kReleaseManifest)] = {
    ArtifactType::kReleaseManifest, 365, 730, false, false, 0
  };

  policies_initialized_ = true;
}

void ArtifactRegistryImpl::generateArtifactId(char* buffer, size_t buffer_size) noexcept {
  std::snprintf(buffer, buffer_size, "ARTIFACT-%08X", next_artifact_counter_++);
}

size_t ArtifactRegistryImpl::findArtifactIndex(const char* id) noexcept {
  if (!id || id[0] == '\0') return kMaxArtifacts;
  for (size_t i = 0; i < artifact_count_; ++i) {
    if (std::strcmp(artifacts_[i].id, id) == 0) {
      return i;
    }
  }
  return kMaxArtifacts;
}

size_t ArtifactRegistry::artifact_count_ = 0;
size_t ArtifactRegistry::link_count_ = 0;
uint32_t ArtifactRegistry::next_artifact_counter_ = 1;

const char* ArtifactRegistry::registerArtifact(const ArtifactMetadata& metadata) noexcept {
  ArtifactRegistryImpl::initializeDefaultPolicies();

  const RetentionPolicy* policy = getRetentionPolicy(metadata.type);
  if (!policy) {
    return nullptr;
  }
  if (!policy->isValidRetention(metadata.retention_days)) {
    return nullptr;
  }

  if (artifact_count_ >= kMaxArtifacts) {
    return nullptr;
  }

  auto& artifacts = ArtifactRegistryImpl::getArtifacts();
  ArtifactMetadata& artifact = artifacts[artifact_count_];

  ArtifactRegistryImpl::generateArtifactId(artifact.id, ArtifactMetadata::kMaxIdLength);
  artifact.type = metadata.type;
  artifact.created_timestamp = metadata.created_timestamp;
  artifact.retention_days = metadata.retention_days;

  if (metadata.expires_timestamp != 0) {
    if (metadata.expires_timestamp < metadata.created_timestamp) {
      return nullptr;
    }
    artifact.expires_timestamp = metadata.expires_timestamp;
  } else {
    artifact.expires_timestamp =
        metadata.created_timestamp + static_cast<uint32_t>(metadata.retention_days) * 86400U;
  }

  std::strncpy(artifact.linked_version, metadata.linked_version, ArtifactMetadata::kMaxVersionLength - 1);
  artifact.linked_version[ArtifactMetadata::kMaxVersionLength - 1] = '\0';

  artifact.linked_profile_count = metadata.linked_profile_count > ArtifactMetadata::kMaxLinkedProfiles
                                      ? ArtifactMetadata::kMaxLinkedProfiles
                                      : metadata.linked_profile_count;
  for (uint8_t i = 0; i < artifact.linked_profile_count; ++i) {
    artifact.linked_profiles[i] = metadata.linked_profiles[i];
  }
  for (uint8_t i = artifact.linked_profile_count; i < ArtifactMetadata::kMaxLinkedProfiles; ++i) {
    artifact.linked_profiles[i] = HardwareProfile{};
  }

  std::strncpy(artifact.source_module, metadata.source_module, ArtifactMetadata::kMaxModuleLength - 1);
  artifact.source_module[ArtifactMetadata::kMaxModuleLength - 1] = '\0';

  artifact_count_++;
  ArtifactRegistryImpl::getArtifactCount() = artifact_count_;

  return artifact.id;
}

size_t ArtifactRegistry::getArtifactsByType(ArtifactType type,
                                             std::array<ArtifactMetadata, kMaxArtifacts>& out) noexcept {
  auto& artifacts = ArtifactRegistryImpl::getArtifacts();
  size_t count = 0;

  for (size_t i = 0; i < artifact_count_ && count < kMaxArtifacts; ++i) {
    if (artifacts[i].type == type) {
      out[count++] = artifacts[i];
    }
  }

  return count;
}

size_t ArtifactRegistry::getArtifactsByVersion(const char* version,
                                                std::array<ArtifactMetadata, kMaxArtifacts>& out) noexcept {
  if (!version || version[0] == '\0') return 0;

  auto& artifacts = ArtifactRegistryImpl::getArtifacts();
  size_t count = 0;

  for (size_t i = 0; i < artifact_count_ && count < kMaxArtifacts; ++i) {
    if (std::strcmp(artifacts[i].linked_version, version) == 0) {
      out[count++] = artifacts[i];
    }
  }

  return count;
}

size_t ArtifactRegistry::getExpiredArtifacts(std::array<ArtifactMetadata, 32>& out,
                                              uint32_t current_timestamp) noexcept {
  auto& artifacts = ArtifactRegistryImpl::getArtifacts();
  size_t count = 0;

  for (size_t i = 0; i < artifact_count_ && count < 32; ++i) {
    if (artifacts[i].isExpired(current_timestamp)) {
      out[count++] = artifacts[i];
    }
  }

  return count;
}

LoRaError ArtifactRegistry::linkArtifacts(const char* source_id, const char* target_id) noexcept {
  if (!source_id || source_id[0] == '\0' || !target_id || target_id[0] == '\0') {
    return LoRaError::kInvalidConfig;
  }

  if (link_count_ >= kMaxLinks) {
    return LoRaError::kRegistryFull;
  }

  size_t source_idx = ArtifactRegistryImpl::findArtifactIndex(source_id);
  size_t target_idx = ArtifactRegistryImpl::findArtifactIndex(target_id);

  if (source_idx >= artifact_count_ || target_idx >= artifact_count_) {
    return LoRaError::kArtifactNotFound;
  }

  auto& links = ArtifactRegistryImpl::getLinks();
  ArtifactLink& link = links[link_count_];

  std::strncpy(link.source_id, source_id, ArtifactLink::kMaxIdLength - 1);
  link.source_id[ArtifactLink::kMaxIdLength - 1] = '\0';

  std::strncpy(link.target_id, target_id, ArtifactLink::kMaxIdLength - 1);
  link.target_id[ArtifactLink::kMaxIdLength - 1] = '\0';

  link.created_timestamp = 0;

  link_count_++;
  ArtifactRegistryImpl::getLinkCount() = link_count_;

  return LoRaError::kOk;
}

bool ArtifactRegistry::purgeExpired(uint32_t current_timestamp) noexcept {
  auto& artifacts = ArtifactRegistryImpl::getArtifacts();
  size_t write_idx = 0;
  bool purged = false;

  for (size_t read_idx = 0; read_idx < artifact_count_; ++read_idx) {
    if (!artifacts[read_idx].isExpired(current_timestamp)) {
      if (write_idx != read_idx) {
        artifacts[write_idx] = artifacts[read_idx];
      }
      ++write_idx;
    } else {
      purged = true;
    }
  }

  artifact_count_ = write_idx;
  ArtifactRegistryImpl::getArtifactCount() = artifact_count_;

  return purged;
}

const ArtifactMetadata* ArtifactRegistry::getArtifact(const char* id) noexcept {
  size_t idx = ArtifactRegistryImpl::findArtifactIndex(id);
  if (idx >= artifact_count_) {
    return nullptr;
  }
  return &ArtifactRegistryImpl::getArtifacts()[idx];
}

void ArtifactRegistry::clear() noexcept {
  artifact_count_ = 0;
  link_count_ = 0;
  ArtifactRegistryImpl::getArtifactCount() = 0;
  ArtifactRegistryImpl::getLinkCount() = 0;
}

const RetentionPolicy* ArtifactRegistry::getRetentionPolicy(ArtifactType type) noexcept {
  ArtifactRegistryImpl::initializeDefaultPolicies();

  size_t idx = static_cast<size_t>(type);
  if (idx >= ArtifactRegistry::kRetentionPolicyCount) {
    return nullptr;
  }
  return &ArtifactRegistryImpl::getPolicies()[idx];
}

void ArtifactRegistry::setRetentionPolicy(ArtifactType type, const RetentionPolicy& policy) noexcept {
  ArtifactRegistryImpl::initializeDefaultPolicies();

  size_t idx = static_cast<size_t>(type);
  if (idx < ArtifactRegistry::kRetentionPolicyCount) {
    ArtifactRegistryImpl::getPolicies()[idx] = policy;
  }
}

size_t ArtifactRegistrySerializer::serializeArtifactTo(const ArtifactMetadata& metadata,
                                                        char* buffer, size_t buffer_size) noexcept {
  if (!buffer || buffer_size == 0) return 0;

  return std::snprintf(buffer, buffer_size,
    "Artifact{id=%s, type=%u, created=%u, expires=%u, retention=%u, version=%s, module=%s}",
    metadata.id,
    static_cast<unsigned>(metadata.type),
    metadata.created_timestamp,
    metadata.expires_timestamp,
    metadata.retention_days,
    metadata.linked_version,
    metadata.source_module);
}

size_t ArtifactRegistrySerializer::serializePolicyTo(const RetentionPolicy& policy,
                                                      char* buffer, size_t buffer_size) noexcept {
  if (!buffer || buffer_size == 0) return 0;

  return std::snprintf(buffer, buffer_size,
    "RetentionPolicy{type=%u, min=%u, max=%u, auto_delete=%d, compress=%d, threshold=%u}",
    static_cast<unsigned>(policy.artifact_type),
    policy.min_retention_days,
    policy.max_retention_days,
    policy.auto_delete_expired ? 1 : 0,
    policy.compress_after_days ? 1 : 0,
    policy.compress_threshold);
}

}  // namespace loradriver
