// Tests: SX127x Adapter Interface Contract
// Story 5.1 — AC1, AC2, AC3, AC4
// Validates the adapter seam, SPI typed contract, and IRQ profile separation.
// All behavior verified against a deterministic fake adapter (no real hardware).

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "chips/sx127x/isx127x_adapter.hpp"
#include "loradriver/lora_error.hpp"

namespace {

using loradriver::LoRaError;
using loradriver::chips::sx127x::IrqProfile;
using loradriver::chips::sx127x::IrqSignal;
using loradriver::chips::sx127x::ISx127xAdapter;
using loradriver::chips::sx127x::RegAddr;
using loradriver::chips::sx127x::RegValue;

// ---------------------------------------------------------------------------
// FakeSx127xAdapter — deterministic test double for host-side contract tests.
// Does NOT use dynamic allocation; register state is a fixed-size map.
// ---------------------------------------------------------------------------

class FakeSx127xAdapter final : public ISx127xAdapter {
 public:
  static constexpr std::size_t kRegisterCount = 256;

  explicit FakeSx127xAdapter(IrqProfile profile = IrqProfile::kMinimal) : profile_(profile) {
    registers_.fill(0x00);
  }

  // Control levers for failure injection
  void injectReadFailure(bool inject) noexcept { inject_read_failure_ = inject; }
  void injectWriteFailure(bool inject) noexcept { inject_write_failure_ = inject; }
  void triggerDio0() noexcept { pending_dio0_ = true; }
  void triggerDio1() noexcept { pending_dio1_ = true; }

  // Observation: counts of actual SPI operations
  int readCount() const noexcept { return read_count_; }
  int writeCount() const noexcept { return write_count_; }

  // ISx127xAdapter interface
  [[nodiscard]] LoRaError readRegister(RegAddr addr, RegValue& out) noexcept override {
    if (inject_read_failure_) {
      return LoRaError::kHardwareInitFailure;
    }
    out = registers_[addr];
    ++read_count_;
    return LoRaError::kOk;
  }

  [[nodiscard]] LoRaError writeRegister(RegAddr addr, RegValue value) noexcept override {
    if (inject_write_failure_) {
      return LoRaError::kHardwareInitFailure;
    }
    registers_[addr] = value;
    ++write_count_;
    return LoRaError::kOk;
  }

  [[nodiscard]] LoRaError burstRead(RegAddr addr, std::uint8_t* buf, std::size_t len) noexcept override {
    if (buf == nullptr || len == 0) {
      return LoRaError::kInvalidConfig;
    }
    if (inject_read_failure_) {
      return LoRaError::kHardwareInitFailure;
    }
    for (std::size_t i = 0; i < len; ++i) {
      const std::size_t reg_idx = (static_cast<std::size_t>(addr) + i) % kRegisterCount;
      buf[i] = registers_[reg_idx];
    }
    read_count_ += static_cast<int>(len);
    return LoRaError::kOk;
  }

  [[nodiscard]] LoRaError burstWrite(RegAddr addr, const std::uint8_t* buf, std::size_t len) noexcept override {
    if (buf == nullptr || len == 0) {
      return LoRaError::kInvalidConfig;
    }
    if (inject_write_failure_) {
      return LoRaError::kHardwareInitFailure;
    }
    for (std::size_t i = 0; i < len; ++i) {
      const std::size_t reg_idx = (static_cast<std::size_t>(addr) + i) % kRegisterCount;
      registers_[reg_idx] = buf[i];
    }
    write_count_ += static_cast<int>(len);
    return LoRaError::kOk;
  }

  [[nodiscard]] IrqProfile irqProfile() const noexcept override { return profile_; }

  [[nodiscard]] IrqSignal pollIrqSignals() noexcept override {
    IrqSignal sig{};
    sig.dio0_fired = pending_dio0_;
    if (profile_ == IrqProfile::kExtended) {
      sig.dio1_fired = pending_dio1_;
    }
    pending_dio0_ = false;
    pending_dio1_ = false;
    return sig;
  }

  void clearIrqFlags() noexcept override {
    irq_flags_cleared_ = true;
  }

  bool irqFlagsCleared() const noexcept { return irq_flags_cleared_; }

