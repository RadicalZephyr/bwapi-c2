# The test suite (plan section 11). Each subdirectory is one layer of the table there.

# The public headers alone, for tests that must see them exactly as a consumer does.
add_library(bwapi_c2_headers INTERFACE)
target_include_directories(bwapi_c2_headers INTERFACE "${PROJECT_SOURCE_DIR}/include")

add_subdirectory(header_hygiene)

add_subdirectory(layout_dump)
add_subdirectory(derive_closure)
add_subdirectory(fixture)
# The static type data with no game (plan section 5.8): constants, accessors, tables. One file
# at this level, as section 7's layout has it, on the same helper as the fixture suites.
add_subdirectory(read_write)
add_subdirectory(errors)
add_subdirectory(bwem)
add_subdirectory(exports)

bwapi_c2_add_test(types_test types_test.cpp)

# The R13 benchmark (docs/research/r13), built only when asked and never registered with CTest,
# because a timing loop is not a test and does not belong on the merge path. It links what a
# suite links, which is the only reason it lives behind this option rather than in a standalone
# script the way R12's probe does. run-event-snapshot-bench.sh sets it.
option(BWAPI_C2_BENCH "Build the docs/research benchmarks" OFF)
if(BWAPI_C2_BENCH)
  add_executable(r13_event_snapshot_bench
    "${PROJECT_SOURCE_DIR}/docs/research/r13/event_snapshot_bench.cpp")
  target_link_libraries(r13_event_snapshot_bench
    PRIVATE bwapi_c2_fixture bwapi_c2_abi bwapi_c2_doctest_main)
endif()
