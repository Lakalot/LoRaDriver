#include <loradriver/versioning.hpp>
#include <cstdio>
#include <cstring>

using namespace loradriver;

namespace {

int tests_passed = 0;
int tests_failed = 0;

void test_pass(const char* name) {
    printf("[PASS] %s\n", name);
    ++tests_passed;
}

void test_fail(const char* name, const char* reason) {
    printf("[FAIL] %s: %s\n", name, reason);
    ++tests_failed;
}

void test_semver_default_init() {
    SemVerVersion v{};

    bool valid = (v.major == 0 && v.minor == 0 && v.patch == 0 &&
                  v.prerelease[0] == '\0' && v.build_metadata[0] == '\0');

    if (valid) {
        test_pass("SemVerVersion default initialization");
    } else {
        test_fail("SemVerVersion default init", "unexpected values");
    }
}

void test_semver_is_release() {
    SemVerVersion release{};
    release.major = 1;
    release.minor = 2;
    release.patch = 3;

    SemVerVersion prerelease{};
    prerelease.major = 1;
    prerelease.minor = 2;
    prerelease.patch = 3;
    std::strncpy(prerelease.prerelease, "alpha.1", sizeof(prerelease.prerelease) - 1);

    if (release.isRelease() && !release.isPrerelease() &&
        !prerelease.isRelease() && prerelease.isPrerelease()) {
        test_pass("SemVerVersion::isRelease/isPrerelease");
    } else {
        test_fail("SemVerVersion::isRelease/isPrerelease", "incorrect logic");
    }
}

void test_semver_compare() {
    SemVerVersion v1{1, 0, 0, {}, {}};
    SemVerVersion v2{2, 0, 0, {}, {}};
    SemVerVersion v3{1, 1, 0, {}, {}};
    SemVerVersion v4{1, 0, 1, {}, {}};
    SemVerVersion v5{1, 0, 0, {}, {}};

    bool valid = true;

    if (v1.compare(v2) >= 0) valid = false;
    if (v2.compare(v1) <= 0) valid = false;
    if (v1.compare(v3) >= 0) valid = false;
    if (v3.compare(v4) <= 0) valid = false;
    if (v1.compare(v5) != 0) valid = false;

    if (valid) {
        test_pass("SemVerVersion::compare (major > minor > patch)");
    } else {
        test_fail("SemVerVersion::compare", "incorrect comparison");
    }
}

void test_semver_operators() {
    SemVerVersion v1{1, 0, 0, {}, {}};
    SemVerVersion v2{1, 0, 1, {}, {}};
    SemVerVersion v3{1, 0, 0, {}, {}};

    bool valid = (v1 < v2) && (v1 <= v2) && (v2 > v1) && (v2 >= v1) &&
                 (v1 == v3) && (v1 != v2) && (v1 <= v3) && (v1 >= v3);

    if (valid) {
        test_pass("SemVerVersion comparison operators");
    } else {
        test_fail("SemVerVersion operators", "incorrect operator behavior");
    }
}

void test_change_category_enum() {
    ChangeCategory categories[] = {
        ChangeCategory::kBreaking,
        ChangeCategory::kFeature,
        ChangeCategory::kFix,
        ChangeCategory::kInternal,
        ChangeCategory::kDeprecation,
        ChangeCategory::kSecurity
    };

    bool all_valid = true;
    for (size_t i = 0; i < sizeof(categories)/sizeof(categories[0]); ++i) {
        if (static_cast<uint8_t>(categories[i]) != i) {
            all_valid = false;
            break;
        }
    }

    if (all_valid) {
        test_pass("ChangeCategory enum values are sequential");
    } else {
        test_fail("ChangeCategory enum", "not sequential");
    }
}

void test_changelog_entry_validation() {
    ChangelogEntry valid_entry{};
    valid_entry.version = SemVerVersion{1, 0, 0, {}, {}};
    valid_entry.date = 1000000;
    valid_entry.category = ChangeCategory::kFeature;
    std::strncpy(valid_entry.description, "Initial release", sizeof(valid_entry.description) - 1);

    if (valid_entry.isValid()) {
        test_pass("ChangelogEntry::isValid for valid entry");
    } else {
        test_fail("ChangelogEntry::isValid", "valid entry rejected");
    }

    ChangelogEntry no_desc{};
    no_desc.version = SemVerVersion{1, 0, 0, {}, {}};
    no_desc.category = ChangeCategory::kFeature;

    if (!no_desc.isValid()) {
        test_pass("ChangelogEntry::isValid rejects missing description");
    } else {
        test_fail("ChangelogEntry::isValid", "should reject missing description");
    }
}

void test_changelog_entry_breaking_validation() {
    ChangelogEntry breaking_no_notes{};
    breaking_no_notes.version = SemVerVersion{2, 0, 0, {}, {}};
    breaking_no_notes.category = ChangeCategory::kBreaking;
    std::strncpy(breaking_no_notes.description, "Breaking change", sizeof(breaking_no_notes.description) - 1);

    if (!breaking_no_notes.isValid()) {
        test_pass("ChangelogEntry::isValid rejects breaking without notes");
    } else {
        test_fail("ChangelogEntry::isValid", "should reject breaking without notes");
    }

    ChangelogEntry breaking_with_notes{};
    breaking_with_notes.version = SemVerVersion{2, 0, 0, {}, {}};
    breaking_with_notes.category = ChangeCategory::kBreaking;
    std::strncpy(breaking_with_notes.description, "Breaking change", sizeof(breaking_with_notes.description) - 1);
    std::strncpy(breaking_with_notes.breaking_notes, "API changed", sizeof(breaking_with_notes.breaking_notes) - 1);
    std::strncpy(breaking_with_notes.migration_guide, "Use new API", sizeof(breaking_with_notes.migration_guide) - 1);

    if (breaking_with_notes.isValid()) {
        test_pass("ChangelogEntry::isValid accepts breaking with notes");
    } else {
        test_fail("ChangelogEntry::isValid", "should accept breaking with notes");
    }
}

void test_changelog_entry_requires_migration() {
    ChangelogEntry breaking{};
    breaking.category = ChangeCategory::kBreaking;

    ChangelogEntry deprecation{};
    deprecation.category = ChangeCategory::kDeprecation;

    ChangelogEntry feature{};
    feature.category = ChangeCategory::kFeature;

    if (breaking.requiresMigration() && deprecation.requiresMigration() && !feature.requiresMigration()) {
        test_pass("ChangelogEntry::requiresMigration");
    } else {
        test_fail("ChangelogEntry::requiresMigration", "incorrect logic");
    }
}

void test_semver_parser_simple() {
    SemVerVersion v{};
    bool parsed = SemVerParser::parse("1.2.3", v);

    if (parsed && v.major == 1 && v.minor == 2 && v.patch == 3 &&
        v.isRelease() && v.prerelease[0] == '\0') {
        test_pass("SemVerParser::parse simple version");
    } else {
        test_fail("SemVerParser::parse simple", "parsing failed");
    }
}

void test_semver_parser_prerelease() {
    SemVerVersion v{};
    bool parsed = SemVerParser::parse("1.2.3-alpha.1", v);

    if (parsed && v.major == 1 && v.minor == 2 && v.patch == 3 &&
        v.isPrerelease() && std::strcmp(v.prerelease, "alpha.1") == 0) {
        test_pass("SemVerParser::parse with prerelease");
    } else {
        test_fail("SemVerParser::parse prerelease", "parsing failed");
    }
}

void test_semver_parser_build_metadata() {
    SemVerVersion v{};
    bool parsed = SemVerParser::parse("1.2.3+build.123", v);

    if (parsed && v.major == 1 && v.minor == 2 && v.patch == 3 &&
        std::strcmp(v.build_metadata, "build.123") == 0) {
        test_pass("SemVerParser::parse with build metadata");
    } else {
        test_fail("SemVerParser::parse build", "parsing failed");
    }
}

void test_semver_parser_full() {
    SemVerVersion v{};
    bool parsed = SemVerParser::parse("2.1.0-rc.2+build.456", v);

    if (parsed && v.major == 2 && v.minor == 1 && v.patch == 0 &&
        std::strcmp(v.prerelease, "rc.2") == 0 &&
        std::strcmp(v.build_metadata, "build.456") == 0) {
        test_pass("SemVerParser::parse full semver");
    } else {
        test_fail("SemVerParser::parse full", "parsing failed");
    }
}

void test_semver_parser_invalid() {
    SemVerVersion v{};

    if (!SemVerParser::parse("", v)) {
        test_pass("SemVerParser::parse rejects empty string");
    } else {
        test_fail("SemVerParser::parse empty", "should reject empty");
    }

    if (!SemVerParser::parse("1", v)) {
        test_pass("SemVerParser::parse rejects incomplete version");
    } else {
        test_fail("SemVerParser::parse incomplete", "should reject incomplete");
    }

    if (!SemVerParser::parse("1.2", v)) {
        test_pass("SemVerParser::parse rejects two-part version");
    } else {
        test_fail("SemVerParser::parse two-part", "should reject");
    }
}

void test_semver_format() {
    SemVerVersion v{1, 2, 3, {}, {}};
    char buffer[64] = {};

    size_t len = SemVerParser::formatTo(v, buffer, sizeof(buffer));

    if (std::strcmp(buffer, "1.2.3") == 0) {
        test_pass("SemVerParser::formatTo simple version");
    } else {
        test_fail("SemVerParser::formatTo simple", "incorrect format");
    }
}

void test_semver_format_prerelease() {
    SemVerVersion v{1, 2, 3, {}, {}};
    std::strncpy(v.prerelease, "beta.1", sizeof(v.prerelease) - 1);
    char buffer[64] = {};

    SemVerParser::formatTo(v, buffer, sizeof(buffer));

    if (std::strcmp(buffer, "1.2.3-beta.1") == 0) {
        test_pass("SemVerParser::formatTo with prerelease");
    } else {
        test_fail("SemVerParser::formatTo prerelease", "incorrect format");
    }
}

void test_version_compatibility_enum() {
    VersionCompatibility compat[] = {
        VersionCompatibility::kCompatible,
        VersionCompatibility::kMigrationRequired,
        VersionCompatibility::kBreaking
    };

    bool all_valid = true;
    for (size_t i = 0; i < sizeof(compat)/sizeof(compat[0]); ++i) {
        if (static_cast<uint8_t>(compat[i]) != i) {
            all_valid = false;
            break;
        }
    }

    if (all_valid) {
        test_pass("VersionCompatibility enum values");
    } else {
        test_fail("VersionCompatibility enum", "not sequential");
    }
}

}

int main() {
    printf("=== Versioning Governance Tests ===\n\n");

    test_semver_default_init();
    test_semver_is_release();
    test_semver_compare();
    test_semver_operators();
    test_change_category_enum();
    test_changelog_entry_validation();
    test_changelog_entry_breaking_validation();
    test_changelog_entry_requires_migration();
    test_semver_parser_simple();
    test_semver_parser_prerelease();
    test_semver_parser_build_metadata();
    test_semver_parser_full();
    test_semver_parser_invalid();
    test_semver_format();
    test_semver_format_prerelease();
    test_version_compatibility_enum();

    printf("\n=== Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
