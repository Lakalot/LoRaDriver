#include <loradriver/artifact_governance.hpp>
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

void test_artifact_type_enum() {
    ArtifactType types[] = {
        ArtifactType::kValidationReport,
        ArtifactType::kIncidentEvidence,
        ArtifactType::kRecoveryProof,
        ArtifactType::kTestMatrix,
        ArtifactType::kGateReport,
        ArtifactType::kTelemetryBaseline,
        ArtifactType::kBuildLog,
        ArtifactType::kReleaseManifest
    };

    bool all_valid = true;
    for (size_t i = 0; i < sizeof(types)/sizeof(types[0]); ++i) {
        if (static_cast<uint8_t>(types[i]) != i) {
            all_valid = false;
            break;
        }
    }

    if (all_valid) {
        test_pass("ArtifactType enum values are sequential from 0");
    } else {
        test_fail("ArtifactType enum values", "not sequential");
    }
}

void test_artifact_metadata_defaults() {
    ArtifactMetadata metadata{};

    bool valid = true;
    if (metadata.id[0] != '\0') valid = false;
    if (metadata.type != ArtifactType::kValidationReport) valid = false;
    if (metadata.created_timestamp != 0) valid = false;
    if (metadata.linked_profile_count != 0) valid = false;

    if (valid) {
        test_pass("ArtifactMetadata default initialization");
    } else {
        test_fail("ArtifactMetadata default initialization", "unexpected values");
    }
}

void test_artifact_metadata_is_expired() {
    ArtifactMetadata metadata{};
    metadata.created_timestamp = 1000;
    metadata.expires_timestamp = 2000;

    if (!metadata.isExpired(1500) && metadata.isExpired(2000) && metadata.isExpired(3000)) {
        test_pass("ArtifactMetadata::isExpired");
    } else {
        test_fail("ArtifactMetadata::isExpired", "incorrect expiration logic");
    }
}

void test_retention_policy_defaults() {
    RetentionPolicy policy{};

    policy.artifact_type = ArtifactType::kValidationReport;
    policy.min_retention_days = RetentionPolicy::kV1MinRetentionDays;
    policy.max_retention_days = RetentionPolicy::kV1MaxRetentionDays;

    bool valid = (policy.min_retention_days == 90 && policy.max_retention_days == 180);

    if (valid) {
        test_pass("RetentionPolicy V1 default values (90-180 days)");
    } else {
        test_fail("RetentionPolicy V1 defaults", "expected 90-180 days");
    }
}

void test_retention_policy_validation() {
    RetentionPolicy policy{};
    policy.min_retention_days = 90;
    policy.max_retention_days = 180;

    bool valid = policy.isValidRetention(90) &&
                 policy.isValidRetention(135) &&
                 policy.isValidRetention(180) &&
                 !policy.isValidRetention(89) &&
                 !policy.isValidRetention(181);

    if (valid) {
        test_pass("RetentionPolicy::isValidRetention (90-180 bounds)");
    } else {
        test_fail("RetentionPolicy::isValidRetention", "incorrect bounds check");
    }
}

void test_artifact_registry_register() {
    ArtifactRegistry::clear();

    ArtifactMetadata metadata{};
    metadata.type = ArtifactType::kValidationReport;
    metadata.created_timestamp = 1000;
    metadata.expires_timestamp = 1000 + (90 * 86400);
    metadata.retention_days = 90;
    std::strncpy(metadata.linked_version, "1.0.0", sizeof(metadata.linked_version) - 1);
    std::strncpy(metadata.source_module, "TestModule", sizeof(metadata.source_module) - 1);

    const char* id = ArtifactRegistry::registerArtifact(metadata);

    if (id != nullptr && std::strncmp(id, "ARTIFACT-", 9) == 0) {
        test_pass("ArtifactRegistry::registerArtifact generates valid ID");
    } else {
        test_fail("ArtifactRegistry::registerArtifact", "invalid ID generated");
    }

    ArtifactRegistry::clear();
}

void test_artifact_registry_get_by_type() {
    ArtifactRegistry::clear();

    for (int i = 0; i < 3; ++i) {
        ArtifactMetadata metadata{};
        metadata.type = ArtifactType::kValidationReport;
        metadata.created_timestamp = 1000 + i;
        metadata.expires_timestamp = metadata.created_timestamp + (90 * 86400);
        metadata.retention_days = 90;
        std::strncpy(metadata.source_module, "Test", sizeof(metadata.source_module) - 1);
        ArtifactRegistry::registerArtifact(metadata);
    }

    for (int i = 0; i < 2; ++i) {
        ArtifactMetadata metadata{};
        metadata.type = ArtifactType::kGateReport;
        metadata.created_timestamp = 2000 + i;
        metadata.expires_timestamp = metadata.created_timestamp + (90 * 86400);
        metadata.retention_days = 90;
        std::strncpy(metadata.source_module, "Test", sizeof(metadata.source_module) - 1);
        ArtifactRegistry::registerArtifact(metadata);
    }

    std::array<ArtifactMetadata, ArtifactRegistry::kMaxArtifacts> results{};
    size_t count = ArtifactRegistry::getArtifactsByType(ArtifactType::kValidationReport, results);

    if (count == 3) {
        test_pass("ArtifactRegistry::getArtifactsByType returns correct count");
    } else {
        test_fail("ArtifactRegistry::getArtifactsByType", "expected 3 results");
    }

    count = ArtifactRegistry::getArtifactsByType(ArtifactType::kGateReport, results);
    if (count == 2) {
        test_pass("ArtifactRegistry::getArtifactsByType for different types");
    } else {
        test_fail("ArtifactRegistry::getArtifactsByType", "expected 2 gate reports");
    }

    ArtifactRegistry::clear();
}

