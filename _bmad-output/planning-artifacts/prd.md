---
stepsCompleted:
  - step-01-init
  - step-02-discovery
  - step-02b-vision
  - step-02c-executive-summary
  - step-03-success
  - step-04-journeys
  - step-05-domain
  - step-06-innovation
  - step-07-project-type
  - step-08-scoping
  - step-09-functional
  - step-10-nonfunctional
  - step-11-polish
  - step-12-complete
inputDocuments:
  - D:\DEV\C++\LoRaDriver\_bmad-output\planning-artifacts\product-brief-LoRaDriver-2026-02-22.md
documentCounts:
  briefCount: 1
  researchCount: 0
  brainstormingCount: 0
  projectDocsCount: 0
workflowType: 'prd'
projectName: 'LoRaDriver'
userName: 'Yaya'
date: '2026-02-22T00:00:00'
classification:
  projectType: iot_embedded
  domain: general
  complexity: medium
  projectContext: greenfield
---

# Product Requirements Document - LoRaDriver

**Author:** Yaya
**Date:** 2026-02-22

## Executive Summary

LoRaDriver is an embedded IoT radio foundation built to make LoRa behavior predictable in real deployment conditions, not just functional in lab demos. It targets teams shipping remote monitoring products on ESP32 with SX127x transceivers in V1, where delivery risk is driven by intermittent, hard-to-reproduce radio failures.

The product solves the deeper operational problem behind most LoRa delays: non-deterministic behavior across chip families and runtime conditions. Instead of recurring patch stacks and ad-hoc retesting, LoRaDriver provides explicit state-driven behavior, diagnosable error semantics, and host-side validation pathways that improve release confidence from development through field rollout.

The V1 value proposition is practical and measurable: reduce time-to-field, reduce radio-driven release disruption, and increase predictability of production outcomes without imposing high-regulatory or safety-certification overhead in the initial phase.

### What Makes This Special

LoRaDriver differentiates on operational predictability, not API aesthetics. Its core advantage is deterministic cross-chip behavior (SX127x/SX126x), testable architecture (explicit FSM + dependency injection), and ESP32 robustness under realistic load patterns.

The key insight is that teams do not fail because they cannot send a LoRa packet; they fail because intermittent behavior destroys planning confidence and absorbs engineering capacity. LoRaDriver turns "intermittent and untraceable" into explicit states and typed failures that can be reproduced, diagnosed, and resolved systematically.

Users should choose LoRaDriver when they need production-grade radio reliability: replacing fragile internal patch layers with a standardizable radio base that sustains faster, safer releases over time.

## Project Classification

- **Project Type:** iot_embedded
- **Domain:** general (horizontal), with initial usage priority on remote monitoring
- **Complexity:** medium (high reliability expectations, without explicit regulatory/safety burden in V1)
- **Project Context:** greenfield

## Success Criteria

### User Success

Firmware teams using ESP32 + SX127x/SX126x can reach a stable first TX/RX quickly, then sustain deterministic radio behavior through non-trivial configuration changes. Success for users means fewer intermittent failures, faster root-cause analysis through explicit state/error signals, and confidence to modify radio logic without fear of hidden regressions.

Remote monitoring teams experience success when radio issues stop dominating delivery cycles: release preparation is no longer blocked by hard-to-reproduce behavior, field incidents become diagnosable, and engineering time shifts toward sensing workflows, data quality, and operations value.

### Business Success

By month 3, LoRaDriver demonstrates early operational ROI: measurable reduction in radio-driven release delays, visible decrease in urgent support load tied to radio instability, and adoption on priority new LoRa initiatives. The product proves it can shorten time-to-field without requiring domain-specific vertical specialization.

By month 12, LoRaDriver becomes the default internal foundation for new SX127x/SX126x projects in target organizations. Business success is reflected in improved roadmap predictability, lower recurring firmware maintenance cost from radio instability, and stronger customer confidence due to stable post-release behavior.

