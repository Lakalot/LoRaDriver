---
stepsCompleted: [1, 2, 3, 4, 5, 6]
inputDocuments: []
date: 2026-02-22
author: Yaya
---

# Product Brief: LoRaDriver

<!-- Content will be appended sequentially through collaborative workflow steps -->

## Executive Summary

LoRaDriver addresses a critical gap in IoT product development: the disconnect between product ambition and embedded radio reality. Teams building LoRa solutions across SX127x and SX126x transceivers face high integration friction, non-deterministic behavior, and prolonged debug cycles that slow delivery and increase risk.

The core value of LoRaDriver is to make radio behavior predictable, testable, and reproducible across chip families, so teams can ship reliable field-ready systems without deep RF expertise. Instead of spending engineering capacity on low-level transceiver instability, teams can focus on business value such as sensing workflows, operational data, and real-world product outcomes.

LoRaDriver is positioned as an industrialization enabler, not just a convenience library: it provides operational confidence, shorter prototype-to-field timelines, and a maintainable technical foundation for robust LoRa products. A central V1 commitment is native optimization for production-relevant MCU environments, especially ESP32, with explicit reliability and performance guarantees under real load conditions.

---

## Core Vision

### Problem Statement

Current LoRa development for SX127x/SX126x typically relies on a fragile mix of generic Arduino libraries, vendor examples, GitHub forks, and custom firmware patches. These approaches can work in happy-path scenarios but frequently break under real product conditions, especially around state transitions, IRQ handling, RX/TX timeouts, sleep/wakeup behavior, and chip-specific differences.

The true problem is not "sending a LoRa packet," but consistently guaranteeing reliable, reproducible, cross-chip radio behavior in production contexts, including high-usage MCU platforms such as ESP32.

### Problem Impact

When radio behavior is not deterministic, teams experience delayed releases, intermittent field incidents, heavy post-deployment support burden, and reduced customer trust. Engineering velocity drops as hardware retesting loops expand and firmware teams spend disproportionate time diagnosing radio issues.

The primary business cost is opportunity loss: engineering effort is consumed by low-level radio instability instead of higher-value product outcomes such as sensor logic, data workflows, and user-facing field experience.

### Why Existing Solutions Fall Short

Existing solutions are optimized for initial bring-up, not repeatable industrialization. They often lack explicit state modeling, robust error semantics, and clear cross-chip compatibility boundaries. As a result, failures are hard to reproduce, regressions are hard to isolate, and operational knowledge becomes concentrated in a few senior firmware experts.

They also rarely provide explicit guarantees for production-heavy targets such as ESP32 under concurrent RTOS workloads and radio coexistence constraints (Wi-Fi/BLE), where IRQ latency, SPI performance, and system load stability are decisive.

### Proposed Solution

LoRaDriver delivers a unifying, production-oriented radio layer with:
- A unified API across SX127x and SX126x families
- An explicit finite-state machine for predictable radio behavior
- Typed error handling for diagnosable failure modes
- Automated host-side tests to validate critical RX/TX/IRQ/timeout flows without requiring constant hardware loops
- Native optimization for ESP32 execution characteristics in real deployment conditions

The V1 objective is to make the radio layer predictable rather than "magical," enabling teams to ship confidently without deep RF specialization.

### Key Differentiators

LoRaDriver's defensible advantage is architectural: a testable and industrializable design based on driver abstraction, dependency injection, explicit state management, and PC-hosted testability. This is not merely another SPI wrapper or LoRa helper library.

The timing is strong: IoT teams are under pressure to move faster from PoC to field deployment, and radio firmware debt has become a primary bottleneck. LoRaDriver directly addresses this bottleneck by selling operational confidence and reduced time-to-field, with explicit V1 guarantees on ESP32 for deterministic RX/TX behavior, clean interrupt handling, and no critical-path performance regressions.

## Target Users

### Primary Users

**1) Alex - Firmware Engineer (IoT Embedded)**
Alex works in a small industrial IoT company (20-80 people), in an embedded team of 2-4 developers shipping field sensor products quickly. His stack includes C++17, PlatformIO/Arduino, ESP32, SX127x/SX126x modules, and CI with CMake host-side tests.

**Core goals and motivations**
- Deliver releases on schedule
- Keep radio behavior stable across chip variants
- Reduce manual retest cycles and intermittent production bugs