void test_artifact_registry_get_by_version() {
    ArtifactRegistry::clear();

    for (int i = 0; i < 2; ++i) {
        ArtifactMetadata metadata{};
        metadata.type = ArtifactType::kValidationReport;
        metadata.created_timestamp = 1000 + i;
        metadata.expires_timestamp = metadata.created_timestamp + (90 * 86400);
        metadata.retention_days = 90;
        std::strncpy(metadata.linked_version, "1.0.0", sizeof(metadata.linked_version) - 1);
        std::strncpy(metadata.source_module, "Test", sizeof(metadata.source_module) - 1);
        ArtifactRegistry::registerArtifact(metadata);
    }

    for (int i = 0; i < 3; ++i) {
        ArtifactMetadata metadata{};
        metadata.type = ArtifactType::kGateReport;
        metadata.created_timestamp = 2000 + i;
        metadata.expires_timestamp = metadata.created_timestamp + (90 * 86400);
        metadata.retention_days = 90;
        std::strncpy(metadata.linked_version, "1.1.0", sizeof(metadata.linked_version) - 1);
        std::strncpy(metadata.source_module, "Test", sizeof(metadata.source_module) - 1);
        ArtifactRegistry::registerArtifact(metadata);
    }

    std::array<ArtifactMetadata, ArtifactRegistry::kMaxArtifacts> results{};
    size_t count = ArtifactRegistry::getArtifactsByVersion("1.0.0", results);

    if (count == 2) {
        test_pass("ArtifactRegistry::getArtifactsByVersion returns correct count");
    } else {
        test_fail("ArtifactRegistry::getArtifactsByVersion", "expected 2 results");
    }

    count = ArtifactRegistry::getArtifactsByVersion("1.1.0", results);
    if (count == 3) {
        test_pass("ArtifactRegistry::getArtifactsByVersion for different versions");
    } else {
        test_fail("ArtifactRegistry::getArtifactsByVersion", "expected 3 results");
    }

    ArtifactRegistry::clear();
}

void test_artifact_registry_expired() {
    ArtifactRegistry::clear();

    ArtifactMetadata fresh{};
    fresh.type = ArtifactType::kValidationReport;
    fresh.created_timestamp = 1000000;
    fresh.expires_timestamp = 2000000;
    fresh.retention_days = 90;
    std::strncpy(fresh.source_module, "Test", sizeof(fresh.source_module) - 1);
    ArtifactRegistry::registerArtifact(fresh);

    ArtifactMetadata expired{};
    expired.type = ArtifactType::kGateReport;
    expired.created_timestamp = 100000;
    expired.expires_timestamp = 200000;
    expired.retention_days = 90;
    std::strncpy(expired.source_module, "Test", sizeof(expired.source_module) - 1);
    ArtifactRegistry::registerArtifact(expired);

    std::array<ArtifactMetadata, 32> results{};
    size_t count = ArtifactRegistry::getExpiredArtifacts(results, 1500000);

    if (count == 1 && results[0].type == ArtifactType::kGateReport) {
        test_pass("ArtifactRegistry::getExpiredArtifacts detects expired artifacts");
    } else {
        test_fail("ArtifactRegistry::getExpiredArtifacts", "incorrect expired detection");
    }

    ArtifactRegistry::clear();
}

