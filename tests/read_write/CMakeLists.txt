bwapi_c2_add_test(read_write_smoke smoke.cpp)
bwapi_c2_add_test(read_write_player player.cpp)
# The event suite pumps through GameImpl directly and then calls the library's own snapshot
# step, so it sees src/ for that one internal, as the errors suite does.
bwapi_c2_add_test(read_write_events events.cpp)
target_include_directories(read_write_events PRIVATE "${PROJECT_SOURCE_DIR}/src")
