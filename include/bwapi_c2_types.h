/* bwapi_c2_types.h -- the types, macros and constants shared by bwapi_c2.h and bwapi_c2_bwem.h.
 *
 * bwapi-c2 is a flat C ABI over BWAPI and BWEM (docs/c-abi-plan.md). This header is C99; it
 * compiles as C, as C++, and twice in one translation unit, and the test suite checks all
 * three. Phase 0 hand-writes it; from phase 1 the generator emits it, byte for byte or in one
 * replacing commit (implementation plan 0.6).
 *
 * The conventions every declaration in both headers follows (plan section 4):
 *
 *   - Everything is extern "C", __cdecl, exported by name with no ordinals.
 *   - Types are int32_t, int64_t, uint32_t, uint8_t, int16_t (bulk grids only), double, char,
 *     void* and function pointers, plus the PODs declared here. Never C++ bool; never an enum
 *     in a signature -- an int32_t with #defined constants, so an unknown future value cannot
 *     be undefined behaviour in a strict-enum language. No struct by value in or out.
 *   - Scalar booleans are int32_t, 0 or 1, in and out. Bulk boolean grids are uint8_t per
 *     element; snapshot flags are bits in a uint32_t.
 *   - A position RETURN is one packed int64_t (bwapi_position below). Position PARAMETERS and
 *     positions inside structs stay as separate int32_t x, y. Position arrays are packed.
 *   - Strings out use the snprintf convention: write at most buf_len bytes including the NUL,
 *     return the length the string needs excluding it; buf may be NULL when buf_len is 0.
 *     Strings in are const char*, passed through without transcoding.
 *   - Collections out fill a caller-provided buffer up to cap and return the total available,
 *     sorted ascending by id; when cap < total the elements are the first cap in id order.
 *     No allocation crosses the boundary, so nothing is ever freed through it.
 *   - Every POD that crosses the boundary begins with int32_t size (see BWAPI_HAS_FIELD).
 *   - Handles are int32_t ids in disjoint spaces, with one exception: a BWEM neutral IS a
 *     BWAPI unit and is addressed by its unit id. BWAPI_NONE (-1) is the neutral id value.
 *   - A handle that could never have been valid (negative, out of range, wrong kind, used
 *     before connect) returns the documented neutral value AND latches BWAPI_ERR_INVALID_HANDLE.
 *     A handle to a unit that has since died returns what BWAPI returns for a dead unit, with
 *     no latch; bwapi_unit_exists() tells the two apart.
 *   - Every export is a noexcept boundary: an exception inside latches an error and returns
 *     the neutral value. Nothing unwinds into the host.
 *   - All calls happen on the thread that calls bwapi_client_update(); any other thread gets
 *     the neutral value and BWAPI_ERR_WRONG_THREAD.
 */
#ifndef BWAPI_C2_TYPES_H
#define BWAPI_C2_TYPES_H

#include <stddef.h>
#include <stdint.h>

/* ---- linkage -------------------------------------------------------------------------- */

/* Explicit on every export; never the project default, because loaders guess. */
#if defined(_WIN32)
#  define BWAPI_C2_CALL __cdecl
#else
#  define BWAPI_C2_CALL
#endif

/* dllexport while building bwapi_c2.dll, dllimport for its consumers; default visibility on
 * ELF, where the library is otherwise built with everything hidden. */
#if defined(_WIN32)
#  if defined(BWAPI_C2_BUILDING)
#    define BWAPI_C2_API __declspec(dllexport)
#  else
#    define BWAPI_C2_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) || defined(__clang__)
#  define BWAPI_C2_API __attribute__((visibility("default")))
#else
#  define BWAPI_C2_API
#endif

/* The noexcept boundary is a promise the C++ definitions keep; C has no spelling for it. */
#if defined(__cplusplus)
#  define BWAPI_C2_NOEXCEPT noexcept
#else
#  define BWAPI_C2_NOEXCEPT
#endif

/* ---- positions ------------------------------------------------------------------------ */

/* A packed position: x in the low 32 bits, y in the high 32, two's complement. One register,
 * no pointer, no ABI ambiguity about 8-byte PODs returned by value. Used for Position,
 * TilePosition and WalkPosition alike; the function's documentation says which scale. */
typedef int64_t bwapi_position;

