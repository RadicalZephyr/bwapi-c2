// The ABI's own surface (plan section 4): versions, the sticky error channel, the log and error
// callbacks, thread affinity, the re-entrancy depth counter, and the buffer helpers every
// wrapper shares. Nothing here touches the game; client.cpp and handles.cpp do that.
#include "abi_internal.h"

#include <BWAPI.h>
#include <bwem.h>
#include <starcraftver.h>
#include <svnrev.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>

#ifndef BWAPI_C2_VERSION_MAJOR
#  error "BWAPI_C2_VERSION_* come from CMake's project(VERSION); build through CMakeLists.txt"
#endif

namespace bwapi_c2 {

namespace {

// The latch and the two callbacks share one mutex. It is never held while a callback runs, so
// a callback may read the channel; and the latch is written from whichever thread made the
// failing call, which is exactly what the WRONG_THREAD test does.
std::mutex g_mutex;
int32_t g_code = BWAPI_ERR_NONE;
std::string g_message;
bwapi_log_callback g_log_cb = nullptr;
void* g_log_user = nullptr;
bwapi_error_callback g_error_cb = nullptr;
void* g_error_user = nullptr;

// The thread bound by connect(); the default-constructed id means unbound.
std::atomic<std::thread::id> g_abi_thread{std::thread::id{}};

thread_local int t_reentrancy_depth = 0;

}  // namespace

void latch(int32_t code, const std::string& message) {
  bwapi_error_callback cb = nullptr;
  void* user = nullptr;
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_code != BWAPI_ERR_NONE) return;
    g_code = code;
    g_message = message;
    cb = g_error_cb;
    user = g_error_user;
  }
  if (cb) {
    ReentrancyScope scope;
    cb(code, message.c_str(), user);
  }
}

void latch(int32_t code, const char* message) {
  latch(code, std::string(message ? message : ""));
}

void log(int32_t level, const std::string& message) {
  bwapi_log_callback cb = nullptr;
  void* user = nullptr;
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    cb = g_log_cb;
    user = g_log_user;
  }
  if (cb) {
    ReentrancyScope scope;
    cb(level, message.c_str(), user);
  }
}

int32_t classify(const std::exception& e) {
  return dynamic_cast<const BWEM::Exception*>(&e) ? BWAPI_ERR_BWEM : BWAPI_ERR_EXCEPTION;
}

void bind_abi_thread() { g_abi_thread.store(std::this_thread::get_id()); }
void unbind_abi_thread() { g_abi_thread.store(std::thread::id{}); }
bool on_abi_thread() {
  const std::thread::id bound = g_abi_thread.load();
  return bound == std::thread::id{} || bound == std::this_thread::get_id();
}

int reentrancy_depth() { return t_reentrancy_depth; }
ReentrancyScope::ReentrancyScope() { ++t_reentrancy_depth; }
ReentrancyScope::~ReentrancyScope() { --t_reentrancy_depth; }

bool game_ready(const char* fn) {
  if (!on_abi_thread()) {
    latch(BWAPI_ERR_WRONG_THREAD, std::string(fn) + ": called from a thread other than the one that connected");
    return false;
  }
  if (!BWAPI::BroodwarPtr) {
    latch(BWAPI_ERR_NOT_CONNECTED, std::string(fn) + ": not connected");
    return false;
  }
  return true;
}

int32_t write_string(char* buf, int32_t buf_len, const char* s, size_t len) {
  if (buf_len < 0 || (buf == nullptr && buf_len != 0)) {
    latch(BWAPI_ERR_BAD_BUFFER, "string out: NULL buffer with a nonzero length, or a negative length");
    return 0;
  }
  if (buf_len > 0) {
    const size_t room = static_cast<size_t>(buf_len) - 1;
    const size_t n = len < room ? len : room;
    if (n) std::memcpy(buf, s, n);
    buf[n] = '\0';
  }
  return len > static_cast<size_t>(INT32_MAX) ? INT32_MAX : static_cast<int32_t>(len);
}

