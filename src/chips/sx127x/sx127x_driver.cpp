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
    if (event_cb_) {
#if __cpp_exceptions
        try { event_cb_(ev, param); } catch (...) { ++stats_.callback_exceptions; }
#else
        event_cb_(ev, param);
#endif
    }
}

LoRaError SX127xDriver::set_op_mode(std::uint8_t mode) noexcept {
    const LoRaError e = spi_.write_register(reg::kOpMode, mode);
    if (e != LoRaError::OK) return e;
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

    // ModemConfig2: SF[7:4] | TxContinuous[3]=0 | CRC[2] | SymbTimeoutMsb[1:0]=11
    const std::uint8_t mc2 = static_cast<std::uint8_t>(
        (cfg.spreading_factor << 4) |
        (cfg.crc_enabled ? 0x04u : 0x00u) |
        0x03u);
    if ((e = spi_.write_register(reg::kModemConfig2, mc2)) != LoRaError::OK) return e;

    // SymbTimeoutLsb
    if ((e = spi_.write_register(reg::kSymbTimeoutLsb,
                                 static_cast<std::uint8_t>(cfg.symbol_timeout & 0xFFu))) != LoRaError::OK) return e;

    // ModemConfig3: LDRO[3] | AgcAuto[2]
    const bool ldro = cfg.ldro_auto && cfg.ldro_required();
    const std::uint8_t mc3 = static_cast<std::uint8_t>(
        (ldro ? 0x08u : 0x00u) | (cfg.agc_auto ? 0x04u : 0x00u));
    return spi_.write_register(reg::kModemConfig3, mc3);
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
    // Errata 2.1: High BW optimisation when BW = 500 kHz and high-band
    if (bw_hz == 500'000u && freq_hz >= 525'000'000u) {
        LoRaError e;
        if ((e = spi_.write_register(reg::kHighBwOptimize1, 0x02)) != LoRaError::OK) return e;
        if ((e = spi_.write_register(reg::kHighBwOptimize2, 0x64)) != LoRaError::OK) return e;
    } else {
        LoRaError e;
        if ((e = spi_.write_register(reg::kHighBwOptimize1, 0x03)) != LoRaError::OK) return e;
        (void)e;
    }
    return LoRaError::OK;
}

LoRaError SX127xDriver::apply_init_sequence(const LoRaConfig& cfg) noexcept {
    LoRaError e;

    // Detect chip
    if ((e = spi_.read_register(reg::kVersion, chip_version_)) != LoRaError::OK) return e;
    if (chip_version_ != kVersionExpected) return LoRaError::UnsupportedChip;

    // FSK sleep → LoRa sleep (precondition for switching mode bit)
    if ((e = set_op_mode(opmode::kFskSleep))  != LoRaError::OK) return e;
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

    // Errata
    if ((e = apply_errata(cfg.bandwidth_hz, cfg.frequency_hz)) != LoRaError::OK) return e;

    // FIFO base addresses (split FIFO: TX=0, RX=0 — overwrite-safe via FifoAddrPtr)
    if ((e = spi_.write_register(reg::kFifoTxBaseAddr, 0)) != LoRaError::OK) return e;
    if ((e = spi_.write_register(reg::kFifoRxBaseAddr, 0)) != LoRaError::OK) return e;

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
    initialized_ = false;
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
    if ((e = spi_.write_register(reg::kFifoRxBaseAddr, 0)) != LoRaError::OK) return e;
    if ((e = spi_.write_register(reg::kFifoAddrPtr, 0)) != LoRaError::OK) return e;
    if ((e = spi_.write_register(reg::kDioMapping1, dio::kDio0RxDone)) != LoRaError::OK) return e;
    return set_op_mode(continuous ? opmode::kLoRaRxCont : opmode::kLoRaRxSingle);
}

int SX127xDriver::read_packet(std::uint8_t* buf, std::size_t max_len) noexcept {
    if (!initialized_ || buf == nullptr || max_len == 0u) return 0;

    std::uint8_t rx_addr = 0;
    std::uint8_t nb_bytes = 0;
    if (spi_.read_register(reg::kFifoRxCurrentAddr, rx_addr) != LoRaError::OK) return 0;
    if (spi_.read_register(reg::kRxNbBytes, nb_bytes) != LoRaError::OK) return 0;
    if (nb_bytes == 0u) return 0;

    const std::size_t to_read = (nb_bytes <= max_len) ? nb_bytes : max_len;
    if (spi_.write_register(reg::kFifoAddrPtr, rx_addr) != LoRaError::OK) return 0;
    if (spi_.burst_read(reg::kFifo, buf, to_read) != LoRaError::OK) return 0;

    return static_cast<int>(to_read);
}

LoRaError SX127xDriver::start_cad() noexcept {
    return LoRaError::InvalidState;
}

std::int16_t SX127xDriver::current_rssi() const noexcept {
    return 0;
}

std::uint8_t SX127xDriver::random_byte() noexcept {
    return 0;
}

void SX127xDriver::process_events() noexcept {}

void SX127xDriver::handle_interrupt() noexcept {}

}  // namespace loradriver::chips