**Current pain and workarounds**
- Loses major time to non-deterministic debug on IRQ handling, RX/TX timing, and sleep/wakeup transitions
- Struggles with behavior differences between SX127x and SX126x
- Relies on repeated hardware retests after each low-level change

**Success vision**
- One unified API, explicit typed errors, stable ESP32 behavior
- Host-side tests validating critical flows before flashing hardware
- Working examples that produce reliable first results in under one hour

---

**2) Sarah - Embedded Tech Lead / Architect**
Sarah leads a 5-12 person firmware organization across multiple connected products. She owns quality and release reliability while managing existing technical debt, tight timelines, and limited test budgets.

**Core goals and motivations**
- Standardize embedded practices and reduce delivery risk
- Ensure field reliability while scaling team output
- Avoid fragile architectural decisions that amplify long-term support costs

**Current pain and risks**
- High concern about industrializing on unstable radio foundations
- Intermittent radio incidents create unpredictable production costs
- Existing solutions often lack strong test signals for release confidence

**Success vision**
- Clear architecture (driver abstraction + explicit FSM)
- Stable API and measurable behavior under ESP32 load
- Test coverage on critical paths (init, RX/TX, IRQ, timeout) as objective release gates

### Secondary Users

**1) Mehdi - IoT Product Manager / Product Owner**
Mehdi is responsible for roadmap predictability and delivery outcomes for connected products.

**Expected business outcomes**
- Shorten time-to-field by several weeks
- Increase release confidence and planning predictability
- Reduce field returns and post-deployment support cost driven by radio instability

**Why this matters to him**
- Firmware unpredictability directly impacts launch dates, customer trust, and support economics
- A reliable radio layer unlocks focus on customer-facing product value instead of low-level debugging cycles

### User Journey

**Primary segment journey (Firmware Engineer + Tech Lead)**

**Discovery**
- Finds LoRaDriver through GitHub, ESP32/LoRa communities, lead engineer recommendations, or searches for reliable SX126x/SX127x drivers with tests

**Onboarding**
- Reaches first successful outcome in 30-90 minutes: builds examples, obtains stable TX/RX behavior, and validates callback/event flow

**Core Usage**
- Weekly workflow: tune radio configuration, run host-side CI tests, perform targeted hardware validation, then integrate into application firmware

**Success Moment ("Aha!")**
- First release where radio behavior stays predictable across config/chip changes without intermittent "ghost" regressions

**Long-Term Adoption**
- LoRaDriver becomes the internal radio standard layer
- Shared quality checklist is institutionalized
- New firmware onboarding accelerates and tribal knowledge dependency decreases

## Success Metrics

LoRaDriver succeeds when embedded teams move from unpredictable radio debugging to a reliable delivery workflow: configure, test, and ship with confidence. User success is demonstrated by faster first wins, lower recurring radio troubleshooting effort, and stable release behavior in real field conditions.

From the user perspective, value is created when:
- Teams achieve a reliable first TX/RX outcome quickly on standard ESP32 + SX127x/SX126x setups
- Prototype reliability carries through to pilot/field deployment without hidden regressions
- Intermittent radio failures (IRQ/timing/sleep-wakeup) decline significantly
- Delivery confidence increases and dependency on a single "radio expert" decreases
- LoRaDriver becomes the default internal workflow rather than ad-hoc forks or custom drivers

### Business Objectives

**3-Month Objectives**
- Reduce near-term delivery risk by decreasing release delays caused by radio-layer instability
- Drive real adoption by using LoRaDriver on priority new LoRa developments
- Lower immediate support burden by reducing critical post-release radio incidents
- Establish reliability proof through CI + host-side tests as explicit quality gates

**12-Month Objectives**
- Make LoRaDriver the internal standard for SX127x/SX126x development, especially on ESP32
- Sustainably reduce firmware radio costs (debug effort, regressions, urgent patching)
- Improve roadmap predictability for LoRa firmware cycle planning
- Accelerate multi-product industrialization through reuse of a validated common radio layer

### Key Performance Indicators

- **Time-to-first-success (developer setup -> first stable TX/RX)**
  Target: < 60 minutes by month 3

- **Radio debug burden (hours/week/team spent on radio incidents)**
  Target: -40% by month 3, -60% by month 12

- **Release delay due to radio (% of releases delayed by radio issues)**
  Target: < 20% by month 3, < 10% by month 12

