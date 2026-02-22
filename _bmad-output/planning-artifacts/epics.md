---
stepsCompleted:
  - step-01-validate-prerequisites
  - step-02-design-epics
  - step-03-create-stories
  - step-04-final-validation
inputDocuments:
  - D:\DEV\C++\LoRaDriver\_bmad-output\planning-artifacts\prd.md
  - D:\DEV\C++\LoRaDriver\_bmad-output\planning-artifacts\architecture.md
  - D:\DEV\C++\LoRaDriver\_bmad-output\project-context.md
---

# LoRaDriver - Epic Breakdown

## Overview

This document provides the complete epic and story breakdown for LoRaDriver, decomposing the requirements from the PRD, UX Design if it exists, and Architecture requirements into implementable stories.

## Requirements Inventory

### Functional Requirements

FR1: Firmware engineers can initialize the radio subsystem for supported hardware profiles.
FR2: Firmware engineers can transition the radio subsystem between active and low-power operational states.
FR3: Firmware engineers can start receive operations and return to receive mode after completed transmissions.
FR4: Firmware engineers can trigger packet transmission flows for supported radio configurations.
FR5: The system can recover radio operation after timeout events without requiring full board restart.
FR6: The system can recover radio operation after sleep/wakeup cycles while preserving operational continuity.
FR7: Firmware engineers can configure core LoRa parameters required for V1 operation.
FR8: Firmware engineers can apply profile-specific configuration for supported SX127x variants and IRQ wiring modes.
FR9: The system can validate configuration inputs and reject invalid or inconsistent radio settings.
FR10: The system can expose explicit compatibility boundaries for supported MCU, module, and band combinations.
FR11: Product teams can classify hardware support status by profile (validated, secondary, deferred).
FR12: Product teams can enforce explicit scope boundaries between V1 and deferred capability tracks.
FR13: Firmware engineers can observe explicit radio state transitions during runtime flows.
FR14: Firmware engineers can receive typed error outcomes for diagnosable radio failure modes.
FR15: The system can surface radio event signals required for operational troubleshooting across supported IRQ modes.
FR16: Support engineers can access incident-level diagnostic context including version, chip family, configuration, and time context.
FR17: Support engineers can classify field incidents using standardized radio failure categories.
FR18: Product teams can retain validation and incident artifacts according to defined retention policy.
FR19: QA engineers can execute a repeatable validation matrix covering supported chip, band, and IRQ profile combinations.
FR20: Release owners can enforce blocking quality gates for critical radio scenarios before release approval.
FR21: Product teams can determine release readiness using explicit go/no-go criteria tied to radio stability signals.
FR22: QA engineers can run non-regression validation on previously stabilized critical radio flows.
FR23: Product teams can require evidence of successful recovery behavior in timeout and sleep/wakeup scenarios before go-live.
FR24: Product teams can track and report post-release critical radio incident outcomes for defined monitoring windows.
FR25: Release owners can gate OTA rollout based on radio health indicators.
FR26: Operations teams can execute rollback to a last-known-good firmware baseline when degradation is detected.
FR27: Operations teams can capture minimal post-update telemetry required for radio health assessment.
FR28: Product teams can block progressive rollout when post-update telemetry indicates regression risk.
FR29: Platform integrators can integrate driver capabilities into product firmware pipelines without custom per-project forks as a default path.
FR30: Product teams can manage versioning and changelog practices to maintain traceable release evolution and prevent silent regression risk.

### NonFunctional Requirements

