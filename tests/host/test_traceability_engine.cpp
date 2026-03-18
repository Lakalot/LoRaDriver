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

void test_traceability_link_default() {
    TraceabilityLink link{};

    if (!link.isValid()) {
        test_pass("TraceabilityLink default is invalid");
    } else {
        test_fail("TraceabilityLink default", "should be invalid");
    }
}

void test_traceability_link_valid() {
    TraceabilityLink link{};
    std::strncpy(link.source_artifact, "ARTIFACT-00000001", sizeof(link.source_artifact) - 1);
    std::strncpy(link.target_artifact, "ARTIFACT-00000002", sizeof(link.target_artifact) - 1);
    link.timestamp = 1000;

    if (link.isValid()) {
        test_pass("TraceabilityLink::isValid for populated link");
    } else {
        test_fail("TraceabilityLink::isValid", "valid link rejected");
    }
}

void test_link_build_to_test() {
    TraceabilityEngine::clear();

    LoRaError result = TraceabilityEngine::linkBuildToTest("BUILD-001", "TEST-001");

    if (result == LoRaError::kOk) {
        test_pass("TraceabilityEngine::linkBuildToTest succeeds");
    } else {
        test_fail("TraceabilityEngine::linkBuildToTest", "link failed");
    }

    TraceabilityEngine::clear();
}

void test_link_test_to_release() {
    TraceabilityEngine::clear();

    LoRaError result = TraceabilityEngine::linkTestToRelease("TEST-001", "RELEASE-001");

    if (result == LoRaError::kOk) {
        test_pass("TraceabilityEngine::linkTestToRelease succeeds");
    } else {
        test_fail("TraceabilityEngine::linkTestToRelease", "link failed");
    }

    TraceabilityEngine::clear();
}

void test_link_incident_to_artifact() {
    TraceabilityEngine::clear();

    LoRaError result = TraceabilityEngine::linkIncidentToArtifact("INCIDENT-001", "ARTIFACT-001");

    if (result == LoRaError::kOk) {
        test_pass("TraceabilityEngine::linkIncidentToArtifact succeeds");
    } else {
        test_fail("TraceabilityEngine::linkIncidentToArtifact", "link failed");
    }

    TraceabilityEngine::clear();
}

void test_link_invalid_params() {
    TraceabilityEngine::clear();

    LoRaError result = TraceabilityEngine::linkBuildToTest(nullptr, "TEST-001");
    if (result == LoRaError::kInvalidConfig) {
        test_pass("TraceabilityEngine rejects null source");
    } else {
        test_fail("TraceabilityEngine null source", "should reject");
    }

    result = TraceabilityEngine::linkBuildToTest("", "TEST-001");
    if (result == LoRaError::kInvalidConfig) {
        test_pass("TraceabilityEngine rejects empty source");
    } else {
        test_fail("TraceabilityEngine empty source", "should reject");
    }

    result = TraceabilityEngine::linkBuildToTest("BUILD-001", nullptr);
    if (result == LoRaError::kInvalidConfig) {
        test_pass("TraceabilityEngine rejects null target");
    } else {
        test_fail("TraceabilityEngine null target", "should reject");
    }

    TraceabilityEngine::clear();
}

void test_get_full_trace_chain() {
    TraceabilityEngine::clear();

    TraceabilityEngine::linkBuildToTest("BUILD-001", "TEST-001");
    TraceabilityEngine::linkTestToRelease("TEST-001", "RELEASE-001");

    std::array<TraceabilityLink, 16> chain{};
    size_t count = TraceabilityEngine::getFullTraceChain("BUILD-001", chain);

    if (count == 2) {
        test_pass("TraceabilityEngine::getFullTraceChain returns full chain");
    } else {
        test_fail("TraceabilityEngine::getFullTraceChain", "expected 2 links");
    }

    if (count >= 2) {
        bool valid_chain = (std::strcmp(chain[0].source_artifact, "BUILD-001") == 0 &&
                           std::strcmp(chain[0].target_artifact, "TEST-001") == 0 &&
                           std::strcmp(chain[1].source_artifact, "TEST-001") == 0 &&
                           std::strcmp(chain[1].target_artifact, "RELEASE-001") == 0);
        if (valid_chain) {
            test_pass("TraceabilityEngine chain links are correct");
        } else {
            test_fail("TraceabilityEngine chain", "incorrect link order");
        }
    }

    TraceabilityEngine::clear();
}

