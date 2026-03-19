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

add_executable(loradriver_test_lora_config_api
  test_lora_config_api.cpp
)

target_link_libraries(loradriver_test_lora_config_api
  PRIVATE
    loradriver
)

add_test(
  NAME lora_config_api
  COMMAND loradriver_test_lora_config_api
)

add_subdirectory(non_regression)

add_executable(loradriver_test_ota_gate
  test_ota_gate.cpp
)

target_link_libraries(loradriver_test_ota_gate
  PRIVATE
    loradriver
)

add_test(
  NAME ota_gate
  COMMAND loradriver_test_ota_gate
)

add_executable(loradriver_test_rollback_governance
  test_rollback_governance.cpp
)

target_link_libraries(loradriver_test_rollback_governance
  PRIVATE
    loradriver
)

add_test(
  NAME rollback_governance
  COMMAND loradriver_test_rollback_governance
)

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

add_executable(loradriver_test_release_monitoring
  test_release_monitoring.cpp
)

target_link_libraries(loradriver_test_release_monitoring
  PRIVATE
    loradriver
)

add_test(
  NAME release_monitoring
  COMMAND loradriver_test_release_monitoring
)

add_executable(loradriver_test_radio_counters
  test_radio_counters.cpp
)

target_link_libraries(loradriver_test_radio_counters
  PRIVATE
    loradriver
)

add_test(
  NAME radio_counters
  COMMAND loradriver_test_radio_counters
)

add_executable(loradriver_test_sx127x_adapter_contract
  test_sx127x_adapter_contract.cpp
)

target_include_directories(loradriver_test_sx127x_adapter_contract
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(loradriver_test_sx127x_adapter_contract
  PRIVATE
    loradriver
)

add_test(
  NAME sx127x_adapter_contract
  COMMAND loradriver_test_sx127x_adapter_contract
)
