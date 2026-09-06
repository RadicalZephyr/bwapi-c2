// Every event export against the fixture (implementation plan 2.1 and 2.2): bwapi_game_event_count(),
// bwapi_game_get_events() and bwapi_game_event_text() over the snapshot the library takes of
// Game::getEvents(). The fixture pumps through GameImpl directly, as every suite's does, and
// runs the library's own after-the-pump step behind start() and frame(), so the snapshot here
// is the one bwapi_client_update() would have taken. It is also the first suite over a
// per-frame struct array, so it proves that half of the section-4 rule on a collection whose
// order is BWAPI's rather than sorted.
#include "doctest.h"
#include "fixture.h"

#include "bwapi_c2.h"

#include <climits>
#include <cstring>
#include <string>
#include <vector>

using namespace BWAPI;
using bwapi_c2::test::Fixture;

namespace {

// Three units (two of them the player's, one a mineral field) and seven queued events of
// seven kinds, so the frame carries ten in queue order: the three UnitDiscover events the
// builder synthesises, then NukeDetect, PlayerLeft, MatchEnd, SendText, ReceiveText,
// SaveGame and UnitComplete.
struct EventScenario {
  Fixture f;
  int scv = -1, marine = -1, mineral = -1;
  static constexpr int32_t kCount = 10;
  EventScenario() {
    bwapi_clear_last_error();
    f.player(0, Races::Terran, 50, 0, "FixtureBot");
    f.player(1, Races::Zerg, 50, 0, "Opponent");
    scv = f.unit(0, UnitTypes::Terran_SCV, 1000, 2000);
    marine = f.unit(0, UnitTypes::Terran_Marine, 1100, 2000);
    mineral = f.neutral(UnitTypes::Resource_Mineral_Field, 10, 10);
    f.event(EventType::NukeDetect, 640, 480);
    f.event(EventType::PlayerLeft, 1);
    f.event(EventType::MatchEnd, 1);
    f.event(EventType::SendText, "gg wp");
    f.event(EventType::ReceiveText, "glhf", 1);
    f.event(EventType::SaveGame, "fixture.sav");
    f.event(EventType::UnitComplete, marine);
    f.start();
  }
  ~EventScenario() { bwapi_clear_last_error(); }
};

// The whole frame, drained in one call at the caller's own stride.
std::vector<bwapi_event> drain(int32_t cap = 64) {
  std::vector<bwapi_event> rows(static_cast<size_t>(cap));
  std::memset(rows.data(), 0x5a, rows.size() * sizeof(bwapi_event));
  const int32_t n = bwapi_game_get_events(rows.data(), cap, sizeof(bwapi_event));
  REQUIRE(n >= 0);
  rows.resize(static_cast<size_t>(n < cap ? n : cap));
  return rows;
}

// One row of this frame, by position, for the per-event assertions below.
bwapi_event get(int32_t index) {
  const auto rows = drain();
  REQUIRE(index < static_cast<int32_t>(rows.size()));
  return rows[static_cast<size_t>(index)];
}

std::string text_of(int32_t index) {
  char buf[512];
  const int32_t n = bwapi_game_event_text(index, buf, sizeof buf);
  return std::string(buf, static_cast<size_t>(n < 511 ? n : 511));
}

std::string message() {
  char buf[256];
  const int32_t n = bwapi_last_error_message(buf, sizeof buf);
  return std::string(buf, static_cast<size_t>(n < 255 ? n : 255));
}

}  // namespace

