bwapi_c2_add_test(bwem_analysis analysis.cpp)

# Only with BWEM's assertions compiled in: the R11.9 overlap must abort inside PutOnTiles.
if(BWAPI_C2_BWEM_ASSERTS)
  add_executable(bwem_overlap_asserts overlap_asserts.cpp)
  target_link_libraries(bwem_overlap_asserts PRIVATE bwapi_c2_fixture)
  target_compile_options(bwem_overlap_asserts PRIVATE -Wall -Wextra)
  # CTest counts a SIGABRT as a failure whatever the output says, so the shell swallows the exit
  # status and the assertion's name in the output is what passes the test; a silent return or a
  # segfault elsewhere prints no such name and fails.
  add_test(NAME bwem_overlap_asserts_at_cause
    COMMAND sh -c "$<TARGET_FILE:bwem_overlap_asserts> 2>&1 || true")
  set_tests_properties(bwem_overlap_asserts_at_cause PROPERTIES
    PASS_REGULAR_EXPRESSION "Neutral::PutOnTiles")
endif()
