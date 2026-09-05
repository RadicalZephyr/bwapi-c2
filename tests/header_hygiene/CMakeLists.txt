# Header hygiene (plan section 11, first row): every public header compiles standalone as C99
# under -pedantic -Werror (or /TC /W4 /WX), as C++17, and twice in one translation unit. The
# cheapest test in the suite, and the first line of defence against a generator emitting
# something C cannot parse. Each check is a tiny executable, built and run by CTest.

set(_headers bwapi_c2_types bwapi_c2 bwapi_c2_bwem)

foreach(_h IN LISTS _headers)
  # C99, strict.
  add_executable(hygiene_c99_${_h} c99/${_h}.c)
  target_link_libraries(hygiene_c99_${_h} PRIVATE bwapi_c2_headers)
  if(MSVC)
    target_compile_options(hygiene_c99_${_h} PRIVATE /TC /W4 /WX)
  else()
    target_compile_options(hygiene_c99_${_h} PRIVATE -std=c99 -pedantic -Wall -Wextra -Werror)
  endif()
  add_test(NAME header_hygiene.c99.${_h} COMMAND hygiene_c99_${_h})

  # C++17, strict.
  add_executable(hygiene_cxx17_${_h} cxx17/${_h}.cpp)
  target_link_libraries(hygiene_cxx17_${_h} PRIVATE bwapi_c2_headers)
  if(MSVC)
    target_compile_options(hygiene_cxx17_${_h} PRIVATE /W4 /WX)
  else()
    target_compile_options(hygiene_cxx17_${_h} PRIVATE -pedantic -Wall -Wextra -Werror)
  endif()
  add_test(NAME header_hygiene.cxx17.${_h} COMMAND hygiene_cxx17_${_h})
endforeach()

# All three headers, each included twice, in C and in C++.
add_executable(hygiene_twice_c twice.c)
target_link_libraries(hygiene_twice_c PRIVATE bwapi_c2_headers)
add_executable(hygiene_twice_cxx twice.cpp)
target_link_libraries(hygiene_twice_cxx PRIVATE bwapi_c2_headers)
if(MSVC)
  target_compile_options(hygiene_twice_c PRIVATE /TC /W4 /WX)
  target_compile_options(hygiene_twice_cxx PRIVATE /W4 /WX)
else()
  target_compile_options(hygiene_twice_c PRIVATE -std=c99 -pedantic -Wall -Wextra -Werror)
  target_compile_options(hygiene_twice_cxx PRIVATE -pedantic -Wall -Wextra -Werror)
endif()
add_test(NAME header_hygiene.twice.c COMMAND hygiene_twice_c)
add_test(NAME header_hygiene.twice.cxx COMMAND hygiene_twice_cxx)