NFR1: The system shall achieve radio initialization failure rate below 1% on the V1 reference validation matrix.
NFR2: The system shall recover deterministically from timeout and sleep/wakeup transitions without requiring board reset.
NFR3: The system shall complete a release cycle including non-trivial radio changes with zero critical radio incidents during the first 30 days post-release (team objective gate).
NFR4: The system shall provide explicit runtime state visibility and typed error outputs for diagnosable failure handling.
NFR5: The system shall maintain stable core radio flows (`begin`, `send`, `startReceive`, `sleep`) in both IRQ profiles, with controlled degradation only in event granularity for DIO0-only mode.
NFR6: The system shall operate with default SPI range 4-8 MHz on validated V1 hardware profiles without introducing instability in core radio flows.
NFR7: Under prolonged ESP32 runtime load tests, the system shall not enter FSM deadlock or unbounded IRQ handling stalls.
NFR8: The system shall preserve deterministic recovery latency behavior for timeout/sleep-wakeup paths across validated SX127x profiles (latency values measured and tracked per profile).
NFR9: The system shall enforce radio configuration integrity checks and reject invalid/inconsistent configuration with explicit error signaling.
NFR10: The delivery process shall enforce repository and CI hygiene controls (protected branches, PR review workflow, dependency scanning) for all release candidates.
NFR11: OTA artifacts used for deployment shall have verifiable provenance through the controlled CI/signing pipeline.
NFR12: Security responsibilities shall remain explicitly partitioned: driver layer for transport reliability/diagnostics, application layer for payload cryptography and key management.
NFR13: The validation framework shall support repeatable execution across the SX127x profile matrix (chip x band x IRQ mode) without manual per-run reconfiguration.
NFR14: The release governance model shall support multi-release operation (regular incremental + hotfix) without bypassing radio quality gates.
NFR15: The capability baseline shall remain reusable across multiple product firmware codebases as a standard integration path.
NFR16: CI shall enforce blocking gates on critical radio scenarios (init, TX/RX, IRQ, timeout) for pull requests affecting radio behavior.
NFR17: OTA rollout shall be blocked when radio KPI degradation is detected against defined go/no-go thresholds.
NFR18: Rollback to last-known-good firmware shall be operationally available for degraded deployments.
NFR19: Post-update telemetry shall include, at minimum: firmware version, radio family, active band, init failure rate, timeout/IRQ overflow events, and TX/RX success rate.
NFR20: Validation and incident artifacts shall be retained for 90-180 days to support traceability and regression investigations.

### Additional Requirements

- Starter template approach is custom from scratch with PlatformIO compatibility; initial implementation should run `pio project init -d . --board esp32dev --project-option "framework=arduino" --no-install-dependencies` and then harden custom architecture boundaries.
- V1 technical focus is SX1276/SX1278 on ESP32; SX126x is deferred to V1-bis and should remain stub/deferred in V1 implementation planning.
- Infrastructure and release architecture must include host-first CI gates (`cmake`/`ctest`) plus target build/smoke checks (PlatformIO/Unity), with blocking gates on init, TX/RX, IRQ, timeout, and recovery.
- Runtime architecture must enforce FSM ownership, deterministic transitions, typed `LoRaError` outcomes, and bounded fixed-size event/IRQ queues (no dynamic allocation in ISR/hot paths).
- OTA governance requires rollout gating by radio KPIs and mandatory rollback to last-known-good when degradation is detected.
- Incident and validation evidence must be retained for traceability windows (90-180 days), including release/firmware/chip/profile context.
- Integration architecture should preserve stable public contracts in `include/loradriver/` and isolate chip/platform internals behind adapters.
- Project context rules reinforce deterministic callback/event contract (`std::function<void(RadioEvent, int)>`) and require host deterministic tests for all critical behavior changes.
- Scope boundary is explicit: LoRa P2P only in V1; LoRaWAN and advanced low-power contractual targets are out of V1 scope.

### FR Coverage Map

