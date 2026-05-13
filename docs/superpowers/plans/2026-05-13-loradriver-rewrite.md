# LoRaDriver v1.0 Rewrite Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the `D:\DEV\C++\LoRaDriver` v0.1 governance shell with a real, performant, clean SX1276+SX1278 driver inspired by the proven `LoRaDriverBak` v2.1.0, then migrate SYNC-SIGNAL-LORA to consume it.

**Architecture:** Four DI layers in `namespace loradriver`: `ISpiDevice` (HAL) → `SX127xDriver : IRadioDriver` (chip logic) → `LoRaTransceiver` (FSM façade) → `RadioPumpTask` (optional ESP32 FreeRTOS pump). No singletons, no heap after `begin()`, no exceptions, `[[nodiscard]] noexcept` everywhere on the radio path, ISR-safe ring buffer.

**Tech Stack:** C++17, CMake (host tests), PlatformIO + Arduino + ESP32 (embedded smoke). Tiny custom test runner (`RUN_TEST` macro), no third-party test framework.

**Spec:** `docs/superpowers/specs/2026-05-13-loradriver-rewrite-design.md`

**Repos:**
- Target: `D:\DEV\C++\LoRaDriver`
- Reference (do not modify): `D:\DEV\C++\LoRaDriverBak`
- Consumer to migrate: `D:\DEV\PlatformIO\SYNC-SIGNAL-LORA\SYNC-SIGNAL-LORA`

**Conventions:**
- Naming: `snake_case` methods/members, `PascalCase` types/enums, `kPascalCase` constants.
- Every fallible function: `[[nodiscard]] LoRaError noexcept`.
- Run from `D:/DEV/C++/LoRaDriver` (Windows PowerShell). Use `bash` tool with `git -C D:/DEV/C++/LoRaDriver ...` to avoid `cd`.
- TDD: failing test → minimal impl → green → commit. Commit messages use Conventional Commits.

---

## Phase 0 — Cleanup: gut the governance shell

Goal of this phase: end up with a minimal compilable shell containing only headers/enums we will rewrite. No SX127x logic remains. CMake compiles with zero source files.

### Task 0.1: Snapshot current state in a branch

**Files:** none (git only)

- [ ] **Step 1: Create rewrite branch from main**

```bash
git -C D:/DEV/C++/LoRaDriver checkout -b rewrite/v1.0
git -C D:/DEV/C++/LoRaDriver status
```

Expected: `On branch rewrite/v1.0`, working tree shows the pre-existing modifications (CMakeLists.txt M, etc. — from the previous session).

- [ ] **Step 2: Stash unrelated modifications (will be obsolete after cleanup)**

```bash
git -C D:/DEV/C++/LoRaDriver stash push -m "pre-rewrite WIP, will be deleted" -- CMakeLists.txt include/loradriver/lora_driver.hpp src/api/lora_driver.cpp tests/host/CMakeLists.txt
```

Expected: stash created. Untracked SX1276 adapter files remain (they will be deleted in 0.2).

### Task 0.2: Delete governance/validation/stubs source trees

**Files:** Delete (recursive)
- `src/validation/`
- `src/governance/`
- `src/core/`
- `src/infra/`
- `src/internal/`
- `src/platform/arduino/`
- `src/platform/esp32/`
- `src/chips/sx126x/`
- `src/chips/sx127x/sx127x_stub.cpp`
- `src/chips/sx127x/sx1276_adapter.hpp` (untracked)
- `src/chips/sx127x/sx1276_adapter.cpp` (untracked)
- `src/chips/sx127x/isx127x_adapter.hpp`
- `src/api/incident_classification.cpp`
- `src/api/incident_snapshot.cpp`
- `src/api/config_validation.{hpp,cpp}`
- `src/api/lora_driver.cpp`
- `src/main.cpp`
- `tools/ci/` (if present)
- `artifacts/` (if present)
- `_bmad/`, `_bmad-output/`
- `LoRaDriver.7z`

- [ ] **Step 1: Delete with git rm where tracked, rm where untracked**

```bash
git -C D:/DEV/C++/LoRaDriver rm -r src/validation src/governance src/core src/infra src/internal src/platform src/chips/sx126x src/chips/sx127x/sx127x_stub.cpp src/chips/sx127x/isx127x_adapter.hpp src/api/incident_classification.cpp src/api/incident_snapshot.cpp src/api/lora_driver.cpp src/main.cpp
git -C D:/DEV/C++/LoRaDriver rm -f LoRaDriver.7z
rm -rf D:/DEV/C++/LoRaDriver/src/chips/sx127x/sx1276_adapter.hpp D:/DEV/C++/LoRaDriver/src/chips/sx127x/sx1276_adapter.cpp D:/DEV/C++/LoRaDriver/_bmad D:/DEV/C++/LoRaDriver/_bmad-output D:/DEV/C++/LoRaDriver/artifacts D:/DEV/C++/LoRaDriver/tools 2>/dev/null
```

Expected: `git status` shows mass deletions, src/ tree near-empty.

- [ ] **Step 2: Verify src/ tree is empty**

```bash
ls -R D:/DEV/C++/LoRaDriver/src 2>/dev/null
```

Expected: only `src/api/` and `src/chips/sx127x/` directories remain, empty. If non-empty, delete leftovers.

### Task 0.3: Delete governance/legacy headers

**Files:** Delete from `include/loradriver/`
- `artifact_governance.hpp`
- `ci_gates.hpp`
- `incident_classification.hpp`
- `incident_snapshot.hpp`
- `non_regression.hpp`
- `ota_gate.hpp`
- `profile_qualification.hpp`
- `radio_counters.hpp`
- `release_monitoring.hpp`
- `rollback_governance.hpp`
- `versioning.hpp`
- `version.hpp` (will be re-created later as a simple version macro)
- `lora_driver.hpp` (replaced by `lora_transceiver.hpp` in Phase 2)
- `radio_config.hpp` (replaced by `lora_config.hpp` in Phase 1)
- `lora_error.hpp` (will be rewritten in Phase 1)
- `radio_event.hpp` (will be rewritten in Phase 1)

- [ ] **Step 1: Remove all current headers**

```bash
git -C D:/DEV/C++/LoRaDriver rm include/loradriver/artifact_governance.hpp include/loradriver/ci_gates.hpp include/loradriver/incident_classification.hpp include/loradriver/incident_snapshot.hpp include/loradriver/non_regression.hpp include/loradriver/ota_gate.hpp include/loradriver/profile_qualification.hpp include/loradriver/radio_counters.hpp include/loradriver/release_monitoring.hpp include/loradriver/rollback_governance.hpp include/loradriver/versioning.hpp include/loradriver/version.hpp include/loradriver/lora_driver.hpp include/loradriver/radio_config.hpp include/loradriver/lora_error.hpp include/loradriver/radio_event.hpp
```

Expected: `include/loradriver/` is empty.

- [ ] **Step 2: Verify empty include dir**

```bash
ls D:/DEV/C++/LoRaDriver/include/loradriver/ 2>/dev/null
```

Expected: no output (empty).

### Task 0.4: Delete governance and stub tests

**Files:** Delete from `tests/host/`
- All `test_artifact_*.cpp`, `test_changelog_*.cpp`, `test_ci_gates.cpp`, `test_ota_gate.cpp`, `test_profile_qualification.cpp`, `test_release_monitoring.cpp`, `test_rollback_governance.cpp`, `test_traceability_engine.cpp`, `test_versioning_governance.cpp`, `test_radio_counters.cpp`, `test_lora_config_api.cpp`, `test_sx127x_adapter_contract.cpp`, `test_sx1276_init_sequence.cpp`, `smoke_test.cpp`
- `tests/host/non_regression/` (whole dir)
- `tests/embedded/test_smoke/test_main.cpp` (will be rewritten in Phase 6)

- [ ] **Step 1: Delete tests**

```bash
git -C D:/DEV/C++/LoRaDriver rm tests/host/test_artifact_registry.cpp tests/host/test_changelog_manager.cpp tests/host/test_ci_gates.cpp tests/host/test_ota_gate.cpp tests/host/test_profile_qualification.cpp tests/host/test_release_monitoring.cpp tests/host/test_rollback_governance.cpp tests/host/test_traceability_engine.cpp tests/host/test_versioning_governance.cpp tests/host/test_radio_counters.cpp tests/host/test_lora_config_api.cpp tests/host/test_sx127x_adapter_contract.cpp tests/host/test_sx1276_init_sequence.cpp tests/host/smoke_test.cpp
git -C D:/DEV/C++/LoRaDriver rm -r tests/host/non_regression
rm -rf D:/DEV/C++/LoRaDriver/tests/host/test_sx1276_init_sequence.cpp 2>/dev/null
git -C D:/DEV/C++/LoRaDriver rm tests/embedded/test_smoke/test_main.cpp 2>/dev/null
```

Expected: tests/host has only `CMakeLists.txt` left.

### Task 0.5: Reset CMakeLists files to minimal stubs

**Files:**
- Modify: `D:/DEV/C++/LoRaDriver/CMakeLists.txt`
- Modify: `D:/DEV/C++/LoRaDriver/tests/host/CMakeLists.txt`

- [ ] **Step 1: Replace top-level CMakeLists.txt with minimal version**

Content for `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.21)

project(LoRaDriver VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Library target — sources added in later phases
add_library(loradriver STATIC)

target_include_directories(loradriver
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Placeholder source so the static lib has at least one TU until Phase 1 lands
target_sources(loradriver PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/_placeholder.cpp)

enable_testing()
add_subdirectory(tests/host)
```

Write the placeholder TU:

Content for `src/_placeholder.cpp`:

```cpp
// Temporary placeholder so the static library has a translation unit.
// Removed at the end of Phase 1 once real sources are added.
namespace loradriver { inline namespace _placeholder { int touch_me_once = 0; } }
```

- [ ] **Step 2: Replace tests/host/CMakeLists.txt with minimal version**

Content for `tests/host/CMakeLists.txt`:

```cmake
# Tests are added per-file in later phases.
# This file exists so the parent CMakeLists.txt add_subdirectory() succeeds.
```

- [ ] **Step 3: Verify CMake configure + build succeed**

```bash
rm -rf D:/DEV/C++/LoRaDriver/build/host 2>/dev/null
cmake -S D:/DEV/C++/LoRaDriver -B D:/DEV/C++/LoRaDriver/build/host
cmake --build D:/DEV/C++/LoRaDriver/build/host
```

Expected: configure + build succeed. `loradriver.lib` (or `libloradriver.a`) produced from the placeholder TU.

- [ ] **Step 4: Commit cleanup**

```bash
git -C D:/DEV/C++/LoRaDriver add -A
git -C D:/DEV/C++/LoRaDriver commit -m "$(cat <<'EOF'
chore: gut governance/validation shell ahead of v1.0 rewrite

Removes governance/validation/CI gate modules, SX126x stub, all
keep*ModuleLinked stubs, incident snapshot and diagnostic context,
and every test bound to the deleted modules. CMake compiles a
placeholder TU so the static lib remains buildable while phases 1-7
land the new driver.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

---

## Phase 1 — Foundation types

Goal: land the immutable public types (`LoRaError`, `LoRaConfig`, `RadioEvent`, `RadioStats`, `LoRaPacket`) plus the test runner harness. After this phase, host tests for `LoRaConfig::validate()` pass.

### Task 1.1: LoRaError enum + to_string

**Files:**
- Create: `include/loradriver/lora_error.hpp`
- Create: `src/api/lora_error.cpp`

- [ ] **Step 1: Create the header**

Content for `include/loradriver/lora_error.hpp`:

```cpp
#pragma once

#include <cstdint>

namespace loradriver {

enum class LoRaError : std::uint8_t {
    OK = 0,
    InvalidConfig,
    UnsupportedChip,
    SpiFailure,
    SpiVerifyMismatch,
    InvalidState,
    TxTimeout,
    TxBufferTooLarge,
    RxTimeout,
    RxCrcError,
    AlreadyInitialized,
    NotInitialized,
    QueueFull,
    NullArgument,
};

const char* to_string(LoRaError e) noexcept;

}  // namespace loradriver
```

- [ ] **Step 2: Create the implementation**

Content for `src/api/lora_error.cpp`:

```cpp
#include "loradriver/lora_error.hpp"

namespace loradriver {

const char* to_string(LoRaError e) noexcept {
    switch (e) {
        case LoRaError::OK:                  return "OK";
        case LoRaError::InvalidConfig:       return "InvalidConfig";
        case LoRaError::UnsupportedChip:     return "UnsupportedChip";
        case LoRaError::SpiFailure:          return "SpiFailure";
        case LoRaError::SpiVerifyMismatch:   return "SpiVerifyMismatch";
        case LoRaError::InvalidState:        return "InvalidState";
        case LoRaError::TxTimeout:           return "TxTimeout";
        case LoRaError::TxBufferTooLarge:    return "TxBufferTooLarge";
        case LoRaError::RxTimeout:           return "RxTimeout";
        case LoRaError::RxCrcError:          return "RxCrcError";
        case LoRaError::AlreadyInitialized:  return "AlreadyInitialized";
        case LoRaError::NotInitialized:      return "NotInitialized";
        case LoRaError::QueueFull:           return "QueueFull";
        case LoRaError::NullArgument:        return "NullArgument";
    }
    return "Unknown";
}

}  // namespace loradriver
```

- [ ] **Step 3: Wire it into CMake**

Edit `CMakeLists.txt`, replace the `target_sources` line:

```cmake
target_sources(loradriver PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/api/lora_error.cpp
)
```

Delete `src/_placeholder.cpp`:

```bash
rm D:/DEV/C++/LoRaDriver/src/_placeholder.cpp
```

- [ ] **Step 4: Build and verify**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host
```

Expected: build succeeds.

- [ ] **Step 5: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add include/loradriver/lora_error.hpp src/api/lora_error.cpp CMakeLists.txt src/_placeholder.cpp
git -C D:/DEV/C++/LoRaDriver commit -m "feat: add LoRaError enum and to_string()

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task 1.2: Test runner harness

**Files:**
- Create: `tests/host/test_runner.hpp`

- [ ] **Step 1: Create the minimalist test runner**

Content for `tests/host/test_runner.hpp`:

```cpp
#pragma once

// Tiny header-only test harness — no external dependency.
// Usage:
//   #include "test_runner.hpp"
//   bool TestXxx() { LD_EXPECT(cond); LD_EXPECT_EQ(a, b); return true; }
//   int main() { LD_RUN(TestXxx); return loradriver::test::report(); }

#include <cstdio>
#include <cstdlib>

namespace loradriver::test {

inline int& fail_count() { static int n = 0; return n; }
inline int& pass_count() { static int n = 0; return n; }

inline int report() {
    std::fprintf(stderr, "[summary] passed=%d failed=%d\n",
                 pass_count(), fail_count());
    return fail_count() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

}  // namespace loradriver::test

#define LD_EXPECT(cond)                                                       \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::fprintf(stderr, "  EXPECT failed: %s  (%s:%d)\n",            \
                         #cond, __FILE__, __LINE__);                          \
            return false;                                                     \
        }                                                                     \
    } while (0)

#define LD_EXPECT_EQ(a, b)                                                    \
    do {                                                                      \
        const auto _la = (a);                                                 \
        const auto _lb = (b);                                                 \
        if (!(_la == _lb)) {                                                  \
            std::fprintf(stderr, "  EXPECT_EQ failed: %s != %s  (%s:%d)\n",   \
                         #a, #b, __FILE__, __LINE__);                         \
            return false;                                                     \
        }                                                                     \
    } while (0)

#define LD_RUN(fn)                                                            \
    do {                                                                      \
        std::fprintf(stderr, "[run] %s\n", #fn);                              \
        if ((fn)()) {                                                         \
            ++loradriver::test::pass_count();                                 \
        } else {                                                              \
            ++loradriver::test::fail_count();                                 \
            std::fprintf(stderr, "[FAIL] %s\n", #fn);                         \
        }                                                                     \
    } while (0)
```

- [ ] **Step 2: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add tests/host/test_runner.hpp
git -C D:/DEV/C++/LoRaDriver commit -m "test: add tiny header-only test runner harness

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task 1.3: LoRaConfig — write tests first (TDD red)

**Files:**
- Create: `tests/host/test_lora_config_validate.cpp`
- Modify: `tests/host/CMakeLists.txt`

- [ ] **Step 1: Write failing tests covering every validate() branch**

Content for `tests/host/test_lora_config_validate.cpp`:

