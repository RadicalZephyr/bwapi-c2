// The sticky error channel and the boundary (plan section 4; implementation plan 1.1): first
// error wins, success does not clear, reading does not clear, the callback fires once at latch
// time, a call from another thread latches WRONG_THREAD, a handle that could never have been
// valid latches INVALID_HANDLE, and an in-range dead unit does not.
//
// The tests reach the internals through abi_internal.h: the thread binding connect() would do,
// the resolvers the generated wrappers call, and guard() itself. The exports over them are
// covered by the per-interface suites (read_write/player.cpp first).
#include "doctest.h"
#include "fixture.h"

#include "abi_internal.h"
#include "bwapi_c2.h"

#include <bwem.h>
#include <svnrev.h>

#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using bwapi_c2::test::Fixture;

namespace {

// The channel is process-wide, so every case starts and ends from a clean one.
struct Channel {
  Channel() { reset(); }
  ~Channel() { reset(); }
  static void reset() {
    bwapi_clear_last_error();
    bwapi_set_error_callback(nullptr, nullptr);
    bwapi_set_log_callback(nullptr, nullptr);
    bwapi_c2::unbind_abi_thread();
  }
};

std::string message() {
  char buf[256];
  const int32_t n = bwapi_last_error_message(buf, sizeof buf);
  return std::string(buf, static_cast<size_t>(n < 255 ? n : 255));
}

struct Seen {
  int32_t code;
  std::string msg;
  int depth;
};

void record(int32_t code, const char* msg, void* user) {
  static_cast<std::vector<Seen>*>(user)->push_back({code, msg, bwapi_c2::reentrancy_depth()});
}

}  // namespace

TEST_CASE("the channel starts clear and reports the versions") {
  Channel c;
  CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
  CHECK(message().empty());

  int32_t major = -1, minor = -1, patch = -1;
  bwapi_abi_version(&major, &minor, &patch);
  CHECK(major == 0);
  CHECK(minor >= 0);
  CHECK(patch >= 0);
  bwapi_abi_version(nullptr, nullptr, nullptr);  // NULL out-params are skipped, not written

  char buf[16];
  const int32_t n = bwapi_abi_version_string(buf, sizeof buf);
  CHECK(n == static_cast<int32_t>(std::strlen(buf)));
  CHECK(std::string(buf) == std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch));

  CHECK(bwapi_client_version() == BWAPI::CLIENT_VERSION);
  CHECK(bwapi_revision() == SVN_REV);
  CHECK((bwapi_is_debug() == 0 || bwapi_is_debug() == 1));
  CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
}

TEST_CASE("the first error wins and nothing clears it but clear") {
  Channel c;
  bwapi_c2::latch(BWAPI_ERR_INVALID_HANDLE, "first");
  bwapi_c2::latch(BWAPI_ERR_BWEM, "second");
  CHECK(bwapi_last_error() == BWAPI_ERR_INVALID_HANDLE);
  CHECK(message() == "first");

  SUBCASE("reading does not clear") {
    CHECK(bwapi_last_error() == BWAPI_ERR_INVALID_HANDLE);
    CHECK(message() == "first");
  }
  SUBCASE("a successful call does not clear") {
    CHECK(bwapi_client_version() == BWAPI::CLIENT_VERSION);
    CHECK(bwapi_c2::guard<int32_t>(0, [] { return 7; }) == 7);
    CHECK(bwapi_last_error() == BWAPI_ERR_INVALID_HANDLE);
  }
  SUBCASE("clear resets both the code and the message") {
    bwapi_clear_last_error();
    CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
    CHECK(message().empty());
    bwapi_c2::latch(BWAPI_ERR_BWEM, "second");
    CHECK(bwapi_last_error() == BWAPI_ERR_BWEM);
    CHECK(message() == "second");
  }
}

TEST_CASE("the message follows the snprintf convention") {
  Channel c;
  bwapi_c2::latch(BWAPI_ERR_BWEM, "hello fixture");

  SUBCASE("a short buffer is truncated and NUL-terminated, and the full length comes back") {
    char buf[6];
    std::memset(buf, 'x', sizeof buf);
    CHECK(bwapi_last_error_message(buf, sizeof buf) == 13);
    CHECK(std::string(buf) == "hello");
  }
  SUBCASE("a NULL buffer with zero length queries the length") {
    CHECK(bwapi_last_error_message(nullptr, 0) == 13);
    CHECK(bwapi_last_error() == BWAPI_ERR_BWEM);
  }
  SUBCASE("a NULL buffer with a nonzero length is a BAD_BUFFER, behind the first error") {
    bwapi_clear_last_error();
    CHECK(bwapi_last_error_message(nullptr, 5) == 0);
    CHECK(bwapi_last_error() == BWAPI_ERR_BAD_BUFFER);
    bwapi_clear_last_error();
    char buf[4];
    CHECK(bwapi_last_error_message(buf, -1) == 0);
    CHECK(bwapi_last_error() == BWAPI_ERR_BAD_BUFFER);
  }
}

