#include "loradriver/lora_transceiver.hpp"

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <chrono>
#endif
#include <cstring>

namespace loradriver {

namespace {
std::uint32_t now_ms() noexcept {
#ifdef ARDUINO
    return static_cast<std::uint32_t>(::millis());
#else
    using namespace std::chrono;
    return static_cast<std::uint32_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
#endif
}
}  // namespace

LoRaTransceiver::LoRaTransceiver(IRadioDriver& driver) noexcept : driver_(driver) {}

LoRaTransceiver::~LoRaTransceiver() {
    end();
}

LoRaError LoRaTransceiver::begin(const LoRaConfig& cfg) noexcept {
    if (state_ != State::Uninit) return LoRaError::AlreadyInitialized;
    const LoRaError e = driver_.begin(cfg);
    if (e != LoRaError::OK) return e;

    driver_.set_event_callback([this](RadioEvent ev, int param) {
        on_driver_event(ev, param);
    });

    state_ = State::Standby;
    return LoRaError::OK;
}

void LoRaTransceiver::end() noexcept {
    if (state_ == State::Uninit) return;
    // Detach driver callback first so any in-flight IRQ can't reach
    // user code through a dead lambda capture.
    driver_.set_event_callback(nullptr);
    driver_.end();
    packet_cb_  = {};
    event_cb_   = {};
    tx_done_cb_ = {};
    state_ = State::Uninit;
}

LoRaError LoRaTransceiver::set_sleep() noexcept {
    if (state_ == State::Uninit) return LoRaError::NotInitialized;
    const LoRaError e = driver_.set_sleep();
    if (e == LoRaError::OK) state_ = State::Sleep;
    return e;
}

LoRaError LoRaTransceiver::set_standby() noexcept {
    if (state_ == State::Uninit) return LoRaError::NotInitialized;
    const LoRaError e = driver_.set_standby();
    if (e == LoRaError::OK) state_ = State::Standby;
    return e;
}

LoRaError LoRaTransceiver::send(const std::uint8_t* data, std::size_t len,
                                std::uint32_t timeout_ms) noexcept {
    if (state_ == State::Uninit) return LoRaError::NotInitialized;
    if (state_ != State::Standby) {
        const LoRaError e = set_standby();
        if (e != LoRaError::OK) return e;
    }

    const LoRaError start = driver_.start_transmit(data, len, timeout_ms);
    if (start != LoRaError::OK) return start;
    state_ = State::Tx;

    const std::uint32_t deadline = now_ms() + timeout_ms;
    while (driver_.is_transmitting()) {
        driver_.process_events();
        if (now_ms() > deadline) {
            // The driver's own watchdog already emitted TxTimeout; restore state.
            state_ = State::Standby;
            return LoRaError::TxTimeout;
        }
    }
    state_ = State::Standby;
    return LoRaError::OK;
}

LoRaError LoRaTransceiver::start_receive(bool continuous) noexcept {
    if (state_ == State::Uninit) return LoRaError::NotInitialized;
    const LoRaError e = driver_.start_receive(continuous);
    if (e != LoRaError::OK) return e;
    rx_continuous_ = continuous;
    state_ = continuous ? State::RxContinuous : State::RxSingle;
    return LoRaError::OK;
}

LoRaError LoRaTransceiver::start_cad(bool auto_rx) noexcept {
    if (state_ == State::Uninit) return LoRaError::NotInitialized;
    const LoRaError e = driver_.start_cad(auto_rx);
    if (e == LoRaError::OK) state_ = State::Cad;
    return e;
}

void LoRaTransceiver::on_receive(PacketCallback cb) noexcept  { packet_cb_  = std::move(cb); }
void LoRaTransceiver::on_event(EventCallback cb)   noexcept  { event_cb_   = std::move(cb); }
void LoRaTransceiver::on_tx_done(TxDoneCallback cb) noexcept { tx_done_cb_ = std::move(cb); }

void LoRaTransceiver::poll() noexcept {
    if (state_ == State::Uninit) return;
    driver_.process_events();
}

void LoRaTransceiver::on_driver_event(RadioEvent ev, int param) noexcept {
    // Forward to user event callback first
    if (event_cb_) event_cb_(ev, param);

    switch (ev) {
        case RadioEvent::RxDone: {
            const int n = driver_.read_packet(rx_buf_, sizeof(rx_buf_));
            if (n > 0 && packet_cb_) {
                LoRaPacket meta{};
                meta.rssi_dbm           = driver_.packet_rssi();
                meta.snr_q4             = static_cast<std::int16_t>(driver_.packet_snr() * 4.0f);
                meta.frequency_error_hz = driver_.frequency_error_hz();
                meta.length             = static_cast<std::uint8_t>(n);
                meta.crc_valid          = true;
                packet_cb_(meta, rx_buf_, static_cast<std::size_t>(n));
            }
            if (!rx_continuous_) state_ = State::Standby;
            break;
        }
        case RadioEvent::TxDone:
        case RadioEvent::TxTimeout:
            if (tx_done_cb_) tx_done_cb_();
            break;
        case RadioEvent::RxTimeout:
            if (!rx_continuous_) state_ = State::Standby;
            break;
        case RadioEvent::CadDone:
            state_ = State::Standby;
            break;
        default: break;
    }
}

}  // namespace loradriver