FR1: Epic 1 - Initialisation radio sur profils supportes
FR2: Epic 1 - Transitions etats operationnels/low-power
FR3: Epic 1 - Reception et retour RX post-TX
FR4: Epic 1 - Flux transmission paquets
FR5: Epic 2 - Recovery apres timeout sans reset
FR6: Epic 2 - Recovery sleep/wakeup
FR7: Epic 1 - Configuration parametres LoRa
FR8: Epic 1 - Profils SX127x et modes IRQ
FR9: Epic 1 - Validation de configuration coherente
FR10: Epic 1 - Bornes de compatibilite explicites
FR11: Epic 3 - Classification statut hardware par profil pour gouvernance QA/release
FR12: Epic 1 - Frontieres V1 vs capacites differees
FR13: Epic 2 - Visibilite des transitions d'etat
FR14: Epic 2 - Erreurs typees diagnosables
FR15: Epic 2 - Signaux evenements radio pour troubleshooting
FR16: Epic 2 - Contexte incident exploitable
FR17: Epic 2 - Classification standard incidents terrain
FR18: Epic 3 - Retention artefacts validation/incidents pour gouvernance qualite
FR19: Epic 3 - Matrice validation repetitive
FR20: Epic 3 - Quality gates bloquants pre-release
FR21: Epic 3 - Criteres go/no-go explicites
FR22: Epic 3 - Non-regression flows critiques
FR23: Epic 3 - Preuve recovery avant go-live
FR24: Epic 4 - Suivi incidents critiques post-release pour continuite de service
FR25: Epic 4 - Gating OTA par indicateurs sante radio
FR26: Epic 4 - Rollback last-known-good
FR27: Epic 4 - Telemetrie minimale post-update
FR28: Epic 4 - Blocage rollout progressif en risque de regression
FR29: Epic 1 - Integration standard sans forks systematiques
FR30: Epic 3 - Discipline versioning/changelog tracable

## Epic List

### Epic 1: Foundation Radio Fiable & Integrable
Permettre aux equipes firmware d'initialiser, configurer et exploiter les flows radio coeur de facon deterministe sur le perimetre V1.
**FRs covered:** FR1, FR2, FR3, FR4, FR7, FR8, FR9, FR10, FR12, FR29

### Epic 2: Resilience Operationnelle & Diagnostics Actionnables
Permettre la recuperation robuste en conditions reelles et rendre les incidents radio reproductibles et diagnosables.
**FRs covered:** FR5, FR6, FR13, FR14, FR15, FR16, FR17

### Epic 3: Validation Systeme & Gouvernance Release
Permettre a QA/Release d'avoir des quality gates bloquants, une qualification formelle des profils et des decisions go/no-go objectives.
**FRs covered:** FR11, FR18, FR19, FR20, FR21, FR22, FR23, FR30

### Epic 4: Securite OTA & Continuite de Service en Production
Permettre des deploiements OTA maitrises avec suivi post-release, blocage preventif et rollback fiable en cas de regression.
**FRs covered:** FR24, FR25, FR26, FR27, FR28

## Epic 1: Foundation Radio Fiable & Integrable

Permettre aux equipes firmware d'initialiser, configurer et exploiter les flows radio coeur de facon deterministe sur le perimetre V1.

### Story 1.1: Set Up Initial Project from Starter Template

As a firmware engineer,
I want to initialize LoRaDriver from the approved starter template and baseline tooling,
So that implementation starts from a consistent, production-ready foundation.

**FRs implemented:** FR1, FR10, FR12, FR29

**Acceptance Criteria:**

**Given** the approved architecture starter approach (custom + PlatformIO compatibility)
**When** project initialization is executed
**Then** `pio project init -d . --board esp32dev --project-option "framework=arduino" --no-install-dependencies` is run successfully
**And** baseline project structure is created for controlled implementation startup.

**Given** initial toolchain and dependencies setup
**When** baseline configuration files are prepared (`platformio.ini`, `CMakeLists.txt`, test harness scaffolding)
**Then** host and target validation lanes are both executable
**And** setup is reproducible across developer environments.

**Given** the initial repository baseline
**When** core boundaries are established
**Then** public headers and internal modules are separated according to architecture conventions
**And** no chip/platform internals leak into public API surface.

**Given** V1 scope boundaries
**When** starter baseline is finalized
**Then** supported V1 profiles (SX1276/SX1278, 433/868, DIO0/DIO0+DIO1) are declared explicitly
**And** deferred tracks (e.g., SX126x production support) are marked as out-of-scope/stub-only.

### Story 1.2: Implement Deterministic Radio Initialization for V1 Profiles

As a firmware engineer,
I want deterministic initialization for supported SX127x hardware and IRQ profiles,
So that I can bring up radio reliably on target boards without ad-hoc retries.