- **Releases without critical radio incident (within 30 days post-release)**
  Target: >= 90% by month 6, >= 95% by month 12

- **Critical field radio incidents (per quarter)**
  Target: -30% by 2 quarters, -50% by month 12

- **Internal adoption (% of new LoRa projects starting with LoRaDriver)**
  Target: >= 60% by month 3, >= 85% by month 12

- **Team confidence (survey 1-5: "I can modify radio layer without fear")**
  Target: >= 4.0/5 by month 3, >= 4.3/5 by month 12

- **Radio CI quality (LoRa test pass rate on PRs + CI failure MTTR)**
  Target: >= 95% continuous pass rate, CI failure MTTR < 24h

**Measurement and Review Cadence**

**Data Sources**
- CI/CD: build/test duration, pass rate, flaky tests, MTTR (GitHub Actions, CMake, CTest)
- Issue tracker: radio ticket volume, severity, resolution time, recurrence patterns (GitHub Issues/Jira)
- Field incidents: count, severity, root cause attribution, support cost (support desk + postmortems)
- Release logs: delays, primary causes, hotfix frequency (release notes/changelog)
- Team pulse survey: confidence, perceived effort, onboarding friction (monthly 5-question pulse)
- Docs/examples onboarding: time-to-onboard and success rate in internal onboarding sessions

**Review Frequency**
- Weekly: leading indicators (CI stability, debug burden, ticket flow)
- Monthly: adoption, team confidence, time-to-first-success, onboarding quality
- Per release: 30-day critical incidents, release delay analysis, root causes
- Quarterly (product QBR): 3/12-month objective progress, ROI, investment decisions

## MVP Scope

### Core Features

The MVP for LoRaDriver is a reliable, unified, and testable radio foundation for SX127x and SX126x, with explicit execution focus on ESP32 production conditions.

Essential MVP capabilities:

- Unified core API:
  - `begin`
  - `send`
  - `startReceive`
  - `onReceive` / `onEvent` callbacks
  - `sleep` / `standby`

- Explicit finite-state behavior and diagnosable failures:
  - Deterministic state transitions
  - Clear `LoRaError` semantics for fast debugging and operational triage

- Robust critical paths:
  - Initialization
  - RX/TX flows
  - IRQ handling
  - Timeout handling
  - Sleep/wakeup transitions
  - Verified behavior consistency across SX127x/SX126x families

- Baseline ESP32 optimization:
  - Stable IRQ and SPI behavior under realistic system load conditions

- Host-side automated validation:
  - CI-backed tests for critical scenarios
  - Coverage focused on reliability of core flows rather than full advanced-option coverage in V1

### Out of Scope for MVP

The following are intentionally deferred beyond MVP:

- Advanced duty-cycle RX optimization
- Advanced CAD modes and optimization workflows
- Fine-grained performance tuning beyond baseline reliability targets
- Extended observability and advanced telemetry/debug tooling
- Niche RF expert workflows and highly specialized profile packs

Scope rule: any feature that does not directly improve reliability, predictability, or time-to-field on the core delivery path is out of MVP.

### MVP Success Criteria

MVP is considered successful when it demonstrates all of the following:

- Teams achieve first stable TX/RX on standard ESP32 + SX127x/SX126x setup in under 60 minutes
- Core radio flows remain deterministic across normal configuration changes
- Critical release paths complete without radio "ghost bug" regressions on common scenarios
- Host-side CI tests are used as quality gates for critical radio behavior
- Early field releases show no critical radio incidents on core usage scenarios

Go/scale signal beyond MVP:
- Core reliability metrics trend toward targets (debug burden reduction, release delay reduction, incident reduction)
- Teams adopt LoRaDriver on new LoRa projects as default starting point
- Engineering confidence improves measurably in monthly team pulse checks

### Future Vision

**Next (Post-MVP):**
- Advanced radio capabilities (duty-cycle RX, advanced CAD modes)
- Deeper performance tuning and platform-specific optimizations
- Enriched observability for faster diagnostics and operational tracking

**Later:**
- Expert-grade RF tooling and specialized profiles for advanced deployment contexts
- Broader ecosystem extensions built on the stable common radio layer
- Additional scale features for multi-product standardization and governance

The long-term vision keeps the same foundation: industrial-grade reliability first, then advanced capability expansion on top of proven core behavior.
