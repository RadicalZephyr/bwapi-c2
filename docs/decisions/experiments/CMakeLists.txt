# The experiments backing docs/decisions/records/. README.md beside this file has the rules,
# including the retirement rule that says the NNNN- entry points should trend toward empty, and
# why the imported rN/ rounds are an exception to it.
#
# Add an experiment with one line:
#
#     bwapi_c2_add_experiment(0001-some-decision)
#
# naming it after the record it serves, and run it from the repository root with
#
#     cmake --build build --target run-0001-some-decision

# bwapi_c2_add_experiment(<name> [<source>...]): an executable over the ABI's own objects, which
# bring the BWAPI and BWEM closure with them. Sources default to <name>.cpp. Deliberately shaped
# like bwapi_c2_add_test() in tests/fixture/CMakeLists.txt, with one difference that is the whole
# point: no add_test(). An experiment is evidence, not a test. Nothing asserts on its output, CI
# does not run it, and it is not expected to keep passing -- it answers a question that was open
# when a record was written.
function(bwapi_c2_add_experiment name)
  set(sources ${ARGN})
  if(NOT sources)
    set(sources "${name}.cpp")
  endif()

  add_executable(${name} ${sources})
  # Both, not just the ABI: bwapi_c2_abi and bwapi_c2_closure are OBJECT libraries, and object
  # libraries do not propagate their objects transitively. The root CMakeLists.txt links both into
  # BWAPI_C2 for the same reason. Linking only the ABI compiles and then fails at link on
  # BWAPI::Type<>::typeNames, which is defined in the closure.
  target_link_libraries(${name} PRIVATE bwapi_c2_abi bwapi_c2_closure)
  if(MSVC)
    target_compile_options(${name} PRIVATE /W4)
  else()
    target_compile_options(${name} PRIVATE -Wall -Wextra)
  endif()

  # How a reader re-derives a number the record quotes.
  add_custom_target(run-${name}
    COMMAND ${name}
    DEPENDS ${name}
    COMMENT "Running experiment ${name}"
    USES_TERMINAL)
endfunction()

# ------------------------------------------------------------------------------------------------
# Experiments live below this line, one call each, and are deleted or promoted when the record
# they serve logs Implemented or Deprecated. Empty is the healthy state.
#
# The rN/ subdirectories are shell scripts and are not built from here; they are run directly, and
# README.md says how they differ from everything above.
