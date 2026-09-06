// What every wrapper in the library shares and no consumer sees: the sticky error latch, the
// log channel, the thread-affinity check, the re-entrancy depth counter, the noexcept boundary
// template and the handle resolvers (plan sections 4, 5.4 and 6). The generated *.gen.cpp files
// include this and nothing else of the library's own.
//
// Nothing here is exported: the object library is built with hidden visibility and the .def
// lists only the BWAPI_C2_API functions.
#pragma once

#include "bwapi_c2.h"
#include "bwapi_c2_bwem.h"

#include <BWAPI.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <exception>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace bwapi_c2 {

// ---- the error channel (abi.cpp) --------------------------------------------------------------

// Sets the sticky code and message only when the current code is BWAPI_ERR_NONE, and invokes
// the error callback at that moment, with the re-entrancy depth raised for the duration of the
// callback. A latch on an already-latched channel is a no-op: the first error is the causal one.
void latch(int32_t code, const std::string& message);
void latch(int32_t code, const char* message = "");

// The log channel: goes to the log callback when one is set, nowhere otherwise.
void log(int32_t level, const std::string& message);

// Which ABI code a caught exception maps to: BWAPI_ERR_BWEM for a BWEM::Exception (told apart
// by dynamic_cast), BWAPI_ERR_EXCEPTION for anything else.
int32_t classify(const std::exception& e);

// ---- thread affinity (abi.cpp) ---------------------------------------------------------------

// connect() binds the calling thread; disconnect() unbinds it. While bound, every call that
// touches the game must come from that thread. While unbound, nothing is checked: before
// connect there is no game to race on, and the fixture-driven tests never connect.
void bind_abi_thread();
void unbind_abi_thread();
bool on_abi_thread();

// ---- re-entrancy depth (abi.cpp) ---------------------------------------------------------------

// A thread-local counter raised while a callback (the error callback today; predicates if they
// ever land, section 5.4) runs. Mutating wrappers will check it; in v1 only the counter exists.
int reentrancy_depth();
struct ReentrancyScope {
  ReentrancyScope();
  ~ReentrancyScope();
  ReentrancyScope(const ReentrancyScope&) = delete;
  ReentrancyScope& operator=(const ReentrancyScope&) = delete;
};

// ---- the boundary ----------------------------------------------------------------------------

// The prologue of every wrapper that touches the game: the thread check, then the connected
// check. `fn` names the export for the message. Returns false having latched.
bool game_ready(const char* fn);

// Every export is a noexcept boundary (section 4): the body runs inside try, an exception
// latches its classified code with its what(), and the neutral value comes back. Two forms,
// because there is no neutral value to name for void.
template <class Neutral, class Fn>
Neutral guard(Neutral neutral, Fn&& fn) noexcept {
  try {
    return static_cast<Neutral>(fn());
  } catch (const std::exception& e) {
    latch(classify(e), e.what());
  } catch (...) {
    latch(BWAPI_ERR_EXCEPTION, "a non-standard exception escaped the wrapped call");
  }
  return neutral;
}

template <class Fn>
void guard(Fn&& fn) noexcept {
  try {
    fn();
  } catch (const std::exception& e) {
    latch(classify(e), e.what());
  } catch (...) {
    latch(BWAPI_ERR_EXCEPTION, "a non-standard exception escaped the wrapped call");
  }
}

// ---- buffers (abi.cpp) -------------------------------------------------------------------------

// The snprintf convention in one place (section 4): writes at most buf_len bytes including the
// NUL and returns the length the string needs, excluding it. A NULL buffer with a nonzero
// length, or a negative length, latches BWAPI_ERR_BAD_BUFFER, writes nothing and returns 0.
int32_t write_string(char* buf, int32_t buf_len, const char* s, size_t len);
int32_t write_string(char* buf, int32_t buf_len, const std::string& s);

// The neutral value of a string_out return: an empty string when there is room for one, and 0.
int32_t empty_string(char* buf, int32_t buf_len);

// The buffer rule checked up front, before the call, so a malformed buffer never reaches BWAPI:
// false having latched BWAPI_ERR_BAD_BUFFER when (buf, buf_len) or (out, cap) is NULL with a
// nonzero length, or negative.
bool check_string_buffer(const char* buf, int32_t buf_len);
bool check_buffer(const void* out, int32_t cap);

// ---- conversions the generated wrappers apply (spec-format.md section 1.4) --------------------

// A BWAPI interface pointer to its id; null is BWAPI_NONE.
template <class T>
inline int32_t id_of(T* p) {
  return p ? static_cast<int32_t>(p->getID()) : BWAPI_NONE;
}

// The collection convention (section 4): the ids of a range of interface pointers, ascending,
// the first cap of them written, the total returned. Assumes check_buffer() passed.
template <class Range>
int32_t write_ids(int32_t* out, int32_t cap, const Range& range) {
  std::vector<int32_t> ids;
  for (auto* p : range)
    if (p) ids.push_back(static_cast<int32_t>(p->getID()));
  std::sort(ids.begin(), ids.end());
  const size_t n = std::min(ids.size(), static_cast<size_t>(cap));
  if (n) std::memcpy(out, ids.data(), n * sizeof(int32_t));
  return static_cast<int32_t>(ids.size());
}

// Packed positions in upstream's order (a chokepoint's geometry is a polyline), the first cap
// written, the total returned.
template <class Range>
int32_t write_positions(bwapi_position* out, int32_t cap, const Range& range) {
  int32_t total = 0;
  for (const auto& p : range) {
    if (total < cap) out[total] = BWAPI_POS_MAKE(p.x, p.y);
    ++total;
  }
  return total;
}

// ---- handles (handles.cpp) --------------------------------------------------------------------

// Range and kind, never existence (section 6.2). A handle that could never have been valid
// returns null having latched BWAPI_ERR_INVALID_HANDLE; a dead unit resolves like a live one.
// Both assume game_ready() already passed.
BWAPI::Unit resolve_unit(bwapi_unit_id id, const char* fn);
BWAPI::Player resolve_player(bwapi_player_id id, const char* fn);
BWAPI::Force resolve_force(bwapi_force_id id, const char* fn);
BWAPI::Region resolve_region(bwapi_region_id id, const char* fn);

// ---- lifecycle hooks (client.cpp) -----------------------------------------------------------

// What disconnect() runs before the client goes: BWEM's teardown. A no-op until phase 3.
void teardown_bwem();

// ---- packing ---------------------------------------------------------------------------------

template <class P>
inline bwapi_position pack(const P& p) {
  return BWAPI_POS_MAKE(p.x, p.y);
}

}  // namespace bwapi_c2
