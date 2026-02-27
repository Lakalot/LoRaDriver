#pragma once

// Internal header for profile qualification matrix data.
// Not part of the public API - use include/loradriver/profile_qualification.hpp instead.
//
// This header exists to allow other validation module files (e.g., ci_gates.cpp,
// non_regression.cpp) to access internal matrix data if needed, without exposing
// implementation details through the public header.

#include "loradriver/profile_qualification.hpp"

namespace loradriver {
namespace internal {

/// Access the raw qualification matrix array (for internal validation module use only).
/// Returns a reference to the static compile-time matrix.
inline const auto& getQualificationMatrix() noexcept {
  // Delegate to public API - the matrix is accessible via
  // ProfileQualificationMatrix static methods. This internal header
  // provides a namespace anchor for future internal-only extensions.
  return ProfileQualificationMatrix::kMatrixSize;
}

}  // namespace internal
}  // namespace loradriver
