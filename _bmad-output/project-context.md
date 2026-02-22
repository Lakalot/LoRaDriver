---
project_name: 'LoRaDriver'
user_name: 'Yaya'
date: '2026-02-22T21:00:00'
sections_completed: ['technology_stack', 'language_rules', 'framework_rules', 'testing_rules', 'quality_rules', 'workflow_rules', 'anti_patterns']
status: 'complete'
rule_count: 36
optimized_for_llm: true
---

# Project Context for AI Agents

_This file contains critical rules and patterns that AI agents must follow when implementing code in this project. Focus on unobvious details that agents might otherwise miss._

---

## Technology Stack & Versions

- Language: C++17
- Target platform: ESP32 (Arduino framework via PlatformIO)
- Build systems: PlatformIO Core v6.1.19 (target), CMake/CTest (host)
- Test frameworks: CTest (primary host gate), Unity 2.6.1 (embedded smoke)
- CI/CD: GitHub Actions (`actions/checkout@v6`, `actions/setup-python@v6`, `actions/cache@v4`)
- Documentation/versioning: Doxygen 1.16.1, Semantic Versioning 2.0.0
- Scope note: V1 hardware focus is SX1276/SX1278; SX126x is deferred (V1-bis/stub only)

## Critical Implementation Rules

### Language-Specific Rules

- Return typed status on all fallible paths with `[[nodiscard]] LoRaError`; do not use exceptions in library runtime code.
- Keep ISR and hot-path code allocation-free; use fixed-size ring buffers/queues for IRQ/event transport.
- Enforce deterministic state transitions through a single FSM authority; adapters must not mutate FSM-owned state directly.
- Keep public contract stable in `include/loradriver/`; do not leak chip/platform internals in public headers.
- Use naming conventions consistently: `PascalCase` types, `camelCase` public methods, `kPascalCase` enum values/constants, trailing `_` for member fields.
- Validate config in two phases (static invariants, then profile compatibility) before applying runtime changes.

### Framework-Specific Rules

- Use PlatformIO as the source of truth for target builds/environments; keep board/framework settings aligned with `platformio.ini`.
- Keep Arduino/ESP32 integration isolated in platform adapters; core logic remains framework-agnostic and testable on host.
- Restrict ISR responsibilities to lightweight signal capture and event enqueue; perform processing in non-ISR context.
- Preserve callback contract shape (`std::function<void(RadioEvent, int)>`) and maintain deterministic event ordering.
- Treat examples as integration references only; do not use `examples/` as quality gates or substitute for tests.
- Keep SX1276/SX1278 as V1 enforced path; avoid introducing SX126x runtime behavior beyond explicit deferred/stub scope.

### Testing Rules

- Keep host tests (`tests/host/`) as the primary quality gate; every critical behavior change must include deterministic host coverage.
- Use embedded tests (`tests/embedded/`) as smoke validation only; do not move core logic verification exclusively to target hardware.
- Separate unit and non-regression intent clearly (e.g., transition rules vs timeout/recovery incident replay).
- Prefer fakes/fixtures for chip/platform dependencies in host tests; avoid non-deterministic timing or hardware-coupled assumptions.
- Test every new/changed recovery path with explicit bounded retry/timeout expectations.
- Validate queue/ring-buffer boundary behavior (capacity, overflow handling, ordering) when modifying IRQ/event flows.

### Code Quality & Style Rules

- Keep public headers lean and stable in `include/loradriver/`; place implementation details in `src/` internals.
- Maintain strict module boundaries (`core`, `chips`, `platform`, `infra`) and avoid cross-layer shortcuts.
- Use one primary type per header/source pair when practical; avoid cyclic includes and hidden coupling.
- Keep naming conventions consistent: `snake_case` files, lowercase namespaces, trailing `_` member fields, `kPascalCase` constants/enums.
- Prefer explicit typed contracts over ad-hoc strings/maps for diagnostics and state snapshots.
- Document non-obvious invariants at architecture/ADR level; avoid excessive inline comments as a substitute for clear structure.

### Development Workflow Rules

- Use clear, scoped commit messages that explain intent (why), not only file-level changes.
- Always commit and push changes when a task is complete and validated; avoid leaving implementation work unpushed.
- Keep commits atomic by concern (contract change, FSM behavior, tests) to simplify review and rollback.
- Treat CI gates as blocking: host deterministic tests and target smoke checks must pass before merge/release.
- Update architecture/context docs when introducing new error codes, state transitions, or boundary changes.
- Preserve SemVer discipline for public API changes in `include/loradriver/` and reflect impacts in changelog/release notes.

### Critical Don't-Miss Rules

- Do not throw exceptions in runtime driver paths; all failures must be surfaced through typed `LoRaError`.
- Do not allocate memory in ISR or radio hot paths; use bounded pre-allocated structures only.
- Do not bypass FSM ownership with direct state mutation from adapters/platform code.
- Do not expose chip-specific internals through public headers or unstable API signatures.
- Do not add ad-hoc error strings/taxonomies; extend the typed error contract and corresponding tests/docs.
- Do not merge behavior changes without deterministic host tests and relevant recovery/non-regression coverage.

---

## Usage Guidelines

**For AI Agents:**

- Read this file before implementing any code.
- Follow all rules exactly as documented.
- When in doubt, prefer the more restrictive option.
- Update this file if new patterns emerge.

**For Humans:**

- Keep this file lean and focused on agent needs.
- Update when technology stack changes.
- Review quarterly for outdated rules.
- Remove rules that become obvious over time.

Last Updated: 2026-02-22
