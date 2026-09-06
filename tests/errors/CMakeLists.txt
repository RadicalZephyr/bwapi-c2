# The error channel and the boundary (plan section 11's errors/ row; implementation plan 1.1).
# This suite reaches the library's internals (the thread binding, the resolvers, guard()), so
# it sees src/ as well as the public headers.
bwapi_c2_add_test(errors_latch latch.cpp)
target_include_directories(errors_latch PRIVATE "${PROJECT_SOURCE_DIR}/src")
