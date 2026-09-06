// Every event export against the fixture (implementation plan 2.1): bwapi_game_event_count(),
// bwapi_game_get_event() and bwapi_game_event_text() over the snapshot the library takes of
// Game::getEvents(). This suite pumps through GameImpl directly, as every fixture suite does,
// and then calls bwapi_c2::snapshot_events(), the step bwapi_client_update() runs after its
// own pump; that is the one internal it reaches for, the way errors/latch.cpp reaches for
// bind_abi_thread(). It is also the first suite over a size-prefixed output struct, so it
// proves the struct-out half of the section-4 rule: the caller's size is honoured up and down.
#include "doctest.h"
#include "fixture.h"

#include "abi_internal.h"
#include "bwapi_c2.h"

#include <climits>
#include <cstddef>
#include <cstring>
#include <string>

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
    bwapi_c2::snapshot_events();
  }
  ~EventScenario() { bwapi_clear_last_error(); }

  // The next frame, pumped the way update() pumps it: GameImpl first, then the snapshot.
  void frame() {
    f.frame();
    bwapi_c2::snapshot_events();
  }
};

bwapi_event get(int32_t index) {
  bwapi_event e;
  std::memset(&e, 0x5a, sizeof e);
  e.size = sizeof e;
  REQUIRE(bwapi_game_get_event(index, &e) == 1);
  return e;
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
  bwapi_c2::snapshot_events();
  bwapi_clear_last_error();
  REQUIRE(bwapi_game_event_count() == 1);
  CHECK(bwapi_game_event_text(0, nullptr, 0) == 255);
  CHECK(text_of(0) == std::string(255, 'x'));
  CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
}

TEST_CASE("an out-of-range index is the neutral value plus INVALID_HANDLE, with nothing written") {
  EventScenario s;
  for (int32_t bad : {-1, EventScenario::kCount, INT_MAX, INT_MIN}) {
    bwapi_clear_last_error();
    bwapi_event e;
    std::memset(&e, 0x5a, sizeof e);
    e.size = sizeof e;
    CHECK(bwapi_game_get_event(bad, &e) == 0);
    CHECK(bwapi_last_error() == BWAPI_ERR_INVALID_HANDLE);
    CHECK(message().find("bwapi_game_get_event") != std::string::npos);
    CHECK(message().find("10 events") != std::string::npos);
    // Untouched past the size the caller set: no field is written on failure.
    CHECK(e.size == static_cast<int32_t>(sizeof e));
    CHECK(e.type == 0x5a5a5a5a);
    CHECK(e.is_winner == 0x5a5a5a5a);
    // The latch is sticky, and the text function fails the same way.
    CHECK(bwapi_game_event_text(bad, nullptr, 0) == 0);
    CHECK(bwapi_last_error() == BWAPI_ERR_INVALID_HANDLE);
    CHECK(message().find("bwapi_game_get_event") != std::string::npos);
  }
}

TEST_CASE("the struct-out rule: the caller's size is honoured going in and coming out") {
  EventScenario s;

  SUBCASE("a larger size is zero-filled past the known fields and size says what was filled") {
    struct Bigger {
      bwapi_event e;
      int32_t extra[4];
    } b;
    std::memset(&b, 0xab, sizeof b);
    b.e.size = sizeof b;
    REQUIRE(bwapi_game_get_event(3, &b.e) == 1);
    CHECK(b.e.size == static_cast<int32_t>(sizeof(bwapi_event)));
    CHECK(b.e.type == BWAPI_EVENT_NUKE_DETECT);
    CHECK(b.e.x == 640);
    for (int32_t v : b.extra) CHECK(v == 0);
    CHECK(BWAPI_HAS_FIELD(bwapi_event, is_winner, b.e.size));
    CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
  }
  SUBCASE("a smaller size, an older consumer's, is written only that far") {
    bwapi_event e;
    std::memset(&e, 0x5a, sizeof e);
    e.size = static_cast<int32_t>(offsetof(bwapi_event, unit_id));  // size and type only
    REQUIRE(bwapi_game_get_event(0, &e) == 1);
    CHECK(e.size == static_cast<int32_t>(offsetof(bwapi_event, unit_id)));
    CHECK(e.type == BWAPI_EVENT_UNIT_DISCOVER);
    CHECK(e.unit_id == 0x5a5a5a5a);  // never written past the caller's size
    CHECK(BWAPI_HAS_FIELD(bwapi_event, type, e.size));
    CHECK_FALSE(BWAPI_HAS_FIELD(bwapi_event, unit_id, e.size));
    CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
  }
  SUBCASE("NULL, or a size that cannot hold size itself, is BAD_BUFFER before the index is looked at") {
    CHECK(bwapi_game_get_event(0, nullptr) == 0);
    CHECK(bwapi_last_error() == BWAPI_ERR_BAD_BUFFER);
    bwapi_clear_last_error();
    bwapi_event e;
    e.size = 3;
    CHECK(bwapi_game_get_event(0, &e) == 0);
    CHECK(bwapi_last_error() == BWAPI_ERR_BAD_BUFFER);
    CHECK(e.size == 3);
    bwapi_clear_last_error();
    e.size = 0;
    CHECK(bwapi_game_get_event(-1, &e) == 0);
    CHECK(bwapi_last_error() == BWAPI_ERR_BAD_BUFFER);
  }
}

TEST_CASE("the next frame replaces the snapshot and its indices") {
  EventScenario s;
  REQUIRE(bwapi_game_event_count() == EventScenario::kCount);

  SUBCASE("a frame with nothing queued carries the MatchFrame alone") {
    s.frame();
    REQUIRE(bwapi_game_event_count() == 1);
    CHECK(get(0).type == BWAPI_EVENT_MATCH_FRAME);
    CHECK(get(0).unit_id == BWAPI_NONE);
    // Last frame's indices are gone with it.
    bwapi_event e;
    e.size = sizeof e;
    CHECK(bwapi_game_get_event(5, &e) == 0);
    CHECK(bwapi_last_error() == BWAPI_ERR_INVALID_HANDLE);
  }
  SUBCASE("events queued between frames follow the MatchFrame") {
    s.f.event(EventType::UnitShow, s.scv);
    s.f.event(EventType::ReceiveText, "again", 1);
    s.frame();
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
  bwapi_c2::snapshot_events();
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
  bwapi_c2::snapshot_events();  // with no game the snapshot is empty, and harmless
  CHECK(bwapi_game_event_count() == 0);
  CHECK(bwapi_last_error() == BWAPI_ERR_NOT_CONNECTED);
  bwapi_clear_last_error();
  bwapi_event e;
  e.size = sizeof e;
  CHECK(bwapi_game_get_event(0, &e) == 0);
  CHECK(bwapi_last_error() == BWAPI_ERR_NOT_CONNECTED);
  bwapi_clear_last_error();
  CHECK(bwapi_game_event_text(0, nullptr, 0) == 0);
  CHECK(bwapi_last_error() == BWAPI_ERR_NOT_CONNECTED);
  bwapi_clear_last_error();
  CHECK(bwapi_game_get_event(0, nullptr) == 0);
  CHECK(bwapi_last_error() == BWAPI_ERR_BAD_BUFFER);
  bwapi_clear_last_error();
}