TEST_CASE("the frame's events come back in the pump's order, each with what its type carries") {
  EventScenario s;
  REQUIRE(bwapi_game_event_count() == EventScenario::kCount);

  // A unit event names its unit and nothing else: BWAPI_NONE where there is no player and the
  // pixel-scale None where there is no position, exactly the defaults Event's accessors hold.
  bwapi_event e = get(0);
  CHECK(e.size == static_cast<int32_t>(sizeof e));
  CHECK(e.type == BWAPI_EVENT_UNIT_DISCOVER);
  CHECK(e.unit_id == s.scv);
  CHECK(e.player_id == BWAPI_NONE);
  CHECK(e.x == BWAPI_POSITION_NONE_X);
  CHECK(e.y == BWAPI_POSITION_NONE_Y);
  CHECK(e.is_winner == 0);
  CHECK(get(1).unit_id == s.marine);
  CHECK(get(2).unit_id == s.mineral);

  e = get(3);
  CHECK(e.type == BWAPI_EVENT_NUKE_DETECT);
  CHECK(e.x == 640);
  CHECK(e.y == 480);
  CHECK(e.unit_id == BWAPI_NONE);
  CHECK(e.player_id == BWAPI_NONE);

  e = get(4);
  CHECK(e.type == BWAPI_EVENT_PLAYER_LEFT);
  CHECK(e.player_id == 1);
  CHECK(e.unit_id == BWAPI_NONE);

  e = get(5);
  CHECK(e.type == BWAPI_EVENT_MATCH_END);
  CHECK(e.is_winner == 1);
  CHECK(e.player_id == BWAPI_NONE);

  CHECK(get(6).type == BWAPI_EVENT_SEND_TEXT);
  CHECK(get(7).type == BWAPI_EVENT_RECEIVE_TEXT);
  CHECK(get(7).player_id == 1);
  CHECK(get(8).type == BWAPI_EVENT_SAVE_GAME);
  e = get(9);
  CHECK(e.type == BWAPI_EVENT_UNIT_COMPLETE);
  CHECK(e.unit_id == s.marine);
  CHECK(bwapi_last_error() == BWAPI_ERR_NONE);

  // And every row agrees with the C++ event it was filled from, so the wrapper cannot drift
  // from upstream's own list.
  int32_t i = 0;
  for (const Event& ev : Broodwar->getEvents()) {
    const bwapi_event row = get(i++);
    CHECK(row.type == static_cast<int32_t>(ev.getType()));
    CHECK(row.unit_id == (ev.getUnit() ? ev.getUnit()->getID() : BWAPI_NONE));
    CHECK(row.player_id == (ev.getPlayer() ? ev.getPlayer()->getID() : BWAPI_NONE));
    CHECK(row.x == ev.getPosition().x);
    CHECK(row.y == ev.getPosition().y);
    CHECK(row.is_winner == (ev.isWinner() ? 1 : 0));
    CHECK(text_of(i - 1) == ev.getText());
  }
  CHECK(i == EventScenario::kCount);
}

TEST_CASE("the text of a text event comes through the string convention") {
  EventScenario s;
  CHECK(text_of(6) == "gg wp");
  CHECK(text_of(7) == "glhf");
  CHECK(text_of(8) == "fixture.sav");
  // An event without text is the empty string, and 0: no latch, nothing to distinguish.
  CHECK(bwapi_game_event_text(0, nullptr, 0) == 0);
  CHECK(text_of(3).empty());
  CHECK(bwapi_last_error() == BWAPI_ERR_NONE);

  SUBCASE("a short buffer is truncated and NUL-terminated, and the full length comes back") {
    char buf[3];
    std::memset(buf, 'x', sizeof buf);
    CHECK(bwapi_game_event_text(6, buf, sizeof buf) == 5);
    CHECK(std::string(buf) == "gg");
  }
  SUBCASE("NULL with zero length queries the length") {
    CHECK(bwapi_game_event_text(6, nullptr, 0) == 5);
    CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
  }
  SUBCASE("a bad buffer latches BAD_BUFFER before anything else") {
    CHECK(bwapi_game_event_text(6, nullptr, 8) == 0);
    CHECK(bwapi_last_error() == BWAPI_ERR_BAD_BUFFER);
    bwapi_clear_last_error();
    // Before the index, too: a bad buffer with a bad index is the buffer's error.
    CHECK(bwapi_game_event_text(-1, nullptr, 8) == 0);
    CHECK(bwapi_last_error() == BWAPI_ERR_BAD_BUFFER);
  }
  SUBCASE("a bad index with a good buffer writes the empty string and latches") {
    char buf[8];
    std::memset(buf, 'x', sizeof buf);
    CHECK(bwapi_game_event_text(EventScenario::kCount, buf, sizeof buf) == 0);
    CHECK(buf[0] == '\0');
    CHECK(bwapi_last_error() == BWAPI_ERR_INVALID_HANDLE);
  }
}

TEST_CASE("a text longer than a shared-memory slot arrives cut to 255 bytes") {
  // GameData::eventStrings is char[1000][256]; the server copies into a slot with its own
  // truncation, and the fixture does the same. What crosses the boundary is what the slot held.
  Fixture f;
  f.player(0, Races::Terran);
  const std::string longer(300, 'x');
  f.event(EventType::SendText, longer.c_str());
  f.start();
  bwapi_clear_last_error();
  REQUIRE(bwapi_game_event_count() == 1);
  CHECK(bwapi_game_event_text(0, nullptr, 0) == 255);
  CHECK(text_of(0) == std::string(255, 'x'));
  CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
}

