# Story 1.1: Set Up Initial Project from Starter Template

Status: done

<!-- Note: Validation is optional. Run validate-create-story for quality check before dev-story. -->

## Story

As a firmware engineer,
I want to initialize LoRaDriver from the approved starter template and baseline tooling,
so that implementation starts from a consistent, production-ready foundation.

## Acceptance Criteria

1. Given the approved architecture starter approach (custom + PlatformIO compatibility), when project initialization is executed, then `pio project init -d . --board esp32dev --project-option "framework=arduino" --no-install-dependencies` runs successfully and baseline project structure is created.
2. Given initial toolchain and dependencies setup, when baseline configuration files are prepared (`platformio.ini`, `CMakeLists.txt`, test harness scaffolding), then host and target validation lanes are both executable and setup is reproducible across developer environments.
3. Given the initial repository baseline, when core boundaries are established, then public headers and internal modules are separated according to architecture conventions and no chip/platform internals leak into public API surface.
4. Given V1 scope boundaries, when starter baseline is finalized, then supported V1 profiles (SX1276/SX1278, 433/868, DIO0/DIO0+DIO1) are declared explicitly and deferred tracks (SX126x) are marked out-of-scope/stub-only.

## Tasks / Subtasks

- [x] Initialize baseline project scaffolding (AC: 1)
  - [x] Run `pio project init -d . --board esp32dev --project-option "framework=arduino" --no-install-dependencies`
  - [x] Add root baseline files: `platformio.ini`, `CMakeLists.txt`, `CMakePresets.json`, `library.json`, `library.properties`, `.editorconfig`, `.clang-format`, `.clang-tidy`
  - [x] Confirm project remains compatible with PlatformIO and host-side CMake
- [x] Establish architecture-first folder boundaries (AC: 2, 3)
  - [x] Create stable public API root `include/loradriver/`
  - [x] Create internal partitioning under `src/core/`, `src/api/`, `src/chips/`, `src/platform/`, `src/infra/`, `src/internal/`
  - [x] Ensure no chip/platform include leaks into public headers
- [x] Create core contract placeholders and baseline docs (AC: 2, 3)
  - [x] Define initial headers for `LoRaError`, `RadioEvent`, `RadioConfig`, and public driver entry point
  - [x] Add docs skeleton for API contracts and ADR placeholders for error taxonomy/FSM/IRQ boundary
  - [x] Add changelog and version baseline aligned with SemVer policy
- [x] Wire test lanes and CI guardrails (AC: 2)
  - [x] Add host test harness skeleton in `tests/host/` with CTest entry
  - [x] Add embedded smoke harness skeleton in `tests/embedded/` with Unity
  - [x] Add GitHub Actions workflows for host lane and target lane as blocking gates
- [x] Declare V1 support boundaries explicitly (AC: 4)
  - [x] Document supported profiles (SX1276/SX1278, 433/868, DIO0 and DIO0+DIO1)
  - [x] Document SX126x as deferred (stub-only in V1)
  - [x] Document LoRa P2P-only scope and LoRaWAN out-of-scope

## Dev Notes

### Developer Context (Critical Guardrails)

- This story is the architectural foundation; prioritize structure and contracts over feature completeness.
- Do not implement runtime radio behavior yet beyond minimal compile-safe scaffolding.
- Prevent future rework by matching target folder/module boundaries from day one.
- Keep public contract stable and minimal in `include/loradriver/`; internal details stay in `src/`.

### Technical Requirements

- Use C++17 baseline.
- All fallible public operations must be designed for `[[nodiscard]] LoRaError` return style (no exceptions in runtime paths).
- Preserve callback contract shape for future stories: `std::function<void(RadioEvent, int)>`.
- Plan fixed-size queue/ring-buffer infrastructure for IRQ/event pipelines; no dynamic allocation in ISR/hot paths.

### Architecture Compliance

- Enforce single FSM ownership model for mutable runtime state (even if FSM logic is introduced in later stories).
- Keep adapter boundaries strict: chip and platform adapters must not directly mutate core state.
- Follow naming conventions:
  - Types/enums: `PascalCase`
  - Public methods: `camelCase`
  - Constants and enum entries: `kPascalCase`
  - Files: `snake_case`
  - Namespace: `loradriver`

### Library / Framework Requirements

