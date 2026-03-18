#pragma once

#include <loradriver/versioning.hpp>
#include <cstring>

namespace loradriver {

class TraceabilityEngineImpl {
 public:
  static constexpr size_t kMaxLinks = TraceabilityEngine::kMaxLinks;

  static std::array<TraceabilityLink, kMaxLinks>& getLinks() noexcept;
  static size_t& getLinkCount() noexcept;
};

}  // namespace loradriver
