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

void test_changelog_manager_add_entry() {
    ChangelogManager::clear();

    ChangelogEntry entry{};
    entry.version = SemVerVersion{1, 0, 0, {}, {}};
    entry.date = 1000000;
    entry.category = ChangeCategory::kFeature;
    std::strncpy(entry.description, "Initial release", sizeof(entry.description) - 1);

    LoRaError result = ChangelogManager::addEntry(entry);

    if (result == LoRaError::kOk) {
        test_pass("ChangelogManager::addEntry succeeds");
    } else {
        test_fail("ChangelogManager::addEntry", "add failed");
    }

    ChangelogManager::clear();
}

void test_changelog_manager_reject_duplicate() {
    ChangelogManager::clear();

    ChangelogEntry entry1{};
    entry1.version = SemVerVersion{1, 0, 0, {}, {}};
    entry1.date = 1000000;
    entry1.category = ChangeCategory::kFeature;
    std::strncpy(entry1.description, "Initial release", sizeof(entry1.description) - 1);
    ChangelogManager::addEntry(entry1);

    ChangelogEntry entry2{};
    entry2.version = SemVerVersion{1, 0, 0, {}, {}};
    entry2.date = 2000000;
    entry2.category = ChangeCategory::kFix;
    std::strncpy(entry2.description, "Bug fix", sizeof(entry2.description) - 1);
    LoRaError result = ChangelogManager::addEntry(entry2);

    if (result == LoRaError::kInvalidConfig) {
        test_pass("ChangelogManager rejects duplicate version");
    } else {
        test_fail("ChangelogManager duplicate", "should reject duplicate");
    }

    ChangelogManager::clear();
}

void test_changelog_manager_reject_invalid() {
    ChangelogManager::clear();

    ChangelogEntry entry{};
    entry.version = SemVerVersion{1, 0, 0, {}, {}};
    entry.date = 1000000;
    entry.category = ChangeCategory::kFeature;

    LoRaError result = ChangelogManager::addEntry(entry);

    if (result == LoRaError::kChangelogValidationFailed) {
        test_pass("ChangelogManager rejects invalid entry");
    } else {
        test_fail("ChangelogManager invalid", "should reject invalid");
    }

    ChangelogManager::clear();
}

void test_changelog_manager_validate() {
    ChangelogManager::clear();

    ChangelogEntry entry{};
    entry.version = SemVerVersion{1, 0, 0, {}, {}};
    entry.date = 1000000;
    entry.category = ChangeCategory::kFeature;
    std::strncpy(entry.description, "Initial release", sizeof(entry.description) - 1);
    ChangelogManager::addEntry(entry);

    bool valid = ChangelogManager::validateChangelog();

    if (valid) {
        test_pass("ChangelogManager::validateChangelog returns true");
    } else {
        test_fail("ChangelogManager::validateChangelog", "expected true");
    }

    ChangelogManager::clear();
}

void test_changelog_manager_get_changes_since() {
    ChangelogManager::clear();

    ChangelogEntry v1{};
    v1.version = SemVerVersion{1, 0, 0, {}, {}};
    v1.date = 1000000;
    v1.category = ChangeCategory::kFeature;
    std::strncpy(v1.description, "Initial", sizeof(v1.description) - 1);
    ChangelogManager::addEntry(v1);

    ChangelogEntry v2{};
    v2.version = SemVerVersion{1, 1, 0, {}, {}};
    v2.date = 2000000;
    v2.category = ChangeCategory::kFeature;
    std::strncpy(v2.description, "New feature", sizeof(v2.description) - 1);
    ChangelogManager::addEntry(v2);

    ChangelogEntry v3{};
    v3.version = SemVerVersion{1, 2, 0, {}, {}};
    v3.date = 3000000;
    v3.category = ChangeCategory::kFix;
    std::strncpy(v3.description, "Bug fix", sizeof(v3.description) - 1);
    ChangelogManager::addEntry(v3);

    std::array<ChangelogEntry, 32> changes{};
    size_t count = ChangelogManager::getChangesSince(SemVerVersion{1, 0, 0, {}, {}}, changes);

    if (count == 2) {
        test_pass("ChangelogManager::getChangesSince returns correct count");
    } else {
        test_fail("ChangelogManager::getChangesSince", "expected 2 changes");
    }

    ChangelogManager::clear();
}

void test_changelog_manager_get_breaking_changes() {
    ChangelogManager::clear();

    ChangelogEntry feature{};
    feature.version = SemVerVersion{1, 0, 0, {}, {}};
    feature.date = 1000000;
    feature.category = ChangeCategory::kFeature;
    std::strncpy(feature.description, "Feature", sizeof(feature.description) - 1);
    ChangelogManager::addEntry(feature);

    ChangelogEntry breaking{};
    breaking.version = SemVerVersion{2, 0, 0, {}, {}};
    breaking.date = 2000000;
    breaking.category = ChangeCategory::kBreaking;
    std::strncpy(breaking.description, "Breaking", sizeof(breaking.description) - 1);
    std::strncpy(breaking.breaking_notes, "API changed", sizeof(breaking.breaking_notes) - 1);
    std::strncpy(breaking.migration_guide, "Use replacement API", sizeof(breaking.migration_guide) - 1);
    ChangelogManager::addEntry(breaking);

    ChangelogEntry deprecation{};
    deprecation.version = SemVerVersion{2, 1, 0, {}, {}};
    deprecation.date = 3000000;
    deprecation.category = ChangeCategory::kDeprecation;
    std::strncpy(deprecation.description, "Deprecation", sizeof(deprecation.description) - 1);
    ChangelogManager::addEntry(deprecation);

    std::array<ChangelogEntry, 16> breaking_changes{};
    size_t count = ChangelogManager::getBreakingChanges(SemVerVersion{1, 0, 0, {}, {}}, breaking_changes);

    if (count == 2) {
        test_pass("ChangelogManager::getBreakingChanges returns breaking and deprecation");
    } else {
        test_fail("ChangelogManager::getBreakingChanges", "expected 2");
    }

    ChangelogManager::clear();
}