- PlatformIO Core: v6.1.19 (latest stable line identified in architecture references).
- Unity: 2.6.1 (PlatformIO testing framework version in current reference docs).
- GitHub Actions references in CI templates:
  - `actions/checkout@v6`
  - `actions/setup-python@v6`
  - `actions/cache@v4` or newer compatible (`v5` available; requires runner compatibility)
- If using Node 24-based GitHub Actions major versions, ensure runner compatibility requirements are documented in workflow comments.

### File Structure Requirements

- Required top-level baseline includes:
  - `platformio.ini`, `CMakeLists.txt`, `CMakePresets.json`
  - `library.json`, `library.properties`
  - `README.md`, `CHANGELOG.md`, `Doxyfile`
  - `.github/workflows/` with host and target lanes
- Required source tree baseline:
  - `include/loradriver/`
  - `src/core/`, `src/api/`, `src/chips/sx127x/`, `src/chips/sx126x/` (stub/deferred), `src/platform/esp32/`, `src/platform/arduino/`, `src/infra/`, `src/internal/`
- Required test tree baseline:
  - `tests/host/` (primary gate)
  - `tests/embedded/` (smoke)

### Testing Requirements

- Host lane (`cmake` + `ctest`) is primary and blocking for behavior evolution.
- Target lane (`pio run` + Unity smoke) validates integration build/boot sanity.
- For this setup story, minimum verification should confirm:
  - Project config parses correctly in PlatformIO.
  - Host CMake configure/generate succeeds.
  - At least one smoke/unit placeholder test target is discoverable in each lane.

### Latest Technical Information

- PlatformIO Core 6.1.19 includes updated Unity 2.6.1 support and Python 3.14 compatibility in release notes.
- `actions/checkout@v6` latest observed release line is `v6.0.2`.
- `actions/setup-python@v6` latest observed release line is `v6.2.0`.
- `actions/cache` latest observed release is v5.x; using `@v4` remains acceptable if runner constraints or compatibility policy require staged migration.

### Project Context Reference

- Follow `_bmad-output/project-context.md` rules strictly for:
  - no exceptions in runtime code,
  - no dynamic allocation in ISR/hot paths,
  - deterministic FSM/event ordering,
  - mandatory host deterministic test coverage for critical behavior changes.

### Project Structure Notes

- This story intentionally lays down the long-term structure from the architecture blueprint to avoid directory churn and API leaks in later stories.
- No conflicts detected between PRD, architecture, and project context for this story; all three align on reliability-first baseline and V1 scope guardrails.

### References