**FRs implemented:** FR1, FR7, FR8, FR9, FR10, FR12

**Acceptance Criteria:**

**Given** a supported V1 profile (SX1276/SX1278, 433/868, DIO0 or DIO0+DIO1)
**When** `begin()` is called with valid config
**Then** initialization completes successfully and enters ready state
**And** emitted state/events reflect a deterministic startup path.

**Given** unsupported or out-of-scope profile combinations
**When** initialization is requested
**Then** initialization is rejected with explicit `LoRaError` classification
**And** failure reason is diagnosable by profile/band/irq context.

**Given** initialization under expected SPI operating range (4-8 MHz)
**When** startup executes on validated V1 hardware targets
**Then** init behavior remains stable and reproducible
**And** no hidden fallback mutates user-provided configuration.

**Given** initialization fails on a valid target due to runtime condition
**When** failure is reported
**Then** typed error and minimum diagnostic context are available
**And** system does not deadlock or require undefined recovery behavior.

### Story 1.3: Deliver Core TX/RX Flows with Deterministic State Transitions

As a firmware engineer,
I want reliable send/receive core flows with explicit state transitions,
So that application logic can depend on predictable radio behavior.

**FRs implemented:** FR2, FR3, FR4

**Acceptance Criteria:**

**Given** the radio is initialized and ready
**When** `send()` is invoked with valid payload/config
**Then** transmission progresses through explicit deterministic states
**And** completion/failure is surfaced through typed event/error outcomes.

**Given** receive mode is started via `startReceive()`
**When** a valid packet is received
**Then** RX completion is emitted through the defined callback contract
**And** runtime can return to the expected listening/next state deterministically.

**Given** DIO0-only and DIO0+DIO1 IRQ profiles
**When** core TX/RX flows are executed
**Then** both profiles preserve functional parity for core behaviors
**And** any DIO0-only degradation is limited to event granularity, not correctness/stability.

**Given** normal operation over repeated TX/RX cycles
**When** state transitions are observed
**Then** no illegal transition path is taken
**And** transition outcomes remain reproducible for troubleshooting and testing.

### Story 1.4: Provide a Standard Integration Path Across Product Firmware Projects

As a platform integration engineer,
I want a standard integration path for LoRaDriver in firmware pipelines,
So that teams can adopt the driver without creating per-project forks by default.

**FRs implemented:** FR29

**Acceptance Criteria:**

**Given** a product firmware repository integrating LoRaDriver
**When** the integration follows the documented baseline contract
**Then** core capabilities are consumed through stable public interfaces
**And** no project-specific fork is required for standard V1 use cases.

**Given** chip/platform-specific behavior is needed internally
**When** implementation extensions are made
**Then** they are isolated behind internal adapters/modules
**And** public API stability in `include/loradriver/` is preserved.

**Given** teams onboard a new SX127x V1 product profile
**When** they apply the reference integration workflow
**Then** configuration and runtime behavior remain consistent with existing products
**And** deviation points are explicit and governable.

**Given** integration guidance and examples are maintained
**When** changes affect public usage patterns
**Then** documentation/changelog are updated with migration-safe notes
**And** standardization is reinforced across teams.

## Epic 2: Resilience Operationnelle & Diagnostics Actionnables

Permettre la recuperation robuste en conditions reelles et rendre les incidents radio reproductibles et diagnosables.

### Story 2.1: Implement Deterministic Recovery for Timeout and Sleep/Wakeup Paths

As a firmware engineer,
I want deterministic recovery after timeout and sleep/wakeup transitions,
So that radio service continuity is preserved without board reset.

**FRs implemented:** FR5, FR6

**Acceptance Criteria:**

**Given** runtime enters timeout conditions during RX/TX lifecycle
**When** recovery logic is applied
**Then** the FSM returns to a valid operational state deterministically
**And** recovery outcomes are surfaced through typed errors/events.

**Given** the driver transitions to sleep and back to active operation
**When** wakeup and reactivation are executed
**Then** radio operation resumes without undefined intermediate states
**And** no full board restart is required.

