#include "traceability_engine.hpp"
#include <cstring>

namespace loradriver {

namespace {
std::array<TraceabilityLink, TraceabilityEngineImpl::kMaxLinks> links_{};
size_t link_count_ = 0;
}  // namespace

std::array<TraceabilityLink, TraceabilityEngineImpl::kMaxLinks>& TraceabilityEngineImpl::getLinks() noexcept {
  return links_;
}

size_t& TraceabilityEngineImpl::getLinkCount() noexcept {
  return link_count_;
}

size_t TraceabilityEngine::link_count_ = 0;

LoRaError TraceabilityEngine::linkBuildToTest(const char* build_id, const char* test_id) noexcept {
  if (!build_id || build_id[0] == '\0' || !test_id || test_id[0] == '\0') {
    return LoRaError::kInvalidConfig;
  }

  if (link_count_ >= kMaxLinks) {
    return LoRaError::kRegistryFull;
  }

  auto& links = TraceabilityEngineImpl::getLinks();
  TraceabilityLink& link = links[link_count_];

  std::strncpy(link.source_artifact, build_id, TraceabilityLink::kMaxIdLength - 1);
  link.source_artifact[TraceabilityLink::kMaxIdLength - 1] = '\0';

  std::strncpy(link.target_artifact, test_id, TraceabilityLink::kMaxIdLength - 1);
  link.target_artifact[TraceabilityLink::kMaxIdLength - 1] = '\0';

  std::strncpy(link.link_type, "build-to-test", 15);
  link.timestamp = 0;

  link_count_++;
  TraceabilityEngineImpl::getLinkCount() = link_count_;

  return LoRaError::kOk;
}

LoRaError TraceabilityEngine::linkTestToRelease(const char* test_id, const char* release_id) noexcept {
  if (!test_id || test_id[0] == '\0' || !release_id || release_id[0] == '\0') {
    return LoRaError::kInvalidConfig;
  }

  if (link_count_ >= kMaxLinks) {
    return LoRaError::kRegistryFull;
  }

  auto& links = TraceabilityEngineImpl::getLinks();
  TraceabilityLink& link = links[link_count_];

  std::strncpy(link.source_artifact, test_id, TraceabilityLink::kMaxIdLength - 1);
  link.source_artifact[TraceabilityLink::kMaxIdLength - 1] = '\0';

  std::strncpy(link.target_artifact, release_id, TraceabilityLink::kMaxIdLength - 1);
  link.target_artifact[TraceabilityLink::kMaxIdLength - 1] = '\0';

  std::strncpy(link.link_type, "test-to-release", 15);
  link.timestamp = 0;

  link_count_++;
  TraceabilityEngineImpl::getLinkCount() = link_count_;

  return LoRaError::kOk;
}

LoRaError TraceabilityEngine::linkIncidentToArtifact(const char* incident_id,
                                                      const char* artifact_id) noexcept {
  if (!incident_id || incident_id[0] == '\0' || !artifact_id || artifact_id[0] == '\0') {
    return LoRaError::kInvalidConfig;
  }

  if (link_count_ >= kMaxLinks) {
    return LoRaError::kRegistryFull;
  }

  auto& links = TraceabilityEngineImpl::getLinks();
  TraceabilityLink& link = links[link_count_];

  std::strncpy(link.source_artifact, incident_id, TraceabilityLink::kMaxIdLength - 1);
  link.source_artifact[TraceabilityLink::kMaxIdLength - 1] = '\0';

  std::strncpy(link.target_artifact, artifact_id, TraceabilityLink::kMaxIdLength - 1);
  link.target_artifact[TraceabilityLink::kMaxIdLength - 1] = '\0';

  std::strncpy(link.link_type, "incident-to-artifact", 15);
  link.timestamp = 0;

  link_count_++;
  TraceabilityEngineImpl::getLinkCount() = link_count_;

  return LoRaError::kOk;
}

size_t TraceabilityEngine::getFullTraceChain(const char* artifact_id,
                                              std::array<TraceabilityLink, 16>& out) noexcept {
  if (!artifact_id || artifact_id[0] == '\0') {
    return 0;
  }

  auto& links = TraceabilityEngineImpl::getLinks();
  size_t count = 0;

  char current_id[TraceabilityLink::kMaxIdLength] = {};
  std::strncpy(current_id, artifact_id, TraceabilityLink::kMaxIdLength - 1);
  current_id[TraceabilityLink::kMaxIdLength - 1] = '\0';

  while (count < 16) {
    bool found = false;
    for (size_t i = 0; i < link_count_; ++i) {
      if (std::strcmp(links[i].source_artifact, current_id) == 0) {
        out[count++] = links[i];
        std::strncpy(current_id, links[i].target_artifact, TraceabilityLink::kMaxIdLength - 1);
        current_id[TraceabilityLink::kMaxIdLength - 1] = '\0';
        found = true;
        break;
      }
    }
    if (!found) break;
  }

  return count;
}

bool TraceabilityEngine::validateTraceIntegrity(const char* version) noexcept {
  (void)version;

  auto& links = TraceabilityEngineImpl::getLinks();

  bool has_chain = false;
  for (size_t i = 0; i < link_count_; ++i) {
    if (!links[i].isValid()) {
      return false;
    }

    const bool is_build_to_test = std::strcmp(links[i].link_type, "build-to-test") == 0;
    const bool is_test_to_release = std::strcmp(links[i].link_type, "test-to-release") == 0;
    const bool is_incident_to_artifact = std::strcmp(links[i].link_type, "incident-to-artifact") == 0;

    if (!is_build_to_test && !is_test_to_release && !is_incident_to_artifact) {
      return false;
    }

    if (is_build_to_test) {
      bool found_release_leg = false;
      for (size_t j = 0; j < link_count_; ++j) {
        if (std::strcmp(links[j].link_type, "test-to-release") == 0 &&
            std::strcmp(links[j].source_artifact, links[i].target_artifact) == 0) {
          found_release_leg = true;
          break;
        }
      }
      if (!found_release_leg) {
        return false;
      }
      has_chain = true;
    }
  }

  return has_chain || link_count_ == 0;
}

void TraceabilityEngine::clear() noexcept {
  link_count_ = 0;
  TraceabilityEngineImpl::getLinkCount() = 0;
}

size_t TraceabilitySerializer::serializeLinkTo(const TraceabilityLink& link,
                                                char* buffer, size_t buffer_size) noexcept {
  if (!buffer || buffer_size == 0) return 0;

  return static_cast<size_t>(std::snprintf(buffer, buffer_size,
    "TraceabilityLink{src=%s, tgt=%s, type=%s, ts=%u}",
    link.source_artifact,
    link.target_artifact,
    link.link_type,
    link.timestamp));
}

}  // namespace loradriver
