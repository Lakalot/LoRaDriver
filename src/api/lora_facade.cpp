#include "loradriver/lora.hpp"

#ifdef ARDUINO

#include <Arduino.h>
#include <SPI.h>

namespace loradriver {

LoRa* LoRa::instance_ = nullptr;

// Defined globally (outside the class) so attachInterrupt can take a
// plain function pointer. IRAM_ATTR keeps the function pinned in IRAM
// on ESP32 so the ISR stays callable while flash is busy.
#ifdef ARDUINO_ARCH_ESP32
void IRAM_ATTR loradriver_isr_dio0_trampoline() {
    if (LoRa::instance_ != nullptr) {
        LoRa::instance_->drv_.handle_interrupt();
        LoRa::instance_->pump_.notify_from_isr();
    }
}
#else
void loradriver_isr_dio0_trampoline() {
    if (LoRa::instance_ != nullptr) {
        LoRa::instance_->drv_.handle_interrupt();
    }
}
#endif

LoRa::~LoRa() { end(); }

LoRaError LoRa::begin(const LoRaConfig& cfg) noexcept {
    if (running_) return LoRaError::AlreadyInitialized;

    const LoRaError vrc = cfg.validate();
    if (vrc != LoRaError::OK) return vrc;

    instance_ = this;

    // 1. SPI bus initialisation.
    if (cfg.spi_pins.sck >= 0 || cfg.spi_pins.miso >= 0 || cfg.spi_pins.mosi >= 0) {
        SPI.begin(cfg.spi_pins.sck, cfg.spi_pins.miso, cfg.spi_pins.mosi);
    } else {
        SPI.begin();
    }

    // 2. Bind the SPI device member to the now-initialised bus.
    spi_.bind(SPI, cfg.pin_ss, cfg.spi_frequency_hz);

    // 3. Run the full transceiver init (resets chip, configures registers).
    const LoRaError err = trx_.begin(cfg);
    if (err != LoRaError::OK) {
        instance_ = nullptr;
        return err;
    }

    // 4. Attach the DIO0 ISR if a pin is configured.
    if (cfg.pin_dio0 >= 0) {
        attachInterrupt(digitalPinToInterrupt(cfg.pin_dio0),
                        &loradriver_isr_dio0_trampoline, RISING);
        attached_dio0_ = cfg.pin_dio0;
    }

    // 5. ESP32: start the pump task if enabled.
#ifdef ARDUINO_ARCH_ESP32
    if (cfg.facade_auto_pump) {
        if (!pump_.start(trx_, cfg.pump.period_ms, cfg.pump.priority,
                         cfg.pump.stack_words, cfg.pump.core_id,
                         cfg.pump.tx_queue_depth, cfg.pump.stop_timeout_ms)) {
            // Pump failed to spawn (queue or task creation error).
            // Tear down what we set up so far.
            if (attached_dio0_ >= 0) {
                detachInterrupt(digitalPinToInterrupt(attached_dio0_));
                attached_dio0_ = -1;
            }
            trx_.end();
            instance_ = nullptr;
            return LoRaError::NotInitialized;
        }
    }
#endif

    // 6. Enter continuous RX if enabled.
    if (cfg.facade_auto_start_receive) {
        const LoRaError rxe = trx_.start_receive(/*continuous=*/true);
        if (rxe != LoRaError::OK) {
            // RX entry failed; tear down to avoid a half-initialised facade.
#ifdef ARDUINO_ARCH_ESP32
            pump_.stop();
#endif
            if (attached_dio0_ >= 0) {
                detachInterrupt(digitalPinToInterrupt(attached_dio0_));
                attached_dio0_ = -1;
            }
            trx_.end();
            instance_ = nullptr;
            return rxe;
        }
    }

    running_ = true;
    return LoRaError::OK;
}

void LoRa::end() noexcept {
    if (!running_) {
        // Still clear instance_ defensively in case begin() partially ran.
        if (instance_ == this) instance_ = nullptr;
        return;
    }

#ifdef ARDUINO_ARCH_ESP32
    pump_.stop();
#endif

    trx_.end();

    if (attached_dio0_ >= 0) {
        detachInterrupt(digitalPinToInterrupt(attached_dio0_));
        attached_dio0_ = -1;
    }

    if (instance_ == this) instance_ = nullptr;
    running_ = false;
}

// === Send ===

LoRaError LoRa::send(const std::uint8_t* data, std::size_t len,
                     std::uint32_t timeout_ms) noexcept {
    return trx_.send(data, len, timeout_ms);
}

#ifdef ARDUINO_ARCH_ESP32
bool LoRa::send_async(const std::uint8_t* data, std::uint8_t len) noexcept {
    if (!pump_.running()) return false;
    return pump_.enqueue_packet(data, len);
}
#endif

// === Receive control ===

LoRaError LoRa::start_receive(bool continuous) noexcept {
    return trx_.start_receive(continuous);
}

LoRaError LoRa::set_standby() noexcept { return trx_.set_standby(); }
LoRaError LoRa::set_sleep()   noexcept { return trx_.set_sleep(); }

LoRaError LoRa::start_cad(bool auto_rx) noexcept {
    return trx_.start_cad(auto_rx);
}

// Callbacks (on_receive/on_event/on_tx_done/on_header) are inline in
// loradriver/lora.hpp so they are available to both Arduino and host
// builds without an out-of-line definition.

// === Metrics ===

std::int16_t LoRa::rssi() const noexcept { return trx_.rssi(); }
float        LoRa::snr()  const noexcept { return trx_.snr(); }
std::int32_t LoRa::frequency_error_hz() const noexcept {
    return trx_.frequency_error_hz();
}
RadioStats LoRa::stats() const noexcept { return trx_.stats(); }
LoRaError  LoRa::check_alive() noexcept { return trx_.check_alive(); }

#ifdef ARDUINO_ARCH_ESP32
platform::esp32::RadioPumpTask::Metrics LoRa::pump_metrics() const noexcept {
    return pump_.metrics();
}
#endif

// === The global instance ===

LoRa lora;

} // namespace loradriver

#else // !ARDUINO — host build

// Host stubs. The facade has no inner SPI/driver/transceiver members on
// host (see lora.hpp), so begin()/end()/send()/etc. cannot run. The
// LORADRIVER_FACADE_HOST_TEST injection ctor exists purely so the
// transceiver() escape hatch and the inline callback forwarders can be
// exercised; everything else is out-of-scope for host coverage.

#include <cstdlib>

namespace loradriver {

LoRa* LoRa::instance_ = nullptr;

// Destructor: on host, end() is a no-op (running_ stays false because
// begin() is never called), so this is safe regardless of test_mode_.
LoRa::~LoRa() { end(); }

void LoRa::end() noexcept {
    // running_ is always false on host (no begin() body linked); nothing
    // to tear down. Keep instance_ housekeeping for symmetry.
    if (instance_ == this) instance_ = nullptr;
    running_ = false;
}

} // namespace loradriver

// Host stub for the unreachable branch of LoRa::transceiver(). Reachable
// only if the host-test ctor was NOT used (i.e. user accidentally
// default-constructed a LoRa on host and called transceiver()). We abort
// loudly rather than UB.
loradriver::LoRaTransceiver& loradriver_facade_no_arduino_transceiver() {
    std::abort();
}

#endif // ARDUINO
