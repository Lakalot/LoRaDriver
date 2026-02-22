cmake_minimum_required(VERSION 3.21)

project(LoRaDriver VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(loradriver STATIC
  src/api/lora_driver.cpp
  src/core/core_stub.cpp
  src/chips/sx127x/sx127x_stub.cpp
  src/chips/sx126x/sx126x_stub.cpp
  src/platform/esp32/esp32_stub.cpp
  src/platform/arduino/arduino_stub.cpp
  src/infra/infra_stub.cpp
  src/internal/internal_stub.cpp
)

target_include_directories(loradriver
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

enable_testing()
add_subdirectory(tests/host)
