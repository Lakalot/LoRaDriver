cmake_minimum_required(VERSION 3.21)

project(LoRaDriver VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Library target — sources added in later phases
add_library(loradriver STATIC)

target_include_directories(loradriver
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Placeholder source so the static lib has at least one TU until Phase 1 lands
target_sources(loradriver PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/_placeholder.cpp)

enable_testing()
add_subdirectory(tests/host)
