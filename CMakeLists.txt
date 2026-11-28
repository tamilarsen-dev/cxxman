cmake_minimum_required(VERSION 3.25)

project(cxxman VERSION 0.9.0 LANGUAGES CXX)

# Appended to PROJECT_VERSION in the displayed version; empty for releases.
set(CXXMAN_VERSION_SUFFIX "-dev")
set(CXXMAN_VERSION "${PROJECT_VERSION}${CXXMAN_VERSION_SUFFIX}")

option(CXXMAN_CHECK_SNIPPETS "Compile every card example and recipe variant (PLAN 5.5)" OFF)
option(CXXMAN_BUILD_TESTS "Build the tests" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(tomlplusplus
  URL https://github.com/marzer/tomlplusplus/archive/refs/tags/v3.5.1.tar.gz
  URL_HASH SHA256=4821c769370a6489b6a37814f16f3769eee0f2e733a5c129d5d99e2515a95c2b
  SYSTEM)
FetchContent_MakeAvailable(tomlplusplus)

function(cxxman_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

# --- cxxman-gen: lints data/ and generates data.cpp and the snippet tests ----
add_library(cxxman-gen-lib STATIC gen/parse.cpp gen/lint.cpp gen/emit.cpp)
target_include_directories(cxxman-gen-lib PUBLIC gen src)
target_link_libraries(cxxman-gen-lib PUBLIC tomlplusplus::tomlplusplus)
cxxman_warnings(cxxman-gen-lib)

add_executable(cxxman-gen gen/main.cpp)
target_link_libraries(cxxman-gen PRIVATE cxxman-gen-lib)
cxxman_warnings(cxxman-gen)

# --- content compiled into the binary (PLAN 7.3) ------------------------------
file(GLOB_RECURSE CXXMAN_DATA_FILES CONFIGURE_DEPENDS
  ${PROJECT_SOURCE_DIR}/data/*.toml ${PROJECT_SOURCE_DIR}/data/*.tsv)
set(CXXMAN_GENERATED_DIR ${PROJECT_BINARY_DIR}/generated)
set(CXXMAN_SNIPPET_DIR ${PROJECT_BINARY_DIR}/snippets)

add_custom_command(
  OUTPUT ${CXXMAN_GENERATED_DIR}/data.cpp
  COMMAND cxxman-gen --check
          --emit ${CXXMAN_GENERATED_DIR}
          --snippets ${CXXMAN_SNIPPET_DIR}
          --changelog ${PROJECT_SOURCE_DIR}/CHANGELOG.md --version ${PROJECT_VERSION}
          ${PROJECT_SOURCE_DIR}/data
  DEPENDS cxxman-gen ${CXXMAN_DATA_FILES} ${PROJECT_SOURCE_DIR}/CHANGELOG.md
  COMMENT "Linting data/ and generating data.cpp"
  VERBATIM)
# One owner for the generated file, so parallel targets never run it twice.
add_custom_target(cxxman-generate DEPENDS ${CXXMAN_GENERATED_DIR}/data.cpp)

add_library(cxxman-data STATIC ${CXXMAN_GENERATED_DIR}/data.cpp)
add_dependencies(cxxman-data cxxman-generate)
target_include_directories(cxxman-data PUBLIC src)
cxxman_warnings(cxxman-data)

# --- the app -------------------------------------------------------------------
# Everything but main(), so the tests can drive app::run directly.
# Platform code lives only in src/term, src/sys, src/net and src/update.
add_library(cxxman-core STATIC
  src/app/app.cpp
  src/app/completions.cpp
  src/app/config.cpp
  src/app/options.cpp
  src/model/lookup.cpp
  src/render/card.cpp
  src/render/highlight.cpp
  src/render/json.cpp
  src/render/lists.cpp
  src/render/layout.cpp
  src/render/style.cpp
  src/search/fuzzy.cpp
  src/search/search.cpp
  src/sys/browser.cpp
  src/sys/clipboard.cpp
  src/sys/paths.cpp
  src/sys/process.cpp
  src/term/keys.cpp
  src/term/terminal.cpp
  src/term/tty.cpp
  src/tui/finder_model.cpp
  src/tui/pager.cpp)
target_link_libraries(cxxman-core PUBLIC cxxman-data)
if(WIN32)
  # ShellExecuteW (--web), the clipboard API (--copy).
  target_link_libraries(cxxman-core PUBLIC shell32 ole32 user32)
endif()
target_compile_definitions(cxxman-core PRIVATE CXXMAN_VERSION="${CXXMAN_VERSION}")
cxxman_warnings(cxxman-core)

add_executable(cxxman src/main.cpp)
target_link_libraries(cxxman PRIVATE cxxman-core)
cxxman_warnings(cxxman)

# --- snippet tests: a nested build with the same toolchain (PLAN 5.5) ----------
# The snippet list is only known after cxxman-gen runs, so the snippets are a
# separate CMake project, configured and built at build time.
set(CXXMAN_SNIPPET_TOOLCHAIN
  -DCXXMAN_SOURCE_DIR=${PROJECT_SOURCE_DIR}
  -DCXXMAN_GENERATOR=${CMAKE_GENERATOR}
  -DCXXMAN_PLATFORM=${CMAKE_GENERATOR_PLATFORM}
  -DCXXMAN_TOOLSET=${CMAKE_GENERATOR_TOOLSET}
  -DCXXMAN_CXX=${CMAKE_CXX_COMPILER}
  -DCXXMAN_BUILD_TYPE=${CMAKE_BUILD_TYPE})

if(CXXMAN_CHECK_SNIPPETS)
  add_custom_command(
    OUTPUT ${PROJECT_BINARY_DIR}/snippets.stamp
    COMMAND ${CMAKE_COMMAND} ${CXXMAN_SNIPPET_TOOLCHAIN}
            -DCXXMAN_SNIPPET_DIR=${CXXMAN_SNIPPET_DIR}
            -DCXXMAN_SNIPPET_BUILD=${PROJECT_BINARY_DIR}/snippets-build
            -DCXXMAN_CONFIG=$<CONFIG>
            -P ${PROJECT_SOURCE_DIR}/cmake/check_snippets.cmake
    COMMAND ${CMAKE_COMMAND} -E touch ${PROJECT_BINARY_DIR}/snippets.stamp
    DEPENDS ${CXXMAN_GENERATED_DIR}/data.cpp
            ${PROJECT_SOURCE_DIR}/cmake/check_snippets.cmake
            ${PROJECT_SOURCE_DIR}/cmake/snippets/CMakeLists.txt
    COMMENT "Compiling card snippets"
    VERBATIM)
  add_custom_target(snippets ALL DEPENDS ${PROJECT_BINARY_DIR}/snippets.stamp)
  add_dependencies(snippets cxxman-generate)
endif()

if(CXXMAN_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
