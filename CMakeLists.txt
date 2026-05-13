cmake_minimum_required(VERSION 3.21)

project(LoRaDriver VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LORADRIVER_SANITIZERS "Enable AddressSanitizer + UndefinedBehaviorSanitizer for host build" OFF)

if(LORADRIVER_SANITIZERS)
  if(MSVC)
    message(WARNING "Sanitizers requested but MSVC support is partial — skipping.")
  else()
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
  endif()
endif()

# Library target — sources added in later phases
add_library(loradriver STATIC)

target_include_directories(loradriver
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(NOT MSVC)
  target_compile_options(loradriver PRIVATE -fno-exceptions -fno-rtti)
endif()

target_sources(loradriver PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/api/lora_error.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/api/lora_config.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/api/version.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/hal/spi_device.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/chips/sx127x/sx127x_driver.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/api/lora_transceiver.cpp
)

enable_testing()
add_subdirectory(tests/host)