// bwapi_game_event_text() is the only event export that takes an index, so it carries the
// section-5.6 rule on its own: an index is a position in this frame's snapshot, not a handle.
TEST_CASE("an out-of-range index is the neutral value plus INVALID_HANDLE") {
  EventScenario s;
  for (int32_t bad : {-1, EventScenario::kCount, INT_MAX, INT_MIN}) {
    bwapi_clear_last_error();
    CHECK(bwapi_game_event_text(bad, nullptr, 0) == 0);
    CHECK(bwapi_last_error() == BWAPI_ERR_INVALID_HANDLE);
    CHECK(message().find("bwapi_game_event_text") != std::string::npos);
    CHECK(message().find("10 events") != std::string::npos);
  }
}

TEST_CASE("the struct-array rule: cap, stride and the order the rows come back in") {
  EventScenario s;
  constexpr int32_t kRow = static_cast<int32_t>(sizeof(bwapi_event));

  SUBCASE("the rows are in BWAPI's queue order, not sorted") {
    const auto rows = drain();
    REQUIRE(rows.size() == static_cast<size_t>(EventScenario::kCount));
    // The builder queues three UnitDiscover events and then seven of seven other kinds; the
    // ids in those rows are not ascending, which is what section 4's sort would have made them.
    CHECK(rows[0].type == BWAPI_EVENT_UNIT_DISCOVER);
    CHECK(rows[3].type == BWAPI_EVENT_NUKE_DETECT);
    CHECK(rows[4].type == BWAPI_EVENT_PLAYER_LEFT);
    CHECK(rows[5].type == BWAPI_EVENT_MATCH_END);
    CHECK(rows[6].type == BWAPI_EVENT_SEND_TEXT);
    CHECK(rows[7].type == BWAPI_EVENT_RECEIVE_TEXT);
    CHECK(rows[8].type == BWAPI_EVENT_SAVE_GAME);
    CHECK(rows[9].type == BWAPI_EVENT_UNIT_COMPLETE);
    CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
  }
  SUBCASE("a cap of 0 with NULL is the count, and reads nothing") {
    CHECK(bwapi_game_get_events(nullptr, 0, 0) == EventScenario::kCount);
    CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
  }
  SUBCASE("a cap short of the count fills cap rows and returns the total") {
    constexpr int32_t cap = 4;
    bwapi_event rows[cap + 1];
    std::memset(rows, 0x5a, sizeof rows);
    CHECK(bwapi_game_get_events(rows, cap, kRow) == EventScenario::kCount);
    for (int32_t i = 0; i < cap; ++i) CHECK(rows[i].size == kRow);
    CHECK(rows[cap].size == 0x5a5a5a5a);   // nothing past cap
    CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
  }
  SUBCASE("a stride larger than the row zero-fills each row's tail and reports what was filled") {
    struct Bigger {
      bwapi_event e;
      int32_t extra[4];
    };
    Bigger rows[EventScenario::kCount];
    std::memset(rows, 0xab, sizeof rows);
    CHECK(bwapi_game_get_events(reinterpret_cast<bwapi_event*>(rows), EventScenario::kCount,
                                sizeof(Bigger)) == EventScenario::kCount);
    for (const auto& r : rows) {
      CHECK(r.e.size == kRow);
      for (int32_t v : r.extra) CHECK(v == 0);
      CHECK(BWAPI_HAS_FIELD(bwapi_event, is_winner, r.e.size));
      CHECK_FALSE(BWAPI_HAS_FIELD(Bigger, extra, r.e.size));
    }
    CHECK(rows[3].e.type == BWAPI_EVENT_NUKE_DETECT);
    CHECK(rows[3].e.x == 640);
  }
  SUBCASE("a stride smaller than the row, an older consumer's, is written only that far") {
    struct Older { int32_t size, type; };
    Older rows[EventScenario::kCount];
    std::memset(rows, 0x5a, sizeof rows);
    CHECK(bwapi_game_get_events(reinterpret_cast<bwapi_event*>(rows), EventScenario::kCount,
                                sizeof(Older)) == EventScenario::kCount);
    for (const auto& r : rows) CHECK(r.size == static_cast<int32_t>(sizeof(Older)));
    CHECK(rows[3].type == BWAPI_EVENT_NUKE_DETECT);
    CHECK_FALSE(BWAPI_HAS_FIELD(bwapi_event, unit_id, rows[0].size));
  }
  SUBCASE("one buffer reused across two frames stays correct (R12)") {
    struct Bigger {
      bwapi_event e;
      int32_t extra[4];
    };
    Bigger rows[EventScenario::kCount];
    std::memset(rows, 0xab, sizeof rows);
    for (int32_t call = 0; call < 2; ++call) {
      CHECK(bwapi_game_get_events(reinterpret_cast<bwapi_event*>(rows), EventScenario::kCount,
                                  sizeof(Bigger)) == EventScenario::kCount);
      CHECK(rows[0].e.size == kRow);
      CHECK(rows[3].e.type == BWAPI_EVENT_NUKE_DETECT);
      CHECK(rows[9].e.type == BWAPI_EVENT_UNIT_COMPLETE);
    }
    CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
  }
  SUBCASE("NULL with a nonzero cap, or a stride that cannot hold size itself, is BAD_BUFFER") {
    CHECK(bwapi_game_get_events(nullptr, 1, kRow) == 0);
    CHECK(bwapi_last_error() == BWAPI_ERR_BAD_BUFFER);
    bwapi_clear_last_error();
    bwapi_event one;
    std::memset(&one, 0x5a, sizeof one);
    CHECK(bwapi_game_get_events(&one, 1, 3) == 0);
    CHECK(bwapi_last_error() == BWAPI_ERR_BAD_BUFFER);
    CHECK(one.size == 0x5a5a5a5a);   // and wrote nothing
  }
}

