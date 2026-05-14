cmake_minimum_required(VERSION 3.21)

project(LoRaDriver VERSION 1.2.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LORADRIVER_SANITIZERS "Enable AddressSanitizer + UndefinedBehaviorSanitizer for host build" OFF)

if(LORADRIVER_SANITIZERS)
  if(MSVC)
    message(WARNING "Sanitizers requested but MSVC support is partial — skipping.")
  else()
    # -fno-sanitize=vptr: the library is built with -fno-rtti so vtable
    # typeinfo isn't emitted; UBSan's vptr check would link-fail otherwise.
    add_compile_options(-fsanitize=address,undefined -fno-sanitize=vptr
                        -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined -fno-sanitize=vptr)
  endif()
endif()

# Library target — sources added in later phases
add_library(loradriver STATIC)

target_include_directories(loradriver
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

option(LORADRIVER_NO_EXCEPTIONS_MSVC "Force /EHs-c- on MSVC to validate noexcept invariants" OFF)

if(NOT MSVC)
  target_compile_options(loradriver PRIVATE -fno-exceptions -fno-rtti)
elseif(LORADRIVER_NO_EXCEPTIONS_MSVC)
  # MSVC: clear default /EHsc then add /EHs-c- (no exceptions, no RTTI).
  string(REGEX REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  target_compile_options(loradriver PRIVATE /EHs-c- /GR-)
endif()

target_sources(loradriver PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/api/lora_error.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/api/lora_config.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/api/version.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/hal/spi_device.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/chips/sx127x/sx127x_driver.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/api/lora_transceiver.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/api/lora_facade.cpp
)

enable_testing()
add_subdirectory(tests/host)
