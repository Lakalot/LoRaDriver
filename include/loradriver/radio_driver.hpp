#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

#include "loradriver/lora_config.hpp"
#include "loradriver/lora_error.hpp"
#include "loradriver/radio_event.hpp"
#include "loradriver/radio_stats.hpp"

namespace loradriver {

/// @brief Chip-agnostic radio driver interface.
///
/// Implemented by SX127xDriver (and future SX126xDriver). All fallible
/// methods return LoRaError. Callbacks must be noexcept. Most users
/// should interact via LoRaTransceiver instead of this interface
/// directly — see lora_transceiver.hpp for the high-level API.
class IRadioDriver {
public:
    /// @brief Event callback signature (signature-compatible across drivers).
    using EventCallback = std::function<void(RadioEvent, int)>;

    virtual ~IRadioDriver() = default;

    [[nodiscard]] virtual LoRaError begin(const LoRaConfig& cfg) noexcept = 0;
    virtual void end() noexcept = 0;
    [[nodiscard]] virtual std::uint8_t chip_version() const noexcept = 0;

    /// Heartbeat: re-read chip version register. Cheap (~5us at 8MHz SPI).
    /// Returns OK if chip still responds with expected signature, otherwise
    /// UnsupportedChip (chip gone / brown-out) or SpiFailure (bus error).
    [[nodiscard]] virtual LoRaError check_alive() noexcept = 0;

    [[nodiscard]] virtual LoRaError set_sleep() noexcept = 0;
    [[nodiscard]] virtual LoRaError set_standby() noexcept = 0;

    [[nodiscard]] virtual LoRaError start_transmit(const std::uint8_t* data, std::size_t len,
                                                   std::uint32_t timeout_ms = 2000) noexcept = 0;
    [[nodiscard]] virtual bool is_transmitting() const noexcept = 0;

    [[nodiscard]] virtual LoRaError start_receive(bool continuous = true) noexcept = 0;
    [[nodiscard]] virtual LoRaError read_packet(std::uint8_t* buf, std::size_t max_len,
                                                std::size_t& out_len) noexcept = 0;

    [[nodiscard]] virtual LoRaError start_cad(bool auto_rx = false) noexcept = 0;

    [[nodiscard]] virtual LoRaError set_frequency(std::uint32_t hz) noexcept = 0;
    [[nodiscard]] virtual LoRaError set_tx_power(std::int8_t dbm, PaOutput out) noexcept = 0;
    [[nodiscard]] virtual LoRaError set_spreading_factor(std::uint8_t sf) noexcept = 0;
    [[nodiscard]] virtual LoRaError set_bandwidth(std::uint32_t hz) noexcept = 0;
    /// Set LNA gain. gain=0 → AGC on; gain=1..6 → AGC off, LnaGain=G1..G6.
    [[nodiscard]] virtual LoRaError set_lna_gain(std::uint8_t gain) noexcept = 0;
    /// Enable or disable OverCurrent Protection.
    [[nodiscard]] virtual LoRaError set_ocp_enabled(bool enabled) noexcept = 0;
    /// Enter continuous-wave (unmodulated carrier) mode for RF certification.
    /// Caller must call set_standby() to exit.
    [[nodiscard]] virtual LoRaError start_continuous_wave() noexcept = 0;

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

} // namespace loradriver
