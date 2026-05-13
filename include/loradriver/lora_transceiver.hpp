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

/// @brief High-level LoRa transceiver façade with FSM and packet dispatch.
///
/// Owns a reference to an IRadioDriver (caller manages driver lifetime).
/// Provides blocking @ref send and a packet callback model for RX.
///
/// State machine: Uninit → begin() → Standby. Standby ↔ Sleep / Tx /
/// RxSingle / RxContinuous / Cad. All transitions enforced — invalid
/// calls return LoRaError::InvalidState.
///
/// @note Thread safety: single-threaded by default. Use the optional
/// RadioPumpTask on ESP32 to serialise main-task + ISR access.
class LoRaTransceiver {
public:
    /// @brief Façade FSM state.
    enum class State : std::uint8_t {
        Uninit,
        Sleep,
        Standby,
        Tx,
        RxSingle,
        RxContinuous,
        Cad,
    };

    /// @brief Invoked on RxDone with full packet metadata + payload.
    using PacketCallback = std::function<void(const LoRaPacket&, const std::uint8_t*, std::size_t)>;
    /// @brief Invoked on every low-level RadioEvent (with detail code).
    using EventCallback = std::function<void(RadioEvent, int)>;
    /// @brief Invoked on TxDone or TxTimeout.
    using TxDoneCallback = std::function<void()>;
    /// @brief Invoked on ValidHeader IRQ (pre-RxDone wake-up).
    using HeaderCallback = std::function<void()>;

    /// @brief Construct the façade with an injected driver.
    /// @param driver Must outlive the LoRaTransceiver instance.
    explicit LoRaTransceiver(IRadioDriver& driver) noexcept;
    ~LoRaTransceiver();

    LoRaTransceiver(const LoRaTransceiver&) = delete;
    LoRaTransceiver& operator=(const LoRaTransceiver&) = delete;

    /// @brief Initialise the radio with the given configuration.
    /// @param cfg Validated by LoRaConfig::validate() first.
    /// @return OK on success; InvalidConfig if validate() fails; pass-through
    ///         from driver_.begin() otherwise.
    [[nodiscard]] LoRaError begin(const LoRaConfig& cfg) noexcept;

    /// @brief Tear down: detach callbacks, put chip to sleep, reset FSM.
    /// @note The DIO0 interrupt attached by the user must be detached
    ///       separately (the driver does not own that GPIO line).
    void end() noexcept;

    /// @brief Put the chip into the lowest-power state with state retain.
    [[nodiscard]] LoRaError set_sleep() noexcept;

    /// @brief Bring the chip back to ready-to-RX/TX without re-init.
    [[nodiscard]] LoRaError set_standby() noexcept;

    /// @brief Blocking transmit. Returns when TxDone fires or timeout expires.
    /// @param data Payload bytes (≤ 255).
    /// @param len  Number of bytes (must be > 0).
    /// @param timeout_ms Total wait budget in milliseconds.
    /// @return OK / TxTimeout / SpiFailure / NullArgument / TxBufferTooLarge.
    [[nodiscard]] LoRaError send(const std::uint8_t* data, std::size_t len,
                                 std::uint32_t timeout_ms = 2000) noexcept;

    /// @brief Put the radio in continuous or single-shot RX mode.
    /// @param continuous If true, RXCONTINUOUS (stays in RX after each packet).
    [[nodiscard]] LoRaError start_receive(bool continuous = true) noexcept;

    /// @brief Trigger Channel Activity Detection.
    /// @param auto_rx If true and a signal is detected, automatically
    ///                enters RX_CONTINUOUS on CadDone.
    [[nodiscard]] LoRaError start_cad(bool auto_rx = false) noexcept;

    /// @brief Register the RX-packet callback (invoked from poll()).
    /// @note Must be noexcept — driver is built with -fno-exceptions.
    void on_receive(PacketCallback cb) noexcept;
    /// @brief Register the general radio-event callback.
    void on_event(EventCallback cb) noexcept;
    /// @brief Register the TxDone / TxTimeout callback.
    void on_tx_done(TxDoneCallback cb) noexcept;
    /// @brief Register the ValidHeader (pre-RxDone) callback.
    void on_header(HeaderCallback cb) noexcept;

    /// @brief Drain pending events from the ring buffer + run watchdogs.
    /// @note Must be called from a single thread (main loop or pump task).
    void poll() noexcept;

    /// @brief Current FSM state.
    [[nodiscard]] State state() const noexcept { return state_; }
    /// @brief RSSI of the last received packet (dBm).
    [[nodiscard]] std::int16_t rssi() const noexcept { return driver_.packet_rssi(); }
    /// @brief SNR of the last received packet (dB).
    [[nodiscard]] float snr() const noexcept { return driver_.packet_snr(); }
    /// @brief Frequency error of the last received packet (Hz).
    [[nodiscard]] std::int32_t frequency_error_hz() const noexcept {
        return driver_.frequency_error_hz();
    }
    /// @brief Snapshot of cumulative driver stats (trivially copyable).
    [[nodiscard]] RadioStats stats() const noexcept { return driver_.get_stats(); }
    /// @brief RegVersion as read at the most recent begin() (0x12 for SX127x).
    [[nodiscard]] std::uint8_t chip_version() const noexcept { return driver_.chip_version(); }

    /// Heartbeat: cheap RegVersion read. Returns OK if chip still responds.
    [[nodiscard]] LoRaError check_alive() noexcept {
        if (state_ == State::Uninit)
            return LoRaError::NotInitialized;
        return driver_.check_alive();
    }

    /// Runtime LNA gain (0 = AGC, 1..6 = manual).
    [[nodiscard]] LoRaError set_lna_gain(std::uint8_t gain) noexcept {
        if (state_ == State::Uninit)
            return LoRaError::NotInitialized;
        return driver_.set_lna_gain(gain);
    }

    /// Enable or disable over-current protection at runtime.
    [[nodiscard]] LoRaError set_ocp_enabled(bool enabled) noexcept {
        if (state_ == State::Uninit)
            return LoRaError::NotInitialized;
        return driver_.set_ocp_enabled(enabled);
    }

    /// Forwarded to the underlying driver (use from ISR shim).
    void handle_interrupt() noexcept { driver_.handle_interrupt(); }

private:
    IRadioDriver& driver_;
    State state_ = State::Uninit;
    PacketCallback packet_cb_{};
    EventCallback event_cb_{};
    TxDoneCallback tx_done_cb_{};
    HeaderCallback header_cb_{};
    bool rx_continuous_ = false;
    std::uint8_t rx_buf_[255]{};

    void on_driver_event(RadioEvent ev, int param) noexcept;
};

} // namespace loradriver