### Technical Success

Core technical success is deterministic radio behavior on validated SX127x profiles under realistic ESP32 conditions, backed by explicit finite-state transitions and typed error semantics. Critical paths (init, RX/TX, IRQ, timeout, sleep/wakeup) are validated consistently through host-side automated tests plus targeted hardware verification.

The platform must reliably convert “intermittent and non-reproducible” failures into diagnosable, bounded failure modes. Technical quality gates are CI-enforced, with high pass stability and rapid recovery when failures occur.

### Measurable Outcomes

- Time-to-first-stable TX/RX (setup to first reliable flow): **< 60 minutes by month 3**
- Radio debug burden (hours/week/team): **-40% by month 3, -60% by month 12**
- Releases delayed by radio issues: **< 20% by month 3, < 10% by month 12**
- Releases with no critical radio incident in first 30 days: **>= 90% by month 6, >= 95% by month 12**
- Critical field radio incidents per quarter: **-30% by 2 quarters, -50% by month 12**
- Adoption on new LoRa projects: **>= 60% by month 3, >= 85% by month 12**
- Team confidence (“I can modify radio layer without fear”, 1-5): **>= 4.0 by month 3, >= 4.3 by month 12**
- Radio CI quality: **>= 95% pass rate**, CI failure MTTR **< 24h**

## Product Scope

### MVP - Minimum Viable Product

- Unified core API across SX127x/SX126x for operationally critical flows (`begin`, `send`, `startReceive`, callbacks, `sleep`, `standby`)
- Deterministic state behavior with explicit error semantics for diagnosability
- Reliable handling of init, RX/TX, IRQ, timeout, and sleep/wakeup transitions
- Baseline ESP32 robustness under realistic load conditions
- Host-side CI tests used as release quality gates for critical radio behavior
- Horizontal `general` domain focus, with first-class applicability to remote monitoring use cases

### Growth Features (Post-MVP)

- Advanced duty-cycle RX and advanced CAD optimization workflows
- Broader performance tuning and platform-specific optimizations beyond baseline stability
- Enhanced observability/telemetry for faster fleet-level diagnostics
- Packaging and enablement patterns that accelerate standardization across multiple teams/products
- Early domain playbooks for high-traction verticals (e.g., process_control, energy) without full regulatory commitments

### Vision (Future)

- Industrial-grade reliability plus advanced RF capabilities at scale
- Progressive verticalization based on traction and incident profile by segment
- Optional compliance-oriented tracks (process, evidence, documentation) for regulated/safety-heavy deployments
- LoRaDriver as a long-term radio governance foundation across multi-product portfolios

## User Journeys

### Journey 1 - Primary User Success Path (Alex, Firmware Engineer)

**Opening Scene**
Alex doit livrer une mise a jour firmware pour des noeuds de remote monitoring (ESP32 + SX1276/SX1278), avec un changement radio non trivial (profil timing + gestion IRQ). Historiquement, ce type de changement introduit des regressions intermittentes en pre-prod.

**Rising Action**
Il configure LoRaDriver via l'API unifiee, execute les tests host-side en CI, puis valide sur hardware cible. Les transitions d'etat radio sont visibles et coherentes; les erreurs sont typees et exploitables. Le pipeline passe sans contournements manuels ni boucle de retest excessive.

**Climax**
Le build candidate part en terrain pilote. Pendant 30 jours, aucun incident radio critique n'apparait malgre la variabilite reelle (charge systeme, conditions RF non ideales). Les anomalies mineures restantes sont reproduites et diagnostiquees rapidement via etats/erreurs explicites.

**Resolution**
Alex passe d'un mode "debug reactif permanent" a un mode "delivery previsible". Il consacre plus de temps a la logique capteur et a la qualite data qu'a stabiliser la couche radio.

### Journey 2 - Primary User Edge Case (Alex, Recovery Scenario)