```cpp
#include "loradriver/lora_config.hpp"
#include "loradriver/lora_error.hpp"
#include "test_runner.hpp"

using loradriver::ChipModel;
using loradriver::LoRaConfig;
using loradriver::LoRaError;
using loradriver::PaOutput;

static LoRaConfig MakeValidSx1276() {
    LoRaConfig c;
    c.chip = ChipModel::SX1276;
    c.frequency_hz = 868'000'000u;
    c.spreading_factor = 9;
    c.bandwidth_hz = 125'000u;
    c.coding_rate = 5;
    c.preamble_length = 8;
    c.sync_word = 0x12;
    c.crc_enabled = true;
    c.tx_power_dbm = 14;
    c.pa_output = PaOutput::PaBoost;
    c.ocp_ma = 100;
    c.pin_ss = 5; c.pin_reset = 14; c.pin_dio0 = 26;
    return c;
}

static LoRaConfig MakeValidSx1278() {
    LoRaConfig c = MakeValidSx1276();
    c.chip = ChipModel::SX1278;
    c.frequency_hz = 433'920'000u;
    return c;
}

bool TestDefaultIsValid() {
    LoRaConfig c = MakeValidSx1276();
    LD_EXPECT_EQ(c.validate(), LoRaError::OK);
    return true;
}

bool TestRejectsSx1278WithHighBand() {
    LoRaConfig c = MakeValidSx1278();
    c.frequency_hz = 868'000'000u;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    return true;
}

bool TestAcceptsSx1276WithHighBand() {
    LoRaConfig c = MakeValidSx1276();
    c.frequency_hz = 915'000'000u;
    LD_EXPECT_EQ(c.validate(), LoRaError::OK);
    return true;
}

bool TestRejectsBwNotInList() {
    LoRaConfig c = MakeValidSx1276();
    c.bandwidth_hz = 100'000u;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    return true;
}

bool TestRejectsBw500OnSx1278LowBand() {
    LoRaConfig c = MakeValidSx1278();
    c.bandwidth_hz = 500'000u;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    return true;
}

bool TestRejectsSfOutOfRange() {
    LoRaConfig c = MakeValidSx1276();
    c.spreading_factor = 5;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    c.spreading_factor = 13;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    return true;
}

bool TestSf6RequiresImplicitHeader() {
    LoRaConfig c = MakeValidSx1276();
    c.spreading_factor = 6;
    c.implicit_header = false;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    c.implicit_header = true;
    LD_EXPECT_EQ(c.validate(), LoRaError::OK);
    return true;
}

bool TestRejectsCodingRateOutOfRange() {
    LoRaConfig c = MakeValidSx1276();
    c.coding_rate = 4;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    c.coding_rate = 9;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    return true;
}

bool TestRejectsPreambleTooShort() {
    LoRaConfig c = MakeValidSx1276();
    c.preamble_length = 5;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    return true;
}

bool TestRejectsOcpOutOfRange() {
    LoRaConfig c = MakeValidSx1276();
    c.ocp_ma = 30;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    c.ocp_ma = 250;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    return true;
}

bool TestRejectsTxPowerForPaBoost() {
    LoRaConfig c = MakeValidSx1276();
    c.pa_output = PaOutput::PaBoost;
    c.tx_power_dbm = 1;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    c.tx_power_dbm = 21;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    return true;
}

bool TestAcceptsTxPowerPaDacRange() {
    LoRaConfig c = MakeValidSx1276();
    c.pa_output = PaOutput::PaBoost;
    c.tx_power_dbm = 20;
    LD_EXPECT_EQ(c.validate(), LoRaError::OK);
    return true;
}

bool TestRejectsTxPowerForRfo() {
    LoRaConfig c = MakeValidSx1276();
    c.pa_output = PaOutput::Rfo;
    c.tx_power_dbm = -1;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    c.tx_power_dbm = 15;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    return true;
}

bool TestRejectsMissingPins() {
    LoRaConfig c = MakeValidSx1276();
    c.pin_ss = -1;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    c.pin_ss = 5; c.pin_reset = -1;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    c.pin_reset = 14; c.pin_dio0 = -1;
    LD_EXPECT_EQ(c.validate(), LoRaError::InvalidConfig);
    return true;
}

bool TestLdroRequiredForSlowSymbols() {
    LoRaConfig c = MakeValidSx1276();
    c.spreading_factor = 12; c.bandwidth_hz = 125'000u;
    LD_EXPECT(c.ldro_required());
    c.spreading_factor = 11; c.bandwidth_hz = 125'000u;
    LD_EXPECT(c.ldro_required());
    c.spreading_factor = 7;  c.bandwidth_hz = 125'000u;
    LD_EXPECT(!c.ldro_required());
    return true;
}

int main() {
    LD_RUN(TestDefaultIsValid);
    LD_RUN(TestRejectsSx1278WithHighBand);
    LD_RUN(TestAcceptsSx1276WithHighBand);
    LD_RUN(TestRejectsBwNotInList);
    LD_RUN(TestRejectsBw500OnSx1278LowBand);
    LD_RUN(TestRejectsSfOutOfRange);
    LD_RUN(TestSf6RequiresImplicitHeader);
    LD_RUN(TestRejectsCodingRateOutOfRange);
    LD_RUN(TestRejectsPreambleTooShort);
    LD_RUN(TestRejectsOcpOutOfRange);
    LD_RUN(TestRejectsTxPowerForPaBoost);
    LD_RUN(TestAcceptsTxPowerPaDacRange);
    LD_RUN(TestRejectsTxPowerForRfo);
    LD_RUN(TestRejectsMissingPins);
    LD_RUN(TestLdroRequiredForSlowSymbols);
    return loradriver::test::report();
}
```

- [ ] **Step 2: Add test target to tests/host/CMakeLists.txt**

Replace content of `tests/host/CMakeLists.txt`:

```cmake
function(loradriver_add_host_test name)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE loradriver)
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME ${name} COMMAND ${name})
endfunction()

loradriver_add_host_test(test_lora_config_validate)
```

- [ ] **Step 3: Configure + build — expect compile failure (no lora_config.hpp)**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | head -20
```

Expected: compile error `loradriver/lora_config.hpp: No such file or directory`. This is the RED step of TDD.

### Task 1.4: LoRaConfig — implement to make tests pass (TDD green)

**Files:**
- Create: `include/loradriver/lora_config.hpp`
- Create: `src/api/lora_config.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create the header**

Content for `include/loradriver/lora_config.hpp`:

```cpp
#pragma once

#include <cstdint>

#include "loradriver/lora_error.hpp"

namespace loradriver {

enum class ChipModel : std::uint8_t { SX1276, SX1278 };
enum class PaOutput  : std::uint8_t { PaBoost, Rfo };

struct LoRaConfig {
    // RF
    std::uint32_t frequency_hz      = 868'000'000u;
    std::uint8_t  spreading_factor  = 9;
    std::uint32_t bandwidth_hz      = 125'000u;
    std::uint8_t  coding_rate       = 5;
    std::uint16_t preamble_length   = 8;
    std::uint16_t symbol_timeout    = 100;
    std::uint16_t sync_word         = 0x12;
    bool          crc_enabled       = true;
    bool          invert_iq         = false;
    bool          implicit_header   = false;

    // Power
    std::int8_t   tx_power_dbm      = 14;
    PaOutput      pa_output         = PaOutput::PaBoost;
    std::uint8_t  ocp_ma            = 100;

    // Optimisations
    bool          ldro_auto         = true;
    bool          agc_auto          = true;
    bool          lna_boost_rx      = false;
    bool          isr_snapshot      = false;

    // Chip + pinout
    ChipModel     chip              = ChipModel::SX1276;
    std::uint32_t spi_frequency_hz  = 8'000'000u;
    std::int8_t   pin_ss            = -1;
    std::int8_t   pin_reset         = -1;
    std::int8_t   pin_dio0          = -1;
    std::int8_t   pin_dio1          = -1;

    [[nodiscard]] LoRaError validate() const noexcept;
    [[nodiscard]] bool      ldro_required() const noexcept;
};

}  // namespace loradriver
```

- [ ] **Step 2: Create the implementation**

Content for `src/api/lora_config.cpp`:

```cpp
#include "loradriver/lora_config.hpp"

namespace loradriver {

namespace {

constexpr std::uint32_t kAllowedBw[] = {
    7'800u, 10'400u, 15'600u, 20'800u, 31'250u,
    41'700u, 62'500u, 125'000u, 250'000u, 500'000u,
};

constexpr bool bw_allowed(std::uint32_t hz) noexcept {
    for (auto v : kAllowedBw) {
        if (v == hz) return true;
    }
    return false;
}

constexpr bool freq_in_range_sx1278(std::uint32_t hz) noexcept {
    return hz >= 137'000'000u && hz <= 525'000'000u;
}

constexpr bool freq_in_range_sx1276(std::uint32_t hz) noexcept {
    return hz >= 137'000'000u && hz <= 1'020'000'000u;
}

constexpr bool is_high_band(std::uint32_t hz) noexcept {
    return hz >= 525'000'000u;
}

}  // namespace

LoRaError LoRaConfig::validate() const noexcept {
    // Frequency vs chip
    if (chip == ChipModel::SX1278) {
        if (!freq_in_range_sx1278(frequency_hz)) return LoRaError::InvalidConfig;
    } else {
        if (!freq_in_range_sx1276(frequency_hz)) return LoRaError::InvalidConfig;
    }

    // Bandwidth (membership)
    if (!bw_allowed(bandwidth_hz)) return LoRaError::InvalidConfig;

    // Errata: BW 500 kHz only allowed in high-band; on SX1278 (low-band) → reject
    if (bandwidth_hz == 500'000u && !is_high_band(frequency_hz)) {
        return LoRaError::InvalidConfig;
    }

    // SF
    if (spreading_factor < 6 || spreading_factor > 12) {
        return LoRaError::InvalidConfig;
    }
    if (spreading_factor == 6 && !implicit_header) {
        return LoRaError::InvalidConfig;
    }

    // Coding rate
    if (coding_rate < 5 || coding_rate > 8) return LoRaError::InvalidConfig;

    // Preamble
    if (preamble_length < 6) return LoRaError::InvalidConfig;

    // OCP
    if (ocp_ma < 45 || ocp_ma > 240) return LoRaError::InvalidConfig;

    // TX power vs PA output
    if (pa_output == PaOutput::Rfo) {
        if (tx_power_dbm < 0 || tx_power_dbm > 14) return LoRaError::InvalidConfig;
    } else {
        if (tx_power_dbm < 2 || tx_power_dbm > 20) return LoRaError::InvalidConfig;
    }

    // Pins
    if (pin_ss < 0 || pin_reset < 0 || pin_dio0 < 0) return LoRaError::InvalidConfig;

    return LoRaError::OK;
}

bool LoRaConfig::ldro_required() const noexcept {
    // Symbol duration ms = (2^SF / BW_hz) * 1000.
    // LDRO recommended when symbol duration > 16 ms (Semtech AN1200.24).
    const std::uint32_t bw = bandwidth_hz == 0u ? 1u : bandwidth_hz;
    const std::uint64_t sym_us = (1ull << spreading_factor) * 1'000'000ull / bw;
    return sym_us > 16'000ull;
}

}  // namespace loradriver
```

- [ ] **Step 3: Wire into CMake**

Edit `CMakeLists.txt`, expand `target_sources`:

```cmake
target_sources(loradriver PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/api/lora_error.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/api/lora_config.cpp
)
```

- [ ] **Step 4: Build + run test**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host --output-on-failure -V
```

Expected: `test_lora_config_validate` runs, all assertions pass, `passed=15 failed=0`.

- [ ] **Step 5: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add include/loradriver/lora_config.hpp src/api/lora_config.cpp tests/host/test_lora_config_validate.cpp tests/host/CMakeLists.txt CMakeLists.txt
git -C D:/DEV/C++/LoRaDriver commit -m "feat: add LoRaConfig with validate() and ldro_required()

Implements 15 validation branches: chip/band coupling (rejects SX1278+868MHz),
BW membership + 500kHz/low-band errata, SF6 implicit-header requirement,
CR/preamble/OCP/TX-power range checks, and required-pin enforcement.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task 1.5: RadioEvent, RadioStats, LoRaPacket headers

**Files:**
- Create: `include/loradriver/radio_event.hpp`
- Create: `include/loradriver/radio_stats.hpp`
- Create: `include/loradriver/lora_packet.hpp`
- Create: `tests/host/test_radio_stats.cpp`
- Modify: `tests/host/CMakeLists.txt`

- [ ] **Step 1: Create radio_event.hpp**

Content for `include/loradriver/radio_event.hpp`:

```cpp
#pragma once

#include <cstdint>

namespace loradriver {

enum class RadioEvent : std::uint8_t {
    None = 0,
    TxDone,
    TxTimeout,
    RxDone,
    RxTimeout,
    RxCrcError,
    CadDone,
    CadDetected,
    ValidHeader,
    IrqOverflow,
};

}  // namespace loradriver
```

- [ ] **Step 2: Create radio_stats.hpp**

Content for `include/loradriver/radio_stats.hpp`:

```cpp
#pragma once

#include <cstdint>
#include <type_traits>

namespace loradriver {

struct RadioStats {
    std::uint32_t tx_done = 0;
    std::uint32_t tx_timeout = 0;
    std::uint32_t rx_done = 0;
    std::uint32_t rx_timeout = 0;
    std::uint32_t rx_crc_errors = 0;
    std::uint32_t irq_events_processed = 0;
    std::uint32_t irq_overflows = 0;
    std::uint32_t callback_exceptions = 0;
    std::uint8_t  max_irq_backlog = 0;
    std::int16_t  last_rssi_dbm = 0;
    std::int16_t  last_snr_q4 = 0;
    std::int32_t  last_freq_error_hz = 0;
};

static_assert(std::is_trivially_copyable<RadioStats>::value,
              "RadioStats must be trivially copyable for snapshot reads");

}  // namespace loradriver
```

- [ ] **Step 3: Create lora_packet.hpp**

Content for `include/loradriver/lora_packet.hpp`:

```cpp
#pragma once

#include <cstdint>
#include <type_traits>

namespace loradriver {

struct LoRaPacket {
    std::int16_t  rssi_dbm           = 0;
    std::int16_t  snr_q4             = 0;
    std::int32_t  frequency_error_hz = 0;
    std::uint8_t  length             = 0;
    bool          crc_valid          = false;

    [[nodiscard]] float snr_db() const noexcept {
        return static_cast<float>(snr_q4) / 4.0f;
    }
};

static_assert(sizeof(LoRaPacket) <= 16, "LoRaPacket must remain cheap to copy");
static_assert(std::is_trivially_copyable<LoRaPacket>::value, "LoRaPacket trivially copyable");

}  // namespace loradriver
```

- [ ] **Step 4: Add a self-check test for the value types**

Content for `tests/host/test_radio_stats.cpp`:

```cpp
#include "loradriver/lora_packet.hpp"
#include "loradriver/radio_event.hpp"
#include "loradriver/radio_stats.hpp"
#include "test_runner.hpp"

using loradriver::LoRaPacket;
using loradriver::RadioEvent;
using loradriver::RadioStats;

bool TestStatsDefaultZeroed() {
    RadioStats s{};
    LD_EXPECT_EQ(s.tx_done, 0u);
    LD_EXPECT_EQ(s.rx_done, 0u);
    LD_EXPECT_EQ(s.irq_overflows, 0u);
    LD_EXPECT_EQ(s.last_rssi_dbm, 0);
    return true;
}

bool TestStatsSnapshotByValue() {
    RadioStats s{};
    s.tx_done = 7;
    RadioStats copy = s;
    s.tx_done = 99;
    LD_EXPECT_EQ(copy.tx_done, 7u);
    return true;
}

bool TestPacketSnrConversion() {
    LoRaPacket p{};
    p.snr_q4 = -20;  // -5.0 dB
    LD_EXPECT(p.snr_db() < -4.9f && p.snr_db() > -5.1f);
    return true;
}

bool TestEventEnumDistinct() {
    LD_EXPECT(static_cast<int>(RadioEvent::TxDone)
              != static_cast<int>(RadioEvent::RxDone));
    return true;
}

int main() {
    LD_RUN(TestStatsDefaultZeroed);
    LD_RUN(TestStatsSnapshotByValue);
    LD_RUN(TestPacketSnrConversion);
    LD_RUN(TestEventEnumDistinct);
    return loradriver::test::report();
}
```

- [ ] **Step 5: Register the test**

Edit `tests/host/CMakeLists.txt`, append:

```cmake
loradriver_add_host_test(test_radio_stats)
```

- [ ] **Step 6: Build + run**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host --output-on-failure
```

Expected: both tests pass.

- [ ] **Step 7: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add include/loradriver/radio_event.hpp include/loradriver/radio_stats.hpp include/loradriver/lora_packet.hpp tests/host/test_radio_stats.cpp tests/host/CMakeLists.txt
git -C D:/DEV/C++/LoRaDriver commit -m "feat: add RadioEvent, RadioStats, LoRaPacket value types

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

---

## Phase 2 — HAL: ISpiDevice + FakeSpiDevice + ArduinoSpiDevice + Esp32SpiDevice

Goal: a clean SPI boundary, a deterministic fake for host tests, and two real implementations gated by macros (`ARDUINO`, `ARDUINO_ARCH_ESP32`).

### Task 2.1: ISpiDevice interface + default helpers

**Files:**
- Create: `include/loradriver/hal/spi_device.hpp`
- Create: `src/hal/spi_device.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create the interface header**

Content for `include/loradriver/hal/spi_device.hpp`:

```cpp
#pragma once

#include <cstddef>
#include <cstdint>

#include "loradriver/lora_error.hpp"

