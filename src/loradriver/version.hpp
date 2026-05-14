#pragma once

#include <cstdint>

namespace loradriver {

/// @brief Compile-time major version. Matches CMake project() VERSION.
constexpr std::uint8_t kVersionMajor = 1;
/// @brief Compile-time minor version.
constexpr std::uint8_t kVersionMinor = 2;
/// @brief Compile-time patch version.
constexpr std::uint8_t kVersionPatch = 1;

/// @brief Runtime major version (matches kVersionMajor at build time).
[[nodiscard]] std::uint8_t version_major() noexcept;
/// @brief Runtime minor version.
[[nodiscard]] std::uint8_t version_minor() noexcept;
/// @brief Runtime patch version.
[[nodiscard]] std::uint8_t version_patch() noexcept;
/// @brief Runtime version string "MAJOR.MINOR.PATCH".
[[nodiscard]] const char* version_string() noexcept;

} // namespace loradriver
