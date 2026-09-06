# The GameData layout dump (plan sections 10.2 and 11): every field's offset and size on
# i386-pc-windows-msvc, x86_64-pc-windows-msvc and x86_64-linux, diffed against baseline.json,
# plus the pinned headers checked against the copy and sizeof(GameData) against 33,017,048.
# Needs clang for the cross targets, which is the only Linux compiler the closure accepts
# anyway; on MSVC only the static assertion and the pack grep run.

find_package(Python3 COMPONENTS Interpreter)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND Python3_Interpreter_FOUND)
  add_test(NAME layout_dump.baseline
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/dump_layout.py"
            --clang "${CMAKE_CXX_COMPILER}"
            --bwapi-include "${BWAPI_C2_BWAPI_ROOT}/include"
            --check)
  # The same, writing the baseline: run after a deliberate pin bump.
  add_custom_target(layout_dump_update
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/dump_layout.py"
            --clang "${CMAKE_CXX_COMPILER}"
            --bwapi-include "${BWAPI_C2_BWAPI_ROOT}/include"
            --update
    COMMENT "Regenerating tests/layout_dump/baseline.json from the pinned headers")
endif()

# Compile-time, on every platform and with the compiler that builds the DLL: the baseline as
# static_asserts against the real headers. On MSVC this is the only place MSVC itself, rather
# than clang predicting MSVC, checks the 84 offsets. Without Python the hand-written
# static_assert.cpp (the size and the first two fields) stands in.
if(Python3_Interpreter_FOUND)
  set(_layout_asserts_cpp "${CMAKE_CURRENT_BINARY_DIR}/layout_asserts.gen.cpp")
  add_custom_command(
    OUTPUT "${_layout_asserts_cpp}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/dump_layout.py"
            --emit-static-asserts "${_layout_asserts_cpp}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/dump_layout.py" "${CMAKE_CURRENT_SOURCE_DIR}/baseline.json"
    COMMENT "Generating GameData layout static_asserts from baseline.json")
  add_library(layout_static_assert OBJECT "${_layout_asserts_cpp}" static_assert.cpp)
else()
  add_library(layout_static_assert OBJECT static_assert.cpp)
endif()
target_include_directories(layout_static_assert PRIVATE
  "${BWAPI_C2_BWAPI_ROOT}/include" "${BWAPI_C2_BWAPI_ROOT}/include/BWAPI/Client")
target_compile_definitions(layout_static_assert PRIVATE NOMINMAX=1)

if(NOT WIN32)
  add_test(NAME layout_dump.no_pack
    COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/check_no_pack.sh"
            "${BWAPI_C2_BWAPI_ROOT}" "${BWAPI_C2_BWEM_ROOT}")
endif()
