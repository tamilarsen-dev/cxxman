cmake_minimum_required(VERSION 3.25)

project(cxxman VERSION 0.9.0 LANGUAGES CXX)

# Appended to PROJECT_VERSION in the displayed version; empty for releases.
set(CXXMAN_VERSION_SUFFIX "-dev")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(cxxman src/main.cpp)
target_compile_definitions(cxxman PRIVATE
  CXXMAN_VERSION="${PROJECT_VERSION}${CXXMAN_VERSION_SUFFIX}")

if(MSVC)
  target_compile_options(cxxman PRIVATE /W4 /permissive- /utf-8)
else()
  target_compile_options(cxxman PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME version COMMAND cxxman)
set_tests_properties(version PROPERTIES
  PASS_REGULAR_EXPRESSION "^cxxman ${PROJECT_VERSION}${CXXMAN_VERSION_SUFFIX}\n$")
