add_executable(loradriver_host_smoke
  smoke_test.cpp
)

target_link_libraries(loradriver_host_smoke
  PRIVATE
    loradriver
)

add_test(
  NAME host_smoke
  COMMAND loradriver_host_smoke
)
