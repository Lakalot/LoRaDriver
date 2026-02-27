add_executable(loradriver_test_timeout_recovery
  test_timeout_recovery.cpp
)

target_link_libraries(loradriver_test_timeout_recovery
  PRIVATE
    loradriver
)

add_test(
  NAME timeout_recovery
  COMMAND loradriver_test_timeout_recovery
)

add_executable(loradriver_test_sleep_wakeup_recovery
  test_sleep_wakeup_recovery.cpp
)

target_link_libraries(loradriver_test_sleep_wakeup_recovery
  PRIVATE
    loradriver
)

add_test(
  NAME sleep_wakeup_recovery
  COMMAND loradriver_test_sleep_wakeup_recovery
)

add_executable(loradriver_test_irq_race_scenarios
  test_irq_race_scenarios.cpp
)

target_link_libraries(loradriver_test_irq_race_scenarios
  PRIVATE
    loradriver
)

add_test(
  NAME irq_race_scenarios
  COMMAND loradriver_test_irq_race_scenarios
)

add_executable(loradriver_test_fsm_regression
  test_fsm_regression.cpp
)

target_link_libraries(loradriver_test_fsm_regression
  PRIVATE
    loradriver
)

add_test(
  NAME fsm_regression
  COMMAND loradriver_test_fsm_regression
)