**Opening Scene**
Apres une evolution de configuration inter-profils SX127x (SX1276 <-> SX1278) et modes IRQ, un comportement RX degrade apparait dans un sous-ensemble de noeuds, sans crash franc.

**Rising Action**
Au lieu d'une chasse aux ghosts bugs, Alex observe les etats FSM, identifie un pattern d'erreur typee lie a une transition timeout/restartReceive, puis reproduit le cas en test host-side. Il ajoute un test de non-regression cible.

**Climax**
Le correctif est valide en CI puis sur hardware avec charge ESP32 representative; le comportement redevient deterministe sans effet de bord sur TX.

**Resolution**
Le cas edge est traite en cycle court, documente, et transforme en garde-fou automatise.

### Journey 3 - Admin/Ops User (Nora, Release & Reliability Owner)

**Opening Scene**
Nora pilote la readiness release pour plusieurs produits connectes. Son objectif: eviter les retards lies au radio stack et les incidents post-release.

**Rising Action**
Elle s'appuie sur des gates clairs: pass rate CI radio, couverture des chemins critiques (init/RX/TX/IRQ/timeout/sleep-wakeup), et verification terrain ciblee. Les signaux sont comprehensibles pour prise de decision go/no-go.

**Climax**
Une release candidate montre un signal de risque (instabilite sur test critique). Le blocage est decide tot, le correctif est applique avant de passer en terrain.

**Resolution**
Le processus release devient previsible. Les urgences terrain diminuent, et la gouvernance produit bascule d'un mode reactif vers un mode maitrise.

### Journey 4 - Support/Troubleshooting (Sam, Field Support Engineer)

**Opening Scene**
Un client terrain signale des pertes intermittentes de trames sur un lot de noeuds. Avant, Sam ouvrait des tickets vagues "intermittent/non reproductible".

**Rising Action**
Avec LoRaDriver, Sam collecte des indicateurs exploitables (etats, erreurs typees, contexte transition). Il classe rapidement l'incident (config, timing, environnement, regression logicielle) et route vers la bonne equipe avec preuves minimales.

**Climax**
Le diagnostic converge en quelques iterations, sans escalade longue ni "war room" prolongee.

**Resolution**
Le support gagne en precision et en delai de resolution. Le cout d'incident diminue, la confiance client remonte.

### Journey 5 - API/Integration (Ilyas, Platform Integration Engineer)

**Opening Scene**
Ilyas integre la couche radio dans un firmware applicatif et des workflows CI/CD multi-produits. Il veut une base standardisable, pas des forks par projet.

**Rising Action**
Il industrialise une configuration commune, des tests de conformite radio, et des conventions d'erreurs utilisables par les outils internes. L'integration cross-chip reste stable entre produits.

**Climax**
Un nouveau projet LoRa demarre en reutilisant la fondation existante, sans recreer une pile radio ad hoc.

**Resolution**
L'onboarding technique s'accelere, la dette firmware diminue, et la standardisation devient un accelerateur de delivery.

### Journey Requirements Summary

Les journeys revelent les capacites indispensables suivantes:

- **Determinisme operationnel:** FSM explicite, transitions observables, comportements coherents cross-chip
- **Diagnostique actionnable:** erreurs typees, contexte de panne exploitable, reproductibilite
- **Validation continue:** tests host-side CI sur chemins critiques + verification hardware ciblee
- **Readiness release:** signaux go/no-go objectifs pour ops/tech lead
- **Recovery rapide:** patterns d'edge cases transformes en tests de non-regression
- **Standardisation multi-equipes:** conventions API/erreurs/tests reutilisables entre produits
- **Support terrain efficace:** triage incident rapide, classification cause probable, reduction MTTR

## Domain-Specific Requirements

### Compliance & Regulatory

V1 cible un contexte general/remote monitoring sans contrainte safety sectorielle lourde, mais avec un socle contractuel et cyber minimum non negociable:

