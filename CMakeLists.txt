cmake_minimum_required(VERSION 3.21)

# bwapi-c2: a flat C ABI over BWAPI and BWEM. docs/c-abi-plan.md is the design; section 7 is
# the layout, section 10.1 the closure this file builds, section 11 the test suite.
project(bwapi_c2 VERSION 0.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# The DLL statically links the CRT (/MT): no allocation crosses the boundary (section 4), and
# section 0 permits it. CMP0091 is NEW at this minimum version, so this variable is honoured.
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# The documented hook, carried over from upstream's CMake (section 10.1): extra flags for the
# BWAPI/BWEM closure only.
set(BWAPI_CUSTOM_COMPILE_FLAGS "" CACHE STRING "Extra compile options for the BWAPI/BWEM closure")

option(BWAPI_C2_BUILD_TESTS "Build the test suite (section 11)" ${PROJECT_IS_TOP_LEVEL})

# Both dependencies are submodules pinned to our forks (docs/pins.md). Say what to do when they
# are missing, and say it here rather than as a compile error five minutes later.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bwapi/bwapi/include/BWAPI.h"
   OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bwem/BWEM/include/bwem.h")
  message(FATAL_ERROR
    "The submodules under third_party/ are not checked out. Run\n"
    "    git submodule update --init --depth 1\n"
    "without --recursive: BWEM's own submodules must never be fetched (docs/pins.md).")
endif()

include(cmake/closure.cmake)
