#pragma once

#ifdef ARDUINO_ARCH_ESP32

#include <Arduino.h>
#include <SPI.h>

#include "loradriver/hal/spi_device.hpp"

namespace loradriver::hal {

/// ESP32-specific SPI device using SPIClass::transferBytes() for DMA-capable
/// burst transfers. ~5-10x faster than byte-by-byte transfer on 255-byte FIFO.
class Esp32SpiDevice : public ISpiDevice {
public:
    /// Default constructor — leaves the device unbound. Call bind() before begin().
    /// Used by the loradriver::LoRa facade where SPI.begin() runs at user
    /// setup() time, after the facade has been zero-initialised in .bss.
    Esp32SpiDevice() noexcept = default;

    /// Parameterised constructor — equivalent to default-construct + bind().
    /// Direct-DI users keep using this verbatim.
    Esp32SpiDevice(SPIClass& bus, std::int8_t cs_pin,
                   std::uint32_t clock_hz = 8'000'000u) noexcept {
        bind(bus, cs_pin, clock_hz);
    }

    /// Deferred bind. Safe to call any number of times before begin().
    /// Calling after begin() is undefined behaviour — call end()/begin() to rebind.
    void bind(SPIClass& bus, std::int8_t cs_pin, std::uint32_t clock_hz = 8'000'000u) noexcept {
        bus_ = &bus;
        cs_pin_ = cs_pin;
        clock_hz_ = clock_hz;
    }

    [[nodiscard]] LoRaError begin() noexcept override {
        if (bus_ == nullptr)
            return LoRaError::NotInitialized;
        pinMode(cs_pin_, OUTPUT);
        digitalWrite(cs_pin_, HIGH);
        return LoRaError::OK;
    }

    [[nodiscard]] LoRaError transfer(std::uint8_t addr, const std::uint8_t* tx, std::uint8_t* rx,
                                     std::size_t len) noexcept override {
        if (bus_ == nullptr)
            return LoRaError::NotInitialized;
        bus_->beginTransaction(SPISettings(clock_hz_, MSBFIRST, SPI_MODE0));
        digitalWrite(cs_pin_, LOW);
        bus_->transfer(addr);
        if (len > 0u) {
            // transferBytes does not mutate tx; const_cast required by Arduino signature.
            if (tx == nullptr && rx == nullptr) {
                // Nothing to do.
            } else if (tx == nullptr) {
                bus_->transferBytes(nullptr, rx, len);
            } else if (rx == nullptr) {
                bus_->transferBytes(const_cast<std::uint8_t*>(tx), nullptr, len);
            } else {
                bus_->transferBytes(const_cast<std::uint8_t*>(tx), rx, len);
            }
        }
        digitalWrite(cs_pin_, HIGH);
        bus_->endTransaction();
        return LoRaError::OK;
    }

private:
    SPIClass* bus_ = nullptr;
    std::int8_t cs_pin_ = -1;
    std::uint32_t clock_hz_ = 8'000'000u;
};

} // namespace loradriver::hal

#endif // ARDUINO_ARCH_ESP32