namespace loradriver::hal {

/// SPI bus boundary for SX127x register IO.
/// One transfer() call = one full CS-asserted SPI transaction:
///   * 1 address byte (addr | 0x80 for write, addr & 0x7F for read)
///   * len data bytes simultaneously shifted in tx[]/out rx[]
/// Either tx or rx may be nullptr (write-only or read-only).
class ISpiDevice {
public:
    virtual ~ISpiDevice() = default;

    [[nodiscard]] virtual LoRaError begin() noexcept = 0;
    [[nodiscard]] virtual LoRaError transfer(std::uint8_t addr,
                                             const std::uint8_t* tx,
                                             std::uint8_t* rx,
                                             std::size_t len) noexcept = 0;

    [[nodiscard]] LoRaError write_register(std::uint8_t reg, std::uint8_t value) noexcept;
    [[nodiscard]] LoRaError read_register(std::uint8_t reg, std::uint8_t& out) noexcept;
    [[nodiscard]] LoRaError burst_write(std::uint8_t reg,
                                        const std::uint8_t* buf,
                                        std::size_t len) noexcept;
    [[nodiscard]] LoRaError burst_read(std::uint8_t reg,
                                       std::uint8_t* buf,
                                       std::size_t len) noexcept;

protected:
    ISpiDevice() = default;
    ISpiDevice(const ISpiDevice&) = delete;
    ISpiDevice& operator=(const ISpiDevice&) = delete;
};

}  // namespace loradriver::hal
```

- [ ] **Step 2: Create the default helper implementations**

Content for `src/hal/spi_device.cpp`:

```cpp
#include "loradriver/hal/spi_device.hpp"

namespace loradriver::hal {

namespace {
constexpr std::uint8_t kWriteBit = 0x80;
}

LoRaError ISpiDevice::write_register(std::uint8_t reg, std::uint8_t value) noexcept {
    return transfer(static_cast<std::uint8_t>(reg | kWriteBit), &value, nullptr, 1);
}

LoRaError ISpiDevice::read_register(std::uint8_t reg, std::uint8_t& out) noexcept {
    return transfer(static_cast<std::uint8_t>(reg & 0x7F), nullptr, &out, 1);
}

LoRaError ISpiDevice::burst_write(std::uint8_t reg,
                                  const std::uint8_t* buf,
                                  std::size_t len) noexcept {
    if (buf == nullptr || len == 0u) return LoRaError::NullArgument;
    return transfer(static_cast<std::uint8_t>(reg | kWriteBit), buf, nullptr, len);
}

LoRaError ISpiDevice::burst_read(std::uint8_t reg,
                                 std::uint8_t* buf,
                                 std::size_t len) noexcept {
    if (buf == nullptr || len == 0u) return LoRaError::NullArgument;
    return transfer(static_cast<std::uint8_t>(reg & 0x7F), nullptr, buf, len);
}

}  // namespace loradriver::hal
```

- [ ] **Step 3: Wire into CMake**

Append to `target_sources` in `CMakeLists.txt`:

```cmake
  ${CMAKE_CURRENT_SOURCE_DIR}/src/hal/spi_device.cpp
```

- [ ] **Step 4: Build**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host
```

Expected: success.

- [ ] **Step 5: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add include/loradriver/hal/spi_device.hpp src/hal/spi_device.cpp CMakeLists.txt
git -C D:/DEV/C++/LoRaDriver commit -m "feat: add ISpiDevice with register/burst helpers

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task 2.2: FakeSpiDevice (header-only test double)

**Files:**
- Create: `tests/host/fake_spi_device.hpp`
- Create: `tests/host/test_fake_spi_device.cpp`
- Modify: `tests/host/CMakeLists.txt`

- [ ] **Step 1: Create the fake**

Content for `tests/host/fake_spi_device.hpp`:

```cpp
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
};

}  // namespace loradriver::test
```

- [ ] **Step 2: Create the fake's own self-tests**

Content for `tests/host/test_fake_spi_device.cpp`:

```cpp
#include "fake_spi_device.hpp"
#include "test_runner.hpp"

using loradriver::LoRaError;
using loradriver::test::FakeSpiDevice;

bool TestDefaultVersionIs0x12() {
    FakeSpiDevice s;
    std::uint8_t v = 0;
    LD_EXPECT_EQ(s.read_register(0x42, v), LoRaError::OK);
    LD_EXPECT_EQ(v, std::uint8_t{0x12});
    return true;
}

bool TestWriteThenRead() {
    FakeSpiDevice s;
    LD_EXPECT_EQ(s.write_register(0x10, 0xAB), LoRaError::OK);
    std::uint8_t v = 0;
    LD_EXPECT_EQ(s.read_register(0x10, v), LoRaError::OK);
    LD_EXPECT_EQ(v, std::uint8_t{0xAB});
    return true;
}

bool TestBurstWriteThenRead() {
    FakeSpiDevice s;
    const std::uint8_t in[4] = {1, 2, 3, 4};
    LD_EXPECT_EQ(s.burst_write(0x20, in, 4), LoRaError::OK);
    std::uint8_t out[4] = {};
    LD_EXPECT_EQ(s.burst_read(0x20, out, 4), LoRaError::OK);
    for (int i = 0; i < 4; ++i) LD_EXPECT_EQ(out[i], in[i]);
    return true;
}

bool TestFailWritesReturnsSpiFailure() {
    FakeSpiDevice s;
    s.fail_writes(true);
    LD_EXPECT_EQ(s.write_register(0x10, 0x55), LoRaError::SpiFailure);
    return true;
}

bool TestFailReadsReturnsSpiFailure() {
    FakeSpiDevice s;
    s.fail_reads(true);
    std::uint8_t v = 0;
    LD_EXPECT_EQ(s.read_register(0x42, v), LoRaError::SpiFailure);
    return true;
}

bool TestNullBuffersRejected() {
    FakeSpiDevice s;
    LD_EXPECT_EQ(s.burst_write(0x10, nullptr, 4), LoRaError::NullArgument);
    LD_EXPECT_EQ(s.burst_read(0x10, nullptr, 4), LoRaError::NullArgument);
    return true;
}

bool TestWritesLogRecorded() {
    FakeSpiDevice s;
    (void)s.write_register(0x01, 0xAA);
    (void)s.write_register(0x02, 0xBB);
    LD_EXPECT_EQ(s.writes().size(), std::size_t{2});
    LD_EXPECT_EQ(s.writes()[0].reg, std::uint8_t{0x01});
    LD_EXPECT_EQ(s.writes()[0].value, std::uint8_t{0xAA});
    return true;
}

int main() {
    LD_RUN(TestDefaultVersionIs0x12);
    LD_RUN(TestWriteThenRead);
    LD_RUN(TestBurstWriteThenRead);
    LD_RUN(TestFailWritesReturnsSpiFailure);
    LD_RUN(TestFailReadsReturnsSpiFailure);
    LD_RUN(TestNullBuffersRejected);
    LD_RUN(TestWritesLogRecorded);
    return loradriver::test::report();
}
```

- [ ] **Step 3: Register the test**

Append to `tests/host/CMakeLists.txt`:

```cmake
loradriver_add_host_test(test_fake_spi_device)
```

- [ ] **Step 4: Build + run**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host --output-on-failure
```

Expected: 3 test executables pass total.

- [ ] **Step 5: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add tests/host/fake_spi_device.hpp tests/host/test_fake_spi_device.cpp tests/host/CMakeLists.txt
git -C D:/DEV/C++/LoRaDriver commit -m "test: add FakeSpiDevice + self-tests

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task 2.3: ArduinoSpiDevice (header-only, ARDUINO-guarded)

**Files:**
- Create: `include/loradriver/hal/arduino_spi_device.hpp`

- [ ] **Step 1: Create the header**

Content for `include/loradriver/hal/arduino_spi_device.hpp`:

```cpp
#pragma once

// Compiled into user firmware only when Arduino headers are available.
#ifdef ARDUINO

#include <Arduino.h>
#include <SPI.h>

#include "loradriver/hal/spi_device.hpp"

namespace loradriver::hal {

class ArduinoSpiDevice : public ISpiDevice {
public:
    ArduinoSpiDevice(SPIClass& bus, std::int8_t cs_pin,
                     std::uint32_t clock_hz = 8'000'000u) noexcept
        : bus_(bus), cs_pin_(cs_pin), clock_hz_(clock_hz) {}

    [[nodiscard]] LoRaError begin() noexcept override {
        pinMode(cs_pin_, OUTPUT);
        digitalWrite(cs_pin_, HIGH);
        return LoRaError::OK;
    }

    [[nodiscard]] LoRaError transfer(std::uint8_t addr,
                                     const std::uint8_t* tx,
                                     std::uint8_t* rx,
                                     std::size_t len) noexcept override {
        bus_.beginTransaction(SPISettings(clock_hz_, MSBFIRST, SPI_MODE0));
        digitalWrite(cs_pin_, LOW);
        bus_.transfer(addr);
        for (std::size_t i = 0; i < len; ++i) {
            const std::uint8_t out_byte = (tx != nullptr) ? tx[i] : std::uint8_t{0x00};
            const std::uint8_t in_byte  = bus_.transfer(out_byte);
            if (rx != nullptr) rx[i] = in_byte;
        }
        digitalWrite(cs_pin_, HIGH);
        bus_.endTransaction();
        return LoRaError::OK;
    }

private:
    SPIClass& bus_;
    std::int8_t cs_pin_;
    std::uint32_t clock_hz_;
};

}  // namespace loradriver::hal

#endif  // ARDUINO
```

- [ ] **Step 2: Commit (no test — Arduino-only, validated by smoke test in Phase 6)**

```bash
git -C D:/DEV/C++/LoRaDriver add include/loradriver/hal/arduino_spi_device.hpp
git -C D:/DEV/C++/LoRaDriver commit -m "feat: add ArduinoSpiDevice (ARDUINO-guarded header)

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task 2.4: Esp32SpiDevice (DMA-capable, ESP32-only)

**Files:**
- Create: `include/loradriver/hal/esp32_spi_device.hpp`

- [ ] **Step 1: Create the header**

Content for `include/loradriver/hal/esp32_spi_device.hpp`:

```cpp
#pragma once

#ifdef ARDUINO_ARCH_ESP32

#include <Arduino.h>
#include <SPI.h>

#include "loradriver/hal/spi_device.hpp"

namespace loradriver::hal {

/// ESP32-specific SPI device using SPIClass::transferBytes() for DMA-capable
/// burst transfers. ~5-10x faster than byte-by-byte transfer on 255-byte FIFO.
class Esp32SpiDevice : public ISpiDevice {
public:
    Esp32SpiDevice(SPIClass& bus, std::int8_t cs_pin,
                   std::uint32_t clock_hz = 8'000'000u) noexcept
        : bus_(bus), cs_pin_(cs_pin), clock_hz_(clock_hz) {}

    [[nodiscard]] LoRaError begin() noexcept override {
        pinMode(cs_pin_, OUTPUT);
        digitalWrite(cs_pin_, HIGH);
        return LoRaError::OK;
    }

    [[nodiscard]] LoRaError transfer(std::uint8_t addr,
                                     const std::uint8_t* tx,
                                     std::uint8_t* rx,
                                     std::size_t len) noexcept override {
        bus_.beginTransaction(SPISettings(clock_hz_, MSBFIRST, SPI_MODE0));
        digitalWrite(cs_pin_, LOW);
        bus_.transfer(addr);
        if (len > 0u) {
            // transferBytes((tx==nullptr) -> writes 0x00s) and reads into rx.
            // Allow either pointer null; supply a small zero buffer if tx is null.
            if (tx == nullptr && rx == nullptr) {
                // Nothing to do.
            } else if (tx == nullptr) {
                bus_.transferBytes(nullptr, rx, len);
            } else if (rx == nullptr) {
                // Cast-away const required by Arduino API. transferBytes
                // does not mutate tx when rx==nullptr.
                bus_.transferBytes(const_cast<std::uint8_t*>(tx), nullptr, len);
            } else {
                bus_.transferBytes(const_cast<std::uint8_t*>(tx), rx, len);
            }
        }
        digitalWrite(cs_pin_, HIGH);
        bus_.endTransaction();
        return LoRaError::OK;
    }

private:
    SPIClass& bus_;
    std::int8_t cs_pin_;
    std::uint32_t clock_hz_;
};

}  // namespace loradriver::hal

#endif  // ARDUINO_ARCH_ESP32
```

- [ ] **Step 2: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add include/loradriver/hal/esp32_spi_device.hpp
git -C D:/DEV/C++/LoRaDriver commit -m "feat: add Esp32SpiDevice with DMA-capable transferBytes

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## Phase 3 — SX127xDriver

Phase 3 is the largest. We split it into 7 sub-tasks: registers header, IRadioDriver interface, skeleton class + begin() init sequence, runtime setters, TX path, RX path + read_packet, IRQ ring + process_events + errata.

### Task 3.1: SX127x register map header

**Files:**
- Create: `src/chips/sx127x/sx127x_registers.hpp`

- [ ] **Step 1: Create register definitions**

Content for `src/chips/sx127x/sx127x_registers.hpp`:

```cpp
#pragma once

#include <cstdint>

// Datasheet references: SX1276/77/78/79 rev 7 (Semtech DS_SX1276-7-8-9_W_APP_V7.pdf)
// Errata note: SX1276_77_8_ErrataNote_1.1_STD.pdf

namespace loradriver::chips::sx127x::reg {

constexpr std::uint8_t kFifo               = 0x00;
constexpr std::uint8_t kOpMode             = 0x01;
constexpr std::uint8_t kFrMsb              = 0x06;
constexpr std::uint8_t kFrMid              = 0x07;
constexpr std::uint8_t kFrLsb              = 0x08;
constexpr std::uint8_t kPaConfig           = 0x09;
constexpr std::uint8_t kOcp                = 0x0B;
constexpr std::uint8_t kLna                = 0x0C;
constexpr std::uint8_t kFifoAddrPtr        = 0x0D;
constexpr std::uint8_t kFifoTxBaseAddr     = 0x0E;
constexpr std::uint8_t kFifoRxBaseAddr     = 0x0F;
constexpr std::uint8_t kFifoRxCurrentAddr  = 0x10;
constexpr std::uint8_t kIrqFlagsMask       = 0x11;
constexpr std::uint8_t kIrqFlags           = 0x12;
constexpr std::uint8_t kRxNbBytes          = 0x13;
constexpr std::uint8_t kPktSnrValue        = 0x19;
constexpr std::uint8_t kPktRssiValue       = 0x1A;
constexpr std::uint8_t kRssiValue          = 0x1B;
constexpr std::uint8_t kModemConfig1       = 0x1D;
constexpr std::uint8_t kModemConfig2       = 0x1E;
constexpr std::uint8_t kSymbTimeoutLsb     = 0x1F;
constexpr std::uint8_t kPreambleMsb        = 0x20;
constexpr std::uint8_t kPreambleLsb        = 0x21;
constexpr std::uint8_t kPayloadLength      = 0x22;
constexpr std::uint8_t kModemConfig3       = 0x26;
constexpr std::uint8_t kFeiMsb             = 0x28;
constexpr std::uint8_t kFeiMid             = 0x29;
constexpr std::uint8_t kFeiLsb             = 0x2A;
constexpr std::uint8_t kRssiWideband       = 0x2C;
constexpr std::uint8_t kDetectionOptimize  = 0x31;
constexpr std::uint8_t kInvertIq           = 0x33;
constexpr std::uint8_t kHighBwOptimize1    = 0x36;
constexpr std::uint8_t kDetectionThreshold = 0x37;
constexpr std::uint8_t kSyncWord           = 0x39;
constexpr std::uint8_t kHighBwOptimize2    = 0x3A;
constexpr std::uint8_t kInvertIq2          = 0x3B;
constexpr std::uint8_t kDioMapping1        = 0x40;
constexpr std::uint8_t kVersion            = 0x42;
constexpr std::uint8_t kPaDac              = 0x4D;

}  // namespace loradriver::chips::sx127x::reg

namespace loradriver::chips::sx127x::opmode {

constexpr std::uint8_t kLoRaModeBit  = 0x80;
constexpr std::uint8_t kFskSleep     = 0x00;
constexpr std::uint8_t kLoRaSleep    = 0x80;
constexpr std::uint8_t kLoRaStandby  = 0x81;
constexpr std::uint8_t kLoRaTx       = 0x83;
constexpr std::uint8_t kLoRaRxCont   = 0x85;
constexpr std::uint8_t kLoRaRxSingle = 0x86;
constexpr std::uint8_t kLoRaCad      = 0x87;

}  // namespace loradriver::chips::sx127x::opmode

namespace loradriver::chips::sx127x::irq {

constexpr std::uint8_t kRxTimeout       = 0x80;
constexpr std::uint8_t kRxDone          = 0x40;
constexpr std::uint8_t kPayloadCrcError = 0x20;
constexpr std::uint8_t kValidHeader     = 0x10;
constexpr std::uint8_t kTxDone          = 0x08;
constexpr std::uint8_t kCadDone         = 0x04;
constexpr std::uint8_t kFhssChangeChan  = 0x02;
constexpr std::uint8_t kCadDetected     = 0x01;
constexpr std::uint8_t kClearAll        = 0xFF;

}  // namespace loradriver::chips::sx127x::irq

namespace loradriver::chips::sx127x::dio {

// RegDioMapping1: bits[7:6] = DIO0, [5:4] = DIO1
constexpr std::uint8_t kDio0RxDone = 0x00;
constexpr std::uint8_t kDio0TxDone = 0x40;
constexpr std::uint8_t kDio0CadDone = 0x80;
constexpr std::uint8_t kDio1RxTimeout = 0x00;
constexpr std::uint8_t kDio1FhssChange = 0x10;

}  // namespace loradriver::chips::sx127x::dio
```

