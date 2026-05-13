#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

#include "loradriver/lora_config.hpp"
#include "loradriver/lora_error.hpp"
#include "loradriver/lora_packet.hpp"
#include "loradriver/radio_driver.hpp"
#include "loradriver/radio_event.hpp"
#include "loradriver/radio_stats.hpp"

namespace loradriver {

class LoRaTransceiver {
public:
    enum class State : std::uint8_t {
        Uninit,
        Sleep,
        Standby,
        Tx,
        RxSingle,
        RxContinuous,
        Cad,
    };

    using PacketCallback  = std::function<void(const LoRaPacket&, const std::uint8_t*, std::size_t)>;
    using EventCallback   = std::function<void(RadioEvent, int)>;
    using TxDoneCallback  = std::function<void()>;

    explicit LoRaTransceiver(IRadioDriver& driver) noexcept;
    ~LoRaTransceiver();

    LoRaTransceiver(const LoRaTransceiver&) = delete;
    LoRaTransceiver& operator=(const LoRaTransceiver&) = delete;

    [[nodiscard]] LoRaError begin(const LoRaConfig& cfg) noexcept;
    void end() noexcept;

    [[nodiscard]] LoRaError set_sleep() noexcept;
    [[nodiscard]] LoRaError set_standby() noexcept;

    /// Blocking send: waits for TxDone or TxTimeout up to timeout_ms.
    [[nodiscard]] LoRaError send(const std::uint8_t* data, std::size_t len,
                                 std::uint32_t timeout_ms = 2000) noexcept;

    [[nodiscard]] LoRaError start_receive(bool continuous = true) noexcept;
    [[nodiscard]] LoRaError start_cad(bool auto_rx = false) noexcept;

    void on_receive(PacketCallback cb) noexcept;
    void on_event(EventCallback cb) noexcept;
    void on_tx_done(TxDoneCallback cb) noexcept;

    void poll() noexcept;

    [[nodiscard]] State        state() const noexcept { return state_; }
    [[nodiscard]] std::int16_t rssi() const noexcept { return driver_.packet_rssi(); }
    [[nodiscard]] float        snr() const noexcept  { return driver_.packet_snr(); }
    [[nodiscard]] std::int32_t frequency_error_hz() const noexcept { return driver_.frequency_error_hz(); }
    [[nodiscard]] RadioStats   stats() const noexcept { return driver_.get_stats(); }
    [[nodiscard]] std::uint8_t chip_version() const noexcept { return driver_.chip_version(); }

    /// Heartbeat: cheap RegVersion read. Returns OK if chip still responds.
    [[nodiscard]] LoRaError check_alive() noexcept {
        if (state_ == State::Uninit) return LoRaError::NotInitialized;
        return driver_.check_alive();
    }

    /// Runtime LNA gain (0 = AGC, 1..6 = manual).
    [[nodiscard]] LoRaError set_lna_gain(std::uint8_t gain) noexcept {
        if (state_ == State::Uninit) return LoRaError::NotInitialized;
        return driver_.set_lna_gain(gain);
    }

    /// Enable or disable over-current protection at runtime.
    [[nodiscard]] LoRaError set_ocp_enabled(bool enabled) noexcept {
        if (state_ == State::Uninit) return LoRaError::NotInitialized;
        return driver_.set_ocp_enabled(enabled);
    }

    /// Forwarded to the underlying driver (use from ISR shim).
    void handle_interrupt() noexcept { driver_.handle_interrupt(); }

private:
    IRadioDriver&    driver_;
    State            state_ = State::Uninit;
    PacketCallback   packet_cb_{};
    EventCallback    event_cb_{};
    TxDoneCallback   tx_done_cb_{};
    bool             rx_continuous_ = false;
    std::uint8_t     rx_buf_[255]{};

    void on_driver_event(RadioEvent ev, int param) noexcept;
};

}  // namespace loradriver
