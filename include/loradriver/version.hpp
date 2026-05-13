#pragma once

#include <cstdint>

namespace loradriver {

constexpr std::uint8_t kVersionMajor = 1;
constexpr std::uint8_t kVersionMinor = 1;
constexpr std::uint8_t kVersionPatch = 0;

[[nodiscard]] std::uint8_t version_major() noexcept;
[[nodiscard]] std::uint8_t version_minor() noexcept;
[[nodiscard]] std::uint8_t version_patch() noexcept;
[[nodiscard]] const char* version_string() noexcept; // "1.1.0"

} // namespace loradriver