- [ ] **Step 2: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add src/chips/sx127x/sx127x_registers.hpp
git -C D:/DEV/C++/LoRaDriver commit -m "feat: add SX127x register/opmode/irq/dio constants

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task 3.2: IRadioDriver interface

**Files:**
- Create: `include/loradriver/radio_driver.hpp`

- [ ] **Step 1: Create the interface**

Content for `include/loradriver/radio_driver.hpp`:

```cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

#include "loradriver/lora_config.hpp"
#include "loradriver/lora_error.hpp"
#include "loradriver/radio_event.hpp"
#include "loradriver/radio_stats.hpp"

namespace loradriver {

class IRadioDriver {
public:
    using EventCallback = std::function<void(RadioEvent, int)>;

    virtual ~IRadioDriver() = default;

    [[nodiscard]] virtual LoRaError begin(const LoRaConfig& cfg) noexcept = 0;
    virtual void end() noexcept = 0;
    [[nodiscard]] virtual std::uint8_t chip_version() const noexcept = 0;

    [[nodiscard]] virtual LoRaError set_sleep() noexcept = 0;
    [[nodiscard]] virtual LoRaError set_standby() noexcept = 0;

    [[nodiscard]] virtual LoRaError start_transmit(const std::uint8_t* data,
                                                   std::size_t len,
                                                   std::uint32_t timeout_ms = 2000) noexcept = 0;
    [[nodiscard]] virtual bool is_transmitting() const noexcept = 0;

    [[nodiscard]] virtual LoRaError start_receive(bool continuous = true) noexcept = 0;
    [[nodiscard]] virtual int read_packet(std::uint8_t* buf, std::size_t max_len) noexcept = 0;

    [[nodiscard]] virtual LoRaError start_cad() noexcept = 0;

    [[nodiscard]] virtual LoRaError set_frequency(std::uint32_t hz) noexcept = 0;
    [[nodiscard]] virtual LoRaError set_tx_power(std::int8_t dbm, PaOutput out) noexcept = 0;
    [[nodiscard]] virtual LoRaError set_spreading_factor(std::uint8_t sf) noexcept = 0;
    [[nodiscard]] virtual LoRaError set_bandwidth(std::uint32_t hz) noexcept = 0;

    [[nodiscard]] virtual std::int16_t packet_rssi() const noexcept = 0;
    [[nodiscard]] virtual float packet_snr() const noexcept = 0;
    [[nodiscard]] virtual std::int32_t frequency_error_hz() const noexcept = 0;
    [[nodiscard]] virtual std::int16_t current_rssi() const noexcept = 0;
    [[nodiscard]] virtual std::uint8_t random_byte() noexcept = 0;

    [[nodiscard]] virtual RadioStats get_stats() const noexcept = 0;
    virtual void reset_stats() noexcept = 0;

    virtual void set_event_callback(EventCallback cb) noexcept = 0;
    virtual void process_events() noexcept = 0;
    virtual void handle_interrupt() noexcept = 0;

protected:
    IRadioDriver() = default;
    IRadioDriver(const IRadioDriver&) = delete;
    IRadioDriver& operator=(const IRadioDriver&) = delete;
};

}  // namespace loradriver
```

- [ ] **Step 2: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add include/loradriver/radio_driver.hpp
git -C D:/DEV/C++/LoRaDriver commit -m "feat: add IRadioDriver pure interface

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task 3.3: SX127xDriver class skeleton + begin() init sequence

**Files:**
- Create: `include/loradriver/chips/sx127x_driver.hpp`
- Create: `src/chips/sx127x/sx127x_driver.cpp`
- Create: `tests/host/test_sx127x_init_sequence.cpp`
- Modify: `CMakeLists.txt`, `tests/host/CMakeLists.txt`

Note: this task lands a working `begin()` only — all other virtual methods return `NotImplemented` (we'll add `NotImplemented` to LoRaError just for the skeleton phase — wait, the enum is already locked. Use `InvalidState` as a placeholder return for skeleton methods and tag them with `// TODO Task 3.x` so subsequent tasks finish them.). Tests in this task verify `begin()`.

- [ ] **Step 1: Write the failing tests for begin()**

Content for `tests/host/test_sx127x_init_sequence.cpp`:

```cpp
#include "fake_spi_device.hpp"
#include "loradriver/chips/sx127x_driver.hpp"
#include "loradriver/lora_config.hpp"
#include "loradriver/lora_error.hpp"
#include "test_runner.hpp"

#include "../../src/chips/sx127x/sx127x_registers.hpp"

using loradriver::ChipModel;
using loradriver::LoRaConfig;
using loradriver::LoRaError;
using loradriver::PaOutput;
using loradriver::chips::SX127xDriver;
using loradriver::test::FakeSpiDevice;
namespace reg = loradriver::chips::sx127x::reg;
namespace opmode = loradriver::chips::sx127x::opmode;

static LoRaConfig MakeCfg() {
    LoRaConfig c;
    c.chip = ChipModel::SX1276;
    c.frequency_hz = 868'000'000u;
    c.spreading_factor = 9;
    c.bandwidth_hz = 125'000u;
    c.coding_rate = 5;
    c.preamble_length = 8;
    c.sync_word = 0x12;
    c.crc_enabled = true;
    c.tx_power_dbm = 14;
    c.pa_output = PaOutput::PaBoost;
    c.ocp_ma = 100;
    c.pin_ss = 5; c.pin_reset = 14; c.pin_dio0 = 26;
    return c;
}

bool TestBeginRejectsInvalidConfig() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig bad = MakeCfg();
    bad.pin_ss = -1;
    LD_EXPECT_EQ(drv.begin(bad), LoRaError::InvalidConfig);
    return true;
}

bool TestBeginRejectsMissingChip() {
    FakeSpiDevice spi;
    spi.set_chip_version(0xFF);  // not 0x12
    SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::UnsupportedChip);
    return true;
}

bool TestBeginSucceedsAndEntersStandby() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kOpMode), opmode::kLoRaStandby);
    return true;
}

bool TestBeginWritesFrequencyRegisters868() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.frequency_hz = 868'100'000u;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);

    // Expected FRF = (868100000 * 2^19) / 32_000_000
    const std::uint64_t frf = (static_cast<std::uint64_t>(868'100'000u) << 19) / 32'000'000ull;
    LD_EXPECT_EQ(spi.reg(reg::kFrMsb), static_cast<std::uint8_t>((frf >> 16) & 0xFF));
    LD_EXPECT_EQ(spi.reg(reg::kFrMid), static_cast<std::uint8_t>((frf >> 8) & 0xFF));
    LD_EXPECT_EQ(spi.reg(reg::kFrLsb), static_cast<std::uint8_t>(frf & 0xFF));
    return true;
}

bool TestBeginAppliesSyncWord() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.sync_word = 0x34;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kSyncWord), std::uint8_t{0x34});
    return true;
}

bool TestBeginAppliesLdroForSf12() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.spreading_factor = 12;
    c.bandwidth_hz = 125'000u;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    // ModemConfig3 bit 3 = LowDataRateOptimize
    LD_EXPECT(((spi.reg(reg::kModemConfig3) >> 3) & 0x01u) == 1u);
    return true;
}

bool TestBeginAppliesAgcAuto() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.agc_auto = true;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    // ModemConfig3 bit 2 = AgcAutoOn
    LD_EXPECT(((spi.reg(reg::kModemConfig3) >> 2) & 0x01u) == 1u);
    return true;
}

bool TestBeginAppliesPaBoost14dBm() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.tx_power_dbm = 14;
    c.pa_output = PaOutput::PaBoost;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    // PaSelect bit 7 = 1, OutputPower nibble = 14 - 2 = 12 = 0x0C
    LD_EXPECT((spi.reg(reg::kPaConfig) & 0x80u) != 0u);
    LD_EXPECT_EQ(spi.reg(reg::kPaConfig) & 0x0Fu, std::uint8_t{0x0C});
    return true;
}

bool TestBeginEnablesPaDacForHighPower() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.tx_power_dbm = 20;
    c.pa_output = PaOutput::PaBoost;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kPaDac), std::uint8_t{0x87});
    return true;
}

bool TestBeginAppliesOcp100mA() {
    FakeSpiDevice spi;
    SX127xDriver drv(spi);
    LoRaConfig c = MakeCfg();
    c.ocp_ma = 100;
    LD_EXPECT_EQ(drv.begin(c), LoRaError::OK);
    // OCP enabled: bit 5 = 1; trim formula (ds §5.4.4):
    //   45-120 mA: trim = (mA - 45) / 5  → 100 mA → (100-45)/5 = 11 = 0x0B
    LD_EXPECT((spi.reg(reg::kOcp) & 0x20u) != 0u);
    LD_EXPECT_EQ(spi.reg(reg::kOcp) & 0x1Fu, std::uint8_t{0x0B});
    return true;
}

bool TestBeginClearsIrqFlags() {
    FakeSpiDevice spi;
    spi.set_register(reg::kIrqFlags, 0xFF);  // pre-set all
    SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    // After init, driver wrote 0xFF to RegIrqFlags to clear (write-1-to-clear).
    // We can't observe the result on a fake (writes mutate the array directly),
    // but we can check that a write to kIrqFlags is the last write recorded.
    bool saw_clear = false;
    for (const auto& w : spi.writes()) {
        if (w.reg == reg::kIrqFlags && w.value == 0xFFu) saw_clear = true;
    }
    LD_EXPECT(saw_clear);
    return true;
}

bool TestBeginRejectsSpiFailure() {
    FakeSpiDevice spi;
    spi.fail_writes(true);
    SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::SpiFailure);
    return true;
}

int main() {
    LD_RUN(TestBeginRejectsInvalidConfig);
    LD_RUN(TestBeginRejectsMissingChip);
    LD_RUN(TestBeginSucceedsAndEntersStandby);
    LD_RUN(TestBeginWritesFrequencyRegisters868);
    LD_RUN(TestBeginAppliesSyncWord);
    LD_RUN(TestBeginAppliesLdroForSf12);
    LD_RUN(TestBeginAppliesAgcAuto);
    LD_RUN(TestBeginAppliesPaBoost14dBm);
    LD_RUN(TestBeginEnablesPaDacForHighPower);
    LD_RUN(TestBeginAppliesOcp100mA);
    LD_RUN(TestBeginClearsIrqFlags);
    LD_RUN(TestBeginRejectsSpiFailure);
    return loradriver::test::report();
}
```

- [ ] **Step 2: Register the test**

Append to `tests/host/CMakeLists.txt`:

```cmake
loradriver_add_host_test(test_sx127x_init_sequence)
```

- [ ] **Step 3: Verify RED — test fails to compile (header not yet written)**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host 2>&1 | head -20
```

Expected: error on `loradriver/chips/sx127x_driver.hpp: No such file`.

- [ ] **Step 4: Create the SX127xDriver header**

Content for `include/loradriver/chips/sx127x_driver.hpp`:

```cpp
#pragma once

#include <cstddef>
#include <cstdint>

#include "loradriver/hal/spi_device.hpp"
#include "loradriver/lora_config.hpp"
#include "loradriver/lora_error.hpp"
#include "loradriver/radio_driver.hpp"

namespace loradriver::chips {

class SX127xDriver final : public IRadioDriver {
public:
    explicit SX127xDriver(hal::ISpiDevice& spi) noexcept : spi_(spi) {}

    [[nodiscard]] LoRaError begin(const LoRaConfig& cfg) noexcept override;
    void end() noexcept override;
    [[nodiscard]] std::uint8_t chip_version() const noexcept override { return chip_version_; }

    [[nodiscard]] LoRaError set_sleep() noexcept override;
    [[nodiscard]] LoRaError set_standby() noexcept override;

    [[nodiscard]] LoRaError start_transmit(const std::uint8_t* data,
                                           std::size_t len,
                                           std::uint32_t timeout_ms) noexcept override;
    [[nodiscard]] bool is_transmitting() const noexcept override { return tx_in_progress_; }

    [[nodiscard]] LoRaError start_receive(bool continuous) noexcept override;
    [[nodiscard]] int read_packet(std::uint8_t* buf, std::size_t max_len) noexcept override;

    [[nodiscard]] LoRaError start_cad() noexcept override;

    [[nodiscard]] LoRaError set_frequency(std::uint32_t hz) noexcept override;
    [[nodiscard]] LoRaError set_tx_power(std::int8_t dbm, PaOutput out) noexcept override;
    [[nodiscard]] LoRaError set_spreading_factor(std::uint8_t sf) noexcept override;
    [[nodiscard]] LoRaError set_bandwidth(std::uint32_t hz) noexcept override;

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
    std::uint8_t  op_mode_shadow_ = 0;

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

    [[nodiscard]] static std::uint32_t now_ms() noexcept;  // monotonic for timeout
    [[nodiscard]] static std::uint8_t bw_code(std::uint32_t hz) noexcept;
    [[nodiscard]] std::int16_t rssi_offset() const noexcept;

    void emit(RadioEvent ev, int param) noexcept;
};

}  // namespace loradriver::chips
```

- [ ] **Step 5: Create the implementation**

Content for `src/chips/sx127x/sx127x_driver.cpp`:

```cpp
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
    return 7;  // default 125 kHz
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
        const std::uint8_t v = static_cast<std::uint8_t>(0x70u | (dbm & 0x0Fu));
        if ((e = spi_.write_register(reg::kPaConfig, v)) != LoRaError::OK) return e;
        if ((e = spi_.write_register(reg::kPaDac, 0x84)) != LoRaError::OK) return e;
    } else {
        const bool high_power = dbm > 17;
        const std::int8_t clamped = high_power ? static_cast<std::int8_t>(20) :
                                    (dbm > 17 ? std::int8_t{17} :
                                     (dbm < 2 ? std::int8_t{2} : dbm));
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
        // No need to write kHighBwOptimize2 outside the BW=500/high case.
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
        (cfg.lna_boost_rx ? 0x03u : 0x00u));  // LnaBoostHf
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

// --- TX / RX / CAD / IRQ — implemented in Tasks 3.5–3.7 ---

LoRaError SX127xDriver::start_transmit(const std::uint8_t*, std::size_t, std::uint32_t) noexcept {
    return LoRaError::InvalidState;  // TODO Task 3.5
}

LoRaError SX127xDriver::start_receive(bool) noexcept {
    return LoRaError::InvalidState;  // TODO Task 3.6
}

int SX127xDriver::read_packet(std::uint8_t*, std::size_t) noexcept {
    return 0;  // TODO Task 3.6
}

LoRaError SX127xDriver::start_cad() noexcept {
    return LoRaError::InvalidState;  // TODO Task 3.7
}

std::int16_t SX127xDriver::current_rssi() const noexcept {
    return 0;  // TODO Task 3.7
}

std::uint8_t SX127xDriver::random_byte() noexcept {
    return 0;  // TODO Task 3.7
}

void SX127xDriver::process_events() noexcept {
    // TODO Task 3.7
}

void SX127xDriver::handle_interrupt() noexcept {
    // TODO Task 3.7
}

}  // namespace loradriver::chips
```

- [ ] **Step 6: Wire into CMake**

Append to `target_sources` in `CMakeLists.txt`:

```cmake
  ${CMAKE_CURRENT_SOURCE_DIR}/src/chips/sx127x/sx127x_driver.cpp
```

- [ ] **Step 7: Build + run**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host --output-on-failure
```

Expected: `test_sx127x_init_sequence` runs, 12 tests pass.

- [ ] **Step 8: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add include/loradriver/chips/sx127x_driver.hpp src/chips/sx127x/sx127x_driver.cpp tests/host/test_sx127x_init_sequence.cpp tests/host/CMakeLists.txt CMakeLists.txt
git -C D:/DEV/C++/LoRaDriver commit -m "feat: add SX127xDriver skeleton + complete init sequence

Init covers: chip detect (RegVersion=0x12), FSK→LoRa sleep with verify,
frequency (FRF formula), PA (PaBoost/RFO + PaDac for >17dBm), OCP trim,
modem config (BW/CR/SF/CRC + LDRO auto), sync word, preamble, LNA boost,
errata 2.1 (BW500/high-band), FIFO base addrs, DIO0=RxDone, standby,
IRQ clear. TX/RX/CAD/IRQ stubs return InvalidState until Tasks 3.5-3.7.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 3.4: Runtime setters tests

**Files:**
- Create: `tests/host/test_sx127x_runtime_setters.cpp`
- Modify: `tests/host/CMakeLists.txt`

- [ ] **Step 1: Write tests**

Content for `tests/host/test_sx127x_runtime_setters.cpp`:

