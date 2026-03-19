#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "loradriver/lora_error.hpp"

namespace loradriver::chips::sx127x {

// ---------------------------------------------------------------------------
// SPI primitive types
// ---------------------------------------------------------------------------

/// 8-bit SPI register address (7-bit address; MSB used for R/W direction by SPI layer).
using RegAddr = std::uint8_t;

/// 8-bit SPI register value.
using RegValue = std::uint8_t;

// ---------------------------------------------------------------------------
// IRQ profile — maps to RadioConfig::DioRouting without leaking it here
// ---------------------------------------------------------------------------

/// IRQ routing profile matching the hardware wiring.
/// kMinimal  = DIO0 only  (RadioConfig::DioRouting::kDio0Only)
/// kExtended = DIO0+DIO1  (RadioConfig::DioRouting::kDio0Dio1)
enum class IrqProfile : std::uint8_t {
  kMinimal = 0,
  kExtended = 1,
};

// ---------------------------------------------------------------------------
// IrqSignal — allocation-free, bounded IRQ capture structure.
// Designed for transfer from ISR context to processing context.
// Must remain trivially copyable and ≤ 8 bytes.
// ---------------------------------------------------------------------------

struct IrqSignal {
  bool dio0_fired = false;  ///< Set when DIO0 IRQ was captured (TX done / RX done / CAD done)
  bool dio1_fired = false;  ///< Set when DIO1 IRQ was captured (RX timeout in extended profile)
};

static_assert(sizeof(IrqSignal) <= 8, "IrqSignal must remain bounded and allocation-free");
static_assert(std::is_trivially_copyable<IrqSignal>::value, "IrqSignal must be trivially copyable");

// ---------------------------------------------------------------------------
// ISx127xAdapter — SPI/register IO boundary interface
//
// Contract invariants:
//   - All fallible methods return [[nodiscard]] LoRaError; no exceptions.
//   - Adapter must NOT mutate FSM-owned state directly.
//   - ISR-safety: clearIrqFlags() and pollIrqSignals() must be allocation-free.
//   - IRQ profile is fixed at construction; adapter does not switch profiles.
// ---------------------------------------------------------------------------

class ISx127xAdapter {
 public:
  virtual ~ISx127xAdapter() = default;

  // Disallow copy/move — adapters are injected by pointer/reference.
  ISx127xAdapter(const ISx127xAdapter&) = delete;
  ISx127xAdapter& operator=(const ISx127xAdapter&) = delete;
  ISx127xAdapter(ISx127xAdapter&&) = delete;
  ISx127xAdapter& operator=(ISx127xAdapter&&) = delete;

  // -------------------------------------------------------------------------
  // SPI register operations
  // -------------------------------------------------------------------------

  /// Read one register byte.
  /// Returns kHardwareInitFailure on MISO timeout or invalid response.
  [[nodiscard]] virtual LoRaError readRegister(RegAddr addr, RegValue& out) noexcept = 0;

  /// Write one register byte.
  /// Returns kHardwareInitFailure on SPI bus error.
  [[nodiscard]] virtual LoRaError writeRegister(RegAddr addr, RegValue value) noexcept = 0;

  /// Burst read: read `len` consecutive bytes starting at `addr`.
  /// Returns kInvalidConfig if buf is null or len is 0.
  /// Returns kHardwareInitFailure on SPI bus error.
  [[nodiscard]] virtual LoRaError burstRead(RegAddr addr, std::uint8_t* buf, std::size_t len) noexcept = 0;

  /// Burst write: write `len` consecutive bytes starting at `addr`.
  /// Returns kInvalidConfig if buf is null or len is 0.
  /// Returns kHardwareInitFailure on SPI bus error.
  [[nodiscard]] virtual LoRaError burstWrite(RegAddr addr, const std::uint8_t* buf, std::size_t len) noexcept = 0;

  // -------------------------------------------------------------------------
  // IRQ signal handling — profile-separated, allocation-free
  // -------------------------------------------------------------------------

  /// Returns the IRQ routing profile this adapter was configured with.
  [[nodiscard]] virtual IrqProfile irqProfile() const noexcept = 0;

  /// Poll and consume pending IRQ signals captured from ISR context.
  /// Edge-triggered: signals are cleared after each poll call.
  /// In kMinimal profile, IrqSignal::dio1_fired is always false.
  [[nodiscard]] virtual IrqSignal pollIrqSignals() noexcept = 0;

  /// Clear chip-level IRQ flags (write-to-clear register or equivalent).
  /// Must be allocation-free and safe to call from non-ISR processing context.
  virtual void clearIrqFlags() noexcept = 0;

 protected:
  ISx127xAdapter() = default;
};

}  // namespace loradriver::chips::sx127x
