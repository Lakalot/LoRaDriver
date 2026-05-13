#include "loradriver/hal/spi_device.hpp"

namespace loradriver::hal {

namespace {
constexpr std::uint8_t kWriteBit = 0x80;
}

LoRaError ISpiDevice::write_register(std::uint8_t reg, std::uint8_t value) noexcept {
    return transfer(static_cast<std::uint8_t>(reg | kWriteBit), &value, nullptr, 1);
}

LoRaError ISpiDevice::read_register(std::uint8_t reg, std::uint8_t& out) noexcept {
    return transfer(static_cast<std::uint8_t>(reg & 0x7F), nullptr, &out, 1);
}

LoRaError ISpiDevice::burst_write(std::uint8_t reg, const std::uint8_t* buf,
                                  std::size_t len) noexcept {
    if (buf == nullptr || len == 0u)
        return LoRaError::NullArgument;
    return transfer(static_cast<std::uint8_t>(reg | kWriteBit), buf, nullptr, len);
}

LoRaError ISpiDevice::burst_read(std::uint8_t reg, std::uint8_t* buf, std::size_t len) noexcept {
    if (buf == nullptr || len == 0u)
        return LoRaError::NullArgument;
    return transfer(static_cast<std::uint8_t>(reg & 0x7F), nullptr, buf, len);
}

} // namespace loradriver::hal
