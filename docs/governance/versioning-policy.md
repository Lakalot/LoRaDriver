# Versioning Policy

This document defines the versioning and changelog governance for LoRaDriver releases.

## Semantic Versioning

LoRaDriver follows [SemVer 2.0.0](https://semver.org/) with the following rules:

### Version Format

```
MAJOR.MINOR.PATCH[-PRERELEASE][+BUILD_METADATA]
```

- **MAJOR**: Breaking API changes
- **MINOR**: New features, backward-compatible
- **PATCH**: Bug fixes, internal changes

### Version Bump Rules

| Change Type | Version Impact | Example |
|-------------|----------------|---------|
| Breaking API change | MAJOR | 1.x.x → 2.0.0 |
| New public method/type | MINOR | 1.0.x → 1.1.0 |
| Bug fix, internal change | PATCH | 1.0.0 → 1.0.1 |

## Change Categories

```cpp
enum class ChangeCategory : uint8_t {
  kBreaking = 0,      // Major version bump required
  kFeature = 1,       // Minor version bump
  kFix = 2,           // Patch version bump
  kInternal = 3,      // No version impact
  kDeprecation = 4,   // Warning, migration guide required
  kSecurity = 5       // Patch with CVE reference
};
```

## Changelog Requirements

### Required Fields

All changelog entries must include:
- Version (SemVer format)
- Date (UTC epoch)
- Category
- Description

### Breaking Changes

Breaking changes **MUST** include:
- `breaking_notes`: Description of what breaks
- `migration_guide`: Steps to migrate

```cpp
ChangelogEntry breaking = {
    .version = {2, 0, 0, {}, {}},
    .date = 1700000000,
    .category = ChangeCategory::kBreaking,
    .description = "Removed deprecated init function",
    .breaking_notes = "init() no longer accepts null config",
    .migration_guide = "Use init(config) with valid config"
};
```

### Security Fixes

Security fixes **MUST** include:
- `issue_refs`: CVE or internal reference

```cpp
ChangelogEntry security = {
    .version = {1, 0, 1, {}, {}},
    .category = ChangeCategory::kSecurity,
    .issue_refs = "CVE-2024-XXXXX"
};
```

## Changelog Management

### Adding Entries

```cpp
ChangelogEntry entry = {
    .version = {1, 1, 0, {}, {}},
    .date = getCurrentTimestamp(),
    .category = ChangeCategory::kFeature,
    .description = "Added profile governance API"
};

LoRaError result = ChangelogManager::addEntry(entry);
if (result != LoRaError::kOk) {
    // Handle validation failure
}
```

### Validation

```cpp
bool valid = ChangelogManager::validateChangelog();
```

### Querying Changes

```cpp
// Get changes since version
SemVerVersion since = {1, 0, 0, {}, {}};
std::array<ChangelogEntry, 32> changes;
size_t count = ChangelogManager::getChangesSince(since, changes);

// Get breaking changes only
std::array<ChangelogEntry, 16> breaking;
count = ChangelogManager::getBreakingChanges(since, breaking);
```

### Formatting for Release Notes

```cpp
char buffer[4096];
ChangelogManager::formatChangelog(buffer, sizeof(buffer));
// Output formatted changelog
```

## Version Compatibility

```cpp
enum class VersionCompatibility : uint8_t {
  kCompatible = 0,        // No migration needed
  kMigrationRequired = 1, // Migration guide available
  kBreaking = 2           // Breaking changes, major upgrade
};
```

## Compliance

This policy satisfies:
- **FR30**: Product teams can manage versioning and changelog practices
- **NFR**: Traceable release evolution
