// R13: what does the per-frame event snapshot cost, and is it worth making lazy?
//
// bwapi_client_update() calls bwapi_c2::after_pump(), which snapshots Game::getEvents() into a
// vector of node pointers (section 5.6). A host that never reads events still pays for that.
// This measures three things at frame sizes a real game produces:
//
//   snapshot   after_pump(): clear, reserve, walk the list, store N pointers
//   walk       the same list walk with no vector, the floor any snapshot must pay
//   drain      bwapi_game_get_events() over the snapshot: what the host pays when it does read
//
// Built only with -DBWAPI_C2_BENCH=ON and never registered with CTest, since a timing loop is
// not a test. Run it through run-event-snapshot-bench.sh, which builds Release: an unoptimized
// build reports numbers 20x larger and in the wrong proportion.
#include "doctest.h"
#include "fixture.h"

#include "bwapi_c2.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

// abi_internal.h is not on this target's include path; the ABI's objects are linked in, so name
// the internal the way tests/support/doctest_main.cpp does.
namespace bwapi_c2 { void after_pump(); }

using namespace BWAPI;
using bwapi_c2::test::Fixture;

namespace {

// The best of several runs, which is the number that is about the code rather than about what
// else the machine was doing.
template <class F>
double best_ns(int repeats, int reps, F f) {
  double best = 1e18;
  for (int r = 0; r < repeats; ++r) {
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < reps; ++i) f();
    const auto t1 = std::chrono::steady_clock::now();
    best = std::min(best, std::chrono::duration<double, std::nano>(t1 - t0).count() / reps);
  }
  return best;
}

}  // namespace

TEST_CASE("R13: the event snapshot against the walk it must do and the drain it enables") {
  std::printf("\n%8s %13s %11s %13s %14s\n",
              "events", "snapshot ns", "walk ns", "drain ns", "snapshot/drain");
  // 2 is an ordinary frame (BWAPI sends MatchFrame every frame); 200 is MatchStart-sized on a
  // populated map; 1000 is past anything observed, short of the 10,000 GameData allows.
  for (int queued : {1, 10, 50, 200, 1000}) {
    Fixture f;
    f.player(0, Races::Terran, 50, 0, "Bench");
    const int scv = f.unit(0, UnitTypes::Terran_SCV, 1000, 2000);
    for (int i = 0; i < queued; ++i) f.event(EventType::UnitShow, scv);
    f.start();
    const int32_t total = bwapi_game_event_count();
    REQUIRE(total > 0);

    const double snap = best_ns(5, 2000, [] { bwapi_c2::after_pump(); });
    const double walk = best_ns(5, 2000, [] {
      int32_t k = 0;
      for (const Event& e : BroodwarPtr->getEvents()) k += static_cast<int32_t>(e.getType());
      static volatile int32_t sink;
      sink = k;
    });
    std::vector<bwapi_event> rows(static_cast<size_t>(total));
    const double drain = best_ns(5, 2000, [&] {
      bwapi_game_get_events(rows.data(), static_cast<int32_t>(rows.size()), sizeof(bwapi_event));
    });
    std::printf("%8d %13.1f %11.1f %13.1f %13.0f%%\n", total, snap, walk, drain,
                100.0 * snap / drain);
  }
  std::printf("\n");
}