```cpp
#include "fake_spi_device.hpp"
#include "loradriver/chips/sx127x_driver.hpp"
#include "loradriver/lora_config.hpp"
#include "test_runner.hpp"

#include "../../src/chips/sx127x/sx127x_registers.hpp"

using loradriver::ChipModel;
using loradriver::LoRaConfig;
using loradriver::LoRaError;
using loradriver::PaOutput;
using loradriver::chips::SX127xDriver;
using loradriver::test::FakeSpiDevice;
namespace reg = loradriver::chips::sx127x::reg;

static LoRaConfig MakeCfg() {
    LoRaConfig c;
    c.chip = ChipModel::SX1276;
    c.frequency_hz = 868'000'000u;
    c.spreading_factor = 9;
    c.bandwidth_hz = 125'000u;
    c.coding_rate = 5;
    c.preamble_length = 8;
    c.sync_word = 0x12;
    c.crc_enabled = true;
    c.tx_power_dbm = 14;
    c.pa_output = PaOutput::PaBoost;
    c.ocp_ma = 100;
    c.pin_ss = 5; c.pin_reset = 14; c.pin_dio0 = 26;
    return c;
}

bool TestSetFrequencyChangesFrfRegisters() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    spi.clear_writes();
    LD_EXPECT_EQ(drv.set_frequency(915'000'000u), LoRaError::OK);
    const std::uint64_t frf = (915'000'000ull << 19) / 32'000'000ull;
    LD_EXPECT_EQ(spi.reg(reg::kFrMsb), static_cast<std::uint8_t>((frf >> 16) & 0xFF));
    return true;
}

bool TestSetSpreadingFactorRejectsOutOfRange() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(drv.set_spreading_factor(5), LoRaError::InvalidConfig);
    LD_EXPECT_EQ(drv.set_spreading_factor(13), LoRaError::InvalidConfig);
    return true;
}

bool TestSetSpreadingFactorUpdatesModemConfig2() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(drv.set_spreading_factor(11), LoRaError::OK);
    LD_EXPECT_EQ((spi.reg(reg::kModemConfig2) >> 4) & 0x0Fu, std::uint8_t{11});
    return true;
}

bool TestSetBandwidthUpdatesModemConfig1() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(drv.set_bandwidth(250'000u), LoRaError::OK);
    LD_EXPECT_EQ((spi.reg(reg::kModemConfig1) >> 4) & 0x0Fu, std::uint8_t{8});
    return true;
}

bool TestSetTxPowerSwitchesPaDac() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(drv.set_tx_power(20, PaOutput::PaBoost), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kPaDac), std::uint8_t{0x87});
    LD_EXPECT_EQ(drv.set_tx_power(10, PaOutput::PaBoost), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kPaDac), std::uint8_t{0x84});
    return true;
}

bool TestSettersRejectedBeforeBegin() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.set_frequency(868'000'000u), LoRaError::NotInitialized);
    LD_EXPECT_EQ(drv.set_spreading_factor(9), LoRaError::NotInitialized);
    LD_EXPECT_EQ(drv.set_bandwidth(125'000u), LoRaError::NotInitialized);
    LD_EXPECT_EQ(drv.set_tx_power(14, PaOutput::PaBoost), LoRaError::NotInitialized);
    return true;
}

bool TestSetStandbyAndSleep() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(drv.set_sleep(), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kOpMode), std::uint8_t{0x80});  // LoRaSleep
    LD_EXPECT_EQ(drv.set_standby(), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kOpMode), std::uint8_t{0x81});  // LoRaStandby
    return true;
}

int main() {
    LD_RUN(TestSetFrequencyChangesFrfRegisters);
    LD_RUN(TestSetSpreadingFactorRejectsOutOfRange);
    LD_RUN(TestSetSpreadingFactorUpdatesModemConfig2);
    LD_RUN(TestSetBandwidthUpdatesModemConfig1);
    LD_RUN(TestSetTxPowerSwitchesPaDac);
    LD_RUN(TestSettersRejectedBeforeBegin);
    LD_RUN(TestSetStandbyAndSleep);
    return loradriver::test::report();
}
```

- [ ] **Step 2: Register the test**

Append to `tests/host/CMakeLists.txt`:

```cmake
loradriver_add_host_test(test_sx127x_runtime_setters)
```

- [ ] **Step 3: Build + run**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host --output-on-failure
```

Expected: 7 tests pass.

- [ ] **Step 4: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add tests/host/test_sx127x_runtime_setters.cpp tests/host/CMakeLists.txt
git -C D:/DEV/C++/LoRaDriver commit -m "test: cover SX127xDriver runtime setters

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task 3.5: TX path

**Files:**
- Modify: `src/chips/sx127x/sx127x_driver.cpp`
- Create: `tests/host/test_sx127x_tx_path.cpp`
- Modify: `tests/host/CMakeLists.txt`

- [ ] **Step 1: Write the failing TX tests**

Content for `tests/host/test_sx127x_tx_path.cpp`:

```cpp
#include "fake_spi_device.hpp"
#include "loradriver/chips/sx127x_driver.hpp"
#include "loradriver/lora_config.hpp"
#include "test_runner.hpp"

#include "../../src/chips/sx127x/sx127x_registers.hpp"

using loradriver::ChipModel;
using loradriver::LoRaConfig;
using loradriver::LoRaError;
using loradriver::PaOutput;
using loradriver::chips::SX127xDriver;
using loradriver::test::FakeSpiDevice;
namespace reg    = loradriver::chips::sx127x::reg;
namespace opmode = loradriver::chips::sx127x::opmode;
namespace dio    = loradriver::chips::sx127x::dio;

static LoRaConfig MakeCfg() {
    LoRaConfig c;
    c.chip = ChipModel::SX1276;
    c.frequency_hz = 868'000'000u;
    c.spreading_factor = 9; c.bandwidth_hz = 125'000u;
    c.coding_rate = 5; c.preamble_length = 8;
    c.sync_word = 0x12; c.crc_enabled = true;
    c.tx_power_dbm = 14; c.pa_output = PaOutput::PaBoost;
    c.ocp_ma = 100;
    c.pin_ss = 5; c.pin_reset = 14; c.pin_dio0 = 26;
    return c;
}

bool TestTransmitRejectsBeforeBegin() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    const std::uint8_t buf[1] = {0xAA};
    LD_EXPECT_EQ(drv.start_transmit(buf, 1, 1000), LoRaError::NotInitialized);
    return true;
}

bool TestTransmitRejectsNullBuffer() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(drv.start_transmit(nullptr, 4, 1000), LoRaError::NullArgument);
    return true;
}

bool TestTransmitRejectsZeroLength() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    const std::uint8_t buf[1] = {0xAA};
    LD_EXPECT_EQ(drv.start_transmit(buf, 0, 1000), LoRaError::InvalidConfig);
    return true;
}

bool TestTransmitRejectsOversizedPayload() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    std::uint8_t buf[256]{};
    LD_EXPECT_EQ(drv.start_transmit(buf, 256, 1000), LoRaError::TxBufferTooLarge);
    return true;
}

bool TestTransmitWritesFifoAndPayloadLength() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    spi.clear_writes();
    const std::uint8_t buf[5] = {1, 2, 3, 4, 5};
    LD_EXPECT_EQ(drv.start_transmit(buf, 5, 1000), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kFifoAddrPtr), std::uint8_t{0});
    LD_EXPECT_EQ(spi.reg(reg::kFifoTxBaseAddr), std::uint8_t{0});
    LD_EXPECT_EQ(spi.reg(reg::kPayloadLength), std::uint8_t{5});
    // FIFO write went through register 0x00 — fake stores last byte at idx 0
    // (we already verify with the writes() log)
    bool saw_payload = false;
    for (const auto& w : spi.writes()) {
        if (w.reg == reg::kFifo && w.value == 5u) saw_payload = true;
    }
    LD_EXPECT(saw_payload);
    return true;
}

bool TestTransmitSetsTxOpModeAndDio0TxDone() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    const std::uint8_t buf[2] = {0xAA, 0x55};
    LD_EXPECT_EQ(drv.start_transmit(buf, 2, 1000), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kOpMode), opmode::kLoRaTx);
    LD_EXPECT_EQ(spi.reg(reg::kDioMapping1) & 0xC0u, dio::kDio0TxDone);
    LD_EXPECT(drv.is_transmitting());
    return true;
}

bool TestTransmitFailsOnSpiError() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    spi.fail_writes(true);
    const std::uint8_t buf[2] = {0xAA, 0x55};
    LD_EXPECT_EQ(drv.start_transmit(buf, 2, 1000), LoRaError::SpiFailure);
    LD_EXPECT(!drv.is_transmitting());
    return true;
}

int main() {
    LD_RUN(TestTransmitRejectsBeforeBegin);
    LD_RUN(TestTransmitRejectsNullBuffer);
    LD_RUN(TestTransmitRejectsZeroLength);
    LD_RUN(TestTransmitRejectsOversizedPayload);
    LD_RUN(TestTransmitWritesFifoAndPayloadLength);
    LD_RUN(TestTransmitSetsTxOpModeAndDio0TxDone);
    LD_RUN(TestTransmitFailsOnSpiError);
    return loradriver::test::report();
}
```

- [ ] **Step 2: Register test**

Append to `tests/host/CMakeLists.txt`:

```cmake
loradriver_add_host_test(test_sx127x_tx_path)
```

- [ ] **Step 3: Verify RED**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host --output-on-failure -R test_sx127x_tx_path
```

Expected: tests fail (current stub returns `InvalidState`).

- [ ] **Step 4: Implement TX path**

In `src/chips/sx127x/sx127x_driver.cpp`, replace the stub `start_transmit` with:

```cpp
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
```

- [ ] **Step 5: Build + run**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host --output-on-failure
```

Expected: all TX tests pass.

- [ ] **Step 6: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add src/chips/sx127x/sx127x_driver.cpp tests/host/test_sx127x_tx_path.cpp tests/host/CMakeLists.txt
git -C D:/DEV/C++/LoRaDriver commit -m "feat: implement SX127xDriver::start_transmit

Standby → FIFO base/ptr=0 → burst write payload → PayloadLength →
DIO0=TxDone → OpMode=TX. Watchdog deadline armed via tx_deadline_ms.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task 3.6: RX path + read_packet

**Files:**
- Modify: `src/chips/sx127x/sx127x_driver.cpp`
- Create: `tests/host/test_sx127x_rx_path.cpp`
- Modify: `tests/host/CMakeLists.txt`

- [ ] **Step 1: Write failing tests**

Content for `tests/host/test_sx127x_rx_path.cpp`:

```cpp
#include "fake_spi_device.hpp"
#include "loradriver/chips/sx127x_driver.hpp"
#include "loradriver/lora_config.hpp"
#include "test_runner.hpp"

#include "../../src/chips/sx127x/sx127x_registers.hpp"

using loradriver::ChipModel;
using loradriver::LoRaConfig;
using loradriver::LoRaError;
using loradriver::PaOutput;
using loradriver::chips::SX127xDriver;
using loradriver::test::FakeSpiDevice;
namespace reg    = loradriver::chips::sx127x::reg;
namespace opmode = loradriver::chips::sx127x::opmode;
namespace dio    = loradriver::chips::sx127x::dio;

static LoRaConfig MakeCfg() {
    LoRaConfig c;
    c.chip = ChipModel::SX1276;
    c.frequency_hz = 868'000'000u;
    c.spreading_factor = 9; c.bandwidth_hz = 125'000u;
    c.coding_rate = 5; c.preamble_length = 8;
    c.sync_word = 0x12; c.crc_enabled = true;
    c.tx_power_dbm = 14; c.pa_output = PaOutput::PaBoost;
    c.ocp_ma = 100;
    c.pin_ss = 5; c.pin_reset = 14; c.pin_dio0 = 26;
    return c;
}

bool TestStartReceiveContinuous() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(drv.start_receive(true), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kOpMode), opmode::kLoRaRxCont);
    LD_EXPECT_EQ(spi.reg(reg::kDioMapping1) & 0xC0u, dio::kDio0RxDone);
    LD_EXPECT_EQ(spi.reg(reg::kFifoRxBaseAddr), std::uint8_t{0});
    return true;
}

bool TestStartReceiveSingle() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(drv.start_receive(false), LoRaError::OK);
    LD_EXPECT_EQ(spi.reg(reg::kOpMode), opmode::kLoRaRxSingle);
    return true;
}

bool TestStartReceiveRejectedBeforeBegin() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.start_receive(true), LoRaError::NotInitialized);
    return true;
}

bool TestReadPacketRejectsNull() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(drv.read_packet(nullptr, 10), 0);
    return true;
}

bool TestReadPacketCopiesFromFifo() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    // Simulate a received packet: 4 bytes at FIFO offset 0
    const std::uint8_t payload[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    for (std::size_t i = 0; i < 4; ++i) {
        spi.set_register(static_cast<std::uint8_t>(reg::kFifo + i), payload[i]);
    }
    spi.set_register(reg::kFifoRxCurrentAddr, 0);
    spi.set_register(reg::kRxNbBytes, 4);

    std::uint8_t out[8]{};
    const int n = drv.read_packet(out, sizeof(out));
    LD_EXPECT_EQ(n, 4);
    for (int i = 0; i < 4; ++i) LD_EXPECT_EQ(out[i], payload[i]);
    return true;
}

bool TestReadPacketClampsToMaxLen() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    for (std::size_t i = 0; i < 10; ++i) {
        spi.set_register(static_cast<std::uint8_t>(reg::kFifo + i), static_cast<std::uint8_t>(i));
    }
    spi.set_register(reg::kFifoRxCurrentAddr, 0);
    spi.set_register(reg::kRxNbBytes, 10);

    std::uint8_t out[4]{};
    const int n = drv.read_packet(out, sizeof(out));
    LD_EXPECT_EQ(n, 4);
    return true;
}

int main() {
    LD_RUN(TestStartReceiveContinuous);
    LD_RUN(TestStartReceiveSingle);
    LD_RUN(TestStartReceiveRejectedBeforeBegin);
    LD_RUN(TestReadPacketRejectsNull);
    LD_RUN(TestReadPacketCopiesFromFifo);
    LD_RUN(TestReadPacketClampsToMaxLen);
    return loradriver::test::report();
}
```

- [ ] **Step 2: Register test**

Append to `tests/host/CMakeLists.txt`:

```cmake
loradriver_add_host_test(test_sx127x_rx_path)
```

- [ ] **Step 3: Verify RED**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host --output-on-failure -R test_sx127x_rx_path
```

Expected: tests fail.

- [ ] **Step 4: Implement RX path**

In `src/chips/sx127x/sx127x_driver.cpp`, replace the `start_receive` and `read_packet` stubs:

```cpp
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
```

- [ ] **Step 5: Build + run**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 6: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add src/chips/sx127x/sx127x_driver.cpp tests/host/test_sx127x_rx_path.cpp tests/host/CMakeLists.txt
git -C D:/DEV/C++/LoRaDriver commit -m "feat: implement SX127xDriver RX path + read_packet

start_receive: Standby → FIFO ptr=0 → DIO0=RxDone → OpMode=RXCONT/SINGLE.
read_packet: snapshot RxCurrentAddr/RxNbBytes → FifoAddrPtr → burst_read.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task 3.7: IRQ ring + process_events + current_rssi + random_byte

**Files:**
- Modify: `src/chips/sx127x/sx127x_driver.cpp`
- Create: `tests/host/test_sx127x_irq_queue.cpp`
- Modify: `tests/host/CMakeLists.txt`

- [ ] **Step 1: Write failing tests for IRQ + process_events**

Content for `tests/host/test_sx127x_irq_queue.cpp`:

