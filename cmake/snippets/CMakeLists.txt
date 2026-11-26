# Compiles every snippet written by cxxman-gen (PLAN 5.5): one translation
# unit each, at its own standard, with the CI warning flags. Driven by
# cmake/check_snippets.cmake; not part of the main project.
cmake_minimum_required(VERSION 3.25)
project(cxxman_snippets LANGUAGES CXX)

if(NOT CXXMAN_SNIPPET_DIR)
  message(FATAL_ERROR "CXXMAN_SNIPPET_DIR is required")
endif()

if(MSVC)
  set(compiler msvc)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(compiler clang)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(compiler gcc)
else()
  message(FATAL_ERROR "snippets: unsupported compiler ${CMAKE_CXX_COMPILER_ID}")
endif()

# Called by the generated manifest.
function(cxxman_snippet name std skip)
  if(compiler IN_LIST skip)
    message(STATUS "snippet skipped on ${compiler}: ${name}")
    return()
  endif()
  set_property(GLOBAL APPEND PROPERTY cxxman_snippets_${std} ${CXXMAN_SNIPPET_DIR}/${name}.cpp)
  set_property(GLOBAL APPEND PROPERTY cxxman_standards ${std})
endfunction()

include(${CXXMAN_SNIPPET_DIR}/manifest.cmake)

get_property(standards GLOBAL PROPERTY cxxman_standards)
list(REMOVE_DUPLICATES standards)
foreach(std IN LISTS standards)
  get_property(files GLOBAL PROPERTY cxxman_snippets_${std})
  list(LENGTH files count)
  message(STATUS "snippets: ${count} at C++${std} on ${compiler}")
  add_library(snippets_cxx${std} OBJECT ${files})
  set_target_properties(snippets_cxx${std} PROPERTIES
    CXX_STANDARD ${std} CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
  if(MSVC)
    target_compile_options(snippets_cxx${std} PRIVATE /W4 /WX /wd4189 /permissive- /utf-8)
    # The VS generator archives object libraries, and every snippet defines
    # main: silence "already defined; second definition ignored".
    set_target_properties(snippets_cxx${std} PROPERTIES STATIC_LIBRARY_OPTIONS /IGNORE:4006)
  else()
    target_compile_options(snippets_cxx${std} PRIVATE
      -Wall -Wextra -Werror -Wno-unused-variable -Wno-unused-but-set-variable)
  endif()
endforeach()
