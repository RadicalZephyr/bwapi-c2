# The derived closure (plan section 10.1, R6): upstream's own ExampleAIClient links against
# bwapi_c2_closure and runs to connect(), and the closure has no undefined symbol outside the
# C/C++ runtime and the seven Win32 transport names. Linux only: nm and sh.

if(WIN32)
  return()
endif()

add_executable(derive_closure_example
  "${BWAPI_C2_BWAPI_ROOT}/ExampleAIClient/Source/ExampleAIClient.cpp")
target_link_libraries(derive_closure_example PRIVATE bwapi_c2_closure)
target_compile_options(derive_closure_example PRIVATE -w)

add_test(NAME derive_closure.example_runs_to_connect
  COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/run_example.sh" $<TARGET_FILE:derive_closure_example>)

add_test(NAME derive_closure.no_foreign_undefined_symbols
  COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/check_undefined.sh" $<TARGET_OBJECTS:bwapi_c2_closure>)
