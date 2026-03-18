add_executable(loradriver_host_smoke
  smoke_test.cpp
)

target_link_libraries(loradriver_host_smoke
  PRIVATE
    loradriver
)

target_compile_definitions(loradriver_host_smoke
  PRIVATE
    LORADRIVER_REPO_ROOT="${CMAKE_SOURCE_DIR}"
)

add_test(
  NAME host_smoke
  COMMAND loradriver_host_smoke
)

add_executable(loradriver_test_profile_qualification
  test_profile_qualification.cpp
)

target_link_libraries(loradriver_test_profile_qualification
  PRIVATE
    loradriver
)

add_test(
  NAME profile_qualification
  COMMAND loradriver_test_profile_qualification
)

add_executable(loradriver_test_ci_gates
  test_ci_gates.cpp
)

target_link_libraries(loradriver_test_ci_gates
  PRIVATE
    loradriver
)

add_test(
  NAME ci_gates
  COMMAND loradriver_test_ci_gates
)

add_subdirectory(non_regression)

add_executable(loradriver_test_artifact_registry
  test_artifact_registry.cpp
)

target_link_libraries(loradriver_test_artifact_registry
  PRIVATE
    loradriver
)

add_test(
  NAME artifact_registry
  COMMAND loradriver_test_artifact_registry
)

add_executable(loradriver_test_versioning_governance
  test_versioning_governance.cpp
)

target_link_libraries(loradriver_test_versioning_governance
  PRIVATE
    loradriver
)

add_test(
  NAME versioning_governance
  COMMAND loradriver_test_versioning_governance
)

add_executable(loradriver_test_traceability_engine
  test_traceability_engine.cpp
)

target_link_libraries(loradriver_test_traceability_engine
  PRIVATE
    loradriver
)

add_test(
  NAME traceability_engine
  COMMAND loradriver_test_traceability_engine
)

add_executable(loradriver_test_changelog_manager
  test_changelog_manager.cpp
)

target_link_libraries(loradriver_test_changelog_manager
  PRIVATE
    loradriver
)

add_test(
  NAME changelog_manager
  COMMAND loradriver_test_changelog_manager
)
