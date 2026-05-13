#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

#include "loradriver/lora_config.hpp"
#include "loradriver/lora_error.hpp"
#include "loradriver/radio_event.hpp"
#include "loradriver/radio_stats.hpp"

namespace loradriver {

class IRadioDriver {
public:
    using EventCallback = std::function<void(RadioEvent, int)>;

    virtual ~IRadioDriver() = default;

    [[nodiscard]] virtual LoRaError begin(const LoRaConfig& cfg) noexcept = 0;
    virtual void end() noexcept = 0;
    [[nodiscard]] virtual std::uint8_t chip_version() const noexcept = 0;

    [[nodiscard]] virtual LoRaError set_sleep() noexcept = 0;
    [[nodiscard]] virtual LoRaError set_standby() noexcept = 0;

    [[nodiscard]] virtual LoRaError start_transmit(const std::uint8_t* data,
                                                   std::size_t len,
                                                   std::uint32_t timeout_ms = 2000) noexcept = 0;
    [[nodiscard]] virtual bool is_transmitting() const noexcept = 0;

    [[nodiscard]] virtual LoRaError start_receive(bool continuous = true) noexcept = 0;
    [[nodiscard]] virtual int read_packet(std::uint8_t* buf, std::size_t max_len) noexcept = 0;

    [[nodiscard]] virtual LoRaError start_cad() noexcept = 0;

    [[nodiscard]] virtual LoRaError set_frequency(std::uint32_t hz) noexcept = 0;
    [[nodiscard]] virtual LoRaError set_tx_power(std::int8_t dbm, PaOutput out) noexcept = 0;
    [[nodiscard]] virtual LoRaError set_spreading_factor(std::uint8_t sf) noexcept = 0;
    [[nodiscard]] virtual LoRaError set_bandwidth(std::uint32_t hz) noexcept = 0;

    [[nodiscard]] virtual std::int16_t packet_rssi() const noexcept = 0;
    [[nodiscard]] virtual float packet_snr() const noexcept = 0;
    [[nodiscard]] virtual std::int32_t frequency_error_hz() const noexcept = 0;
    [[nodiscard]] virtual std::int16_t current_rssi() const noexcept = 0;
    [[nodiscard]] virtual std::uint8_t random_byte() noexcept = 0;

    [[nodiscard]] virtual RadioStats get_stats() const noexcept = 0;
    virtual void reset_stats() noexcept = 0;

    virtual void set_event_callback(EventCallback cb) noexcept = 0;
    virtual void process_events() noexcept = 0;
    virtual void handle_interrupt() noexcept = 0;

protected:
    IRadioDriver() = default;
    IRadioDriver(const IRadioDriver&) = delete;
    IRadioDriver& operator=(const IRadioDriver&) = delete;
};

}  // namespace loradriver
