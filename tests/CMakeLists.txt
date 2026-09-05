# The test suite (plan section 11). Each subdirectory is one layer of the table there.

# The public headers alone, for tests that must see them exactly as a consumer does.
add_library(bwapi_c2_headers INTERFACE)
target_include_directories(bwapi_c2_headers INTERFACE "${CMAKE_SOURCE_DIR}/include")

add_subdirectory(header_hygiene)