- Incident traceability obligatoire: chaque incident radio doit inclure cause probable, version firmware/driver, famille chip, configuration radio et timestamp
- Conservation des artefacts de validation: retention CI + logs de validation entre 90 et 180 jours
- Vulnerability management: politique de correction des dependances avec SLA interne explicite
- Versioning discipline: versioning strict + changelog obligatoire pour prevenir les regressions silencieuses
- Lightweight security governance: pratiques alignees ISO 27001/NIST (controle d'acces repo, revue PR, scan dependances)

### Technical Constraints

Le produit doit garantir une fiabilite radio operationnelle sous contraintes reelles ESP32, avec profils de compatibilite explicites:

- **Modes de cablage supportes (V1):**
  - `IRQ_MINIMAL`: DIO0 seul (chemin prioritaire d'integration rapide)
  - `IRQ_EXTENDED`: DIO0 + DIO1 (granularite evenementielle superieure)
- **Parite fonctionnelle des flows coeur** dans les deux modes:
  - `begin`, `send`, `startReceive`, `sleep`
  - Degradation controlee autorisee en DIO0 seul (moins de signaux fins), sans regression de stabilite
- Initialisation stable: taux d'echec `begin()` < 1% sur banc valide
- Robustesse sous charge ESP32: aucun blocage FSM/IRQ sur campagnes prolongees
- Recovery obligatoire: reprise apres timeout et sleep/wakeup sans reset carte
- Determinisme observable: transitions d'etat explicites et erreurs typees via `LoRaError`
- Fiabilite post-release: objectif equipe de 0 incident radio critique sur 30 jours
- Validation par profils distincts:
  - SX127x (DIO0)
  - SX127x (DIO0 + DIO1)
  - SX126x (DIO0)
  - SX126x (DIO0 + DIO1)
- Ordre V1: qualification initiale sur SX127x, puis extension SX126x apres stabilisation des gates

### Integration Requirements

Priorisation d'integration V1:

1. **CI pipeline (priorite 1):**
   - Tests host-side bloquants sur PR
   - Couverture minimale: IRQ, state machine, timeout
2. **Observabilite firmware (priorite 2):**
   - Exposition de compteurs radio erreurs/evenements pour diagnostic operationnel
3. **OTA workflow (priorite 3):**
   - Verification explicite de non-regression radio lors des updates firmware
4. **Telemetry backend (support):**
   - Schema minimal pour remonter des incidents terrain exploitables (triage rapide, attribution cause probable)

### Risk Mitigations

- **Risque 1: Regressions cross-chip (SX127x vs SX126x)**
  - Mitigation: matrice de tests croisant chip + mode IRQ + configurations radio cles
- **Risque 2: Race conditions IRQ/FSM sous charge ESP32**
  - Mitigation: tests de stress, file d'evenements bornee, metriques overflow/backlog
- **Risque 3: Faux positifs CI (confiance trompeuse)**
  - Mitigation: scenarios deterministes, campagnes hardware-in-loop periodiques, gates go/no-go par release

## IoT Embedded Specific Requirements

### Project-Type Overview

LoRaDriver V1 targets embedded IoT delivery in real field conditions, with reliability-first execution on ESP32 and SX127x modules. The immediate objective is not broad chip-family coverage but production-grade stability on a constrained, high-impact baseline that teams can ship with confidence.

V1 strict scope prioritizes SX127x (SX1276/SX1278 module families) for remote monitoring deployments across EU868 and 433 MHz targets. SX126x support is explicitly deferred to a V1-bis phase after core reliability gates are stabilized on SX127x.

### Technical Architecture Considerations

#### Hardware Requirements (`hardware_reqs`)

- Primary MCU target: ESP32-WROOM-32 (ESP-IDF/Arduino compatibility expected)
- Secondary validation target: ESP32-S3 (non-blocking for initial V1 release readiness)
- Primary LoRa module references (V1 strict):
  - SX1276-based module (e.g., RFM95W class)
  - SX1278-based module (e.g., RA-02 class or equivalent)
- SX126x family: out of strict V1 scope; planned for V1-bis
- Priority RF bands: EU868 and 433 MHz with equal product-level priority
- Baseline bus/interrupt assumptions:
  - SPI default operating range: 4-8 MHz
  - IRQ wiring profiles: DIO0-only (`IRQ_MINIMAL`) and DIO0+DIO1 (`IRQ_EXTENDED`)

#### Connectivity Protocol (`connectivity_protocol`)

- Mandatory V1 protocol: LoRa P2P only
- LoRaWAN: explicitly out of V1 scope (possible via upper-layer architecture later)
- Minimum radio parameter support in V1:
  - Spreading factor: SF7-SF12
  - Bandwidth: 125/250/500 kHz
  - Coding rate: 4/5 to 4/8
  - Sync word configurable
  - TX power configurable
  - CRC on/off
  - Preamble length configurable

#### Power Profile (`power_profile`)

- V1 principle: reliability first, power optimization second
- Required V1 behavior:
  - Stable `sleep` and `standby` operation without wake deadlocks
  - Deterministic RX/TX recovery after timeout and sleep/wakeup transitions
- No ultra-low-power contractual target in V1
- Required observability:
  - Document relative power impact between `IRQ_MINIMAL` (DIO0) and `IRQ_EXTENDED` (DIO0+DIO1)
  - Power impact reporting is informative in V1, not a release-blocking gate

#### Security Model (`security_model`)

- Out of driver scope (explicit boundary):
  - Payload/application key management
  - Business-layer encryption semantics
  - Backend PKI concerns
- In V1 driver/platform integration scope:
  - Radio configuration integrity checks with explicit typed errors on invalid states
  - Build/CI hardening baseline (PR review discipline, dependency hygiene, branch protections)
  - OTA binary provenance assurance handled by product platform CI/signing chain
- Architectural position:
  - Driver responsibility = deterministic transport behavior and diagnosable state/error semantics
  - Application/security policy = higher-layer responsibility

#### Update Mechanism (`update_mechanism`)

- Release cadence: regular incremental releases (e.g., monthly) plus hotfix channel for critical incidents
- Rollback: mandatory “last known good” fallback strategy
- OTA deployment gate: block rollout on regressions in radio KPIs:
  - init stability
  - TX/RX success behavior
  - timeout recovery
  - ESP32 runtime stability
- Minimum post-update telemetry:
  - firmware version
  - radio chip family
  - active band (433/868)
  - init failure rate
  - timeout and IRQ overflow events
  - TX/RX success rates

### Implementation Considerations

- V1 validation strategy must prioritize SX127x test matrix depth over chip-family breadth
- Qualification should run per hardware/profile combination:
  - SX1276 + DIO0
  - SX1276 + DIO0+DIO1
  - SX1278 + DIO0
  - SX1278 + DIO0+DIO1
- ESP32-S3 compatibility evidence should be collected in parallel but not block initial V1 go-live unless explicitly promoted to gate
- V1-bis planning should predefine entry criteria for SX126x onboarding (test matrix extension, parity checks, and regression budget)

## Project Scoping & Phased Development

### MVP Strategy & Philosophy

**MVP Approach:** Problem-solving MVP
LoRaDriver V1 is explicitly positioned to solve production radio reliability first (ESP32 + SX1276/SX1278 on 433/868), then expand scope after proving deterministic behavior and release stability in real deployment conditions.

**Resource Requirements:**
- 2 embedded firmware engineers (driver implementation + integration/test bench)
- 1 part-time QA/validation engineer (hardware matrix + regression discipline)
- 1 part-time PM/Lead (prioritization, KPI tracking, release gate governance)
- Optional (recommended): 0.2 DevOps capacity for CI/release hygiene

### MVP Feature Set (Phase 1)

**Core User Journeys Supported:**
- Primary firmware success path (non-trivial radio changes shipped without critical incidents)
- Primary edge-case recovery path (diagnosable timeout/IRQ/FSM failures with deterministic recovery)
- Ops/release readiness path (objective go/no-go signals before field rollout)
- Support/triage baseline path (minimum telemetry for incident attribution and rapid response)

**Must-Have Capabilities:**
1. Radio init failure rate < 1% on V1 reference matrix
2. Deterministic recovery after timeout/sleep/wakeup without board reset
3. Blocking CI gates for critical scenarios (init, TX/RX, IRQ, timeout)
4. Validated SX127x priority matrix: SX1276 + SX1278 across 868 + 433
5. OTA safety gate with “last known good” rollback and KPI-based rollout block on degradation

### Post-MVP Features

**Phase 2 (Post-MVP / V1-bis):**
- SX126x production-grade onboarding and parity validation expansion
- Observability enhancement beyond minimum post-update telemetry
- Strengthening of cross-family regression automation and release diagnostics

**Phase 3 (Expansion):**
- LoRaWAN-oriented enablement via higher-layer architecture
- Advanced power optimization track with contractual low-power targets
- Broader MCU target expansion beyond ESP32 baseline (S3 and additional families as justified by traction)

### Risk Mitigation Strategy

**Technical Risks:**
- IRQ/FSM race conditions and cross-profile regressions
- Mitigation: stress testing, bounded event queues, matrix-driven qualification, deterministic failure signatures

**Market Risks:**
- Perception gap between “works in demo” and “safe for production adoption”
- Mitigation: enforce measurable release gates, 30-day post-release stability target, publish reliability evidence

**Resource Risks:**
- Limited capacity can dilute reliability focus if parallel scope expands too early
- Mitigation: hard cut line governance, strict deferral policy, and phase-gated expansion only after V1 proof thresholds are met

### Explicit Cut Line (Anti-Scope-Creep)

Out of V1 scope by decision:
- LoRaWAN support
- SX126x production-grade support (deferred to V1-bis)
- Contractual ultra-low-power optimization commitments
- Advanced observability tooling beyond minimal post-update telemetry
- Additional MCU family expansion beyond ESP32 baseline (ESP32-S3 remains secondary validation only)

## Functional Requirements

### Radio Lifecycle Management

- FR1: Firmware engineers can initialize the radio subsystem for supported hardware profiles.
- FR2: Firmware engineers can transition the radio subsystem between active and low-power operational states.
- FR3: Firmware engineers can start receive operations and return to receive mode after completed transmissions.
- FR4: Firmware engineers can trigger packet transmission flows for supported radio configurations.
- FR5: The system can recover radio operation after timeout events without requiring full board restart.
- FR6: The system can recover radio operation after sleep/wakeup cycles while preserving operational continuity.

### Configuration & Compatibility Profiles

- FR7: Firmware engineers can configure core LoRa parameters required for V1 operation.
- FR8: Firmware engineers can apply profile-specific configuration for supported SX127x variants and IRQ wiring modes.
- FR9: The system can validate configuration inputs and reject invalid or inconsistent radio settings.
- FR10: The system can expose explicit compatibility boundaries for supported MCU, module, and band combinations.
- FR11: Product teams can classify hardware support status by profile (validated, secondary, deferred).
- FR12: Product teams can enforce explicit scope boundaries between V1 and deferred capability tracks.

### Eventing, State Visibility & Diagnostics

- FR13: Firmware engineers can observe explicit radio state transitions during runtime flows.
- FR14: Firmware engineers can receive typed error outcomes for diagnosable radio failure modes.
- FR15: The system can surface radio event signals required for operational troubleshooting across supported IRQ modes.
- FR16: Support engineers can access incident-level diagnostic context including version, chip family, configuration, and time context.
- FR17: Support engineers can classify field incidents using standardized radio failure categories.
- FR18: Product teams can retain validation and incident artifacts according to defined retention policy.

### Validation, Release Readiness & Quality Gates

- FR19: QA engineers can execute a repeatable validation matrix covering supported chip, band, and IRQ profile combinations.
- FR20: Release owners can enforce blocking quality gates for critical radio scenarios before release approval.
- FR21: Product teams can determine release readiness using explicit go/no-go criteria tied to radio stability signals.
- FR22: QA engineers can run non-regression validation on previously stabilized critical radio flows.
- FR23: Product teams can require evidence of successful recovery behavior in timeout and sleep/wakeup scenarios before go-live.
- FR24: Product teams can track and report post-release critical radio incident outcomes for defined monitoring windows.

### OTA Safety & Operational Continuity

- FR25: Release owners can gate OTA rollout based on radio health indicators.
- FR26: Operations teams can execute rollback to a last-known-good firmware baseline when degradation is detected.
- FR27: Operations teams can capture minimal post-update telemetry required for radio health assessment.
- FR28: Product teams can block progressive rollout when post-update telemetry indicates regression risk.

### Integration & Governance

- FR29: Platform integrators can integrate driver capabilities into product firmware pipelines without custom per-project forks as a default path.
- FR30: Product teams can manage versioning and changelog practices to maintain traceable release evolution and prevent silent regression risk.

## Non-Functional Requirements

### Reliability

- NFR1: The system shall achieve radio initialization failure rate below 1% on the V1 reference validation matrix.
- NFR2: The system shall recover deterministically from timeout and sleep/wakeup transitions without requiring board reset.
- NFR3: The system shall complete a release cycle including non-trivial radio changes with zero critical radio incidents during the first 30 days post-release (team objective gate).
- NFR4: The system shall provide explicit runtime state visibility and typed error outputs for diagnosable failure handling.
- NFR5: The system shall maintain stable core radio flows (`begin`, `send`, `startReceive`, `sleep`) in both IRQ profiles, with controlled degradation only in event granularity for DIO0-only mode.

### Performance

- NFR6: The system shall operate with default SPI range 4-8 MHz on validated V1 hardware profiles without introducing instability in core radio flows.
- NFR7: Under prolonged ESP32 runtime load tests, the system shall not enter FSM deadlock or unbounded IRQ handling stalls.
- NFR8: The system shall preserve deterministic recovery latency behavior for timeout/sleep-wakeup paths across validated SX127x profiles (latency values measured and tracked per profile).

### Security

- NFR9: The system shall enforce radio configuration integrity checks and reject invalid/inconsistent configuration with explicit error signaling.
- NFR10: The delivery process shall enforce repository and CI hygiene controls (protected branches, PR review workflow, dependency scanning) for all release candidates.
- NFR11: OTA artifacts used for deployment shall have verifiable provenance through the controlled CI/signing pipeline.
- NFR12: Security responsibilities shall remain explicitly partitioned: driver layer for transport reliability/diagnostics, application layer for payload cryptography and key management.

### Scalability (Product Adoption & Engineering Scale)

- NFR13: The validation framework shall support repeatable execution across the SX127x profile matrix (chip x band x IRQ mode) without manual per-run reconfiguration.
- NFR14: The release governance model shall support multi-release operation (regular incremental + hotfix) without bypassing radio quality gates.
- NFR15: The capability baseline shall remain reusable across multiple product firmware codebases as a standard integration path.

### Integration & Operability

- NFR16: CI shall enforce blocking gates on critical radio scenarios (init, TX/RX, IRQ, timeout) for pull requests affecting radio behavior.
- NFR17: OTA rollout shall be blocked when radio KPI degradation is detected against defined go/no-go thresholds.
- NFR18: Rollback to last-known-good firmware shall be operationally available for degraded deployments.
- NFR19: Post-update telemetry shall include, at minimum: firmware version, radio family, active band, init failure rate, timeout/IRQ overflow events, and TX/RX success rate.
- NFR20: Validation and incident artifacts shall be retained for 90-180 days to support traceability and regression investigations.
