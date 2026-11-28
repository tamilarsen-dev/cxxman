FetchContent_Declare(doctest
  URL https://github.com/doctest/doctest/archive/refs/tags/v2.5.3.tar.gz
  URL_HASH SHA256=174ebc4e769928959614789c5b4e9c3d0a0f81a62bb608756b127bfebfb21331
  SYSTEM)
FetchContent_MakeAvailable(doctest)

add_executable(cxxman-tests
  main.cpp
  data_test.cpp
  gen_test.cpp
  golden_test.cpp
  json_test.cpp
  keys_test.cpp
  lookup_test.cpp
  options_test.cpp
  pager_test.cpp
  search_test.cpp
  render_test.cpp)
target_link_libraries(cxxman-tests PRIVATE cxxman-gen-lib cxxman-core doctest::doctest)
target_compile_definitions(cxxman-tests PRIVATE
  DOCTEST_CONFIG_USE_STD_HEADERS
  CXXMAN_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
cxxman_warnings(cxxman-tests)
add_test(NAME unit COMMAND cxxman-tests)

add_test(NAME version COMMAND cxxman --version)
set_tests_properties(version PROPERTIES
  PASS_REGULAR_EXPRESSION "^cxxman ${CXXMAN_VERSION}\n$")

# PLAN 12.1 M1: an unknown field fails the lint, naming file and line.
add_test(NAME lint-unknown-field
  COMMAND cxxman-gen ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/unknown-field/data)
set_tests_properties(lint-unknown-field PROPERTIES
  PASS_REGULAR_EXPRESSION "data/entries/frob\\.toml:1: error: unknown field 'colour'")

# PLAN 12.1 M1: a broken snippet fails with file.toml:line (GCC/Clang use
# file:line:col, MSVC file(line,col)). Needs a nested build, so only with snippets.
if(CXXMAN_CHECK_SNIPPETS)
  add_test(NAME snippet-error-names-toml-line
    COMMAND ${CMAKE_COMMAND} ${CXXMAN_SNIPPET_TOOLCHAIN}
            -DCXXMAN_GEN=$<TARGET_FILE:cxxman-gen>
            -DCXXMAN_DATA=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/broken-snippet/data
            -DCXXMAN_SNIPPET_DIR=${CMAKE_CURRENT_BINARY_DIR}/broken-snippet/snippets
            -DCXXMAN_SNIPPET_BUILD=${CMAKE_CURRENT_BINARY_DIR}/broken-snippet/build
            -DCXXMAN_CONFIG=$<CONFIG>
            -P ${PROJECT_SOURCE_DIR}/cmake/check_snippets.cmake)
  set_tests_properties(snippet-error-names-toml-line PROPERTIES
    PASS_REGULAR_EXPRESSION "frob\\.toml(:15:|\\(15[,)])")
endif()
