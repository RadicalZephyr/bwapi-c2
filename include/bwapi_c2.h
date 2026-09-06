/* bwapi_c2.h -- the BWAPI half of bwapi-c2: lifecycle, versions, the error channel, the two
 * callbacks, and the client. Every wrapper over BWAPI::Game, Unit, Player, ... is emitted into
 * this header from phase 1 on; phase 0 declares only what the ABI itself owns.
 *
 * The conventions are stated once, at the top of bwapi_c2_types.h, and hold for every
 * declaration here. The protocol is section 4.1 of docs/c-abi-plan.md:
 *
 *     while (!bwapi_client_connect()) sleep(1000);
 *     for (;;) {
 *       while (!bwapi_game_is_in_game()) { bwapi_client_update(); if (!bwapi_client_is_connected()) goto reconnect; }
 *       while (bwapi_game_is_in_game()) {
 *         bwapi_clear_last_error();
 *         ... poll events, take snapshots, issue commands ...
 *         bwapi_client_update();                  -- blocks on the pipe
 *         if (!bwapi_client_is_connected()) break;
 *       }
 *     }
 */
#ifndef BWAPI_C2_H
#define BWAPI_C2_H

#include "bwapi_c2_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- versions --------------------------------------------------------------------------- */

/* The ABI's own version. 0.x is explicitly unstable; append-only begins at 1.0. */
BWAPI_C2_API void BWAPI_C2_CALL bwapi_abi_version(int32_t* major, int32_t* minor, int32_t* patch) BWAPI_C2_NOEXCEPT;
/* "major.minor.patch"; snprintf convention. */
BWAPI_C2_API int32_t BWAPI_C2_CALL bwapi_abi_version_string(char* buf, int32_t buf_len) BWAPI_C2_NOEXCEPT;
/* BWAPI::CLIENT_VERSION of the pinned BWAPI (10003 today); the server refuses a mismatch. */
BWAPI_C2_API int32_t BWAPI_C2_CALL bwapi_client_version(void) BWAPI_C2_NOEXCEPT;
/* SVN_REV of the pinned BWAPI, from upstream's own generator (plan section 10.3). */
BWAPI_C2_API int32_t BWAPI_C2_CALL bwapi_revision(void) BWAPI_C2_NOEXCEPT;
/* 1 when the pinned BWAPI was built with BUILD_DEBUG. */
BWAPI_C2_API int32_t BWAPI_C2_CALL bwapi_is_debug(void) BWAPI_C2_NOEXCEPT;

/* ---- the ABI error channel -------------------------------------------------------------- */

/* Sticky: latches the FIRST error since the last clear and is never cleared implicitly.
 * Successful calls do not touch it and reading does not clear it. Clear it at the top of the
 * frame, check it before update(). This is a different lifetime from BWAPI's own
 * Game::getLastError(), which is per call and comes back through bwapi_game_get_last_error(). */
BWAPI_C2_API int32_t BWAPI_C2_CALL bwapi_last_error(void) BWAPI_C2_NOEXCEPT;
/* The message that came with the latched code (a BWEM::Exception's what(), the two unit ids
 * of an overlap, ...); empty when nothing is latched. snprintf convention. */
BWAPI_C2_API int32_t BWAPI_C2_CALL bwapi_last_error_message(char* buf, int32_t buf_len) BWAPI_C2_NOEXCEPT;
/* Back to BWAPI_ERR_NONE. */
BWAPI_C2_API void BWAPI_C2_CALL bwapi_clear_last_error(void) BWAPI_C2_NOEXCEPT;

/* ---- callbacks, both optional ------------------------------------------------------------- */

typedef void (BWAPI_C2_CALL *bwapi_log_callback)(int32_t level, const char* msg, void* user);
typedef void (BWAPI_C2_CALL *bwapi_error_callback)(int32_t code, const char* msg, void* user);

/* Where the ABI's diagnostics go, at BWAPI_LOG_* levels. NULL disables. Client::connect()
 * still writes to std::cout and std::cerr itself in v1. */
BWAPI_C2_API void BWAPI_C2_CALL bwapi_set_log_callback(bwapi_log_callback cb, void* user) BWAPI_C2_NOEXCEPT;
/* Invoked at the moment an error is latched, so a wrapper can raise at the failing call
 * rather than poll. Off by default; the sticky latch is unchanged either way. A mutating call
 * from inside this callback is rejected with BWAPI_ERR_REENTRANT_MUTATION. */
BWAPI_C2_API void BWAPI_C2_CALL bwapi_set_error_callback(bwapi_error_callback cb, void* user) BWAPI_C2_NOEXCEPT;

/* ---- the client ----------------------------------------------------------------------------- */

/* Connects to a running BWAPI server over its shared memory and pipe. Returns 1 on success,
 * 0 when no server is up (retry after a pause). Records the calling thread as the thread every
 * later call must come from. Already connected: BWAPI_ERR_ALREADY_CONNECTED and 0. */
BWAPI_C2_API int32_t BWAPI_C2_CALL bwapi_client_connect(void) BWAPI_C2_NOEXCEPT;
/* Ends the frame and BLOCKS on the pipe until the server hands back the next one. Every event
 * index and snapshot from the previous frame is invalid afterwards. */
BWAPI_C2_API void BWAPI_C2_CALL bwapi_client_update(void) BWAPI_C2_NOEXCEPT;
/* Tears down BWEM first (its map points into the game data), then the client. Safe to call
 * when not connected. */
BWAPI_C2_API void BWAPI_C2_CALL bwapi_client_disconnect(void) BWAPI_C2_NOEXCEPT;
/* 1 while connected. Becomes 0 when the server goes away during update(). */
BWAPI_C2_API int32_t BWAPI_C2_CALL bwapi_client_is_connected(void) BWAPI_C2_NOEXCEPT;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* BWAPI_C2_H */
