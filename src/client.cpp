// The client lifecycle (plan sections 4.1 and 5.6): connect, update, disconnect, is_connected
// over BWAPI::BWAPIClient. Everything the frame loop needs and nothing per frame beyond it.
#include "abi_internal.h"

#include <BWAPI/Client.h>

#include <vector>

namespace bwapi_c2 {

namespace {

// getEvents() is a std::list; indexing it would be O(n^2) over a frame's events, so update()
// snapshots it into a vector and the indices are stable until the next update (section 5.6).
// The vector exists now; the accessors over it come in phase 2.
std::vector<BWAPI::Event> g_events;

}  // namespace

void teardown_bwem() {
  // Phase 3 makes this BWEM's reset; until then there is nothing to tear down.
}

}  // namespace bwapi_c2

using namespace bwapi_c2;

extern "C" {

BWAPI_C2_API int32_t BWAPI_C2_CALL bwapi_client_connect(void) BWAPI_C2_NOEXCEPT {
  return guard<int32_t>(0, [] {
    if (BWAPI::BWAPIClient.isConnected()) {
      latch(BWAPI_ERR_ALREADY_CONNECTED, "bwapi_client_connect: already connected");
      return 0;
    }
    // Client::connect() writes its own diagnostics to std::cout and std::cerr in v1 (section 4).
    if (!BWAPI::BWAPIClient.connect()) return 0;
    bind_abi_thread();
    g_events.clear();
    return 1;
  });
}

BWAPI_C2_API void BWAPI_C2_CALL bwapi_client_update(void) BWAPI_C2_NOEXCEPT {
  guard([] {
    if (!game_ready("bwapi_client_update")) return;
    g_events.clear();
    BWAPI::BWAPIClient.update();
    // The server may have gone away inside update(); Client::disconnect() then nulled
    // BroodwarPtr, and there is nothing to snapshot.
    if (!BWAPI::BroodwarPtr) {
      unbind_abi_thread();
      return;
    }
    const auto& events = BWAPI::BroodwarPtr->getEvents();
    g_events.assign(events.begin(), events.end());
  });
}

BWAPI_C2_API void BWAPI_C2_CALL bwapi_client_disconnect(void) BWAPI_C2_NOEXCEPT {
  guard([] {
    if (!on_abi_thread()) {
      latch(BWAPI_ERR_WRONG_THREAD, "bwapi_client_disconnect: called from a thread other than the one that connected");
      return;
    }
    // BWEM's map points into the game data, so it goes first (section 8.2); then the client.
    // Safe when not connected: both halves are no-ops then.
    teardown_bwem();
    g_events.clear();
    BWAPI::BWAPIClient.disconnect();
    unbind_abi_thread();
  });
}

BWAPI_C2_API int32_t BWAPI_C2_CALL bwapi_client_is_connected(void) BWAPI_C2_NOEXCEPT {
  return guard<int32_t>(0, [] { return BWAPI::BWAPIClient.isConnected() ? 1 : 0; });
}

}  // extern "C"