int32_t write_string(char* buf, int32_t buf_len, const std::string& s) {
  return write_string(buf, buf_len, s.data(), s.size());
}

int32_t empty_string(char* buf, int32_t buf_len) {
  if (buf && buf_len > 0) buf[0] = '\0';
  return 0;
}

bool check_buffer(const void* out, int32_t cap) {
  if (cap < 0 || (out == nullptr && cap != 0)) {
    latch(BWAPI_ERR_BAD_BUFFER, "array out: NULL buffer with a nonzero cap, or a negative cap");
    return false;
  }
  return true;
}

}  // namespace bwapi_c2

using namespace bwapi_c2;

extern "C" {

// ---- versions ----------------------------------------------------------------------------------

// Three int32_t out-params rather than a packed word (section 4): one integer width for every
// scalar in the ABI, and nothing to document about packing. 0.x means unstable; append-only
// begins at 1.0, the exit of phase 4 (section 12).
BWAPI_C2_API void BWAPI_C2_CALL bwapi_abi_version(int32_t* major, int32_t* minor,
                                                  int32_t* patch) BWAPI_C2_NOEXCEPT {
  if (major) *major = BWAPI_C2_VERSION_MAJOR;
  if (minor) *minor = BWAPI_C2_VERSION_MINOR;
  if (patch) *patch = BWAPI_C2_VERSION_PATCH;
}

BWAPI_C2_API int32_t BWAPI_C2_CALL bwapi_abi_version_string(char* buf, int32_t buf_len) BWAPI_C2_NOEXCEPT {
  char s[32];
  const int n = std::snprintf(s, sizeof s, "%d.%d.%d", BWAPI_C2_VERSION_MAJOR, BWAPI_C2_VERSION_MINOR,
                              BWAPI_C2_VERSION_PATCH);
  return write_string(buf, buf_len, s, n > 0 ? static_cast<size_t>(n) : 0);
}

BWAPI_C2_API int32_t BWAPI_C2_CALL bwapi_client_version(void) BWAPI_C2_NOEXCEPT {
  return BWAPI::CLIENT_VERSION;
}

BWAPI_C2_API int32_t BWAPI_C2_CALL bwapi_revision(void) BWAPI_C2_NOEXCEPT {
  return SVN_REV;
}

BWAPI_C2_API int32_t BWAPI_C2_CALL bwapi_is_debug(void) BWAPI_C2_NOEXCEPT {
  return BUILD_DEBUG ? 1 : 0;
}

// ---- the error channel --------------------------------------------------------------------------

BWAPI_C2_API int32_t BWAPI_C2_CALL bwapi_last_error(void) BWAPI_C2_NOEXCEPT {
  std::lock_guard<std::mutex> lock(g_mutex);
  return g_code;
}

BWAPI_C2_API int32_t BWAPI_C2_CALL bwapi_last_error_message(char* buf, int32_t buf_len) BWAPI_C2_NOEXCEPT {
  std::string copy;
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    copy = g_message;
  }
  return write_string(buf, buf_len, copy);
}

BWAPI_C2_API void BWAPI_C2_CALL bwapi_clear_last_error(void) BWAPI_C2_NOEXCEPT {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_code = BWAPI_ERR_NONE;
  g_message.clear();
}

// ---- callbacks ------------------------------------------------------------------------------------

BWAPI_C2_API void BWAPI_C2_CALL bwapi_set_log_callback(bwapi_log_callback cb, void* user) BWAPI_C2_NOEXCEPT {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_log_cb = cb;
  g_log_user = user;
}

BWAPI_C2_API void BWAPI_C2_CALL bwapi_set_error_callback(bwapi_error_callback cb, void* user) BWAPI_C2_NOEXCEPT {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_error_cb = cb;
  g_error_user = user;
}

}  // extern "C"
