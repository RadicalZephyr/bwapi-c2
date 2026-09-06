# The golden .def diff (plan section 11): the built library exports exactly what bwapi_c2.def
# lists. dumpbin on Windows, nm elsewhere; the script picks.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_test(NAME exports.def_matches_library
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_exports.py"
            --lib $<TARGET_FILE:BWAPI_C2> --def "${PROJECT_SOURCE_DIR}/bwapi_c2.def")
endif()