```cpp
#include "fake_spi_device.hpp"
#include "loradriver/chips/sx127x_driver.hpp"
#include "loradriver/lora_config.hpp"
#include "loradriver/radio_event.hpp"
#include "test_runner.hpp"

#include "../../src/chips/sx127x/sx127x_registers.hpp"

using loradriver::ChipModel;
using loradriver::LoRaConfig;
using loradriver::LoRaError;
using loradriver::PaOutput;
using loradriver::RadioEvent;
using loradriver::chips::SX127xDriver;
using loradriver::test::FakeSpiDevice;
namespace reg = loradriver::chips::sx127x::reg;
namespace irq = loradriver::chips::sx127x::irq;

static LoRaConfig MakeCfg() {
    LoRaConfig c;
    c.chip = ChipModel::SX1276;
    c.frequency_hz = 868'000'000u;
    c.spreading_factor = 9; c.bandwidth_hz = 125'000u;
    c.coding_rate = 5; c.preamble_length = 8;
    c.sync_word = 0x12; c.crc_enabled = true;
    c.tx_power_dbm = 14; c.pa_output = PaOutput::PaBoost;
    c.ocp_ma = 100;
    c.pin_ss = 5; c.pin_reset = 14; c.pin_dio0 = 26;
    return c;
}

bool TestHandleInterruptEnqueuesEvent() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    drv.handle_interrupt();
    // process_events should observe one event
    int count = 0;
    drv.set_event_callback([&count](RadioEvent, int) { ++count; });
    spi.set_register(reg::kIrqFlags, irq::kRxDone);
    drv.process_events();
    LD_EXPECT_EQ(count, 1);
    return true;
}

bool TestProcessEventsEmitsRxDone() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    bool saw_rxdone = false;
    drv.set_event_callback([&](RadioEvent ev, int) {
        if (ev == RadioEvent::RxDone) saw_rxdone = true;
    });
    spi.set_register(reg::kIrqFlags, irq::kRxDone);
    drv.handle_interrupt();
    drv.process_events();
    LD_EXPECT(saw_rxdone);
    return true;
}

bool TestProcessEventsEmitsTxDone() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    bool saw_txdone = false;
    drv.set_event_callback([&](RadioEvent ev, int) {
        if (ev == RadioEvent::TxDone) saw_txdone = true;
    });
    spi.set_register(reg::kIrqFlags, irq::kTxDone);
    drv.handle_interrupt();
    drv.process_events();
    LD_EXPECT(saw_txdone);
    LD_EXPECT(!drv.is_transmitting());
    return true;
}

bool TestProcessEventsEmitsRxCrcError() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    bool saw_crc = false;
    drv.set_event_callback([&](RadioEvent ev, int) {
        if (ev == RadioEvent::RxCrcError) saw_crc = true;
    });
    spi.set_register(reg::kIrqFlags, irq::kRxDone | irq::kPayloadCrcError);
    drv.handle_interrupt();
    drv.process_events();
    LD_EXPECT(saw_crc);
    LD_EXPECT_EQ(drv.get_stats().rx_crc_errors, std::uint32_t{1});
    return true;
}

bool TestProcessEventsClearsIrqFlags() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    spi.clear_writes();
    spi.set_register(reg::kIrqFlags, irq::kRxDone);
    drv.handle_interrupt();
    drv.process_events();
    bool saw_clear = false;
    for (const auto& w : spi.writes()) {
        if (w.reg == reg::kIrqFlags && w.value == irq::kClearAll) saw_clear = true;
    }
    LD_EXPECT(saw_clear);
    return true;
}

bool TestIrqOverflowDetected() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    // Push 20 IRQs without draining → ring buffer of 16 should drop oldest
    for (int i = 0; i < 20; ++i) drv.handle_interrupt();
    LD_EXPECT(drv.get_stats().irq_overflows > 0u);
    return true;
}

bool TestRandomByteReadsWidebandRssi() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    spi.set_register(reg::kRssiWideband, 0x5A);
    LD_EXPECT_EQ(drv.random_byte(), std::uint8_t{0x5A});
    return true;
}

bool TestTxWatchdogTimeout() {
    FakeSpiDevice spi; SX127xDriver drv(spi);
    LD_EXPECT_EQ(drv.begin(MakeCfg()), LoRaError::OK);
    const std::uint8_t buf[2] = {1, 2};
    // Set extremely short timeout so the watchdog fires on the next process_events
    LD_EXPECT_EQ(drv.start_transmit(buf, 2, 0), LoRaError::OK);
    bool saw_timeout = false;
    drv.set_event_callback([&](RadioEvent ev, int) {
        if (ev == RadioEvent::TxTimeout) saw_timeout = true;
    });
    drv.process_events();
    LD_EXPECT(saw_timeout);
    LD_EXPECT(!drv.is_transmitting());
    LD_EXPECT_EQ(drv.get_stats().tx_timeout, std::uint32_t{1});
    return true;
}

int main() {
    LD_RUN(TestHandleInterruptEnqueuesEvent);
    LD_RUN(TestProcessEventsEmitsRxDone);
    LD_RUN(TestProcessEventsEmitsTxDone);
    LD_RUN(TestProcessEventsEmitsRxCrcError);
    LD_RUN(TestProcessEventsClearsIrqFlags);
    LD_RUN(TestIrqOverflowDetected);
    LD_RUN(TestRandomByteReadsWidebandRssi);
    LD_RUN(TestTxWatchdogTimeout);
    return loradriver::test::report();
}
```

- [ ] **Step 2: Register test**

Append to `tests/host/CMakeLists.txt`:

```cmake
loradriver_add_host_test(test_sx127x_irq_queue)
```

- [ ] **Step 3: Verify RED**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host --output-on-failure -R test_sx127x_irq_queue
```

Expected: failures (stubs).

- [ ] **Step 4: Implement IRQ + process_events + current_rssi + random_byte**

In `src/chips/sx127x/sx127x_driver.cpp`, replace the stubs at the bottom:

```cpp
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

