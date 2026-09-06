# The shared fixture builder (plan section 11) and the test-executable helper the suites use.

add_library(bwapi_c2_fixture STATIC fixture.cpp)
target_link_libraries(bwapi_c2_fixture PUBLIC bwapi_c2_closure)
target_include_directories(bwapi_c2_fixture PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
if(MSVC)
  target_compile_options(bwapi_c2_fixture PRIVATE /W4)
else()
  target_compile_options(bwapi_c2_fixture PRIVATE -Wall -Wextra)
endif()

# doctest's runtime and main(), compiled once.
add_library(bwapi_c2_doctest_main OBJECT "${PROJECT_SOURCE_DIR}/tests/support/doctest_main.cpp")
target_include_directories(bwapi_c2_doctest_main PUBLIC "${PROJECT_SOURCE_DIR}/tests/support")

# bwapi_c2_add_test(<name> <source>...): a doctest executable over the fixture, registered with
# CTest under its own name so a failure names the suite.
function(bwapi_c2_add_test name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE bwapi_c2_fixture bwapi_c2_doctest_main)
  if(MSVC)
    target_compile_options(${name} PRIVATE /W4)
  else()
    target_compile_options(${name} PRIVATE -Wall -Wextra)
  endif()
  add_test(NAME ${name} COMMAND ${name})
endfunction()