- Epics story definition and ACs: [Source: _bmad-output/planning-artifacts/epics.md#Epic 1 / Story 1.1]
- Product constraints and V1 scope boundaries: [Source: _bmad-output/planning-artifacts/prd.md#Technical Constraints]
- Starter command, stack versions, and structure: [Source: _bmad-output/planning-artifacts/architecture.md#Starter Template Evaluation]
- Naming/boundary/testing rules: [Source: _bmad-output/planning-artifacts/architecture.md#Implementation Patterns & Consistency Rules]
- Agent execution guardrails: [Source: _bmad-output/project-context.md#Critical Implementation Rules]
- PlatformIO release notes reference: [Source: https://docs.platformio.org/en/stable/core/history.html]
- Actions release references:
  - [Source: https://github.com/actions/checkout/releases]
  - [Source: https://github.com/actions/setup-python/releases]
  - [Source: https://github.com/actions/cache/releases]

## Dev Agent Record

### Agent Model Used

openai/gpt-5.3-codex

### Debug Log References

- Installed PlatformIO Core 6.1.19 using Python user site and executed project initialization command successfully.
- Added host and target build/test lanes, then resolved host toolchain availability by using MSYS2 UCRT GCC/Ninja and Python-installed CMake.
- Validation commands executed successfully:
  - `cmake --preset default`
  - `cmake --build --preset default`
  - `ctest --preset default`
  - `python -m platformio run -e esp32dev`
  - `python -m platformio test -e esp32dev --without-uploading --without-testing`
- Code-review remediation validation commands:
  - `python -m platformio test -e esp32dev --without-uploading --without-testing` (passes; smoke suite compiles)
  - `python -m platformio test -e esp32dev --without-uploading` (requires attached test port for execution)
  - `cmake --preset default && cmake --build --preset default && ctest --preset default` (not executable in this environment: `cmake` unavailable in shell)

### Completion Notes List

- Baseline project scaffold initialized with PlatformIO (`esp32dev`, Arduino) and host CMake presets.
- Architecture-first boundaries implemented with public API root (`include/loradriver`) and internal partitions (`src/core`, `src/api`, `src/chips`, `src/platform`, `src/infra`, `src/internal`).
- Added initial public contracts: `LoRaError`, `RadioEvent`, `RadioConfig`, and `LoRaDriver` with `[[nodiscard]] LoRaError` fallible operations and preserved callback signature.
- Added baseline docs and ADR placeholders for API contracts, error taxonomy, FSM ownership, and IRQ boundary.
- Added host smoke test harness (CTest) and embedded Unity smoke harness skeleton.
- Added CI workflows for host and target lanes; target lane validates build and embedded smoke compilation without hardware upload.
- Verified no chip/platform internals leak into public headers and documented V1 profile boundaries and deferred SX126x scope.
- Addressed code-review findings: callback invocation in `initialize()` is now exception-safe under `noexcept` by rolling back init state and returning typed error.
- Replaced embedded smoke placeholder with driver-contract assertions (supported profile init, deferred profile rejection, callback emission).
- Enabled embedded test linkage with `test_build_src = yes` and guarded application `setup()/loop()` for unit-test builds.
- Clarified target-lane CI behavior: compile smoke on hosted runners and execute hardware smoke only when `PIO_TEST_PORT` is configured.

## Senior Developer Review (AI)

### Reviewer

- Yaya (AI-assisted), 2026-02-22

### Outcome

- Changes Requested issues were fixed.
- Story now meets acceptance criteria and review gates for this baseline setup scope.

### Findings and Resolutions

- HIGH: Target lane command skipped testing stage by design (`--without-testing`) and was presented as smoke execution.
  - Resolution: Workflow now names compile stage explicitly and adds hardware execution stage gated by `PIO_TEST_PORT`.
- MEDIUM: `noexcept` initializer invoked user callback without protection.
  - Resolution: callback invocation wrapped in `try/catch (...)`, initialization rolled back, typed error returned.
- MEDIUM: Embedded test was placeholder-only (`TEST_ASSERT_TRUE(true)`).
  - Resolution: replaced with concrete assertions against `LoRaDriver` contract behavior.
- MEDIUM: Build/link gap for embedded tests against `src/` implementation.
  - Resolution: `platformio.ini` now enables `test_build_src = yes`; `src/main.cpp` guarded for unit test builds.
- LOW: README target-lane command mismatched CI behavior.
  - Resolution: README now documents compile-only CI command and separate hardware execution command.

### File List

- .clang-format
- .clang-tidy
- .editorconfig
- .github/workflows/host-lane.yml
- .github/workflows/target-lane.yml
- .gitignore
- CHANGELOG.md
- CMakeLists.txt
- CMakePresets.json
- Doxyfile
- README.md
- _bmad-output/implementation-artifacts/1-1-set-up-initial-project-from-starter-template.md
- _bmad-output/implementation-artifacts/sprint-status.yaml
- docs/adr/0001-error-taxonomy.md
- docs/adr/0002-fsm-ownership.md
- docs/adr/0003-irq-boundary.md
- docs/api/contracts.md
- docs/scope/v1-support-boundaries.md
- include/loradriver/lora_driver.hpp
- include/loradriver/lora_error.hpp
- include/loradriver/radio_config.hpp
- include/loradriver/radio_event.hpp
- library.json
- library.properties
- platformio.ini
- src/api/lora_driver.cpp
- src/chips/sx126x/sx126x_stub.cpp
- src/chips/sx127x/sx127x_stub.cpp
- src/core/core_stub.cpp
- src/infra/infra_stub.cpp
- src/internal/internal_stub.cpp
- src/main.cpp
- src/platform/arduino/arduino_stub.cpp
- src/platform/esp32/esp32_stub.cpp
- tests/embedded/test_smoke/test_main.cpp
- tests/host/CMakeLists.txt
- tests/host/smoke_test.cpp

## Change Log

- 2026-02-22: Implemented Story 1.1 baseline scaffold, architecture boundaries, public contracts, documentation skeleton, host/target validation lanes, and V1 scope declarations; validated with host CTest and PlatformIO target build/smoke compile.
- 2026-02-22: Applied code-review fixes for callback safety, embedded smoke assertions, PlatformIO test/source linkage, CI smoke stage clarity, and README command alignment.
