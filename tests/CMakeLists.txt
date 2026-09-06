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