TEST_CASE("the next frame replaces the snapshot and its indices") {
  EventScenario s;
  REQUIRE(bwapi_game_event_count() == EventScenario::kCount);

  SUBCASE("a frame with nothing queued carries the MatchFrame alone") {
    s.f.frame();
    REQUIRE(bwapi_game_event_count() == 1);
    CHECK(get(0).type == BWAPI_EVENT_MATCH_FRAME);
    CHECK(get(0).unit_id == BWAPI_NONE);
    // Last frame's indices are gone with it.
    CHECK(drain().size() == 1);
    CHECK(bwapi_game_event_text(5, nullptr, 0) == 0);
    CHECK(bwapi_last_error() == BWAPI_ERR_INVALID_HANDLE);
  }
  SUBCASE("events queued between frames follow the MatchFrame") {
    s.f.event(EventType::UnitShow, s.scv);
    s.f.event(EventType::ReceiveText, "again", 1);
    s.f.frame();
    REQUIRE(bwapi_game_event_count() == 3);
    CHECK(get(0).type == BWAPI_EVENT_MATCH_FRAME);
    CHECK(get(1).type == BWAPI_EVENT_UNIT_SHOW);
    CHECK(get(1).unit_id == s.scv);
    CHECK(get(2).type == BWAPI_EVENT_RECEIVE_TEXT);
    CHECK(get(2).player_id == 1);
    CHECK(text_of(2) == "again");
    CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
  }
}

TEST_CASE("a null text is the empty string, as upstream's Event::SendText(nullptr) makes it") {
  Fixture f;
  f.player(0, Races::Terran);
  f.event(EventType::SendText, nullptr);
  f.start();
  bwapi_clear_last_error();
  REQUIRE(bwapi_game_event_count() == 1);
  CHECK(get(0).type == BWAPI_EVENT_SEND_TEXT);
  CHECK(bwapi_game_event_text(0, nullptr, 0) == 0);
  CHECK(text_of(0).empty());
  CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
}

TEST_CASE("the fixture pairs text with the types that carry it, and only those") {
  Fixture f;
  // Text on a type that carries none.
  CHECK_THROWS_AS(f.event(EventType::NukeDetect, "boom"), bwapi_c2::test::FixtureError);
  // A text type without its text: the client would read eventStrings[-1].
  CHECK_THROWS_AS(f.event(EventType::SendText), bwapi_c2::test::FixtureError);
  CHECK_THROWS_AS(f.event(EventType::ReceiveText, 1), bwapi_c2::test::FixtureError);
  CHECK_THROWS_AS(f.event(EventType::SaveGame, 0), bwapi_c2::test::FixtureError);
  CHECK(f.data()->eventCount == 0);
  CHECK(f.data()->eventStringCount == 0);
}

TEST_CASE("before the game exists every event call is NOT_CONNECTED, behind BAD_BUFFER") {
  bwapi_clear_last_error();
  REQUIRE(BroodwarPtr == nullptr);
  CHECK(bwapi_game_event_count() == 0);
  CHECK(bwapi_last_error() == BWAPI_ERR_NOT_CONNECTED);
  bwapi_clear_last_error();
  bwapi_event e;
  CHECK(bwapi_game_get_events(&e, 1, sizeof e) == 0);
  CHECK(bwapi_last_error() == BWAPI_ERR_NOT_CONNECTED);
  bwapi_clear_last_error();
  CHECK(bwapi_game_event_text(0, nullptr, 0) == 0);
  CHECK(bwapi_last_error() == BWAPI_ERR_NOT_CONNECTED);
  bwapi_clear_last_error();
  CHECK(bwapi_game_get_events(nullptr, 1, sizeof(bwapi_event)) == 0);
  CHECK(bwapi_last_error() == BWAPI_ERR_BAD_BUFFER);
  bwapi_clear_last_error();
}
