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

// The buffer rule, checked once and up front so a malformed buffer never reaches BWAPI and
// BAD_BUFFER is latched before NOT_CONNECTED or INVALID_HANDLE could be: false having latched
// BWAPI_ERR_BAD_BUFFER when (buf, buf_len) or (out, cap) is NULL with a nonzero length, or
// negative. The generated prologue calls one of these first; a hand-written export does the
// same; and every writer below assumes it passed.
bool check_string_buffer(const char* buf, int32_t buf_len);
bool check_buffer(const void* out, int32_t cap);

// The snprintf convention in one place (section 4): writes at most buf_len bytes including the
// NUL and returns the length the string needs, excluding it. Assumes check_string_buffer()
// passed, as write_ids() assumes check_buffer() did.
int32_t write_string(char* buf, int32_t buf_len, const char* s, size_t len);
int32_t write_string(char* buf, int32_t buf_len, const std::string& s);

// The neutral value of a string_out return: an empty string when there is room for one, and 0.
int32_t empty_string(char* buf, int32_t buf_len);

// ---- type ids ------------------------------------------------------------------------------------

// The Unknown id of a type class, and how many ids the class has (0 to Unknown inclusive). Both
// rest on one fact about upstream: Type<>'s constructor clamps any id outside 0..UnknownId to
// UnknownId, so T(-1) is Unknown for every class, Color included, which has no Unknown
// enumerator to name. That fact is spelled here and nowhere else; the neutral of a type: return,
// the isValid bodies and the table emitter all go through these.
template <class T>
inline int32_t unknown_id() {
  return T(-1).getID();
}

template <class T>
inline int32_t id_count() {
  return unknown_id<T>() + 1;
}

// ---- conversions the generated wrappers apply (spec-format.md section 1.4) --------------------

// A BWAPI interface pointer to its id; null is BWAPI_NONE.
template <class T>
inline int32_t id_of(T* p) {
  return p ? static_cast<int32_t>(p->getID()) : BWAPI_NONE;
}

// The collection convention (section 4): the ids of a range of interfaces (pointers, as a
// Unitset holds) or of types (values, as a SetContainer<TechType> holds), ascending, the first
// cap of them written, the total returned. Assumes check_buffer() passed.
template <class T>
inline int32_t id_of_element(const T& e) {
  if constexpr (std::is_pointer_v<T>) {
    return e ? static_cast<int32_t>(e->getID()) : BWAPI_NONE;
  } else {
    return static_cast<int32_t>(e.getID());
  }
}

template <class Range>
int32_t write_ids(int32_t* out, int32_t cap, const Range& range) {
  std::vector<int32_t> ids;
  for (const auto& e : range) {
    const int32_t id = id_of_element(e);
    if (id != BWAPI_NONE) ids.push_back(id);
  }
  std::sort(ids.begin(), ids.end());
  const size_t n = std::min(ids.size(), static_cast<size_t>(cap));
  if (n) std::memcpy(out, ids.data(), n * sizeof(int32_t));
  return static_cast<int32_t>(ids.size());
}

// The struct-evolution rule of section 4, in one place for both shapes it takes. The caller's
// size is the stride: check_stride() is the up-front check, like check_buffer(), false having
// latched BWAPI_ERR_BAD_BUFFER for a stride that cannot hold size itself; write_row() then
// writes one row of that stride at dst: the fields it knows, size set to the bytes filled (so
// BWAPI_HAS_FIELD on a returned row says what is valid), the rest of the stride zeroed, and
// never a byte past it. Like cap, the stride is the caller's word for how much memory is there.
inline bool check_stride(int32_t stride) {
  if (stride < static_cast<int32_t>(sizeof(int32_t))) {
    latch(BWAPI_ERR_BAD_BUFFER, "struct out: size must be the caller's stride, and at least hold size itself");
    return false;
  }
  return true;
}

template <class Row, class Fill>
void write_row(void* dst, int32_t stride, Fill fill) {
  const size_t filled = std::min(static_cast<size_t>(stride), sizeof(Row));
  Row row{};
  fill(row);
  row.size = static_cast<int32_t>(filled);
  char* bytes = static_cast<char*>(dst);
  std::memcpy(bytes, &row, filled);
  std::memset(bytes + filled, 0, static_cast<size_t>(stride) - filled);
}

// The struct-array convention: the caller sets size on element zero and that is the stride of
// the whole array; the first cap rows are written by write_row() and the total returned. cap of
// 0 is the size query and reads nothing.
template <class Row, class Fill>
int32_t write_rows(Row* out, int32_t cap, int32_t total, Fill fill) {
  if (cap <= 0) return total;
  const int32_t stride = out[0].size;
  if (!check_stride(stride)) return 0;
  char* dst = reinterpret_cast<char*>(out);
  const int32_t n = std::min(cap, total);
  for (int32_t i = 0; i < n; ++i, dst += stride)
    write_row<Row>(dst, stride, [&](Row& row) { fill(row, i); });
  return total;
}

// The struct-out convention, one struct rather than an array: check_struct_out() is the
// up-front check, NULL or a bad stride being BAD_BUFFER, and write_struct() is one write_row()
// at the caller's size.
template <class Row>
bool check_struct_out(const Row* out) {
  if (out == nullptr) {
    latch(BWAPI_ERR_BAD_BUFFER, "struct out: NULL");
    return false;
  }
  return check_stride(out->size);
}

template <class Row, class Fill>
void write_struct(Row* out, Fill fill) {
  write_row<Row>(out, out->size, fill);
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

// ---- after the pump (client.cpp) ---------------------------------------------------------------

// Everything bwapi_client_update() does once the frame is in, in the order it does it: today
// the event snapshot of section 5.6, in phase 3 the UnitDestroy dispatch to BWEM's filtered
// hooks (section 8.2). One function, so the order lives in one place. The fixture-driven tests
// pump through GameImpl directly, with no server, and run this same step through the hook
// tests/support/doctest_main.cpp installs on the Fixture once per test binary; no suite replays
// update() by hand, and the Fixture runs it at teardown too, so a snapshot never outlives the
// game it was taken from. With no game the step leaves nothing behind.
void after_pump();

// The frame's events: Game::getEvents() is a std::list, so after_pump() snapshots its nodes
// into this vector and the event exports index it (section 5.6). Pointers, not copies: the
// list lives in the client GameImpl and is cleared only by the next pump, which is when this
// vector is cleared too, so the pointers are valid for exactly the window the plan gives the
// indices, and a frame of ten thousand events costs one walk and no Event copy (an Event copy
// allocates its text). Stable until the next after_pump().
const std::vector<const BWAPI::Event*>& frame_events();

// ---- packing ---------------------------------------------------------------------------------

template <class P>
inline bwapi_position pack(const P& p) {
  return BWAPI_POS_MAKE(p.x, p.y);
}

}  // namespace bwapi_c2