#define BWAPI_POS_X(p)       ((int32_t)(uint32_t)((uint64_t)(p) & 0xFFFFFFFFu))
#define BWAPI_POS_Y(p)       ((int32_t)(uint32_t)((uint64_t)(p) >> 32))
#define BWAPI_POS_MAKE(x, y) ((bwapi_position)(((uint64_t)(uint32_t)(y) << 32) | (uint32_t)(x)))

/* BWAPI's own sentinels (BWAPI/Position.h), in both forms. Packing is lossless, so these are
 * the same values a C++ bot sees; no new bit pattern is invented. Positions::None packed is the
 * neutral return of every position-returning function given an invalid handle. */
#define BWAPI_POSITION_INVALID_X      32000
#define BWAPI_POSITION_INVALID_Y      32000
#define BWAPI_POSITION_NONE_X         32000
#define BWAPI_POSITION_NONE_Y         32032
#define BWAPI_POSITION_UNKNOWN_X      32000
#define BWAPI_POSITION_UNKNOWN_Y      32064
#define BWAPI_POSITION_ORIGIN_X       0
#define BWAPI_POSITION_ORIGIN_Y       0
#define BWAPI_POSITION_INVALID        BWAPI_POS_MAKE(BWAPI_POSITION_INVALID_X, BWAPI_POSITION_INVALID_Y)
#define BWAPI_POSITION_NONE           BWAPI_POS_MAKE(BWAPI_POSITION_NONE_X, BWAPI_POSITION_NONE_Y)
#define BWAPI_POSITION_UNKNOWN        BWAPI_POS_MAKE(BWAPI_POSITION_UNKNOWN_X, BWAPI_POSITION_UNKNOWN_Y)
#define BWAPI_POSITION_ORIGIN         BWAPI_POS_MAKE(BWAPI_POSITION_ORIGIN_X, BWAPI_POSITION_ORIGIN_Y)

/* WalkPosition: scale 8. */
#define BWAPI_WALKPOSITION_INVALID_X  4000
#define BWAPI_WALKPOSITION_INVALID_Y  4000
#define BWAPI_WALKPOSITION_NONE_X     4000
#define BWAPI_WALKPOSITION_NONE_Y     4004
#define BWAPI_WALKPOSITION_UNKNOWN_X  4000
#define BWAPI_WALKPOSITION_UNKNOWN_Y  4008
#define BWAPI_WALKPOSITION_ORIGIN_X   0
#define BWAPI_WALKPOSITION_ORIGIN_Y   0
#define BWAPI_WALKPOSITION_INVALID    BWAPI_POS_MAKE(BWAPI_WALKPOSITION_INVALID_X, BWAPI_WALKPOSITION_INVALID_Y)
#define BWAPI_WALKPOSITION_NONE       BWAPI_POS_MAKE(BWAPI_WALKPOSITION_NONE_X, BWAPI_WALKPOSITION_NONE_Y)
#define BWAPI_WALKPOSITION_UNKNOWN    BWAPI_POS_MAKE(BWAPI_WALKPOSITION_UNKNOWN_X, BWAPI_WALKPOSITION_UNKNOWN_Y)
#define BWAPI_WALKPOSITION_ORIGIN     BWAPI_POS_MAKE(BWAPI_WALKPOSITION_ORIGIN_X, BWAPI_WALKPOSITION_ORIGIN_Y)

/* TilePosition: scale 32. */
#define BWAPI_TILEPOSITION_INVALID_X  1000
#define BWAPI_TILEPOSITION_INVALID_Y  1000
#define BWAPI_TILEPOSITION_NONE_X     1000
#define BWAPI_TILEPOSITION_NONE_Y     1001
#define BWAPI_TILEPOSITION_UNKNOWN_X  1000
#define BWAPI_TILEPOSITION_UNKNOWN_Y  1002
#define BWAPI_TILEPOSITION_ORIGIN_X   0
#define BWAPI_TILEPOSITION_ORIGIN_Y   0
#define BWAPI_TILEPOSITION_INVALID    BWAPI_POS_MAKE(BWAPI_TILEPOSITION_INVALID_X, BWAPI_TILEPOSITION_INVALID_Y)
#define BWAPI_TILEPOSITION_NONE       BWAPI_POS_MAKE(BWAPI_TILEPOSITION_NONE_X, BWAPI_TILEPOSITION_NONE_Y)
#define BWAPI_TILEPOSITION_UNKNOWN    BWAPI_POS_MAKE(BWAPI_TILEPOSITION_UNKNOWN_X, BWAPI_TILEPOSITION_UNKNOWN_Y)
#define BWAPI_TILEPOSITION_ORIGIN     BWAPI_POS_MAKE(BWAPI_TILEPOSITION_ORIGIN_X, BWAPI_TILEPOSITION_ORIGIN_Y)

