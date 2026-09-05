// The ABI's own surface: version, the sticky error channel, the log and error callbacks, and
// the noexcept boundary every generated wrapper goes through (plan section 4). Phase 0 ships
// exactly one export, enough to prove a DLL with an undecorated symbol comes out of the build
// on every platform; the rest arrives with the header skeletons and phase 1.
#include <cstdint>

// The public header (include/bwapi_c2.h, step 0.6) owns these macros from the next step on.
#if defined(_WIN32)
#  define BWAPI_C2_EXPORT __declspec(dllexport)
#  define BWAPI_C2_CALL __cdecl
#else
#  define BWAPI_C2_EXPORT __attribute__((visibility("default")))
#  define BWAPI_C2_CALL
#endif

#ifndef BWAPI_C2_VERSION_MAJOR
#  error "BWAPI_C2_VERSION_* come from CMake's project(VERSION); build through CMakeLists.txt"
#endif

extern "C" {

// Three int32_t out-params rather than a packed word (section 4): one integer width for every
// scalar in the ABI, and nothing to document about packing. 0.x means unstable; append-only
// begins at 1.0, the exit of phase 4 (section 12).
BWAPI_C2_EXPORT void BWAPI_C2_CALL bwapi_abi_version(int32_t* major, int32_t* minor,
                                                     int32_t* patch) noexcept {
  if (major) *major = BWAPI_C2_VERSION_MAJOR;
  if (minor) *minor = BWAPI_C2_VERSION_MINOR;
  if (patch) *patch = BWAPI_C2_VERSION_PATCH;
}

}  // extern "C"