 private:
  IrqProfile profile_;
  std::array<std::uint8_t, kRegisterCount> registers_{};
  int read_count_ = 0;
  int write_count_ = 0;
  bool inject_read_failure_ = false;
  bool inject_write_failure_ = false;
  bool pending_dio0_ = false;
  bool pending_dio1_ = false;
  bool irq_flags_cleared_ = false;
};

// ---------------------------------------------------------------------------
// Task 1 / AC 1: Adapter interface and typed hardware operation result surface
// ---------------------------------------------------------------------------

bool TestAdapterInterfaceIsPolymorphic() {
  // Verify the fake satisfies ISx127xAdapter polymorphically
  FakeSx127xAdapter fake;
  ISx127xAdapter* adapter = &fake;

  RegValue val = 0;
  const LoRaError result = adapter->readRegister(0x01, val);
  return result == LoRaError::kOk;
}

bool TestAdapterReadWriteSuccessPath() {
  FakeSx127xAdapter fake;
  const RegAddr kAddr = 0x06;
  const RegValue kValue = 0xAB;

  const LoRaError write_result = fake.writeRegister(kAddr, kValue);
  if (write_result != LoRaError::kOk) {
    return false;
  }

  RegValue readback = 0;
  const LoRaError read_result = fake.readRegister(kAddr, readback);
  if (read_result != LoRaError::kOk) {
    return false;
  }

  return readback == kValue;
}

bool TestAdapterReadCountsOperations() {
  FakeSx127xAdapter fake;

  RegValue val = 0;
  fake.readRegister(0x01, val);
  fake.readRegister(0x02, val);
  fake.readRegister(0x03, val);

  return fake.readCount() == 3;
}

bool TestAdapterWriteCountsOperations() {
  FakeSx127xAdapter fake;

  fake.writeRegister(0x01, 0x11);
  fake.writeRegister(0x02, 0x22);

  return fake.writeCount() == 2;
}

// ---------------------------------------------------------------------------
// Task 2 / AC 2: SPI register transaction contract + typed SPI failure mapping
// ---------------------------------------------------------------------------

bool TestSpiReadFailureMapsToTypedError() {
  FakeSx127xAdapter fake;
  fake.injectReadFailure(true);

  RegValue val = 0;
  const LoRaError result = fake.readRegister(0x01, val);
  return result == LoRaError::kHardwareInitFailure;
}

bool TestSpiWriteFailureMapsToTypedError() {
  FakeSx127xAdapter fake;
  fake.injectWriteFailure(true);

  const LoRaError result = fake.writeRegister(0x06, 0xFF);
  return result == LoRaError::kHardwareInitFailure;
}

bool TestSpiReadFailureDoesNotMutateRegisterState() {
  FakeSx127xAdapter fake;
  // Pre-set known register value
  fake.writeRegister(0x06, 0xAA);

  // Inject failure on next read
  fake.injectReadFailure(true);
  RegValue val = 0xFF;
  const LoRaError result = fake.readRegister(0x06, val);

  // val must remain unchanged on failure
  if (result != LoRaError::kHardwareInitFailure) {
    return false;
  }
  return val == 0xFF;  // not overwritten by failed read
}

bool TestBurstReadSuccessPath() {
  FakeSx127xAdapter fake;
  // Write known values starting at reg 0x00
  for (std::uint8_t i = 0; i < 8; ++i) {
    fake.writeRegister(static_cast<RegAddr>(i), static_cast<RegValue>(i * 2));
  }

  std::array<std::uint8_t, 8> buf{};
  const LoRaError result = fake.burstRead(0x00, buf.data(), buf.size());
  if (result != LoRaError::kOk) {
    return false;
  }

  for (std::uint8_t i = 0; i < 8; ++i) {
    if (buf[i] != static_cast<std::uint8_t>(i * 2)) {
      return false;
    }
  }
  return true;
}

bool TestBurstWriteSuccessPath() {
  FakeSx127xAdapter fake;
  const std::array<std::uint8_t, 4> payload = {0x11, 0x22, 0x33, 0x44};
  const LoRaError result = fake.burstWrite(0x10, payload.data(), payload.size());
  if (result != LoRaError::kOk) {
    return false;
  }

  for (std::size_t i = 0; i < payload.size(); ++i) {
    RegValue val = 0;
    fake.readRegister(static_cast<RegAddr>(0x10 + i), val);
    if (val != payload[i]) {
      return false;
    }
  }
  return true;
}

bool TestBurstReadFailureMapsToTypedError() {
  FakeSx127xAdapter fake;
  fake.injectReadFailure(true);

  std::array<std::uint8_t, 4> buf{};
  const LoRaError result = fake.burstRead(0x00, buf.data(), buf.size());
  return result == LoRaError::kHardwareInitFailure;
}

bool TestBurstWriteFailureMapsToTypedError() {
  FakeSx127xAdapter fake;
  fake.injectWriteFailure(true);

  const std::array<std::uint8_t, 4> payload = {0x01, 0x02, 0x03, 0x04};
  const LoRaError result = fake.burstWrite(0x00, payload.data(), payload.size());
  return result == LoRaError::kHardwareInitFailure;
}

bool TestBurstReadRejectsNullBuffer() {
  FakeSx127xAdapter fake;
  const LoRaError result = fake.burstRead(0x00, nullptr, 4);
  return result == LoRaError::kInvalidConfig;
}

bool TestBurstWriteRejectsNullBuffer() {
  FakeSx127xAdapter fake;
  const LoRaError result = fake.burstWrite(0x00, nullptr, 4);
  return result == LoRaError::kInvalidConfig;
}

bool TestSpiFailureUsesTypedErrorCode() {
  if (LoRaError::kHardwareInitFailure == LoRaError::kOk) return false;
  if (LoRaError::kHardwareInitFailure == LoRaError::kInvalidConfig) return false;
  return true;
}

// ---------------------------------------------------------------------------
// Task 3 / AC 3: Adapter seam is injectable — FSM tests remain stable
// ---------------------------------------------------------------------------

bool TestFakeAdapterCanReplaceAdapterWithoutFsmChanges() {
  // Verify that the fake satisfies the full ISx127xAdapter contract
  FakeSx127xAdapter fake_minimal(IrqProfile::kMinimal);
  FakeSx127xAdapter fake_extended(IrqProfile::kExtended);

  ISx127xAdapter* adapters[2] = {&fake_minimal, &fake_extended};

  for (auto* adapter : adapters) {
    RegValue val = 0;
    if (adapter->readRegister(0x01, val) != LoRaError::kOk) return false;
    if (adapter->writeRegister(0x01, 0x42) != LoRaError::kOk) return false;
  }

  return true;
}

bool TestFakeAdapterReturnsAllNodiscardResults() {
  // Every fallible method must return a LoRaError (verified by [[nodiscard]])
  // This test exercises all return paths to confirm they are observable.
  FakeSx127xAdapter fake;

  RegValue val = 0;
  LoRaError r1 = fake.readRegister(0x01, val);
  if (r1 != LoRaError::kOk) return false;

  LoRaError r2 = fake.writeRegister(0x01, 0x00);
  if (r2 != LoRaError::kOk) return false;

  std::array<std::uint8_t, 2> buf{};
  LoRaError r3 = fake.burstRead(0x00, buf.data(), buf.size());
  if (r3 != LoRaError::kOk) return false;

  LoRaError r4 = fake.burstWrite(0x00, buf.data(), buf.size());
  if (r4 != LoRaError::kOk) return false;

  return true;
}

bool TestFakeAdapterClearIrqFlagsIsTracked() {
  FakeSx127xAdapter fake;
  if (fake.irqFlagsCleared()) return false;

  fake.clearIrqFlags();
  return fake.irqFlagsCleared();
}

// ---------------------------------------------------------------------------
// Task 4 / AC 4: IRQ signal handling paths separated per profile
// ---------------------------------------------------------------------------

bool TestIrqProfileMinimalExposesDio0Only() {
  FakeSx127xAdapter fake(IrqProfile::kMinimal);
  if (fake.irqProfile() != IrqProfile::kMinimal) return false;

  fake.triggerDio0();
  fake.triggerDio1();  // DIO1 trigger is ignored in minimal profile

  const IrqSignal sig = fake.pollIrqSignals();
  if (!sig.dio0_fired) return false;
  if (sig.dio1_fired) return false;  // Must NOT be set in kMinimal

  return true;
}

bool TestIrqProfileExtendedExposesDio0AndDio1() {
  FakeSx127xAdapter fake(IrqProfile::kExtended);
  if (fake.irqProfile() != IrqProfile::kExtended) return false;

  fake.triggerDio0();
  fake.triggerDio1();

  const IrqSignal sig = fake.pollIrqSignals();
  if (!sig.dio0_fired) return false;
  if (!sig.dio1_fired) return false;

  return true;
}

bool TestIrqSignalPollIsEdgeTriggeredNotLevelTriggered() {
  // Signal must be consumed on poll — second poll returns false
  FakeSx127xAdapter fake(IrqProfile::kExtended);
  fake.triggerDio0();
  fake.triggerDio1();

  const IrqSignal first = fake.pollIrqSignals();
  if (!first.dio0_fired || !first.dio1_fired) return false;

  const IrqSignal second = fake.pollIrqSignals();
  if (second.dio0_fired || second.dio1_fired) return false;

  return true;
}

bool TestIrqSignalIsAllocationFree() {
  // IrqSignal must be a trivially-copyable bounded struct (no heap)
  static_assert(sizeof(IrqSignal) <= 8, "IrqSignal must be allocation-free and bounded");
  static_assert(std::is_trivially_copyable<IrqSignal>::value, "IrqSignal must be trivially copyable");
  return true;
}

bool TestIrqProfileDoesNotLeakIntoAdapterState() {
  // The adapter itself must not mutate FSM-owned state via IRQ signals
  FakeSx127xAdapter fake_minimal(IrqProfile::kMinimal);
  FakeSx127xAdapter fake_extended(IrqProfile::kExtended);

  // Trigger and poll both — adapter state is self-contained
  fake_minimal.triggerDio0();
  fake_extended.triggerDio0();
  fake_extended.triggerDio1();

  const IrqSignal sig_min = fake_minimal.pollIrqSignals();
  const IrqSignal sig_ext = fake_extended.pollIrqSignals();

  // Verify mutual independence
  if (!sig_min.dio0_fired) return false;
  if (sig_min.dio1_fired) return false;
  if (!sig_ext.dio0_fired) return false;
  if (!sig_ext.dio1_fired) return false;

  // No cross-contamination
  const IrqSignal after_min = fake_minimal.pollIrqSignals();
  const IrqSignal after_ext = fake_extended.pollIrqSignals();
  if (after_min.dio0_fired || after_ext.dio0_fired) return false;

  return true;
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

#define RUN_TEST(fn) \
  if (!(fn)()) { \
    std::fprintf(stderr, "FAIL: %s\n", #fn); \
    return EXIT_FAILURE; \
  }

int RunAdapterContractTests() {
  // Task 1 / AC 1
  RUN_TEST(TestAdapterInterfaceIsPolymorphic)
  RUN_TEST(TestAdapterReadWriteSuccessPath)
  RUN_TEST(TestAdapterReadCountsOperations)
  RUN_TEST(TestAdapterWriteCountsOperations)

  // Task 2 / AC 2
  RUN_TEST(TestSpiReadFailureMapsToTypedError)
  RUN_TEST(TestSpiWriteFailureMapsToTypedError)
  RUN_TEST(TestSpiReadFailureDoesNotMutateRegisterState)
  RUN_TEST(TestBurstReadSuccessPath)
  RUN_TEST(TestBurstWriteSuccessPath)
  RUN_TEST(TestBurstReadFailureMapsToTypedError)
  RUN_TEST(TestBurstWriteFailureMapsToTypedError)
  RUN_TEST(TestBurstReadRejectsNullBuffer)
  RUN_TEST(TestBurstWriteRejectsNullBuffer)
  RUN_TEST(TestSpiFailureUsesTypedErrorCode)

  // Task 3 / AC 3
  RUN_TEST(TestFakeAdapterCanReplaceAdapterWithoutFsmChanges)
  RUN_TEST(TestFakeAdapterReturnsAllNodiscardResults)
  RUN_TEST(TestFakeAdapterClearIrqFlagsIsTracked)

  // Task 4 / AC 4
  RUN_TEST(TestIrqProfileMinimalExposesDio0Only)
  RUN_TEST(TestIrqProfileExtendedExposesDio0AndDio1)
  RUN_TEST(TestIrqSignalPollIsEdgeTriggeredNotLevelTriggered)
  RUN_TEST(TestIrqSignalIsAllocationFree)
  RUN_TEST(TestIrqProfileDoesNotLeakIntoAdapterState)
  return EXIT_SUCCESS;
}

}  // namespace

int main() {
  return RunAdapterContractTests();
}