TEST_CASE("the error callback fires once, at latch time, with the depth raised") {
  Channel c;
  std::vector<Seen> seen;
  bwapi_set_error_callback(record, &seen);

  bwapi_c2::latch(BWAPI_ERR_WRONG_THREAD, "one");
  REQUIRE(seen.size() == 1);
  CHECK(seen[0].code == BWAPI_ERR_WRONG_THREAD);
  CHECK(seen[0].msg == "one");
  CHECK(seen[0].depth == 1);
  CHECK(bwapi_c2::reentrancy_depth() == 0);

  // A second failure while latched is not a second callback: the first error is the one.
  bwapi_c2::latch(BWAPI_ERR_BWEM, "two");
  CHECK(seen.size() == 1);
  CHECK(bwapi_last_error() == BWAPI_ERR_WRONG_THREAD);

  // The channel is readable from inside the callback; nothing is held across the call.
  bwapi_clear_last_error();
  bwapi_set_error_callback(
      [](int32_t code, const char*, void* user) {
        *static_cast<int32_t*>(user) = bwapi_last_error() == code ? code : -1;
      },
      &seen[0].code);
  bwapi_c2::latch(BWAPI_ERR_BAD_BUFFER, "three");
  CHECK(seen[0].code == BWAPI_ERR_BAD_BUFFER);

  // Off again: latching is unchanged, nothing is called.
  bwapi_clear_last_error();
  bwapi_set_error_callback(nullptr, nullptr);
  bwapi_c2::latch(BWAPI_ERR_BWEM, "four");
  CHECK(bwapi_last_error() == BWAPI_ERR_BWEM);
}

TEST_CASE("the boundary latches a classified code and returns the neutral value") {
  Channel c;
  SUBCASE("a BWEM::Exception is BWAPI_ERR_BWEM with its what()") {
    CHECK(bwapi_c2::guard<int32_t>(-7, []() -> int32_t { throw BWEM::Exception("bwem says no"); }) == -7);
    CHECK(bwapi_last_error() == BWAPI_ERR_BWEM);
    CHECK(message() == "bwem says no");
  }
  SUBCASE("any other std::exception is BWAPI_ERR_EXCEPTION with its what()") {
    CHECK(bwapi_c2::guard<double>(0.0, []() -> double { throw std::runtime_error("plain"); }) == 0.0);
    CHECK(bwapi_last_error() == BWAPI_ERR_EXCEPTION);
    CHECK(message() == "plain");
  }
  SUBCASE("a non-standard exception is BWAPI_ERR_EXCEPTION too") {
    bwapi_c2::guard([] { throw 42; });
    CHECK(bwapi_last_error() == BWAPI_ERR_EXCEPTION);
  }
  SUBCASE("a body that returns is passed through untouched") {
    CHECK(bwapi_c2::guard<bwapi_position>(BWAPI_POSITION_NONE, [] { return BWAPI_POS_MAKE(3, 4); }) ==
          BWAPI_POS_MAKE(3, 4));
    CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
  }
}

TEST_CASE("a call from another thread latches WRONG_THREAD and returns the neutral value") {
  Channel c;
  Fixture f;
  f.player(0, BWAPI::Races::Terran);
  f.start();
  bwapi_c2::bind_abi_thread();  // what connect() does

  CHECK(bwapi_c2::game_ready("here"));
  CHECK(bwapi_last_error() == BWAPI_ERR_NONE);

  bool ready = true;
  BWAPI::Player p = nullptr;
  std::thread([&] {
    ready = bwapi_c2::game_ready("elsewhere");
    p = ready ? bwapi_c2::resolve_player(0, "elsewhere") : nullptr;
  }).join();
  CHECK_FALSE(ready);
  CHECK(p == nullptr);
  CHECK(bwapi_last_error() == BWAPI_ERR_WRONG_THREAD);
  CHECK(message().find("elsewhere") != std::string::npos);

  // The ABI's own surface is not thread-checked: the latch can be read and cleared anywhere.
  int32_t other = -1;
  std::thread([&] { other = bwapi_last_error(); }).join();
  CHECK(other == BWAPI_ERR_WRONG_THREAD);

  // Unbound (before connect, and in every fixture-driven suite), any thread passes.
  bwapi_c2::unbind_abi_thread();
  bwapi_clear_last_error();
  std::thread([&] { ready = bwapi_c2::game_ready("elsewhere"); }).join();
  CHECK(ready);
  CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
}

TEST_CASE("a game call before connect latches NOT_CONNECTED") {
  Channel c;
  REQUIRE(BWAPI::BroodwarPtr == nullptr);
  CHECK_FALSE(bwapi_c2::game_ready("early"));
  CHECK(bwapi_last_error() == BWAPI_ERR_NOT_CONNECTED);
  CHECK(bwapi_client_is_connected() == 0);
  bwapi_clear_last_error();
  bwapi_client_disconnect();  // safe when not connected
  CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
}

