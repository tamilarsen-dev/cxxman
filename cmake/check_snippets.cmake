# Configures and builds cmake/snippets with the main build's toolchain.
# Run with cmake -P. With CXXMAN_GEN and CXXMAN_DATA set, it first runs
# cxxman-gen on that data (used by the tests).
foreach(var CXXMAN_SOURCE_DIR CXXMAN_GENERATOR CXXMAN_SNIPPET_DIR CXXMAN_SNIPPET_BUILD)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "check_snippets.cmake: ${var} is required")
  endif()
endforeach()

if(CXXMAN_GEN)
  execute_process(COMMAND ${CXXMAN_GEN} --snippets ${CXXMAN_SNIPPET_DIR} ${CXXMAN_DATA}
                  RESULT_VARIABLE rc)
  if(rc)
    message(FATAL_ERROR "cxxman-gen failed")
  endif()
endif()

set(configure
  -S ${CXXMAN_SOURCE_DIR}/cmake/snippets
  -B ${CXXMAN_SNIPPET_BUILD}
  -G ${CXXMAN_GENERATOR}
  -DCXXMAN_SNIPPET_DIR=${CXXMAN_SNIPPET_DIR})
if(CXXMAN_PLATFORM)
  list(APPEND configure -A ${CXXMAN_PLATFORM})
endif()
if(CXXMAN_TOOLSET)
  list(APPEND configure -T ${CXXMAN_TOOLSET})
endif()
if(NOT CXXMAN_GENERATOR MATCHES "^Visual Studio")
  list(APPEND configure -DCMAKE_CXX_COMPILER=${CXXMAN_CXX})
endif()
if(CXXMAN_BUILD_TYPE)
  list(APPEND configure -DCMAKE_BUILD_TYPE=${CXXMAN_BUILD_TYPE})
endif()
execute_process(COMMAND ${CMAKE_COMMAND} ${configure} RESULT_VARIABLE rc)
if(rc)
  message(FATAL_ERROR "configuring the snippet project failed")
endif()

set(build --build ${CXXMAN_SNIPPET_BUILD} --parallel)
if(CXXMAN_CONFIG)
  list(APPEND build --config ${CXXMAN_CONFIG})
endif()
execute_process(COMMAND ${CMAKE_COMMAND} ${build} RESULT_VARIABLE rc)
if(rc)
  message(FATAL_ERROR "a snippet does not compile; the error above names its TOML file and line")
endif()
