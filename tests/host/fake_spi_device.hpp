#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "loradriver/hal/spi_device.hpp"
#include "loradriver/lora_error.hpp"

namespace loradriver::test {

/// Deterministic SPI fake — simulates a 256-byte register file.
/// Failure injection:
///   * fail_writes(true)  → every write transfer returns SpiFailure
///   * fail_reads(true)   → every read transfer returns SpiFailure
///   * set_chip_version(v) → preset RegVersion (0x42) value
class FakeSpiDevice : public hal::ISpiDevice {
public:
    static constexpr std::size_t kRegCount = 256;
    static constexpr std::uint8_t kRegVersion = 0x42;

    FakeSpiDevice() {
        regs_.fill(0x00);
        regs_[kRegVersion] = 0x12;  // default = SX1276/77/78/79
    }

    [[nodiscard]] LoRaError begin() noexcept override { return LoRaError::OK; }

    [[nodiscard]] LoRaError transfer(std::uint8_t addr,
                                     const std::uint8_t* tx,
                                     std::uint8_t* rx,
                                     std::size_t len) noexcept override {
        const bool is_write = (addr & 0x80u) != 0u;
        const std::uint8_t reg = addr & 0x7Fu;

        if (is_write) {
            if (fail_writes_) return LoRaError::SpiFailure;
            if (tx == nullptr) return LoRaError::NullArgument;
            ++write_count_;
            for (std::size_t i = 0; i < len; ++i) {
                const std::size_t idx = (reg + i) % kRegCount;
                // Dead-register simulation: drop writes after the budget runs out.
                if (dead_active_ && static_cast<std::uint8_t>(idx) == dead_reg_) {
                    if (dead_remaining_writes_ == 0u) {
                        writes_.push_back({static_cast<std::uint8_t>(idx), tx[i]});
                        continue;  // record the attempt, but don't mutate regs_
                    }
                    --dead_remaining_writes_;
                }
                regs_[idx] = tx[i];
                writes_.push_back({static_cast<std::uint8_t>(idx), tx[i]});
            }
        } else {
            if (fail_reads_) return LoRaError::SpiFailure;
            if (rx == nullptr) return LoRaError::NullArgument;
            ++read_count_;
            for (std::size_t i = 0; i < len; ++i) {
                const std::size_t idx = (reg + i) % kRegCount;
                rx[i] = regs_[idx];
            }
        }
        return LoRaError::OK;
    }

    // Setters
    void fail_writes(bool v) noexcept { fail_writes_ = v; }
    void fail_reads(bool v) noexcept  { fail_reads_  = v; }
    void set_register(std::uint8_t reg, std::uint8_t value) noexcept { regs_[reg] = value; }
    void set_chip_version(std::uint8_t v) noexcept { regs_[kRegVersion] = v; }

    /// After `count` successful writes to `reg`, every subsequent write to
    /// that register is silently dropped (bus says OK but storage unchanged).
    /// Used to simulate a chip register that has gone dead.
    void set_dead_after_writes(std::uint8_t reg, std::uint32_t count) noexcept {
        dead_reg_ = reg;
        dead_remaining_writes_ = count;
        dead_active_ = true;
    }

    // Observers
    [[nodiscard]] std::uint8_t reg(std::uint8_t addr) const noexcept { return regs_[addr]; }
    [[nodiscard]] std::uint32_t write_count() const noexcept { return write_count_; }
    [[nodiscard]] std::uint32_t read_count() const noexcept { return read_count_; }

    struct WriteEntry { std::uint8_t reg; std::uint8_t value; };
    [[nodiscard]] const std::vector<WriteEntry>& writes() const noexcept { return writes_; }
    void clear_writes() noexcept { writes_.clear(); }

private:
    std::array<std::uint8_t, kRegCount> regs_{};
    std::vector<WriteEntry> writes_{};
    std::uint32_t write_count_ = 0;
    std::uint32_t read_count_  = 0;
    bool fail_writes_ = false;
    bool fail_reads_  = false;
    std::uint8_t  dead_reg_ = 0;
    std::uint32_t dead_remaining_writes_ = 0;
    bool          dead_active_ = false;
};

}  // namespace loradriver::test