void test_artifact_registry_link() {
    ArtifactRegistry::clear();

    ArtifactMetadata m1{};
    m1.type = ArtifactType::kBuildLog;
    m1.created_timestamp = 1000;
    m1.expires_timestamp = 2000;
    m1.retention_days = 30;
    std::strncpy(m1.source_module, "Test", sizeof(m1.source_module) - 1);
    const char* id1 = ArtifactRegistry::registerArtifact(m1);

    ArtifactMetadata m2{};
    m2.type = ArtifactType::kTestMatrix;
    m2.created_timestamp = 1000;
    m2.expires_timestamp = 2000;
    m2.retention_days = 90;
    std::strncpy(m2.source_module, "Test", sizeof(m2.source_module) - 1);
    const char* id2 = ArtifactRegistry::registerArtifact(m2);

    LoRaError result = ArtifactRegistry::linkArtifacts(id1, id2);

    if (result == LoRaError::kOk) {
        test_pass("ArtifactRegistry::linkArtifacts succeeds");
    } else {
        test_fail("ArtifactRegistry::linkArtifacts", "link failed");
    }

    result = ArtifactRegistry::linkArtifacts("nonexistent", id2);
    if (result == LoRaError::kArtifactNotFound) {
        test_pass("ArtifactRegistry::linkArtifacts rejects nonexistent source");
    } else {
        test_fail("ArtifactRegistry::linkArtifacts", "should reject nonexistent");
    }

    ArtifactRegistry::clear();
}

void test_artifact_registry_purge() {
    ArtifactRegistry::clear();

    ArtifactMetadata fresh{};
    fresh.type = ArtifactType::kValidationReport;
    fresh.created_timestamp = 1000000;
    fresh.expires_timestamp = 2000000;
    fresh.retention_days = 90;
    std::strncpy(fresh.source_module, "Test", sizeof(fresh.source_module) - 1);
    ArtifactRegistry::registerArtifact(fresh);

    ArtifactMetadata expired{};
    expired.type = ArtifactType::kGateReport;
    expired.created_timestamp = 100000;
    expired.expires_timestamp = 200000;
    expired.retention_days = 90;
    std::strncpy(expired.source_module, "Test", sizeof(expired.source_module) - 1);
    ArtifactRegistry::registerArtifact(expired);

    bool purged = ArtifactRegistry::purgeExpired(1500000);

    if (purged) {
        test_pass("ArtifactRegistry::purgeExpired returns true when purging");
    } else {
        test_fail("ArtifactRegistry::purgeExpired", "expected true");
    }

    std::array<ArtifactMetadata, ArtifactRegistry::kMaxArtifacts> results{};
    size_t count = ArtifactRegistry::getArtifactsByType(ArtifactType::kValidationReport, results);
    if (count == 1) {
        test_pass("ArtifactRegistry::purgeExpired keeps non-expired artifacts");
    } else {
        test_fail("ArtifactRegistry::purgeExpired", "fresh artifact should remain");
    }

    count = ArtifactRegistry::getArtifactsByType(ArtifactType::kGateReport, results);
    if (count == 0) {
        test_pass("ArtifactRegistry::purgeExpired removes expired artifacts");
    } else {
        test_fail("ArtifactRegistry::purgeExpired", "expired artifact should be removed");
    }

    ArtifactRegistry::clear();
}

void test_retention_policy_lookup() {
    const RetentionPolicy* policy = ArtifactRegistry::getRetentionPolicy(ArtifactType::kValidationReport);

    if (policy != nullptr &&
        policy->min_retention_days == 90 &&
        policy->max_retention_days == 180) {
        test_pass("ArtifactRegistry::getRetentionPolicy returns V1 defaults");
    } else {
        test_fail("ArtifactRegistry::getRetentionPolicy", "unexpected policy values");
    }

    policy = ArtifactRegistry::getRetentionPolicy(ArtifactType::kTelemetryBaseline);
    if (policy != nullptr &&
        policy->min_retention_days == 180 &&
        policy->max_retention_days == 365) {
        test_pass("ArtifactRegistry::getRetentionPolicy for TelemetryBaseline");
    } else {
        test_fail("ArtifactRegistry::getRetentionPolicy", "unexpected telemetry policy");
    }
}

void test_artifact_link_struct() {
    ArtifactLink link{};
    std::strncpy(link.source_id, "ARTIFACT-00000001", sizeof(link.source_id) - 1);
    std::strncpy(link.target_id, "ARTIFACT-00000002", sizeof(link.target_id) - 1);
    link.created_timestamp = 1000;

    if (link.isValid() && std::strcmp(link.source_id, "ARTIFACT-00000001") == 0) {
        test_pass("ArtifactLink struct validation");
    } else {
        test_fail("ArtifactLink struct", "validation failed");
    }

    ArtifactLink empty_link{};
    if (!empty_link.isValid()) {
        test_pass("ArtifactLink empty validation");
    } else {
        test_fail("ArtifactLink empty", "should be invalid");
    }
}

}

int main() {
    printf("=== Artifact Registry Tests ===\n\n");

    test_artifact_type_enum();
    test_artifact_metadata_defaults();
    test_artifact_metadata_is_expired();
    test_retention_policy_defaults();
    test_retention_policy_validation();
    test_artifact_registry_register();
    test_artifact_registry_get_by_type();
    test_artifact_registry_get_by_version();
    test_artifact_registry_expired();
    test_artifact_registry_link();
    test_artifact_registry_purge();
    test_retention_policy_lookup();
    test_artifact_link_struct();

    printf("\n=== Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