void test_trace_chain_empty_start() {
    TraceabilityEngine::clear();

    TraceabilityEngine::linkBuildToTest("BUILD-001", "TEST-001");

    std::array<TraceabilityLink, 16> chain{};
    size_t count = TraceabilityEngine::getFullTraceChain("NONEXISTENT", chain);

    if (count == 0) {
        test_pass("TraceabilityEngine::getFullTraceChain returns 0 for nonexistent");
    } else {
        test_fail("TraceabilityEngine::getFullTraceChain", "expected 0 for nonexistent");
    }

    count = TraceabilityEngine::getFullTraceChain("", chain);
    if (count == 0) {
        test_pass("TraceabilityEngine::getFullTraceChain returns 0 for empty");
    } else {
        test_fail("TraceabilityEngine::getFullTraceChain", "expected 0 for empty");
    }

    TraceabilityEngine::clear();
}

void test_validate_trace_integrity() {
    TraceabilityEngine::clear();

    TraceabilityEngine::linkBuildToTest("BUILD-001", "TEST-001");
    TraceabilityEngine::linkTestToRelease("TEST-001", "RELEASE-001");

    bool valid = TraceabilityEngine::validateTraceIntegrity("1.0.0");

    if (valid) {
        test_pass("TraceabilityEngine::validateTraceIntegrity returns true");
    } else {
        test_fail("TraceabilityEngine::validateTraceIntegrity", "expected true");
    }

    TraceabilityEngine::clear();
}

void test_traceability_clear() {
    TraceabilityEngine::linkBuildToTest("BUILD-001", "TEST-001");
    TraceabilityEngine::linkTestToRelease("TEST-001", "RELEASE-001");

    TraceabilityEngine::clear();

    std::array<TraceabilityLink, 16> chain{};
    size_t count = TraceabilityEngine::getFullTraceChain("BUILD-001", chain);

    if (count == 0) {
        test_pass("TraceabilityEngine::clear removes all links");
    } else {
        test_fail("TraceabilityEngine::clear", "links still exist");
    }
}

void test_traceability_rca_scenario() {
    TraceabilityEngine::clear();

    TraceabilityEngine::linkBuildToTest("BUILD-1.0.0", "TEST-1.0.0");
    TraceabilityEngine::linkTestToRelease("TEST-1.0.0", "RELEASE-1.0.0");
    TraceabilityEngine::linkIncidentToArtifact("INCIDENT-001", "ARTIFACT-001");

    std::array<TraceabilityLink, 16> chain{};
    size_t count = TraceabilityEngine::getFullTraceChain("BUILD-1.0.0", chain);

    if (count == 2) {
        test_pass("RCA scenario: build to release chain");
    } else {
        test_fail("RCA scenario", "expected 2 links in chain");
    }

    chain = {};
    count = TraceabilityEngine::getFullTraceChain("INCIDENT-001", chain);

    if (count == 1 && std::strcmp(chain[0].target_artifact, "ARTIFACT-001") == 0) {
        test_pass("RCA scenario: incident to artifact chain");
    } else {
        test_fail("RCA incident chain", "unexpected result");
    }

    TraceabilityEngine::clear();
}

void test_traceability_serializer() {
    TraceabilityLink link{};
    std::strncpy(link.source_artifact, "SRC-001", sizeof(link.source_artifact) - 1);
    std::strncpy(link.target_artifact, "TGT-001", sizeof(link.target_artifact) - 1);
    std::strncpy(link.link_type, "test-link", sizeof(link.link_type) - 1);
    link.timestamp = 1234567890;

    char buffer[256] = {};
    size_t len = TraceabilitySerializer::serializeLinkTo(link, buffer, sizeof(buffer));

    if (len > 0 && std::strstr(buffer, "SRC-001") != nullptr) {
        test_pass("TraceabilitySerializer::serializeLinkTo");
    } else {
        test_fail("TraceabilitySerializer", "serialization failed");
    }
}

}

int main() {
    printf("=== Traceability Engine Tests ===\n\n");

    test_traceability_link_default();
    test_traceability_link_valid();
    test_link_build_to_test();
    test_link_test_to_release();
    test_link_incident_to_artifact();
    test_link_invalid_params();
    test_get_full_trace_chain();
    test_trace_chain_empty_start();
    test_validate_trace_integrity();
    test_traceability_clear();
    test_traceability_rca_scenario();
    test_traceability_serializer();

    printf("\n=== Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
