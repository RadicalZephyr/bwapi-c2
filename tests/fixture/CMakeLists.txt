# The shared fixture builder (plan section 11) and the test-executable helper the suites use.

add_library(bwapi_c2_fixture STATIC fixture.cpp)
target_link_libraries(bwapi_c2_fixture PUBLIC bwapi_c2_closure)
target_include_directories(bwapi_c2_fixture PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
if(MSVC)
  target_compile_options(bwapi_c2_fixture PRIVATE /W4)
else()
  target_compile_options(bwapi_c2_fixture PRIVATE -Wall -Wextra)
endif()

# doctest's runtime and main(), compiled once. main() hands the Fixture the ABI's after-the-pump
# step, so this object sees src/ and takes its include paths from the fixture and the ABI; the
# objects themselves come from bwapi_c2_add_test(), which links both beside it.
add_library(bwapi_c2_doctest_main OBJECT "${PROJECT_SOURCE_DIR}/tests/support/doctest_main.cpp")
target_include_directories(bwapi_c2_doctest_main
  PUBLIC "${PROJECT_SOURCE_DIR}/tests/support"
  PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(bwapi_c2_doctest_main PRIVATE bwapi_c2_fixture bwapi_c2_abi)

# bwapi_c2_add_test(<name> <source>...): a doctest executable over the fixture and the ABI's own
# objects, registered with CTest under its own name so a failure names the suite. The ABI is
# linked as objects, not as the shared library, so the exports and the fixture share one
# closure, one BroodwarPtr and one error latch.
function(bwapi_c2_add_test name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE bwapi_c2_fixture bwapi_c2_abi bwapi_c2_doctest_main)
  if(MSVC)
    target_compile_options(${name} PRIVATE /W4)
  else()
    target_compile_options(${name} PRIVATE -Wall -Wextra)
  endif()
  add_test(NAME ${name} COMMAND ${name})
endfunction()