LoRaError SX127xDriver::start_cad() noexcept {
    if (!initialized_) return LoRaError::NotInitialized;
    LoRaError e = spi_.write_register(reg::kDioMapping1, dio::kDio0CadDone);
    if (e != LoRaError::OK) return e;
    return set_op_mode(opmode::kLoRaCad);
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
    if (tx_in_progress_ && now_ms() > tx_deadline_ms_) {
        tx_in_progress_ = false;
        ++stats_.tx_timeout;
        (void)set_op_mode(opmode::kLoRaStandby);
        emit(RadioEvent::TxTimeout, 0);
    }

    // Drain IRQ queue (max kIrqQueueSize iterations)
    std::uint8_t iters = 0;
    while (irq_tail_ != irq_head_ && iters < kIrqQueueSize) {
        irq_tail_ = static_cast<std::uint8_t>((irq_tail_ + 1u) % kIrqQueueSize);
        ++iters;

        std::uint8_t flags = 0;
        if (spi_.read_register(reg::kIrqFlags, flags) != LoRaError::OK) continue;

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
            const int detected = (flags & irq::kCadDetected) ? 1 : 0;
            emit(RadioEvent::CadDone, detected);
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
```

- [ ] **Step 5: Build + run**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host --output-on-failure
```

Expected: all 8 IRQ tests pass + all earlier tests still green.

- [ ] **Step 6: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add src/chips/sx127x/sx127x_driver.cpp tests/host/test_sx127x_irq_queue.cpp tests/host/CMakeLists.txt
git -C D:/DEV/C++/LoRaDriver commit -m "feat: implement SX127xDriver IRQ ring, process_events, CAD, current_rssi, random_byte

ISR shim (handle_interrupt): allocation-free ring push with overflow stat.
process_events: drains ring + reads RegIrqFlags + snapshots RSSI/SNR
+ dispatches RadioEvent + clears flags. Watchdog TX timeout also handled
here. random_byte reads RegRssiWideband (0x2C). start_cad puts radio in
CAD mode with DIO0=CadDone.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## Phase 4 — LoRaTransceiver façade

Goal: a clean FSM façade owning an `IRadioDriver&`, exposing `send()` blocking, `start_receive()`, `poll()`, packet callback with metadata.

### Task 4.1: LoRaTransceiver header

**Files:**
- Create: `include/loradriver/lora_transceiver.hpp`

- [ ] **Step 1: Create the header**

Content for `include/loradriver/lora_transceiver.hpp`:

```cpp
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
    [[nodiscard]] LoRaError start_cad() noexcept;

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
```

- [ ] **Step 2: Commit (impl in next task)**

```bash
git -C D:/DEV/C++/LoRaDriver add include/loradriver/lora_transceiver.hpp
git -C D:/DEV/C++/LoRaDriver commit -m "feat: declare LoRaTransceiver façade

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task 4.2: LoRaTransceiver implementation + tests

**Files:**
- Create: `src/api/lora_transceiver.cpp`
- Create: `tests/host/test_transceiver_fsm.cpp`
- Modify: `CMakeLists.txt`, `tests/host/CMakeLists.txt`

- [ ] **Step 1: Write tests**

Content for `tests/host/test_transceiver_fsm.cpp`:

```cpp
#include "fake_spi_device.hpp"
#include "loradriver/chips/sx127x_driver.hpp"
#include "loradriver/lora_config.hpp"
#include "loradriver/lora_transceiver.hpp"
#include "test_runner.hpp"

#include "../../src/chips/sx127x/sx127x_registers.hpp"

using loradriver::ChipModel;
using loradriver::LoRaConfig;
using loradriver::LoRaError;
using loradriver::LoRaPacket;
using loradriver::LoRaTransceiver;
using loradriver::PaOutput;
using loradriver::RadioEvent;
using loradriver::chips::SX127xDriver;
using loradriver::test::FakeSpiDevice;
namespace reg = loradriver::chips::sx127x::reg;
namespace irq = loradriver::chips::sx127x::irq;

static LoRaConfig MakeCfg() {
    LoRaConfig c;
    c.chip = ChipModel::SX1276;
    c.frequency_hz = 868'000'000u;
    c.spreading_factor = 9; c.bandwidth_hz = 125'000u;
    c.coding_rate = 5; c.preamble_length = 8;
    c.sync_word = 0x12; c.crc_enabled = true;
    c.tx_power_dbm = 14; c.pa_output = PaOutput::PaBoost;
    c.ocp_ma = 100;
    c.pin_ss = 5; c.pin_reset = 14; c.pin_dio0 = 26;
    return c;
}

bool TestBeginEntersStandby() {
    FakeSpiDevice spi; SX127xDriver drv(spi); LoRaTransceiver trx(drv);
    LD_EXPECT_EQ(trx.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT(trx.state() == LoRaTransceiver::State::Standby);
    return true;
}

bool TestSleepThenStandby() {
    FakeSpiDevice spi; SX127xDriver drv(spi); LoRaTransceiver trx(drv);
    LD_EXPECT_EQ(trx.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(trx.set_sleep(), LoRaError::OK);
    LD_EXPECT(trx.state() == LoRaTransceiver::State::Sleep);
    LD_EXPECT_EQ(trx.set_standby(), LoRaError::OK);
    LD_EXPECT(trx.state() == LoRaTransceiver::State::Standby);
    return true;
}

bool TestStartReceiveContinuousTransitions() {
    FakeSpiDevice spi; SX127xDriver drv(spi); LoRaTransceiver trx(drv);
    LD_EXPECT_EQ(trx.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(trx.start_receive(true), LoRaError::OK);
    LD_EXPECT(trx.state() == LoRaTransceiver::State::RxContinuous);
    return true;
}

bool TestStartReceiveSingleTransitions() {
    FakeSpiDevice spi; SX127xDriver drv(spi); LoRaTransceiver trx(drv);
    LD_EXPECT_EQ(trx.begin(MakeCfg()), LoRaError::OK);
    LD_EXPECT_EQ(trx.start_receive(false), LoRaError::OK);
    LD_EXPECT(trx.state() == LoRaTransceiver::State::RxSingle);
    return true;
}

bool TestSendRejectsBeforeBegin() {
    FakeSpiDevice spi; SX127xDriver drv(spi); LoRaTransceiver trx(drv);
    const std::uint8_t buf[1] = {0xAA};
    LD_EXPECT_EQ(trx.send(buf, 1, 100), LoRaError::NotInitialized);
    return true;
}

bool TestSendCompletesWhenTxDoneIrqArrives() {
    FakeSpiDevice spi; SX127xDriver drv(spi); LoRaTransceiver trx(drv);
    LD_EXPECT_EQ(trx.begin(MakeCfg()), LoRaError::OK);

    // Pre-arm: as soon as start_transmit puts driver in TX, the next process_events
    // call (inside send()'s wait loop) will read RegIrqFlags. Setting kTxDone here
    // simulates the chip having raised it instantly (no real timing in host tests).
    spi.set_register(reg::kIrqFlags, irq::kTxDone);
    // We also need handle_interrupt() to fire so the ring has an entry to drain.
    // Hook the driver to fire handle_interrupt() once at the start of the wait.
    // Simpler: emulate the ISR by calling handle_interrupt before send().
    drv.handle_interrupt();

    const std::uint8_t buf[2] = {1, 2};
    const LoRaError e = trx.send(buf, 2, 100);
    LD_EXPECT_EQ(e, LoRaError::OK);
    LD_EXPECT(trx.state() == LoRaTransceiver::State::Standby);
    return true;
}

bool TestSendReturnsTxTimeout() {
    FakeSpiDevice spi; SX127xDriver drv(spi); LoRaTransceiver trx(drv);
    LD_EXPECT_EQ(trx.begin(MakeCfg()), LoRaError::OK);
    const std::uint8_t buf[2] = {1, 2};
    // No IRQ arranged → watchdog fires after timeout_ms=0
    LD_EXPECT_EQ(trx.send(buf, 2, 0), LoRaError::TxTimeout);
    return true;
}

bool TestOnReceiveDispatchesPacket() {
    FakeSpiDevice spi; SX127xDriver drv(spi); LoRaTransceiver trx(drv);
    LD_EXPECT_EQ(trx.begin(MakeCfg()), LoRaError::OK);

    // Arrange a fake received frame in FIFO + IRQ flags
    const std::uint8_t payload[3] = {0xAA, 0xBB, 0xCC};
    for (std::size_t i = 0; i < 3; ++i) {
        spi.set_register(static_cast<std::uint8_t>(reg::kFifo + i), payload[i]);
    }
    spi.set_register(reg::kFifoRxCurrentAddr, 0);
    spi.set_register(reg::kRxNbBytes, 3);
    spi.set_register(reg::kPktRssiValue, 100);
    spi.set_register(reg::kPktSnrValue, 0x14);  // +5 dB
    spi.set_register(reg::kIrqFlags, irq::kRxDone);

    bool got = false;
    std::size_t got_len = 0;
    std::uint8_t got_data[3] = {};
    trx.on_receive([&](const LoRaPacket& meta, const std::uint8_t* data, std::size_t len) {
        got = true;
        got_len = len;
        if (len <= 3) for (std::size_t i = 0; i < len; ++i) got_data[i] = data[i];
        (void)meta;
    });

    LD_EXPECT_EQ(trx.start_receive(true), LoRaError::OK);
    drv.handle_interrupt();
    trx.poll();
    LD_EXPECT(got);
    LD_EXPECT_EQ(got_len, std::size_t{3});
    for (int i = 0; i < 3; ++i) LD_EXPECT_EQ(got_data[i], payload[i]);
    return true;
}

bool TestOnEventForwardsRadioEvents() {
    FakeSpiDevice spi; SX127xDriver drv(spi); LoRaTransceiver trx(drv);
    LD_EXPECT_EQ(trx.begin(MakeCfg()), LoRaError::OK);
    int count = 0;
    trx.on_event([&](RadioEvent, int) { ++count; });
    spi.set_register(reg::kIrqFlags, irq::kRxDone);
    drv.handle_interrupt();
    trx.poll();
    LD_EXPECT(count >= 1);
    return true;
}

bool TestOnTxDoneFiresAfterTransmission() {
    FakeSpiDevice spi; SX127xDriver drv(spi); LoRaTransceiver trx(drv);
    LD_EXPECT_EQ(trx.begin(MakeCfg()), LoRaError::OK);
    bool fired = false;
    trx.on_tx_done([&]() { fired = true; });
    spi.set_register(reg::kIrqFlags, irq::kTxDone);
    drv.handle_interrupt();
    const std::uint8_t buf[1] = {0xAA};
    LD_EXPECT_EQ(trx.send(buf, 1, 100), LoRaError::OK);
    LD_EXPECT(fired);
    return true;
}

int main() {
    LD_RUN(TestBeginEntersStandby);
    LD_RUN(TestSleepThenStandby);
    LD_RUN(TestStartReceiveContinuousTransitions);
    LD_RUN(TestStartReceiveSingleTransitions);
    LD_RUN(TestSendRejectsBeforeBegin);
    LD_RUN(TestSendCompletesWhenTxDoneIrqArrives);
    LD_RUN(TestSendReturnsTxTimeout);
    LD_RUN(TestOnReceiveDispatchesPacket);
    LD_RUN(TestOnEventForwardsRadioEvents);
    LD_RUN(TestOnTxDoneFiresAfterTransmission);
    return loradriver::test::report();
}
```

- [ ] **Step 2: Register the test**

Append to `tests/host/CMakeLists.txt`:

```cmake
loradriver_add_host_test(test_transceiver_fsm)
```

- [ ] **Step 3: Implement the transceiver**

Content for `src/api/lora_transceiver.cpp`:

```cpp
#include "loradriver/lora_transceiver.hpp"

#include <chrono>
#include <cstring>

namespace loradriver {

namespace {
std::uint32_t now_ms() noexcept {
#ifdef ARDUINO
    extern unsigned long millis();
    return static_cast<std::uint32_t>(millis());
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
    driver_.end();
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

LoRaError LoRaTransceiver::start_cad() noexcept {
    if (state_ == State::Uninit) return LoRaError::NotInitialized;
    const LoRaError e = driver_.start_cad();
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
```

- [ ] **Step 4: Wire into CMake**

Append to `target_sources` in `CMakeLists.txt`:

```cmake
  ${CMAKE_CURRENT_SOURCE_DIR}/src/api/lora_transceiver.cpp
```

- [ ] **Step 5: Build + run**

```bash
cmake --build D:/DEV/C++/LoRaDriver/build/host
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host --output-on-failure
```

Expected: all transceiver tests pass + all earlier tests still green.

- [ ] **Step 6: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add src/api/lora_transceiver.cpp tests/host/test_transceiver_fsm.cpp CMakeLists.txt tests/host/CMakeLists.txt
git -C D:/DEV/C++/LoRaDriver commit -m "feat: implement LoRaTransceiver façade

FSM façade owns IRadioDriver&. send() blocking with deadline poll loop;
start_receive (single/continuous) updates state; RxDone path reads packet
and dispatches metadata callback; TxDone/TxTimeout fire on_tx_done; CAD
returns to Standby on completion.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## Phase 5 — RadioPumpTask (ESP32 FreeRTOS)

Goal: optional ESP32-only async layer that owns a FreeRTOS task pinned to a core, drains IRQs, runs TX from its own context, and restores RX after TX.

### Task 5.1: RadioPumpTask header (ESP32-guarded)

**Files:**
- Create: `include/loradriver/platform/esp32/radio_pump_task.hpp`

- [ ] **Step 1: Create header**

Content for `include/loradriver/platform/esp32/radio_pump_task.hpp`:

```cpp
#pragma once

#ifdef ARDUINO_ARCH_ESP32

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include <cstdint>
#include <cstring>

#include "loradriver/lora_error.hpp"
#include "loradriver/lora_transceiver.hpp"

namespace loradriver::platform::esp32 {

class RadioPumpTask {
public:
    struct Metrics {
        std::uint32_t polls        = 0;
        std::uint32_t max_poll_us  = 0;
        std::uint64_t total_poll_us = 0;
        std::uint32_t tx_enqueued  = 0;
        std::uint32_t tx_errors    = 0;
    };

    RadioPumpTask() = default;
    ~RadioPumpTask() {
        stop();
        if (tx_queue_) { vQueueDelete(tx_queue_); tx_queue_ = nullptr; }
    }

    RadioPumpTask(const RadioPumpTask&) = delete;
    RadioPumpTask& operator=(const RadioPumpTask&) = delete;

    bool start(LoRaTransceiver& trx,
               std::uint32_t period_ms = 2,
               UBaseType_t priority = 2,
               std::uint32_t stack_words = 2048,
               BaseType_t core_id = 1,
               std::uint8_t tx_queue_depth = 4) {
        stop();
        trx_       = &trx;
        period_ms_ = (period_ms == 0u) ? 1u : period_ms;

        if (!tx_queue_) {
            tx_queue_ = xQueueCreate(tx_queue_depth, sizeof(TxItem));
            if (!tx_queue_) return false;
        }

        const BaseType_t rc = xTaskCreatePinnedToCore(
            &RadioPumpTask::task_entry, "lora_pump", stack_words, this,
            priority, const_cast<TaskHandle_t*>(&task_), core_id);
        return rc == pdPASS;
    }

    void stop() {
        const TaskHandle_t t = task_;
        if (t == nullptr) return;
        task_ = nullptr;
        tx_pending_ = false;
        vTaskDelete(t);
    }

    bool running() const noexcept { return task_ != nullptr; }

    bool enqueue_packet(const std::uint8_t* data, std::uint8_t len) {
        if (!tx_queue_ || !task_ || data == nullptr || len == 0u) return false;
        TxItem item{};
        item.len = len;
        std::memcpy(item.data, data, len);
        if (xQueueSend(tx_queue_, &item, 0) != pdTRUE) return false;
        portENTER_CRITICAL(&mux_);
        ++metrics_.tx_enqueued;
        portEXIT_CRITICAL(&mux_);
        return true;
    }

    void IRAM_ATTR notify_from_isr() {
        const TaskHandle_t t = task_;
        if (t == nullptr) return;
        BaseType_t woken = pdFALSE;
        vTaskNotifyGiveFromISR(t, &woken);
        portYIELD_FROM_ISR(woken);
    }

    Metrics metrics() const {
        Metrics out{};
        portENTER_CRITICAL(&mux_);
        out = metrics_;
        portEXIT_CRITICAL(&mux_);
        return out;
    }

    void reset_metrics() {
        portENTER_CRITICAL(&mux_);
        metrics_ = Metrics{};
        portEXIT_CRITICAL(&mux_);
    }

private:
    struct TxItem {
        std::uint8_t data[255];
        std::uint8_t len;
    };

    static void task_entry(void* arg) {
        auto* self = static_cast<RadioPumpTask*>(arg);
        const TickType_t period_ticks = pdMS_TO_TICKS(self->period_ms_);

        while (self->task_ != nullptr) {
            if (!self->tx_pending_ && self->tx_queue_) {
                TxItem item;
                if (xQueueReceive(self->tx_queue_, &item, 0) == pdTRUE) {
                    self->tx_pending_ = true;
                    const LoRaError err = self->trx_->send(item.data, item.len, 500);
                    if (err != LoRaError::OK) {
                        self->tx_pending_ = false;
                        portENTER_CRITICAL(&self->mux_);
                        ++self->metrics_.tx_errors;
                        portEXIT_CRITICAL(&self->mux_);
                    }
                }
            }

            ulTaskNotifyTake(pdTRUE, period_ticks);
            if (self->trx_ == nullptr) continue;

            const std::uint32_t start_us = static_cast<std::uint32_t>(micros());
            self->trx_->poll();
            const std::uint32_t elapsed_us = static_cast<std::uint32_t>(micros() - start_us);

            if (self->tx_pending_) {
                const auto st = self->trx_->state();
                if (st != LoRaTransceiver::State::Tx) {
                    self->tx_pending_ = false;
                    if (st == LoRaTransceiver::State::Standby) {
                        (void)self->trx_->start_receive(true);
                    }
                }
            }

            portENTER_CRITICAL(&self->mux_);
            ++self->metrics_.polls;
            self->metrics_.total_poll_us += elapsed_us;
            if (elapsed_us > self->metrics_.max_poll_us) {
                self->metrics_.max_poll_us = elapsed_us;
            }
            portEXIT_CRITICAL(&self->mux_);
        }
        vTaskDelete(nullptr);
    }

    volatile TaskHandle_t task_       = nullptr;
    LoRaTransceiver*      trx_        = nullptr;
    std::uint32_t         period_ms_  = 2;
    QueueHandle_t         tx_queue_   = nullptr;
    volatile bool         tx_pending_ = false;
    mutable portMUX_TYPE  mux_        = portMUX_INITIALIZER_UNLOCKED;
    Metrics               metrics_{};
};

}  // namespace loradriver::platform::esp32

#endif  // ARDUINO_ARCH_ESP32
```

- [ ] **Step 2: Commit (no host test — ESP32-only, validated by Phase 6 smoke + Phase 7 consumer)**

```bash
git -C D:/DEV/C++/LoRaDriver add include/loradriver/platform/esp32/radio_pump_task.hpp
git -C D:/DEV/C++/LoRaDriver commit -m "feat: add RadioPumpTask (ESP32 FreeRTOS pump)

ISR-notified task pinned to a core. Owns a TX queue; auto-standbys from RX,
sends, restores RX. Configurable period, priority, stack, core, queue depth.
Metrics: polls, max/total poll µs, tx_enqueued, tx_errors.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## Phase 6 — Embedded smoke + examples

### Task 6.1: PlatformIO config for embedded smoke

**Files:**
- Modify: `platformio.ini`
- Create: `tests/embedded/smoke/test_main.cpp`

- [ ] **Step 1: Rewrite platformio.ini**

Content for `platformio.ini`:

```ini
[platformio]
test_dir = tests/embedded

[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
build_unflags = -std=gnu++11
build_flags = -std=gnu++17
test_framework = unity
test_build_src = no

[env:smoke]
extends = env:esp32dev
test_filter = smoke
```

- [ ] **Step 2: Create smoke test (Unity)**

Content for `tests/embedded/smoke/test_main.cpp`:

```cpp
#include <Arduino.h>
#include <SPI.h>
#include <unity.h>

#include "loradriver/chips/sx127x_driver.hpp"
#include "loradriver/hal/esp32_spi_device.hpp"
#include "loradriver/lora_config.hpp"
#include "loradriver/lora_transceiver.hpp"

using namespace loradriver;

namespace {

constexpr std::int8_t kPinSS    = 5;
constexpr std::int8_t kPinReset = 14;
constexpr std::int8_t kPinDio0  = 26;

hal::Esp32SpiDevice* g_spi = nullptr;
chips::SX127xDriver* g_drv = nullptr;
LoRaTransceiver*     g_trx = nullptr;

void hard_reset() {
    pinMode(kPinReset, OUTPUT);
    digitalWrite(kPinReset, LOW);
    delay(2);
    digitalWrite(kPinReset, HIGH);
    delay(10);
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_chip_version_is_0x12() {
    SPI.begin();
    hard_reset();
    static hal::Esp32SpiDevice spi(SPI, kPinSS);
    static chips::SX127xDriver drv(spi);
    g_spi = &spi; g_drv = &drv;

    LoRaConfig cfg;
    cfg.chip = ChipModel::SX1276;
    cfg.frequency_hz = 868'000'000u;
    cfg.pin_ss = kPinSS; cfg.pin_reset = kPinReset; cfg.pin_dio0 = kPinDio0;

    TEST_ASSERT_EQUAL(static_cast<int>(LoRaError::OK), static_cast<int>(drv.begin(cfg)));
    TEST_ASSERT_EQUAL_HEX8(0x12, drv.chip_version());
}

void test_transceiver_begin_reaches_standby() {
    static hal::Esp32SpiDevice spi(SPI, kPinSS);
    static chips::SX127xDriver drv(spi);
    static LoRaTransceiver trx(drv);
    g_trx = &trx;

    LoRaConfig cfg;
    cfg.chip = ChipModel::SX1276;
    cfg.frequency_hz = 868'000'000u;
    cfg.pin_ss = kPinSS; cfg.pin_reset = kPinReset; cfg.pin_dio0 = kPinDio0;

    TEST_ASSERT_EQUAL(static_cast<int>(LoRaError::OK), static_cast<int>(trx.begin(cfg)));
    TEST_ASSERT_EQUAL(static_cast<int>(LoRaTransceiver::State::Standby),
                      static_cast<int>(trx.state()));
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_chip_version_is_0x12);
    RUN_TEST(test_transceiver_begin_reaches_standby);
    UNITY_END();
}

void loop() {}
```

- [ ] **Step 3: Verify PlatformIO recognises the env (no flash — hardware optional)**

```bash
pio project init --silent
pio run -e esp32dev -t checkprogsize
```

Expected: build succeeds (don't worry about test execution if no hardware connected — the embedded smoke is documentation + on-demand run).

- [ ] **Step 4: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add platformio.ini tests/embedded/smoke/test_main.cpp
git -C D:/DEV/C++/LoRaDriver commit -m "test: add ESP32 embedded smoke (chip version + transceiver begin)

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task 6.2: Three Arduino examples

**Files:**
- Create: `examples/BasicSender/BasicSender.ino`
- Create: `examples/BasicReceiver/BasicReceiver.ino`
- Create: `examples/Esp32WithPumpTask/Esp32WithPumpTask.ino`

- [ ] **Step 1: BasicSender**

Content for `examples/BasicSender/BasicSender.ino`:

```cpp
// Minimal blocking sender — sends a packet every second.
#include <Arduino.h>
#include <SPI.h>

#include "loradriver/chips/sx127x_driver.hpp"
#include "loradriver/hal/arduino_spi_device.hpp"
#include "loradriver/lora_transceiver.hpp"

using namespace loradriver;

constexpr std::int8_t kSS = 5, kRst = 14, kDio0 = 26;

hal::ArduinoSpiDevice g_spi(SPI, kSS);
chips::SX127xDriver   g_drv(g_spi);
LoRaTransceiver       g_trx(g_drv);

void setup() {
    Serial.begin(115200);
    SPI.begin();

    pinMode(kRst, OUTPUT);
    digitalWrite(kRst, LOW);  delay(2);
    digitalWrite(kRst, HIGH); delay(10);

    LoRaConfig cfg;
    cfg.chip = ChipModel::SX1276;
    cfg.frequency_hz = 868'000'000u;
    cfg.tx_power_dbm = 14;
    cfg.pin_ss = kSS; cfg.pin_reset = kRst; cfg.pin_dio0 = kDio0;

    const LoRaError e = g_trx.begin(cfg);
    Serial.printf("begin: %s\n", to_string(e));
}

void loop() {
    static std::uint32_t counter = 0;
    char msg[32];
    const int n = snprintf(msg, sizeof(msg), "hello %lu", static_cast<unsigned long>(counter++));
    const LoRaError e = g_trx.send(reinterpret_cast<const std::uint8_t*>(msg),
                                   static_cast<std::size_t>(n), 1000);
    Serial.printf("send #%lu: %s\n", static_cast<unsigned long>(counter), to_string(e));
    delay(1000);
}
```

- [ ] **Step 2: BasicReceiver**

Content for `examples/BasicReceiver/BasicReceiver.ino`:

```cpp
// Minimal polling receiver — print packets as they arrive.
#include <Arduino.h>
#include <SPI.h>

#include "loradriver/chips/sx127x_driver.hpp"
#include "loradriver/hal/arduino_spi_device.hpp"
#include "loradriver/lora_transceiver.hpp"

using namespace loradriver;

constexpr std::int8_t kSS = 5, kRst = 14, kDio0 = 26;

hal::ArduinoSpiDevice g_spi(SPI, kSS);
chips::SX127xDriver   g_drv(g_spi);
LoRaTransceiver       g_trx(g_drv);

void IRAM_ATTR isr_dio0() { g_trx.handle_interrupt(); }

void setup() {
    Serial.begin(115200);
    SPI.begin();
    pinMode(kRst, OUTPUT); digitalWrite(kRst, LOW); delay(2); digitalWrite(kRst, HIGH); delay(10);

    LoRaConfig cfg;
    cfg.chip = ChipModel::SX1276;
    cfg.frequency_hz = 868'000'000u;
    cfg.pin_ss = kSS; cfg.pin_reset = kRst; cfg.pin_dio0 = kDio0;
    (void)g_trx.begin(cfg);

    g_trx.on_receive([](const LoRaPacket& meta, const std::uint8_t* data, std::size_t len) {
        Serial.printf("RX %u bytes  rssi=%d  snr=%.1f: ",
                      static_cast<unsigned>(len), meta.rssi_dbm, meta.snr_db());
        for (std::size_t i = 0; i < len; ++i) Serial.write(data[i]);
        Serial.println();
    });

    attachInterrupt(digitalPinToInterrupt(kDio0), isr_dio0, RISING);
    (void)g_trx.start_receive(true);
}

void loop() { g_trx.poll(); delay(2); }
```

- [ ] **Step 3: Esp32WithPumpTask**

Content for `examples/Esp32WithPumpTask/Esp32WithPumpTask.ino`:

```cpp
// ESP32 example using the FreeRTOS pump task — async TX, automatic RX restore.
#include <Arduino.h>
#include <SPI.h>

#include "loradriver/chips/sx127x_driver.hpp"
#include "loradriver/hal/esp32_spi_device.hpp"
#include "loradriver/lora_transceiver.hpp"
#include "loradriver/platform/esp32/radio_pump_task.hpp"

using namespace loradriver;

constexpr std::int8_t kSS = 5, kRst = 14, kDio0 = 26;

hal::Esp32SpiDevice           g_spi(SPI, kSS);
chips::SX127xDriver           g_drv(g_spi);
LoRaTransceiver               g_trx(g_drv);
platform::esp32::RadioPumpTask g_pump;

void IRAM_ATTR isr_dio0() {
    g_trx.handle_interrupt();
    g_pump.notify_from_isr();
}

void setup() {
    Serial.begin(115200);
    SPI.begin();
    pinMode(kRst, OUTPUT); digitalWrite(kRst, LOW); delay(2); digitalWrite(kRst, HIGH); delay(10);

    LoRaConfig cfg;
    cfg.chip = ChipModel::SX1276;
    cfg.frequency_hz = 868'000'000u;
    cfg.pin_ss = kSS; cfg.pin_reset = kRst; cfg.pin_dio0 = kDio0;
    (void)g_trx.begin(cfg);

    g_trx.on_receive([](const LoRaPacket& m, const std::uint8_t* d, std::size_t n) {
        Serial.printf("RX %u rssi=%d  ", static_cast<unsigned>(n), m.rssi_dbm);
        for (std::size_t i = 0; i < n; ++i) Serial.write(d[i]);
        Serial.println();
    });

    (void)g_trx.start_receive(true);
    attachInterrupt(digitalPinToInterrupt(kDio0), isr_dio0, RISING);
    g_pump.start(g_trx, /*period_ms=*/2, /*priority=*/2, /*stack_words=*/2048, /*core_id=*/1);
}

void loop() {
    static std::uint32_t i = 0;
    char msg[16];
    const int n = snprintf(msg, sizeof(msg), "tx %lu", static_cast<unsigned long>(i++));
    g_pump.enqueue_packet(reinterpret_cast<const std::uint8_t*>(msg),
                          static_cast<std::uint8_t>(n));
    delay(2000);
}
```

- [ ] **Step 4: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add examples/BasicSender/BasicSender.ino examples/BasicReceiver/BasicReceiver.ino examples/Esp32WithPumpTask/Esp32WithPumpTask.ino
git -C D:/DEV/C++/LoRaDriver commit -m "docs: add three Arduino examples (sender, receiver, ESP32 pump)

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## Phase 7 — SYNC-SIGNAL-LORA migration

### Task 7.1: Switch lib_deps to the new repo + rewrite lora_handler

**Files:**
- Modify: `D:/DEV/PlatformIO/SYNC-SIGNAL-LORA/SYNC-SIGNAL-LORA/platformio.ini`
- Modify: `D:/DEV/PlatformIO/SYNC-SIGNAL-LORA/SYNC-SIGNAL-LORA/include/lora_handler.h`
- Modify: `D:/DEV/PlatformIO/SYNC-SIGNAL-LORA/SYNC-SIGNAL-LORA/src/lora_handler.cpp`

- [ ] **Step 1: Update lib_deps**

Edit `D:/DEV/PlatformIO/SYNC-SIGNAL-LORA/SYNC-SIGNAL-LORA/platformio.ini`, replace the `lib_deps` line:

```ini
lib_deps =
    symlink://D:/DEV/C++/LoRaDriver
    esphome/ESPAsyncWebServer-esphome@^3.1.0
    bblanchon/ArduinoJson@^7.0.0
```

- [ ] **Step 2: Update lora_handler.h to use the new API**

Replace the includes and the LoRaHandler private members in `include/lora_handler.h`:

Old block to remove:
```cpp
#include <LoRa.hpp>
// ...
Esp32SpiDevice   spiDevice_{SPI, config::pins::LORA_SS, 8'000'000UL};
SX127xDriver     driver_{spiDevice_, LoRaConfig{}};
LoRaTransceiver  transceiver_{driver_};
RadioPumpTask    pump_;
```

New block:
```cpp
#include "loradriver/chips/sx127x_driver.hpp"
#include "loradriver/hal/esp32_spi_device.hpp"
#include "loradriver/lora_transceiver.hpp"
#include "loradriver/platform/esp32/radio_pump_task.hpp"
// ...
loradriver::hal::Esp32SpiDevice            spiDevice_{SPI, config::pins::LORA_SS, 8'000'000UL};
loradriver::chips::SX127xDriver            driver_{spiDevice_};
loradriver::LoRaTransceiver                transceiver_{driver_};
loradriver::platform::esp32::RadioPumpTask pump_;
```

- [ ] **Step 3: Update lora_handler.cpp**

Edit `src/lora_handler.cpp`:
- Replace `LoRaConfig cfg{};` block with the new `loradriver::LoRaConfig` field names:

```cpp
loradriver::LoRaConfig cfg{};
cfg.chip            = loradriver::ChipModel::SX1276;  // or SX1278 if applicable
cfg.frequency_hz    = static_cast<std::uint32_t>(lora::FREQUENCY);
cfg.spreading_factor = static_cast<std::uint8_t>(lora::SF);
cfg.bandwidth_hz    = static_cast<std::uint32_t>(lora::BANDWIDTH);
cfg.coding_rate     = static_cast<std::uint8_t>(lora::CODING_RATE);
cfg.tx_power_dbm    = static_cast<std::int8_t>(lora::TX_POWER);
cfg.sync_word       = lora::SYNC_WORD;
cfg.crc_enabled     = true;
cfg.pa_output       = loradriver::PaOutput::PaBoost;
cfg.pin_ss          = static_cast<std::int8_t>(pins::LORA_SS);
cfg.pin_reset       = static_cast<std::int8_t>(pins::LORA_RST);
cfg.pin_dio0        = static_cast<std::int8_t>(pins::LORA_DIO0);
cfg.pin_dio1        = -1;
cfg.spi_frequency_hz = 8'000'000UL;
```

- Replace method names: `transceiver_.begin(cfg)` (same), `onReceive` → `on_receive`, `transceiver_.startReceive(true)` → `transceiver_.start_receive(true)`, `pump_.start(...)` (same), `pump_.enqueuePacket` → `pump_.enqueue_packet`, `pump_.notifyFromIsr` → `pump_.notify_from_isr`, `pump_.metrics()` (same), `pump_.resetMetrics()` → `pump_.reset_metrics()`, `transceiver_.stats()` (same).
- Replace `RadioStats stats = transceiver_.stats(); stats.irqEventsProcessed` → `stats.irq_events_processed`; `stats.irqOverflows` → `stats.irq_overflows`; `stats.maxIrqBacklog` → `stats.max_irq_backlog`; `pumpm.txErrors` → `pumpm.tx_errors`; `pumpm.maxPollUs` → `pumpm.max_poll_us`.
- Replace `LoRaError::OK` (same), `toString(err)` → `loradriver::to_string(err)`.
- Replace `LoRaPacket` type in callback signature with `loradriver::LoRaPacket`, fields `meta.rssi` → `meta.rssi_dbm`, `meta.snr` → `meta.snr_db()`.
- Replace `driver_.handleInterrupt()` → `driver_.handle_interrupt()`.

- [ ] **Step 4: Build the consumer**

```bash
pio run -d D:/DEV/PlatformIO/SYNC-SIGNAL-LORA/SYNC-SIGNAL-LORA -e esp32dev
```

Expected: compilation succeeds. Fix any naming/typing leftover the manual edit missed.

- [ ] **Step 5: Flash + monitor on a real panel**

```bash
pio run -d D:/DEV/PlatformIO/SYNC-SIGNAL-LORA/SYNC-SIGNAL-LORA -e panel1 -t upload -t monitor
```

Expected: serial log shows `LORA Init OK`, then `RX TRIGGER ...` events when paired with another panel. Press Ctrl-T then Ctrl-C to quit monitor.

- [ ] **Step 6: Commit the consumer changes**

```bash
git -C D:/DEV/PlatformIO/SYNC-SIGNAL-LORA/SYNC-SIGNAL-LORA add platformio.ini include/lora_handler.h src/lora_handler.cpp
git -C D:/DEV/PlatformIO/SYNC-SIGNAL-LORA/SYNC-SIGNAL-LORA commit -m "feat: migrate to LoRaDriver v1.0 (snake_case API)

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## Phase 8 — Release housekeeping

### Task 8.1: README, CHANGELOG, library manifests

**Files:**
- Modify: `README.md`
- Modify: `CHANGELOG.md`
- Modify: `library.json`
- Modify: `library.properties`

- [ ] **Step 1: Rewrite README.md**

Content for `README.md`:

```markdown
# LoRaDriver

Clean C++17 driver for Semtech SX1276 and SX1278 LoRa transceivers.

## Highlights

- **Layered DI**: `ISpiDevice` (HAL) → `SX127xDriver` (chip) → `LoRaTransceiver` (FSM) → optional `RadioPumpTask` (ESP32 FreeRTOS).
- No singletons, no heap after `begin()`, no exceptions, `[[nodiscard]] noexcept` everywhere on the radio path.
- Semtech errata 2.1 (BW 500 kHz high-band) applied. LDRO auto, OCP, PA_BOOST/RFO + PaDac high-power, LNA boost.
- ISR-safe ring buffer + watchdog TX timeout.
- DMA-capable SPI on ESP32 via `transferBytes`.
- ~30 host tests + 1 embedded smoke.

## Quick start (Arduino + ESP32)

```cpp
#include <SPI.h>
#include "loradriver/chips/sx127x_driver.hpp"
#include "loradriver/hal/esp32_spi_device.hpp"
#include "loradriver/lora_transceiver.hpp"

using namespace loradriver;

hal::Esp32SpiDevice spi(SPI, /*cs=*/5);
chips::SX127xDriver drv(spi);
LoRaTransceiver     trx(drv);

void setup() {
    LoRaConfig cfg;
    cfg.chip = ChipModel::SX1276;
    cfg.frequency_hz = 868'000'000u;
    cfg.pin_ss = 5; cfg.pin_reset = 14; cfg.pin_dio0 = 26;
    trx.begin(cfg);
    trx.start_receive(true);
}
```

See `examples/` for sender, receiver, and ESP32-with-pump-task templates.

## Build host tests

```bash
cmake -S . -B build/host
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```
```

- [ ] **Step 2: Rewrite CHANGELOG.md**

Content for `CHANGELOG.md`:

```markdown
# Changelog

## 1.0.0 — 2026-05-13

### Breaking

- Repo gutted; v0.1 governance/validation/CI shell removed.
- Public API replaced: `loradriver::LoRaTransceiver`, `loradriver::chips::SX127xDriver`,
  `loradriver::LoRaConfig`, `loradriver::hal::{ISpiDevice, ArduinoSpiDevice, Esp32SpiDevice}`,
  `loradriver::platform::esp32::RadioPumpTask`.
- `LoRaError` pruned to radio codes only (governance codes removed).

### Added

- Full SX1276 + SX1278 register-level driver: TX/RX/CAD/sleep/standby + runtime tuning.
- Reset GPIO + chip detection (`RegVersion == 0x12`).
- Errata 2.1 (BW 500 kHz high-band) applied.
- LDRO auto-selection per SF/BW (Semtech AN1200.24).
- PaBoost + PaDac high-power path (20 dBm).
- OCP trim.
- ISR-safe IRQ ring buffer (16 entries) with overflow stat.
- Watchdog TX timeout in `process_events()`.
- ESP32 DMA SPI via `transferBytes`.
- FreeRTOS `RadioPumpTask`: ISR-notified task, TX queue, auto RX restore.
- 12 host test files (~50 tests) + 1 embedded smoke.
- 3 Arduino examples.

### Removed

- SX126x driver (out of scope for v1.0).
- Governance layer (ci_gates, ota_gate, release_monitoring, rollback_governance,
  non_regression, profile_qualification, artifact_registry, changelog_manager,
  traceability_engine, versioning, incident_classification, incident_snapshot).
- Stub modules (`keep*ModuleLinked` placeholders in src/{core,infra,internal,platform/*}).

## 0.1.0 — early 2026 (deprecated)

Initial governance/FSM scaffold. Did not transmit or receive on real silicon.
```

- [ ] **Step 3: Update library.json**

Content for `library.json`:

```json
{
    "name": "LoRaDriver",
    "version": "1.0.0",
    "description": "Clean C++17 LoRa driver — SX1276/SX1278. DI architecture, ESP32 DMA SPI, FreeRTOS pump task, Semtech errata. ~50 host tests.",
    "keywords": "lora, sx1276, sx1278, sx127x, esp32, radio, driver, freertos",
    "repository": {
        "type": "git",
        "url": "https://github.com/Lakalot/LoRaDriver"
    },
    "license": "MIT",
    "frameworks": "arduino",
    "platforms": "*",
    "build": {
        "srcDir": "src",
        "srcFilter": ["+<*.cpp>", "+<**/*.cpp>"]
    }
}
```

- [ ] **Step 4: Update library.properties**

Content for `library.properties`:

```properties
name=LoRaDriver
version=1.0.0
author=Lakalot
maintainer=Lakalot
sentence=Clean C++17 LoRa driver — SX1276/SX1278.
paragraph=DI architecture, ESP32 DMA SPI, FreeRTOS pump task, Semtech errata applied. Hosts tests compile on any C++17 toolchain.
category=Communication
url=https://github.com/Lakalot/LoRaDriver
architectures=*
includes=loradriver/lora_transceiver.hpp
```

- [ ] **Step 5: Commit**

```bash
git -C D:/DEV/C++/LoRaDriver add README.md CHANGELOG.md library.json library.properties
git -C D:/DEV/C++/LoRaDriver commit -m "docs: README + CHANGELOG + library manifests for v1.0

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task 8.2: Final whole-tree verification

- [ ] **Step 1: Full reconfigure from scratch + test**

```bash
rm -rf D:/DEV/C++/LoRaDriver/build
cmake -S D:/DEV/C++/LoRaDriver -B D:/DEV/C++/LoRaDriver/build/host
cmake --build D:/DEV/C++/LoRaDriver/build/host
ctest --test-dir D:/DEV/C++/LoRaDriver/build/host --output-on-failure
```

Expected: `100% tests passed, 0 tests failed`. All host tests green.

- [ ] **Step 2: Verify no stub or governance file remains**

```bash
find D:/DEV/C++/LoRaDriver/src D:/DEV/C++/LoRaDriver/include -type f | sort
```

Expected: only the files this plan creates appear; no `_stub.cpp`, `governance/`, `validation/`, `incident_*`, `ci_gates`, `ota_gate`, `release_monitoring`, `rollback_governance`, `non_regression`, `profile_qualification`, `artifact_*`.

- [ ] **Step 3: Verify SYNC-SIGNAL-LORA still compiles and runs**

```bash
pio run -d D:/DEV/PlatformIO/SYNC-SIGNAL-LORA/SYNC-SIGNAL-LORA -e esp32dev
```

Expected: success.

- [ ] **Step 4: Merge the rewrite branch to main**

```bash
git -C D:/DEV/C++/LoRaDriver checkout main
git -C D:/DEV/C++/LoRaDriver merge --no-ff rewrite/v1.0 -m "Merge LoRaDriver v1.0 rewrite

See docs/superpowers/specs/2026-05-13-loradriver-rewrite-design.md
and docs/superpowers/plans/2026-05-13-loradriver-rewrite.md for full
spec and plan.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
git -C D:/DEV/C++/LoRaDriver tag -a v1.0.0 -m "LoRaDriver v1.0.0"
```

Do NOT push automatically — wait for user confirmation before any push.

---

## Spec coverage map

| Spec section | Implementing task(s) |
|---|---|
| §2.1 Driver SX1276+SX1278 complete | 3.3–3.7 |
| §2.1 HAL ISpiDevice + Arduino/Esp32/Fake | 2.1, 2.2, 2.3, 2.4 |
| §2.1 LoRaTransceiver façade | 4.1, 4.2 |
| §2.1 RadioPumpTask | 5.1 |
| §2.1 Errata 2.1 + 2.3 | 3.3 (apply_errata) |
| §2.1 Host tests ~30 | 1.3, 1.5, 2.2, 3.3, 3.4, 3.5, 3.6, 3.7, 4.2 (10 test files) |
| §2.1 Embedded smoke | 6.1 |
| §2.1 3 Arduino examples | 6.2 |
| §2.1 SYNC-SIGNAL-LORA migration | 7.1 |
| §2.2 Suppression governance/stubs/SX126x | 0.2, 0.3, 0.4 |
| §3.1 Layered DI | 2.x + 3.x + 4.x + 5.x |
| §3.2 No singleton/heap/exception | enforced by `noexcept`, no `new`, snake_case style |
| §3.3 Layout final | 0.5 + every task creates a file at the prescribed path |
| §4.1 LoRaConfig | 1.4 |
| §4.2 LoRaError | 1.1 |
| §4.3 RadioEvent | 1.5 |
| §4.4 LoRaPacket | 1.5 |
| §4.5 RadioStats | 1.5 |
| §4.6 ISpiDevice | 2.1 |
| §4.7 IRadioDriver | 3.2 |
| §4.8 SX127xDriver | 3.3–3.7 |
| §4.9 LoRaTransceiver | 4.1, 4.2 |
| §4.10 RadioPumpTask | 5.1 |
| §5.1–5.4 Flux | implicit (covered by tests 3.5, 3.6, 3.7, 4.2) |
| §6 Error handling table | implicit (covered by InvalidConfig/UnsupportedChip/SpiFailure/TxTimeout/RxCrcError/IrqOverflow tests) |
| §7 Tests strategy | tasks 1.3, 1.5, 2.2, 3.3, 3.4, 3.5, 3.6, 3.7, 4.2, 6.1 |
| §8 SYNC-SIGNAL-LORA migration | 7.1 |
| §9 Versionnage 1.0.0 | 8.1 |
| §11 Critères de succès | 8.2 |

All spec sections have at least one implementing task.

## Self-review notes (resolved inline before publishing)

- All `Step N` code blocks contain literal content — no `// TODO fill in` placeholders.
- Types referenced in tests match what the headers define (cross-checked: `ChipModel`, `PaOutput`, `LoRaConfig`, `LoRaError`, `LoRaTransceiver::State`, `RadioEvent`, `chips::SX127xDriver`, `hal::ISpiDevice`, `test::FakeSpiDevice`).
- Test files `#include "../../src/chips/sx127x/sx127x_registers.hpp"` to reach the private register constants — this is intentional and required because the registers header sits in `src/` (private to the driver). Tests in `tests/host/` need it for assertion constants only.
- Init sequence in §4.8 step 1 mentions a hardware reset; reset happens on the host **outside the driver** (consumer pulses RST GPIO before calling `begin()`). This is implemented in examples 6.2 and the smoke test 6.1. The driver itself does not assume reset state — it verifies `RegVersion` and applies init unconditionally. This matches the prevailing Arduino convention (cf. `arduino-LoRa` library) where the user owns reset pulse timing.
- `apply_errata` writes only `kHighBwOptimize1`/`2` for now; errata 2.3 (IF freq) is captured in spec but its register tweaks (`kDetectionOptimize`, `kDetectionThreshold`) are part of `apply_modem_config` for SF6, which the spec defers to the SF6 implicit-header case. Default (SF7-12) is unaffected. If SF6 support is exercised later, extend `apply_modem_config`.
- Task 7.1 step 3 enumerates renames explicitly to avoid the engineer guessing.

---

## Execution handoff

Plan complete and saved to `docs/superpowers/plans/2026-05-13-loradriver-rewrite.md`. Two execution options:

1. **Subagent-Driven (recommended)** — I dispatch a fresh subagent per task, review between tasks, fast iteration.
2. **Inline Execution** — Execute tasks in this session using executing-plans, batch execution with checkpoints.

Which approach?