**Given** repeated timeout and sleep/wakeup scenarios
**When** non-regression tests execute
**Then** behavior remains stable and reproducible
**And** recovery latency/sequence stays within expected profile bounds.

### Story 2.2: Deliver Operational Eventing and Incident Diagnostic Context

As a support engineer,
I want actionable radio events and incident context,
So that field issues can be triaged quickly and routed with evidence.

**FRs implemented:** FR13, FR14, FR15, FR16

**Acceptance Criteria:**

**Given** radio runtime events occur across supported IRQ profiles
**When** events are emitted
**Then** they include deterministic signal semantics for troubleshooting
**And** event behavior remains consistent across DIO0 and DIO0+DIO1 modes.

**Given** a runtime radio failure or anomalous behavior
**When** diagnostics are captured
**Then** incident context includes firmware/driver version, chip family, profile/config, and time context
**And** the output is sufficient for reproducible investigation.

**Given** diagnostic collection is integrated into support workflows
**When** incidents are handed off to engineering
**Then** required evidence is attached in a standardized format
**And** ambiguous "non reproducible" tickets are reduced.

### Story 2.3: Standardize Field Incident Classification for Fast Response

As a reliability owner,
I want a standardized taxonomy for radio incident categories,
So that response prioritization and remediation paths are consistent across teams.

**FRs implemented:** FR17

**Acceptance Criteria:**

**Given** incident patterns from timeout, IRQ, configuration, and runtime transitions
**When** incidents are classified
**Then** categories are explicit, stable, and mapped to probable cause types
**And** support/engineering use the same shared vocabulary.

**Given** a new incident enters triage
**When** classification is applied
**Then** expected escalation path and ownership are unambiguous
**And** remediation playbooks can be selected without ad-hoc interpretation.

**Given** classification rules evolve from production learning
**When** updates are made
**Then** taxonomy changes remain backward-readable for trend analysis
**And** existing historical incident data remains comparable.

## Epic 3: Validation Systeme & Gouvernance Release

Permettre a QA/Release d'avoir des quality gates bloquants, une qualification formelle des profils et des decisions go/no-go objectives.

### Story 3.1: Define Hardware Profile Qualification Matrix and Support Status Governance

As a QA lead,
I want a formal profile qualification matrix with support-status classification,
So that release decisions are based on explicit validation evidence.

**FRs implemented:** FR11, FR19

**Acceptance Criteria:**

**Given** V1 supported profile combinations (chip, band, IRQ mode)
**When** qualification scope is defined
**Then** each profile has a clear status (validated, secondary, deferred)
**And** status criteria are linked to reproducible validation outcomes.

**Given** a release candidate is evaluated
**When** matrix evidence is reviewed
**Then** pass/fail results are traceable per profile
**And** unsupported/deferred profiles are prevented from implicit promotion.

**Given** scope changes or newly proposed profiles
**When** governance review occurs
**Then** classification updates are explicitly approved
**And** release gates are updated before rollout exposure.

### Story 3.2: Implement Blocking CI Quality Gates and Explicit Go/No-Go Rules

As a release owner,
I want blocking CI quality gates with explicit go/no-go thresholds,
So that unstable radio behavior never reaches production by default.

**FRs implemented:** FR20, FR21

**Acceptance Criteria:**

**Given** a pull request affecting radio behavior
**When** CI runs host and target validation lanes
**Then** critical scenarios (init, TX/RX, IRQ, timeout, recovery) are blocking checks
**And** merge/release progression is denied on gate failure.

**Given** release readiness review
**When** go/no-go criteria are evaluated
**Then** decisions are based on predefined measurable thresholds
**And** waiver paths, if any, require explicit documented approval.

**Given** hotfix and regular release channels
**When** pipelines execute
**Then** gate policy remains consistent across channels
**And** urgency does not bypass mandatory critical checks.

### Story 3.3: Add Non-Regression Suites and Recovery Evidence Requirements

As a QA engineer,
I want non-regression suites and mandatory recovery evidence for release,
So that previously stabilized radio paths do not silently degrade.

**FRs implemented:** FR22, FR23

**Acceptance Criteria:**

