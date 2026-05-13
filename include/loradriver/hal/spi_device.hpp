#pragma once

#include <cstddef>
#include <cstdint>

#include "loradriver/lora_error.hpp"

namespace loradriver::hal {

/// SPI bus boundary for SX127x register IO.
/// One transfer() call = one full CS-asserted SPI transaction:
///   * 1 address byte (addr | 0x80 for write, addr & 0x7F for read)
///   * len data bytes simultaneously shifted in tx[]/out rx[]
/// Either tx or rx may be nullptr (write-only or read-only).
class ISpiDevice {
public:
    virtual ~ISpiDevice() = default;

    [[nodiscard]] virtual LoRaError begin() noexcept = 0;
    [[nodiscard]] virtual LoRaError transfer(std::uint8_t addr,
                                             const std::uint8_t* tx,
                                             std::uint8_t* rx,
                                             std::size_t len) noexcept = 0;

    [[nodiscard]] LoRaError write_register(std::uint8_t reg, std::uint8_t value) noexcept;
    [[nodiscard]] LoRaError read_register(std::uint8_t reg, std::uint8_t& out) noexcept;
    [[nodiscard]] LoRaError burst_write(std::uint8_t reg,
                                        const std::uint8_t* buf,
                                        std::size_t len) noexcept;
    [[nodiscard]] LoRaError burst_read(std::uint8_t reg,
                                       std::uint8_t* buf,
                                       std::size_t len) noexcept;

protected:
    ISpiDevice() = default;
    ISpiDevice(const ISpiDevice&) = delete;
    ISpiDevice& operator=(const ISpiDevice&) = delete;
};

}  // namespace loradriver::hal
