# Hardware Profile Qualification Test Matrix

This document defines the V1 profile qualification matrix for LoRaDriver, establishing which hardware profile combinations are validated, secondary, deferred, or experimental.

## V1 Qualification Scope

### Profile Definition

A **Hardware Profile** is defined by three configuration parameters:

| Parameter | Type | Values |
|-----------|------|--------|
| Chip | `RadioConfig::Chip` | `kSx1276`, `kSx1278`, `kSx126xStub` |
| Band | `RadioConfig::Band` | `k433`, `k868` |
| IRQ Routing | `RadioConfig::DioRouting` | `kDio0Only`, `kDio0Dio1` |

### Profile Status Codes

| Status | Code | Description |
|--------|------|-------------|
| `kValidated` | V | Primary V1 support - blocking gate for releases |
| `kSecondary` | S | Supported but non-blocking for release |
| `kDeferred` | D | Out of V1 scope - explicit tracking |
| `kExperimental` | E | Under evaluation - not for production |

## V1 Validated Profiles

All combinations of SX1276/SX1278 with 433/868 MHz bands and DIO0/DIO0+DIO1 IRQ routing are **validated** for V1:

| Chip | Band | IRQ Mode | Status | Test ID Prefix |
|------|------|----------|--------|----------------|
| SX1276 | 433 MHz | DIO0 | Validated | 1001-1004 |
| SX1276 | 433 MHz | DIO0+DIO1 | Validated | 1011-1014 |
| SX1276 | 868 MHz | DIO0 | Validated | 1021-1024 |
| SX1276 | 868 MHz | DIO0+DIO1 | Validated | 1031-1034 |
| SX1278 | 433 MHz | DIO0 | Validated | 2001-2004 |
| SX1278 | 433 MHz | DIO0+DIO1 | Validated | 2011-2014 |
| SX1278 | 868 MHz | DIO0 | Validated | 2021-2024 |
| SX1278 | 868 MHz | DIO0+DIO1 | Validated | 2031-2034 |

**Total V1 Validated Profiles: 8**

## Deferred Profiles (V1-bis Scope)

| Chip | Band | IRQ Mode | Status | Reason |
|------|------|----------|--------|--------|
| SX126x | any | any | Deferred | V1-bis scope - deferred until V2 |

## Required Test Coverage

Each validated profile requires the following test categories:

### Unit Tests (Test IDs: XX01-XX02)
- Profile configuration validation
- Status lookup determinism

### Integration Tests (Test IDs: XX03-XX04)
- TX/RX flow completion
- State transition correctness

### Pass Rate Threshold
- **Validated profiles**: 100% pass rate required for release

## Usage

### Querying Profile Status

```cpp
#include <loradriver/profile_qualification.hpp>

using loradriver::ProfileQualificationMatrix;
using loradriver::RadioConfig;

// Check if a profile is validated
bool isValidated = ProfileQualificationMatrix::isProfileValidated(
    RadioConfig::Chip::kSx1276,
    RadioConfig::Band::k433,
    RadioConfig::DioRouting::kDio0Only
);

// Check if profile blocks release
bool isBlocking = ProfileQualificationMatrix::isReleaseBlocking(
    RadioConfig::Chip::kSx1276,
    RadioConfig::Band::k433,
    RadioConfig::DioRouting::kDio0Only
);

// Get all validated profiles for CI matrix
std::array<HardwareProfile, 16> profiles;
size_t count = ProfileQualificationMatrix::getAllValidatedProfiles(profiles);
```

### Generating Qualification Report

```cpp
#include <loradriver/profile_qualification.hpp>

using loradriver::QualificationReport;
using loradriver::ProfileQualificationMatrix;
using loradriver::QualificationReportSerializer;

QualificationReport report;
ProfileQualificationMatrix::generateQualificationReport(report, 1, 0, 0);

char buffer[4096];
size_t written = QualificationReportSerializer::serializeTo(report, buffer, sizeof(buffer));
// buffer contains CI-ready qualification report
```

## Governance

Profile status changes require approval through the governance workflow:

```cpp
using loradriver::ProfileGovernance;

// Propose a status change
uint32_t changeId = ProfileGovernance::proposeStatusChange(
    profile,
    ProfileStatus::kSecondary,
    "Demoting due to field issues",
    1, 0, 0  // version
);

// Approve the change
ProfileGovernance::approveStatusChange(changeId, approverId);
```

## References

- ADR-0004: Profile Qualification Governance Model
- Release Gates: [release-gates.md](./release-gates.md)
- API Documentation: `include/loradriver/profile_qualification.hpp`
