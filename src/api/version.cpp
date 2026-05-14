#include "loradriver/version.hpp"

namespace loradriver {

std::uint8_t version_major() noexcept {
    return kVersionMajor;
}
std::uint8_t version_minor() noexcept {
    return kVersionMinor;
}
std::uint8_t version_patch() noexcept {
    return kVersionPatch;
}
const char* version_string() noexcept {
    return "1.3.0";
}

} // namespace loradriver
