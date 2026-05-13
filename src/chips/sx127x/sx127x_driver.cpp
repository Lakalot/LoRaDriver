#include "loradriver/chips/sx127x_driver.hpp"

#include "sx127x_registers.hpp"

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <chrono>

namespace loradriver::chips {

namespace reg    = sx127x::reg;
namespace opmode = sx127x::opmode;
namespace irq    = sx127x::irq;
namespace dio    = sx127x::dio;

namespace {

constexpr std::uint8_t kVersionExpected = 0x12;
constexpr std::uint32_t kFxOsc = 32'000'000u;

std::uint8_t cr_code(std::uint8_t denom) noexcept {
    // 5→1, 6→2, 7→3, 8→4
    return static_cast<std::uint8_t>(denom - 4u);
}

std::uint8_t ocp_trim(std::uint8_t ma) noexcept {
    // Datasheet §5.4.4 OcpTrim formula
    if (ma <= 120u) return static_cast<std::uint8_t>((ma - 45u) / 5u);
    if (ma <= 240u) return static_cast<std::uint8_t>((ma + 30u) / 10u);
    return 27u;  // = 240 mA cap
}

void pulse_reset(const LoRaConfig& cfg) noexcept {
    if (SX127xDriver::s_reset_hook_) {
        SX127xDriver::s_reset_hook_();
        return;
    }
#ifdef ARDUINO
    if (cfg.pin_reset < 0) return;
    pinMode(cfg.pin_reset, OUTPUT);
    digitalWrite(cfg.pin_reset, LOW);
    delay(cfg.reset_low_ms);
    digitalWrite(cfg.pin_reset, HIGH);
    delay(cfg.reset_settle_ms);
#else
    (void)cfg;  // host build with no hook: skip
#endif
}

}  // namespace

std::uint32_t SX127xDriver::now_ms() noexcept {
#ifdef ARDUINO
    return static_cast<std::uint32_t>(millis());
#else
    using namespace std::chrono;
    return static_cast<std::uint32_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
#endif
}

std::uint8_t SX127xDriver::bw_code(std::uint32_t hz) noexcept {
    switch (hz) {
        case   7'800u: return 0;
        case  10'400u: return 1;
        case  15'600u: return 2;
        case  20'800u: return 3;
        case  31'250u: return 4;
        case  41'700u: return 5;
        case  62'500u: return 6;
        case 125'000u: return 7;
        case 250'000u: return 8;
        case 500'000u: return 9;
    }
    return 7;
}

std::int16_t SX127xDriver::rssi_offset() const noexcept {
    return (cfg_.frequency_hz >= 525'000'000u) ? -164 : -157;
}

void SX127xDriver::emit(RadioEvent ev, int param) noexcept {
    // Callbacks must be noexcept (documented in docs/api.md). The driver
    // is built with -fno-exceptions on Clang/GCC; a throwing callback
    // is undefined behaviour.
    if (event_cb_) event_cb_(ev, param);
}

LoRaError SX127xDriver::set_op_mode(std::uint8_t mode) noexcept {
    LoRaError e = spi_.write_register(reg::kOpMode, mode);
    if (e != LoRaError::OK) return e;
    // Read-back verify on critical mode transitions: TX, RX (any), CAD.
    // We only check that the LoRa-mode bit (0x80) stayed high — a strict
    // equality check is too aggressive because the chip can be in a brief
    // transitional state (e.g. FSTX between Standby and TX) when we read
    // back, and reads/writes are not strictly synchronous on real silicon.
    // The init-sequence verify step (after FSK→LoRa transition) covers
    // the "chip is alive" case; here we just catch a fully-dead chip.
    const bool needs_verify = (mode == opmode::kLoRaTx ||
                               mode == opmode::kLoRaRxCont ||
                               mode == opmode::kLoRaRxSingle ||
                               mode == opmode::kLoRaCad);
    if (needs_verify) {
        std::uint8_t readback = 0;
        e = spi_.read_register(reg::kOpMode, readback);
        if (e != LoRaError::OK) return e;
        if ((readback & opmode::kLoRaModeBit) == 0u) return LoRaError::SpiVerifyMismatch;
    }
    op_mode_shadow_ = mode;
    return LoRaError::OK;
}

LoRaError SX127xDriver::apply_frequency(std::uint32_t hz) noexcept {
    const std::uint64_t frf = (static_cast<std::uint64_t>(hz) << 19) / kFxOsc;
    LoRaError e;
    if ((e = spi_.write_register(reg::kFrMsb, static_cast<std::uint8_t>((frf >> 16) & 0xFF))) != LoRaError::OK) return e;
    if ((e = spi_.write_register(reg::kFrMid, static_cast<std::uint8_t>((frf >> 8) & 0xFF))) != LoRaError::OK) return e;
    if ((e = spi_.write_register(reg::kFrLsb, static_cast<std::uint8_t>(frf & 0xFF))) != LoRaError::OK) return e;
    cfg_.frequency_hz = hz;
    return LoRaError::OK;
}

LoRaError SX127xDriver::apply_modem_config(const LoRaConfig& cfg) noexcept {
    // ModemConfig1: BW[7:4] | CR[3:1] | ImplicitHeader[0]
    const std::uint8_t mc1 = static_cast<std::uint8_t>(
        (bw_code(cfg.bandwidth_hz) << 4) | (cr_code(cfg.coding_rate) << 1) |
        (cfg.implicit_header ? 0x01u : 0x00u));
    LoRaError e;
    if ((e = spi_.write_register(reg::kModemConfig1, mc1)) != LoRaError::OK) return e;

    // ModemConfig2: SF[7:4] | TxContinuous[3]=0 | CRC[2] | SymbTimeoutMsb[1:0]
    const std::uint16_t symb_to = (cfg.symbol_timeout > 0x3FFu)
        ? std::uint16_t{0x3FFu}
        : cfg.symbol_timeout;
    const std::uint8_t mc2 = static_cast<std::uint8_t>(
        (cfg.spreading_factor << 4) |
        (cfg.crc_enabled ? 0x04u : 0x00u) |
        ((symb_to >> 8) & 0x03u));
    if ((e = spi_.write_register(reg::kModemConfig2, mc2)) != LoRaError::OK) return e;

    // SymbTimeoutLsb
    if ((e = spi_.write_register(reg::kSymbTimeoutLsb,
                                 static_cast<std::uint8_t>(symb_to & 0xFFu))) != LoRaError::OK) return e;

    // ModemConfig3: LDRO[3] | AgcAuto[2]
    const bool ldro = cfg.ldro_auto && cfg.ldro_required();
    const std::uint8_t mc3 = static_cast<std::uint8_t>(
        (ldro ? 0x08u : 0x00u) | (cfg.agc_auto ? 0x04u : 0x00u));
    if ((e = spi_.write_register(reg::kModemConfig3, mc3)) != LoRaError::OK) return e;

    // SF6 requires specific DetectionOptimize and DetectionThreshold values
    // (datasheet table 28). Other SF: defaults.
    if (cfg.spreading_factor == 6u) {
        if ((e = spi_.write_register(reg::kDetectionOptimize, 0x05)) != LoRaError::OK) return e;
        if ((e = spi_.write_register(reg::kDetectionThreshold, 0x0C)) != LoRaError::OK) return e;
    } else {
        if ((e = spi_.write_register(reg::kDetectionOptimize, 0x03)) != LoRaError::OK) return e;
        if ((e = spi_.write_register(reg::kDetectionThreshold, 0x0A)) != LoRaError::OK) return e;
    }
    return LoRaError::OK;
}

LoRaError SX127xDriver::apply_tx_power(std::int8_t dbm, PaOutput out) noexcept {
    LoRaError e;
    if (out == PaOutput::Rfo) {
        // RegPaConfig: PaSelect=0, MaxPower=7, OutputPower = dbm (0..14)
        const std::uint8_t v = static_cast<std::uint8_t>(0x70u | (static_cast<std::uint8_t>(dbm) & 0x0Fu));
        if ((e = spi_.write_register(reg::kPaConfig, v)) != LoRaError::OK) return e;
        if ((e = spi_.write_register(reg::kPaDac, 0x84)) != LoRaError::OK) return e;
    } else {
        const bool high_power = dbm > 17;
        // PA_BOOST: Pout = OutputPower + 2 (normal) or OutputPower + 5 (high-power via PaDac=0x87)
        const std::int8_t clamped = (dbm > 20) ? std::int8_t{20} : (dbm < 2 ? std::int8_t{2} : dbm);
        const std::uint8_t out_pow = static_cast<std::uint8_t>(
            high_power ? (clamped - 5) : (clamped - 2)) & 0x0Fu;
        const std::uint8_t v = static_cast<std::uint8_t>(0x80u | 0x70u | out_pow);
        if ((e = spi_.write_register(reg::kPaConfig, v)) != LoRaError::OK) return e;
        if ((e = spi_.write_register(reg::kPaDac, high_power ? 0x87u : 0x84u)) != LoRaError::OK) return e;
    }
    return LoRaError::OK;
}

LoRaError SX127xDriver::apply_ocp(std::uint8_t ma) noexcept {
    const std::uint8_t v = static_cast<std::uint8_t>(0x20u | (ocp_trim(ma) & 0x1Fu));
    return spi_.write_register(reg::kOcp, v);
}

LoRaError SX127xDriver::apply_errata(std::uint32_t bw_hz, std::uint32_t freq_hz) noexcept {
    LoRaError e;

    // Errata 2.1: High BW optimisation when BW = 500 kHz and high-band
    if (bw_hz == 500'000u && freq_hz >= 525'000'000u) {
        if ((e = spi_.write_register(reg::kHighBwOptimize1, 0x02)) != LoRaError::OK) return e;
        if ((e = spi_.write_register(reg::kHighBwOptimize2, 0x64)) != LoRaError::OK) return e;
    } else {
        if ((e = spi_.write_register(reg::kHighBwOptimize1, 0x03)) != LoRaError::OK) return e;
    }

    // Errata 2.3: receiver spurious reception of a LoRa signal. The chip
    // generates a spurious IF artifact; the datasheet errata note gives a
    // per-BW table for RegIfFreq1/2 + a NOP on RegDetectOptimize bit 7.
    // For BW = 500 kHz the workaround is bit 7 set on DetectOptimize and
    // IfFreq1/2 = 0x00. Below 500 kHz, bit 7 cleared and BW-specific IfFreq
    // (P2.1 expands the full table; here we land the 125 kHz default).
    if (bw_hz < 500'000u) {
        std::uint8_t det = 0;
        if ((e = spi_.read_register(reg::kDetectionOptimize, det)) != LoRaError::OK) return e;
        det &= ~0x80u;
        if ((e = spi_.write_register(reg::kDetectionOptimize, det)) != LoRaError::OK) return e;
        if ((e = spi_.write_register(reg::kIfFreq1, 0x40)) != LoRaError::OK) return e;
        if ((e = spi_.write_register(reg::kIfFreq2, 0x00)) != LoRaError::OK) return e;
    } else {
        std::uint8_t det = 0;
        if ((e = spi_.read_register(reg::kDetectionOptimize, det)) != LoRaError::OK) return e;
        det |= 0x80u;
        if ((e = spi_.write_register(reg::kDetectionOptimize, det)) != LoRaError::OK) return e;
        if ((e = spi_.write_register(reg::kIfFreq1, 0x00)) != LoRaError::OK) return e;
        if ((e = spi_.write_register(reg::kIfFreq2, 0x00)) != LoRaError::OK) return e;
    }

    return LoRaError::OK;
}

LoRaError SX127xDriver::apply_init_sequence(const LoRaConfig& cfg) noexcept {
    LoRaError e;

    // Detect chip
    if ((e = spi_.read_register(reg::kVersion, chip_version_)) != LoRaError::OK) return e;
    if (chip_version_ != kVersionExpected) return LoRaError::UnsupportedChip;

    // TCXO config: if enabled, set TcxoInputOn (bit 4). Datasheet §6.6.
    {
        std::uint8_t tcxo = 0;
        if ((e = spi_.read_register(reg::kTcxo, tcxo)) != LoRaError::OK) return e;
        if (cfg.tcxo_enabled) tcxo |= 0x10u; else tcxo &= ~0x10u;
        if ((e = spi_.write_register(reg::kTcxo, tcxo)) != LoRaError::OK) return e;
    }

    // FSK sleep → image calibration → LoRa sleep
    if ((e = set_op_mode(opmode::kFskSleep))  != LoRaError::OK) return e;
    if ((e = run_rx_image_calibration()) != LoRaError::OK) return e;
    if ((e = set_op_mode(opmode::kLoRaSleep)) != LoRaError::OK) return e;

    // Verify LoRa bit (read-back of OpMode)
    std::uint8_t op = 0;
    if ((e = spi_.read_register(reg::kOpMode, op)) != LoRaError::OK) return e;
    if ((op & opmode::kLoRaModeBit) == 0u) return LoRaError::SpiVerifyMismatch;

    // Frequency
    if ((e = apply_frequency(cfg.frequency_hz)) != LoRaError::OK) return e;

    // PA + OCP
    if ((e = apply_tx_power(cfg.tx_power_dbm, cfg.pa_output)) != LoRaError::OK) return e;
    if ((e = apply_ocp(cfg.ocp_ma)) != LoRaError::OK) return e;

    // Modem
    if ((e = apply_modem_config(cfg)) != LoRaError::OK) return e;

    // Sync word
    if ((e = spi_.write_register(reg::kSyncWord, static_cast<std::uint8_t>(cfg.sync_word & 0xFFu))) != LoRaError::OK) return e;

    // Preamble length
    if ((e = spi_.write_register(reg::kPreambleMsb,
                                 static_cast<std::uint8_t>(cfg.preamble_length >> 8))) != LoRaError::OK) return e;
    if ((e = spi_.write_register(reg::kPreambleLsb,
                                 static_cast<std::uint8_t>(cfg.preamble_length & 0xFFu))) != LoRaError::OK) return e;

    // LNA boost
    const std::uint8_t lna = static_cast<std::uint8_t>(
        (0x01u << 5) |  // LnaGain=G1 (max)
        (cfg.lna_boost_rx ? 0x03u : 0x00u));
    if ((e = spi_.write_register(reg::kLna, lna)) != LoRaError::OK) return e;

    // Invert IQ (datasheet table 23). Std: 0x27/0x1D, Inverted: 0x67/0x19.
    const std::uint8_t inv_iq  = cfg.invert_iq ? std::uint8_t{0x67} : std::uint8_t{0x27};
    const std::uint8_t inv_iq2 = cfg.invert_iq ? std::uint8_t{0x19} : std::uint8_t{0x1D};
    if ((e = spi_.write_register(reg::kInvertIq,  inv_iq))  != LoRaError::OK) return e;
    if ((e = spi_.write_register(reg::kInvertIq2, inv_iq2)) != LoRaError::OK) return e;

    // Errata
    if ((e = apply_errata(cfg.bandwidth_hz, cfg.frequency_hz)) != LoRaError::OK) return e;

    // FIFO base addresses: split halves so concurrent TX prep and RX cannot
    // stomp each other. TX writes from 0, RX receives into 128.
    if ((e = spi_.write_register(reg::kFifoTxBaseAddr, 0)) != LoRaError::OK) return e;
    if ((e = spi_.write_register(reg::kFifoRxBaseAddr, 128)) != LoRaError::OK) return e;

    // DIO mapping: DIO0=RxDone by default
    if ((e = spi_.write_register(reg::kDioMapping1, dio::kDio0RxDone)) != LoRaError::OK) return e;

    // Standby
    if ((e = set_op_mode(opmode::kLoRaStandby)) != LoRaError::OK) return e;

    // Clear all IRQ flags (write 1 to clear)
    if ((e = spi_.write_register(reg::kIrqFlags, irq::kClearAll)) != LoRaError::OK) return e;

    return LoRaError::OK;
}

LoRaError SX127xDriver::begin(const LoRaConfig& cfg) noexcept {
    if (initialized_) return LoRaError::AlreadyInitialized;

    LoRaError e = cfg.validate();
    if (e != LoRaError::OK) return e;

    if (cfg.auto_reset) {
        pulse_reset(cfg);
    }

    if ((e = spi_.begin()) != LoRaError::OK) return e;

    cfg_ = cfg;
    e = apply_init_sequence(cfg);
    if (e != LoRaError::OK) {
        initialized_ = false;
        return e;
    }
    initialized_ = true;
    return LoRaError::OK;
}

void SX127xDriver::end() noexcept {
    if (!initialized_) return;
    (void)set_op_mode(opmode::kLoRaSleep);
    // Reset all runtime state so a subsequent begin() restarts from a clean slate.
    initialized_ = false;
    tx_in_progress_ = false;
    tx_deadline_ms_ = 0;
    rx_silence_deadline_ms_ = 0;
    op_mode_shadow_ = 0;
    cad_auto_rx_ = false;
    irq_head_ = 0;
    irq_tail_ = 0;
    event_cb_ = nullptr;
    cfg_ = LoRaConfig{};
}

LoRaError SX127xDriver::set_sleep() noexcept {
    if (!initialized_) return LoRaError::NotInitialized;
    return set_op_mode(opmode::kLoRaSleep);
}

LoRaError SX127xDriver::set_standby() noexcept {
    if (!initialized_) return LoRaError::NotInitialized;
    return set_op_mode(opmode::kLoRaStandby);
}

LoRaError SX127xDriver::set_frequency(std::uint32_t hz) noexcept {
    if (!initialized_) return LoRaError::NotInitialized;
    return apply_frequency(hz);
}

LoRaError SX127xDriver::set_tx_power(std::int8_t dbm, PaOutput out) noexcept {
    if (!initialized_) return LoRaError::NotInitialized;
    cfg_.tx_power_dbm = dbm;
    cfg_.pa_output    = out;
    return apply_tx_power(dbm, out);
}

LoRaError SX127xDriver::set_spreading_factor(std::uint8_t sf) noexcept {
    if (!initialized_) return LoRaError::NotInitialized;
    if (sf < 6 || sf > 12) return LoRaError::InvalidConfig;
    cfg_.spreading_factor = sf;
    return apply_modem_config(cfg_);
}

LoRaError SX127xDriver::set_bandwidth(std::uint32_t hz) noexcept {
    if (!initialized_) return LoRaError::NotInitialized;
    cfg_.bandwidth_hz = hz;
    LoRaError e = apply_modem_config(cfg_);
    if (e != LoRaError::OK) return e;
    return apply_errata(cfg_.bandwidth_hz, cfg_.frequency_hz);
}

LoRaError SX127xDriver::set_lna_gain(std::uint8_t gain) noexcept {
    if (!initialized_) return LoRaError::NotInitialized;
    if (gain > 6u) return LoRaError::InvalidConfig;
    if (gain == 0u) {
        // AGC on, ModemConfig3 bit 2 set
        std::uint8_t mc3 = 0;
        const LoRaError e1 = spi_.read_register(reg::kModemConfig3, mc3);
        if (e1 != LoRaError::OK) return e1;
        mc3 |= 0x04u;
        return spi_.write_register(reg::kModemConfig3, mc3);
    }
    // AGC off + RegLna LnaGain field
    std::uint8_t mc3 = 0;
    if (spi_.read_register(reg::kModemConfig3, mc3) != LoRaError::OK) return LoRaError::SpiFailure;
    mc3 &= ~0x04u;
    const LoRaError e2 = spi_.write_register(reg::kModemConfig3, mc3);
    if (e2 != LoRaError::OK) return e2;
    const std::uint8_t lna = static_cast<std::uint8_t>(
        (gain << 5) | (cfg_.lna_boost_rx ? 0x03u : 0x00u));
    return spi_.write_register(reg::kLna, lna);
}

LoRaError SX127xDriver::set_ocp_enabled(bool enabled) noexcept {
    if (!initialized_) return LoRaError::NotInitialized;
    std::uint8_t v = 0;
    LoRaError e = spi_.read_register(reg::kOcp, v);
    if (e != LoRaError::OK) return e;
    if (enabled) v |= 0x20u; else v &= ~0x20u;
    return spi_.write_register(reg::kOcp, v);
}

LoRaError SX127xDriver::start_continuous_wave() noexcept {
    if (!initialized_) return LoRaError::NotInitialized;
    LoRaError e;
    std::uint8_t mc2 = 0;
    if ((e = spi_.read_register(reg::kModemConfig2, mc2)) != LoRaError::OK) return e;
    mc2 |= 0x08u;  // TxContinuousMode
    if ((e = spi_.write_register(reg::kModemConfig2, mc2)) != LoRaError::OK) return e;
    return set_op_mode(opmode::kLoRaTx);
}

// --- TX / RX / CAD / IRQ stubs — implemented in Tasks 3.5–3.7 ---

LoRaError SX127xDriver::start_transmit(const std::uint8_t* data,
                                       std::size_t len,
                                       std::uint32_t timeout_ms) noexcept {
    if (!initialized_) return LoRaError::NotInitialized;
    if (data == nullptr) return LoRaError::NullArgument;
    if (len == 0u) return LoRaError::InvalidConfig;
    if (len > 255u) return LoRaError::TxBufferTooLarge;

    LoRaError e;
    if ((e = set_op_mode(opmode::kLoRaStandby)) != LoRaError::OK) return e;
    if ((e = spi_.write_register(reg::kFifoTxBaseAddr, 0)) != LoRaError::OK) return e;
    if ((e = spi_.write_register(reg::kFifoAddrPtr, 0)) != LoRaError::OK) return e;
    if ((e = spi_.burst_write(reg::kFifo, data, len)) != LoRaError::OK) return e;
    if ((e = spi_.write_register(reg::kPayloadLength, static_cast<std::uint8_t>(len))) != LoRaError::OK) return e;

    // DIO0 = TxDone before entering TX mode
    if ((e = spi_.write_register(reg::kDioMapping1, dio::kDio0TxDone)) != LoRaError::OK) return e;

    if ((e = set_op_mode(opmode::kLoRaTx)) != LoRaError::OK) return e;

    tx_in_progress_ = true;
    tx_deadline_ms_ = now_ms() + timeout_ms;
    return LoRaError::OK;
}

LoRaError SX127xDriver::start_receive(bool continuous) noexcept {
    if (!initialized_) return LoRaError::NotInitialized;
    LoRaError e;
    if ((e = set_op_mode(opmode::kLoRaStandby)) != LoRaError::OK) return e;
    if ((e = spi_.write_register(reg::kFifoRxBaseAddr, 128)) != LoRaError::OK) return e;
    if ((e = spi_.write_register(reg::kFifoAddrPtr, 128)) != LoRaError::OK) return e;
    if ((e = spi_.write_register(reg::kDioMapping1, dio::kDio0RxDone)) != LoRaError::OK) return e;
    e = set_op_mode(continuous ? opmode::kLoRaRxCont : opmode::kLoRaRxSingle);
    if (e == LoRaError::OK && continuous && cfg_.rx_silence_timeout_ms > 0) {
        rx_silence_deadline_ms_ = now_ms() + cfg_.rx_silence_timeout_ms;
    } else {
        rx_silence_deadline_ms_ = 0;
    }
    return e;
}

LoRaError SX127xDriver::read_packet(std::uint8_t* buf,
                                    std::size_t max_len,
                                    std::size_t& out_len) noexcept {
    out_len = 0;
    if (!initialized_) return LoRaError::NotInitialized;
    if (buf == nullptr || max_len == 0u) return LoRaError::NullArgument;

    std::uint8_t rx_addr = 0;
    std::uint8_t nb_bytes = 0;
    LoRaError e;
    if ((e = spi_.read_register(reg::kFifoRxCurrentAddr, rx_addr)) != LoRaError::OK) return e;
    if ((e = spi_.read_register(reg::kRxNbBytes, nb_bytes)) != LoRaError::OK) return e;
    if (nb_bytes == 0u) return LoRaError::OK;

    const std::size_t to_read = (nb_bytes <= max_len) ? nb_bytes : max_len;
    if ((e = spi_.write_register(reg::kFifoAddrPtr, rx_addr)) != LoRaError::OK) return e;
    if ((e = spi_.burst_read(reg::kFifo, buf, to_read)) != LoRaError::OK) return e;
    out_len = to_read;
    return LoRaError::OK;
}

LoRaError SX127xDriver::start_cad(bool auto_rx) noexcept {
    if (!initialized_) return LoRaError::NotInitialized;
    cad_auto_rx_ = auto_rx;
    LoRaError e = spi_.write_register(reg::kDioMapping1, dio::kDio0CadDone);
    if (e != LoRaError::OK) return e;
    return set_op_mode(opmode::kLoRaCad);
}

std::int16_t SX127xDriver::current_rssi() const noexcept {
    if (!initialized_) return 0;
    std::uint8_t raw = 0;
    if (spi_.read_register(reg::kRssiValue, raw) != LoRaError::OK) return 0;
    return static_cast<std::int16_t>(rssi_offset() + static_cast<int>(raw));
}

std::uint8_t SX127xDriver::random_byte() noexcept {
    std::uint8_t v = 0;
    (void)spi_.read_register(reg::kRssiWideband, v);
    return v;
}

void SX127xDriver::handle_interrupt() noexcept {
    const std::uint8_t next = static_cast<std::uint8_t>((irq_head_ + 1u) % kIrqQueueSize);
    if (next == irq_tail_) {
        ++stats_.irq_overflows;
        return;
    }
    irq_queue_[irq_head_] = 1u;  // marker; actual flags read in process_events
    irq_head_ = next;
}

void SX127xDriver::process_events() noexcept {
    // Watchdog TX
    if (tx_in_progress_ && now_ms() >= tx_deadline_ms_) {
        tx_in_progress_ = false;
        ++stats_.tx_timeout;
        (void)set_op_mode(opmode::kLoRaStandby);
        emit(RadioEvent::TxTimeout, 0);
    }

    // RX silence watchdog
    if (rx_silence_deadline_ms_ != 0u && now_ms() >= rx_silence_deadline_ms_) {
        rx_silence_deadline_ms_ = 0u;  // disarm to avoid storm
        ++stats_.rx_timeout;
        emit(RadioEvent::RxTimeout, 0);
        (void)set_op_mode(opmode::kLoRaStandby);
    }

    // Drain IRQ queue (max kIrqQueueSize iterations).
    // In polling_mode, synthesize a queue entry so the loop runs once per
    // process_events() call even without handle_interrupt being invoked.
    if (cfg_.polling_mode && irq_tail_ == irq_head_) {
        const std::uint8_t next = static_cast<std::uint8_t>((irq_head_ + 1u) % kIrqQueueSize);
        if (next != irq_tail_) {
            irq_queue_[irq_head_] = 1u;
            irq_head_ = next;
        }
    }

    std::uint8_t iters = 0;
    while (irq_tail_ != irq_head_ && iters < kIrqQueueSize) {
        irq_tail_ = static_cast<std::uint8_t>((irq_tail_ + 1u) % kIrqQueueSize);
        ++iters;

        std::uint8_t flags = 0;
        if (spi_.read_register(reg::kIrqFlags, flags) != LoRaError::OK) continue;
        if (flags == 0u) continue;  // synthetic poll with nothing to do

        // Clear flags (write 1 to clear)
        (void)spi_.write_register(reg::kIrqFlags, irq::kClearAll);
        ++stats_.irq_events_processed;

        if (flags & irq::kPayloadCrcError) {
            ++stats_.rx_crc_errors;
            emit(RadioEvent::RxCrcError, flags);
        } else if (flags & irq::kRxDone) {
            // Snapshot metrics
            std::uint8_t rssi_raw = 0, snr_raw = 0;
            (void)spi_.read_register(reg::kPktRssiValue, rssi_raw);
            (void)spi_.read_register(reg::kPktSnrValue, snr_raw);
            stats_.last_rssi_dbm = static_cast<std::int16_t>(rssi_offset() + rssi_raw);
            stats_.last_snr_q4   = static_cast<std::int16_t>(static_cast<std::int8_t>(snr_raw));
            ++stats_.rx_done;
            if (cfg_.rx_silence_timeout_ms > 0u) {
                rx_silence_deadline_ms_ = now_ms() + cfg_.rx_silence_timeout_ms;
            }
            emit(RadioEvent::RxDone, flags);
        }

        if (flags & irq::kTxDone) {
            tx_in_progress_ = false;
            ++stats_.tx_done;
            emit(RadioEvent::TxDone, flags);
        }
        if (flags & irq::kRxTimeout) {
            ++stats_.rx_timeout;
            emit(RadioEvent::RxTimeout, flags);
        }
        if (flags & irq::kCadDone) {
            const bool detected = (flags & irq::kCadDetected) != 0u;
            emit(RadioEvent::CadDone, detected ? 1 : 0);
            if (cad_auto_rx_ && detected) {
                (void)start_receive(true);
            }
            cad_auto_rx_ = false;
        }
        if (flags & irq::kValidHeader) {
            emit(RadioEvent::ValidHeader, flags);
        }
    }

    // Update backlog stat
    const std::uint8_t backlog = static_cast<std::uint8_t>(
        (irq_head_ + kIrqQueueSize - irq_tail_) % kIrqQueueSize);
    if (backlog > stats_.max_irq_backlog) stats_.max_irq_backlog = backlog;
}

LoRaError SX127xDriver::run_rx_image_calibration() noexcept {
    // Datasheet §4.2.3.8: image calibration must be done in FSK mode.
    // We're called between FSK sleep and LoRa sleep in apply_init_sequence,
    // so the chip is already in FSK access mode.
    LoRaError e;
    std::uint8_t v = 0;
    if ((e = spi_.read_register(reg::kImageCal, v)) != LoRaError::OK) return e;
    v |= 0x40u;  // ImageCalStart bit
    if ((e = spi_.write_register(reg::kImageCal, v)) != LoRaError::OK) return e;

    // Wait for ImageCalRunning to clear (bit 5). Bounded poll, ~1 ms.
    for (int i = 0; i < 100; ++i) {
        if (spi_.read_register(reg::kImageCal, v) != LoRaError::OK) return LoRaError::SpiFailure;
        if ((v & 0x20u) == 0u) return LoRaError::OK;
#ifdef ARDUINO
        delayMicroseconds(10);
#endif
    }
    return LoRaError::OK;  // best-effort
}

LoRaError SX127xDriver::check_alive() noexcept {
    if (!initialized_) return LoRaError::NotInitialized;
    std::uint8_t v = 0;
    const LoRaError e = spi_.read_register(reg::kVersion, v);
    if (e != LoRaError::OK) return e;
    if (v != kVersionExpected) return LoRaError::UnsupportedChip;
    return LoRaError::OK;
}

}  // namespace loradriver::chips