void test_changelog_manager_format() {
    ChangelogManager::clear();

    ChangelogEntry entry{};
    entry.version = SemVerVersion{1, 0, 0, {}, {}};
    entry.date = 1000000;
    entry.category = ChangeCategory::kFeature;
    std::strncpy(entry.description, "Initial release", sizeof(entry.description) - 1);
    ChangelogManager::addEntry(entry);

    char buffer[1024] = {};
    LoRaError result = ChangelogManager::formatChangelog(buffer, sizeof(buffer));

    if (result == LoRaError::kOk &&
        std::strstr(buffer, "# Changelog") != nullptr &&
        std::strstr(buffer, "## v1.0.0") != nullptr &&
        std::strstr(buffer, "- Initial release") != nullptr) {
        test_pass("ChangelogManager::formatChangelog produces output");
    } else {
        test_fail("ChangelogManager::formatChangelog", "formatting failed");
    }

    ChangelogManager::clear();
}

void test_changelog_manager_format_invalid() {
    ChangelogManager::clear();

    char buffer[1024] = {};
    LoRaError result = ChangelogManager::formatChangelog(nullptr, sizeof(buffer));

    if (result == LoRaError::kInvalidConfig) {
        test_pass("ChangelogManager::formatChangelog rejects null buffer");
    } else {
        test_fail("ChangelogManager::formatChangelog null", "should reject null");
    }

    result = ChangelogManager::formatChangelog(buffer, 0);
    if (result == LoRaError::kInvalidConfig) {
        test_pass("ChangelogManager::formatChangelog rejects zero size");
    } else {
        test_fail("ChangelogManager::formatChangelog zero", "should reject zero size");
    }
}

void test_changelog_manager_get_entry() {
    ChangelogManager::clear();

    ChangelogEntry entry{};
    entry.version = SemVerVersion{1, 0, 0, {}, {}};
    entry.date = 1000000;
    entry.category = ChangeCategory::kFeature;
    std::strncpy(entry.description, "Test", sizeof(entry.description) - 1);
    ChangelogManager::addEntry(entry);

    const ChangelogEntry* retrieved = ChangelogManager::getEntry(0);

    if (retrieved != nullptr && retrieved->version.major == 1) {
        test_pass("ChangelogManager::getEntry returns entry");
    } else {
        test_fail("ChangelogManager::getEntry", "entry not found");
    }

    retrieved = ChangelogManager::getEntry(100);
    if (retrieved == nullptr) {
        test_pass("ChangelogManager::getEntry returns null for invalid index");
    } else {
        test_fail("ChangelogManager::getEntry invalid", "should return null");
    }

    ChangelogManager::clear();
}

void test_changelog_manager_get_entry_count() {
    ChangelogManager::clear();

    if (ChangelogManager::getEntryCount() == 0) {
        test_pass("ChangelogManager::getEntryCount returns 0 when empty");
    } else {
        test_fail("ChangelogManager::getEntryCount", "expected 0");
    }

    ChangelogEntry entry{};
    entry.version = SemVerVersion{1, 0, 0, {}, {}};
    entry.date = 1000000;
    entry.category = ChangeCategory::kFeature;
    std::strncpy(entry.description, "Test", sizeof(entry.description) - 1);
    ChangelogManager::addEntry(entry);

    if (ChangelogManager::getEntryCount() == 1) {
        test_pass("ChangelogManager::getEntryCount returns 1 after add");
    } else {
        test_fail("ChangelogManager::getEntryCount", "expected 1");
    }

    ChangelogManager::clear();
}

void test_changelog_manager_clear() {
    ChangelogEntry entry{};
    entry.version = SemVerVersion{1, 0, 0, {}, {}};
    entry.date = 1000000;
    entry.category = ChangeCategory::kFeature;
    std::strncpy(entry.description, "Test", sizeof(entry.description) - 1);
    ChangelogManager::addEntry(entry);

    ChangelogManager::clear();

    if (ChangelogManager::getEntryCount() == 0) {
        test_pass("ChangelogManager::clear removes all entries");
    } else {
        test_fail("ChangelogManager::clear", "entries still exist");
    }
}

void test_changelog_serializer() {
    ChangelogEntry entry{};
    entry.version = SemVerVersion{1, 2, 3, {}, {}};
    entry.date = 1000000;
    entry.category = ChangeCategory::kFeature;
    std::strncpy(entry.description, "Test entry", sizeof(entry.description) - 1);

    char buffer[512] = {};
    size_t len = ChangelogSerializer::serializeEntryTo(entry, buffer, sizeof(buffer));

    if (len > 0 && std::strstr(buffer, "1.2.3") != nullptr) {
        test_pass("ChangelogSerializer::serializeEntryTo");
    } else {
        test_fail("ChangelogSerializer", "serialization failed");
    }
}

}

int main() {
    printf("=== Changelog Manager Tests ===\n\n");

    test_changelog_manager_add_entry();
    test_changelog_manager_reject_duplicate();
    test_changelog_manager_reject_invalid();
    test_changelog_manager_validate();
    test_changelog_manager_get_changes_since();
    test_changelog_manager_get_breaking_changes();
    test_changelog_manager_format();
    test_changelog_manager_format_invalid();
    test_changelog_manager_get_entry();
    test_changelog_manager_get_entry_count();
    test_changelog_manager_clear();
    test_changelog_serializer();

    printf("\n=== Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