**Given** stabilized critical flows from prior releases
**When** non-regression suites are executed
**Then** baseline outcomes are revalidated deterministically
**And** regressions are surfaced with reproducible diagnostics.

**Given** timeout and sleep/wakeup recovery capabilities
**When** release evidence is assembled
**Then** explicit proof of successful recovery behavior is included
**And** go-live is blocked if required recovery evidence is missing.

**Given** new bugfixes in critical runtime areas
**When** tests are updated
**Then** added cases map to observed incident patterns
**And** they remain in the suite for future release protection.

### Story 3.4: Establish Artifact Retention and Versioning Governance for Traceable Releases

As a product team,
I want validation/incident artifact retention and disciplined versioning governance,
So that release evolution is traceable and silent regression risk is reduced.

**FRs implemented:** FR18, FR30

**Acceptance Criteria:**

**Given** validation outputs and incident evidence
**When** artifacts are stored
**Then** retention policy (90-180 days) is enforced
**And** retrieval supports audits and regression investigation.

**Given** public behavior changes or release updates
**When** versions are published
**Then** changelog entries describe user-impacting changes clearly
**And** SemVer intent is applied consistently.

**Given** teams investigate a post-release regression
**When** evidence and version history are reviewed
**Then** linkage across build, test, and release records is intact
**And** root-cause analysis can be completed without missing governance data.

## Epic 4: Securite OTA & Continuite de Service en Production

Permettre des deploiements OTA maitrises avec suivi post-release, blocage preventif et rollback fiable en cas de regression.

### Story 4.1: Gate OTA Rollout with Radio Health Signals and Minimum Telemetry

As an operations engineer,
I want OTA rollout decisions gated by radio health indicators and minimum telemetry,
So that degraded firmware is detected before broad production impact.

**FRs implemented:** FR25, FR27, FR28

**Acceptance Criteria:**

**Given** an OTA rollout candidate
**When** gating evaluates radio health indicators
**Then** rollout progression depends on predefined KPI thresholds
**And** candidates failing thresholds are blocked from expansion.

**Given** post-update telemetry collection
**When** data is ingested
**Then** minimum fields include firmware version, radio family, active band, init failure rate, timeout/IRQ overflow events, and TX/RX success rate
**And** telemetry quality is sufficient for operational decision-making.

**Given** early deployment wave results
**When** KPI trend indicates degradation risk
**Then** rollout state is moved to blocked/hold
**And** escalation is triggered with attached evidence.

### Story 4.2: Implement Last-Known-Good Rollback Execution Path

As a release owner,
I want a reliable rollback to last-known-good firmware,
So that service continuity is restored quickly when OTA regression is detected.

**FRs implemented:** FR26

**Acceptance Criteria:**

**Given** a deployed firmware version fails health thresholds
**When** rollback is triggered
**Then** deployment reverts to the designated last-known-good baseline
**And** rollback status is observable end-to-end.

**Given** rollback execution under incident pressure
**When** operations follow the playbook
**Then** required prechecks and postchecks are explicit and fast
**And** no ambiguous manual branching is required.

**Given** rollback events are completed
**When** incident review occurs
**Then** telemetry and timeline evidence are preserved
**And** follow-up corrective actions can be prioritized with confidence.

### Story 4.3: Track Post-Release Critical Incidents and Enforce Progressive Rollout Blocking

As a product and operations team,
I want critical post-release incidents tracked and tied to rollout controls,
So that production risk is actively managed during monitoring windows.

**FRs implemented:** FR24

**Acceptance Criteria:**

**Given** defined post-release monitoring windows
**When** critical radio incidents are reported
**Then** incidents are tracked against release/version/profile context
**And** trend visibility supports rapid risk assessment.

**Given** progressive rollout is active
**When** incident trend or KPI signals cross risk thresholds
**Then** progressive expansion is blocked automatically or by explicit policy
**And** decision rationale is recorded for governance.

**Given** release monitoring concludes
**When** outcomes are reviewed
**Then** incident metrics are compared against target objectives
**And** lessons learned feed future gate threshold calibration.
