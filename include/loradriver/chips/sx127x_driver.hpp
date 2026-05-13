#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

#include "loradriver/hal/spi_device.hpp"
#include "loradriver/lora_config.hpp"
#include "loradriver/lora_error.hpp"
#include "loradriver/radio_driver.hpp"

namespace loradriver::chips {

class SX127xDriver final : public IRadioDriver {
public:
    /// Host-test injection point: function called by begin() in lieu of GPIO.
    /// On Arduino targets this stays nullptr and the driver pulses pin_reset
    /// directly via digitalWrite.
    using ResetHook = std::function<void()>;
    static inline ResetHook s_reset_hook_{};

    explicit SX127xDriver(hal::ISpiDevice& spi) noexcept : spi_(spi) {}

    [[nodiscard]] LoRaError begin(const LoRaConfig& cfg) noexcept override;
    void end() noexcept override;
    [[nodiscard]] std::uint8_t chip_version() const noexcept override { return chip_version_; }
    [[nodiscard]] LoRaError check_alive() noexcept override;

    [[nodiscard]] LoRaError set_sleep() noexcept override;
    [[nodiscard]] LoRaError set_standby() noexcept override;

    [[nodiscard]] LoRaError start_transmit(const std::uint8_t* data,
                                           std::size_t len,
                                           std::uint32_t timeout_ms) noexcept override;
    [[nodiscard]] bool is_transmitting() const noexcept override { return tx_in_progress_; }

    [[nodiscard]] LoRaError start_receive(bool continuous) noexcept override;
    [[nodiscard]] LoRaError read_packet(std::uint8_t* buf,
                                        std::size_t max_len,
                                        std::size_t& out_len) noexcept override;

    [[nodiscard]] LoRaError start_cad(bool auto_rx = false) noexcept override;

    [[nodiscard]] LoRaError set_frequency(std::uint32_t hz) noexcept override;
    [[nodiscard]] LoRaError set_tx_power(std::int8_t dbm, PaOutput out) noexcept override;
    [[nodiscard]] LoRaError set_spreading_factor(std::uint8_t sf) noexcept override;
    [[nodiscard]] LoRaError set_bandwidth(std::uint32_t hz) noexcept override;
    [[nodiscard]] LoRaError set_lna_gain(std::uint8_t gain) noexcept override;
    [[nodiscard]] LoRaError set_ocp_enabled(bool enabled) noexcept override;
    [[nodiscard]] LoRaError start_continuous_wave() noexcept override;

    [[nodiscard]] std::int16_t packet_rssi() const noexcept override { return stats_.last_rssi_dbm; }
    [[nodiscard]] float packet_snr() const noexcept override {
        return static_cast<float>(stats_.last_snr_q4) / 4.0f;
    }
    [[nodiscard]] std::int32_t frequency_error_hz() const noexcept override {
        return stats_.last_freq_error_hz;
    }
    [[nodiscard]] std::int16_t current_rssi() const noexcept override;
    [[nodiscard]] std::uint8_t random_byte() noexcept override;

    [[nodiscard]] RadioStats get_stats() const noexcept override { return stats_; }
    void reset_stats() noexcept override { stats_ = RadioStats{}; }

    void set_event_callback(EventCallback cb) noexcept override { event_cb_ = std::move(cb); }
    void process_events() noexcept override;
    void handle_interrupt() noexcept override;

private:
    hal::ISpiDevice& spi_;
    LoRaConfig       cfg_{};
    RadioStats       stats_{};
    EventCallback    event_cb_{};

    std::uint8_t chip_version_ = 0;
    bool         initialized_  = false;
    bool         tx_in_progress_ = false;
    std::uint32_t tx_deadline_ms_ = 0;
    std::uint32_t rx_silence_deadline_ms_ = 0;  // 0 = disarmed
    std::uint8_t  op_mode_shadow_ = 0;
    bool          cad_auto_rx_ = false;

    // IRQ ring buffer (filled by handle_interrupt, drained by process_events)
    static constexpr std::uint8_t kIrqQueueSize = 16;
    volatile std::uint8_t  irq_queue_[kIrqQueueSize]{};
    volatile std::uint8_t  irq_head_ = 0;
    volatile std::uint8_t  irq_tail_ = 0;

    [[nodiscard]] LoRaError set_op_mode(std::uint8_t mode) noexcept;
    [[nodiscard]] LoRaError apply_init_sequence(const LoRaConfig& cfg) noexcept;
    [[nodiscard]] LoRaError apply_modem_config(const LoRaConfig& cfg) noexcept;
    [[nodiscard]] LoRaError apply_tx_power(std::int8_t dbm, PaOutput out) noexcept;
    [[nodiscard]] LoRaError apply_ocp(std::uint8_t ma) noexcept;
    [[nodiscard]] LoRaError apply_frequency(std::uint32_t hz) noexcept;
    [[nodiscard]] LoRaError apply_errata(std::uint32_t bw_hz, std::uint32_t freq_hz) noexcept;
    [[nodiscard]] LoRaError run_rx_image_calibration() noexcept;

    [[nodiscard]] static std::uint32_t now_ms() noexcept;
    [[nodiscard]] static std::uint8_t bw_code(std::uint32_t hz) noexcept;
    [[nodiscard]] std::int16_t rssi_offset() const noexcept;

    void emit(RadioEvent ev, int param) noexcept;
};

}  // namespace loradriver::chips