/* ---- handles -------------------------------------------------------------------------- */

/* Each is BWAPI's or BWEM's own id, widened to int32_t where narrower. The spaces are
 * disjoint: a unit id is never valid where a player id is expected. The typedefs exist so a
 * binding generator can tell them apart; at the ABI they are all int32_t. */
typedef int32_t bwapi_unit_id;      /* Unit::getID(); a BWEM neutral is addressed by this too */
typedef int32_t bwapi_player_id;    /* Player::getID(), 0..11 */
typedef int32_t bwapi_force_id;     /* Force::getID(), 0..4 */
typedef int32_t bwapi_region_id;    /* Region::getID() */
typedef int32_t bwapi_bullet_id;    /* Bullet::getID() */
typedef int32_t bwapi_bwem_area_id; /* BWEM Area::Id(), 1..N; 0 is not walkable */
typedef int32_t bwapi_bwem_choke_id;/* BWEM ChokePoint::Index(), 0..N-1 */
typedef int32_t bwapi_bwem_base_id; /* synthesised by the ABI, 0..N-1; BWEM has none */

/* The neutral value of every id space; BWAPI's own "none" for every unit-index field. */
#define BWAPI_NONE (-1)

/* ---- struct evolution ----------------------------------------------------------------- */

/* Every POD crossing the boundary begins with int32_t size. The caller sets it on input
 * structs; the callee reads only the prefix it understands. The callee sets it on output
 * structs, zero-fills up to the caller's size and never writes past it. For array-out
 * functions the caller sets size on element zero and that is the stride for the whole array.
 *
 * BWAPI_HAS_FIELD(type, field, size) is true when a struct of the given size, as filled by the
 * DLL actually loaded, includes the field: a consumer compiled against a newer header tests
 * this before reading. */
#define BWAPI_HAS_FIELD(type, field, size) \
  (offsetof(type, field) + sizeof(((type*)0)->field) <= (size_t)(size))

/* ---- the ABI's own error codes ----------------------------------------------------------- */

/* Latched by the sticky first-error channel in bwapi_c2.h. These are the ABI's errors, about
 * how it was called; BWAPI's own Errors:: codes come back unchanged through
 * bwapi_game_get_last_error(), on a different lifetime. A code says WHICH section-4 rule
 * fired, because the error callback exists so a wrapper can raise at the failing call. None of
 * these values is ever reused. */
#define BWAPI_ERR_NONE                  0 /* nothing latched since the last clear */
#define BWAPI_ERR_ALREADY_CONNECTED     1 /* bwapi_client_connect() while connected */
#define BWAPI_ERR_WRONG_THREAD          2 /* a call from a thread other than the update thread */
#define BWAPI_ERR_REENTRANT_MUTATION    3 /* a mutating call from inside a callback */
#define BWAPI_ERR_BWEM                  4 /* a BWEM::Exception, or a map BWEM would crash on */
#define BWAPI_ERR_INVALID_HANDLE        5 /* an id that could never have been valid */
#define BWAPI_ERR_NOT_CONNECTED         6 /* a game or unit call before connect, or after disconnect */
#define BWAPI_ERR_BWEM_NOT_INITIALIZED  7 /* a bwapi_bwem_* query before bwapi_bwem_initialize() */
#define BWAPI_ERR_BAD_BUFFER            8 /* a NULL buffer with a nonzero cap or buf_len, or a negative one */

/* ---- log levels ------------------------------------------------------------------------- */

/* The level argument of the log callback (bwapi_c2.h). Client::connect()'s own std::cout and
 * std::cerr output is not redirected in v1; these carry the ABI's diagnostics. */
#define BWAPI_LOG_INFO   0
#define BWAPI_LOG_WARN   1 /* a rejected re-entrant call, among others */
#define BWAPI_LOG_ERROR  2

#endif /* BWAPI_C2_TYPES_H */
