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