TEST_CASE("a handle that could never have been valid latches; a dead unit in range does not") {
  Channel c;
  Fixture f;
  f.player(0, BWAPI::Races::Terran);
  const int scv = f.unit(0, BWAPI::UnitTypes::Terran_SCV, 100, 100);
  f.start();
  REQUIRE(bwapi_c2::game_ready("resolve"));

  SUBCASE("negative and out-of-range ids, every kind") {
    CHECK(bwapi_c2::resolve_unit(-1, "u") == nullptr);
    CHECK(bwapi_last_error() == BWAPI_ERR_INVALID_HANDLE);
    CHECK(message() == "u: unit id -1 could never have been valid");
    bwapi_clear_last_error();
    CHECK(bwapi_c2::resolve_unit(10000, "u") == nullptr);
    CHECK(bwapi_last_error() == BWAPI_ERR_INVALID_HANDLE);
    bwapi_clear_last_error();
    CHECK(bwapi_c2::resolve_player(12, "p") == nullptr);
    CHECK(bwapi_last_error() == BWAPI_ERR_INVALID_HANDLE);
    bwapi_clear_last_error();
    CHECK(bwapi_c2::resolve_force(5, "f") == nullptr);
    CHECK(bwapi_last_error() == BWAPI_ERR_INVALID_HANDLE);
    bwapi_clear_last_error();
    CHECK(bwapi_c2::resolve_region(0, "r") == nullptr);  // the fixture has no regions
    CHECK(bwapi_last_error() == BWAPI_ERR_INVALID_HANDLE);
  }
  SUBCASE("in range resolves, and the latch is untouched") {
    CHECK(bwapi_c2::resolve_unit(scv, "u") == BWAPI::Broodwar->getUnit(scv));
    CHECK(bwapi_c2::resolve_player(0, "p") == BWAPI::Broodwar->self());
    CHECK(bwapi_c2::resolve_player(11, "p") == BWAPI::Broodwar->neutral());
    CHECK(bwapi_c2::resolve_force(0, "f") != nullptr);
    CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
  }
  SUBCASE("a unit that has died resolves like a live one, with no latch") {
    f.data()->units[scv].exists = false;
    BWAPI::Unit u = bwapi_c2::resolve_unit(scv, "u");
    REQUIRE(u != nullptr);
    CHECK_FALSE(u->exists());
    CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
    // And an id past the units the fixture made, but inside the table, is in range too: it
    // answers as BWAPI answers for a slot that was never filled.
    CHECK(bwapi_c2::resolve_unit(scv + 1, "u") != nullptr);
    CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
  }
}

TEST_CASE("string and array buffers are checked once, the same way") {
  Channel c;
  char buf[8];
  SUBCASE("write_string truncates, terminates and reports the full length") {
    CHECK(bwapi_c2::write_string(buf, sizeof buf, std::string("0123456789")) == 10);
    CHECK(std::string(buf) == "0123456");
    CHECK(bwapi_c2::write_string(buf, sizeof buf, std::string("abc")) == 3);
    CHECK(std::string(buf) == "abc");
    CHECK(bwapi_c2::write_string(buf, 1, std::string("abc")) == 3);
    CHECK(buf[0] == '\0');
    CHECK(bwapi_c2::write_string(nullptr, 0, std::string("abc")) == 3);
    CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
  }
  SUBCASE("the neutral string is empty and 0") {
    buf[0] = 'x';
    CHECK(bwapi_c2::empty_string(buf, sizeof buf) == 0);
    CHECK(buf[0] == '\0');
    CHECK(bwapi_c2::empty_string(nullptr, 0) == 0);
  }
  SUBCASE("a malformed array buffer latches BAD_BUFFER") {
    int32_t ids[4];
    CHECK(bwapi_c2::check_buffer(ids, 4));
    CHECK(bwapi_c2::check_buffer(nullptr, 0));
    CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
    CHECK_FALSE(bwapi_c2::check_buffer(nullptr, 4));
    CHECK(bwapi_last_error() == BWAPI_ERR_BAD_BUFFER);
    bwapi_clear_last_error();
    CHECK_FALSE(bwapi_c2::check_buffer(ids, -1));
    CHECK(bwapi_last_error() == BWAPI_ERR_BAD_BUFFER);
  }
}

TEST_CASE("the log callback carries the ABI's diagnostics and nothing without one") {
  Channel c;
  std::vector<Seen> seen;
  bwapi_c2::log(BWAPI_LOG_WARN, "dropped");  // no callback: nowhere to go, no crash
  bwapi_set_log_callback(record, &seen);
  bwapi_c2::log(BWAPI_LOG_WARN, "kept");
  REQUIRE(seen.size() == 1);
  CHECK(seen[0].code == BWAPI_LOG_WARN);
  CHECK(seen[0].msg == "kept");
  CHECK(seen[0].depth == 1);
  CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
}
