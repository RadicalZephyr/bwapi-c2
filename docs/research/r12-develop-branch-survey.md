# R12. Upstream `develop` against `v4.4.0`: what BWAPI 5 does to the plan's churn mechanisms

Surveyed on 2026-09-06 against the fork's `develop` (`b593656`, identical to upstream
`bwapi/bwapi@develop`, last merged 2026-06-10), the `v4.4.0` tag (`7687da8`) and the pinned
upstream base `d727fed` (`docs/pins.md`). Reproducible via
[`r12/audit-diff.py`](r12/audit-diff.py), which runs the coverage audit's own declaration walker
over both trees and diffs the result, and [`r12/probe-develop.sh`](r12/probe-develop.sh), the
compile probes. The git facts are stated with the commands that produced them.

**Headline: `main`, which the pin tracks, has not moved: fourteen commits since `v4.4.0`, none
touching a non-comment line of any public header, so none of the plan's mechanisms has anything
to do. `develop` is not a branch of BWAPI 4; it is BWAPI 5, a rewrite of everything below the
public headers — protobuf over TCP/UDP instead of shared memory, a concrete `Game` over an
abstract `Client`, value-type handles instead of interface pointers — while the headers
themselves keep 693 of 708 declared names.** Every churn mechanism the plan has fires at a pin
bump to it, and the survey's result is which ones are the right size. The coverage audit, the
constants regeneration and the spec-as-source-of-truth rule are: they report the header churn
exactly (15 removed, 28 added, 49 re-signatured, 55 spec entries to touch of which 54 are one
class rename) and the 245 C signatures survive untouched. The closure file list, the layout
dump, `svnrev.h`, the `CLIENT_VERSION` export and the synthetic-`GameData` fixture detect the
bump loudly and then have nothing useful to say, because each assumes the client architecture
under the headers is fixed. **The plan has a checklist for a pin bump and no procedure for a
bump across `CLIENT_VERSION`; that is the gap. Nothing needs doing until upstream tags a 5.x,
and develop shows no sign of one: the version handshake is stubbed, `getRevision()` is
hardcoded to 9000, and the branch had two commits between 2022 and 2026.**

---

## 1. The three refs

| Ref | Commit | Date | Relation |
|---|---|---|---|
| `v4.4.0` | `7687da8` | 2019-04 | "Merge pull request #824 from bwapi/develop" |
| `main` (pin base) | `d727fed` | 2026-05-08 | 14 commits past `v4.4.0`, fast-forward |
| `develop` | `b593656` | 2026-06-10 | 405 commits past its merge-base with `main`, which is `ede8725` ("Whoops."), *before* `v4.4.0` |

`git merge-base --is-ancestor v4.4.0 develop` is false, and so is it for `v4.4.0^2`, the develop
tip that `#824` merged: develop was rewritten after the tag, so `v4.4.0..develop` is the whole
BWAPI 5 history and not a delta on the release. Commits per year past the merge-base: 12, 12,
**246**, 25, 97, 2, 11 for 2017 through 2026, with nothing in 2023–2025. The 2026 burst is eleven
build-system commits (GitHub Actions, protobuf 34.1, a newer googletest, documentation
tooling), the same week `main` got its README cleanup.

Two facts about the record. `CONTRIBUTING.md` on develop says "Do development on the `develop`
branch", so `main` receives releases and documentation only, which is what the fourteen commits
are. And `docs/pins.md` calls the pin base "upstream `master`"; upstream's default branch is
`main` (`git ls-remote --heads` lists `main` and `develop`, no `master`). One word, worth
fixing at the next edit of that file.

**`main` since `v4.4.0`, in full:** README and documentation edits, the Discord link, a typo
sweep across 46 files. `git diff v4.4.0 d727fed -- bwapi/include`, filtered to lines that are not
comments, is empty. The pin-bump checklist would run to completion with every check green and
every baseline unchanged. That is the case §10.3 was sized for, and it is the case that exists.

---

## 2. What develop is

The tree is squashed one level (`bwapi/` is gone; `include/`, `Library/`, `Network/`, `Clients/`,
`1.16.1/` at the root), CMake is the only build, and the client side is rebuilt:

| Then (`v4.4.0`, the pin) | Now (`develop`) |
|---|---|
| `BWAPIClient/Source/Client.cpp`: maps `Local\bwapi_shared_memory_<pid>` onto a 33,017,048-byte POD `GameData`; seven Win32 imports | `Network/BWAPIFrontendClient/ProtoClient.cpp`: UDP broadcast on port 1024 to find a server, TCP on a negotiated port, protobuf messages (`Network/Messages/*.proto`, seven files) over a vendored SFML 2.5.1 network module with Unix and Win32 socket backends |
| `GameData` is pointer-free and layout-frozen (§1.4, R5) | `GameData` holds `std::string`, `std::vector<PlayerID>`, `TilePosition::list`, a `float`; it is the client's private model, filled from messages, and never crosses a process boundary. `MapData` keeps the same `bool[1024][1024]` and `int[256][256]` grids |
| `BWAPI::Game` abstract; `GameImpl` in the client, `Shared/Templates.h` (3,098 lines) compiled into both modes | `BWAPI::Game` concrete, constructed over an abstract `BWAPI::Client` of 20 pure virtuals (`connect`, `update(Game&)`, `issueCommand(const Unitset&, UnitCommand)`, `drawShape`, `createUnit`, `killUnits`, …). `Shared/` is gone; the `canXxx` rule engine lives in `Library/BWAPILIB/Source/Interface/Unit.cpp` (3,486 lines), non-virtual |
| `Unit` is `UnitInterface*`; `Interface<T>` CRTP base; `getID()` per interface | `Unit` is `InterfaceDataWrapper<Unit, UnitData>`, a value holding a `UnitData const*`; `UnitID`, `PlayerID`, `RegionID`, `BulletID`, `ForceID` are `Identifier<T>` newtypes over `int` with `None = -1`, `explicit operator int`, and `std::hash` (`include/BWAPI/IDs.h`). `operator->` is kept "for backwards compatibility with when BWAPI::Unit etc were pointers" |
| `Game::getUnit(int)` indexes a 10,000-entry vector, O(1), regardless of `exists()` | `Game::getUnit(UnitID)` is `std::set<Unit, IDCompare>::find`, O(log n), and returns `nullptr` for an id not in the set. `getBulletData(BulletID)` exists |
| `CommandTemp.h:34` (two-phase lookup) and `Convenience.h:33` (`va_list&`) block non-MSVC compilers | Both gone: `CommandTemp.h` deleted, `Convenience.h` moved to `Library/BWAPILIB/Source/` with `va_list ap` by value. Upstream CI builds on `ubuntu-latest` with g++ and clang++, on macOS, and on Windows Win32 and x64 |
| `svnrev.h` generated by `revisionUpdate.vbs`; `Game::getRevision()` | `revisionUpdate.vbs` and `starcraftver.h` deleted from the frontend; `Game::getRevision` removed; the free function `BWAPI_getRevision()` returns the literal `9000`; `BWAPI_isDebug()` replaces `BUILD_DEBUG`. `svnrev.h` survives only inside the 1.16.1 backend |
| `CLIENT_VERSION = 10003`; `Client.cpp:120` refuses a mismatch | `CLIENT_VERSION = 10002` on both ends; `ProtoClient::connect()` calls `lookForServer(0, "x", false)` and the server answers `checkForConnection(CLIENT_VERSION, "x", "x")`. The gate is a stub |
| Module mode: the injected DLL loads `bwapi-data/AI/<bot>.dll` | `1.16.1/BWAPIBackend_1161/` is a server only. No `newAIModule`/`gameInit` anywhere in it; `AIModule.h` remains as a header for the client-side callback class |
| Boost `#if 0`'d out; no other third-party code in the client path | libprotobuf (`FetchContent` of `protocolbuffers/protobuf` at tag `v34.1` at configure time, plus a `protoc` binary download), abseil through protobuf, SFML network (vendored, zlib licence). A second protobuf client, `SCRAPINetworkCore`, targets StarCraft: Remastered's API; the README says Remastered and OpenBW are "unsupported (but being worked on)" |

`BWAPI_VERSION` is `5.0.0` in the top-level `CMakeLists.txt`; no `5.x` tag exists upstream (the
tag list ends at `v4.4.0`); the README says "Download and extract a BWAPI 5+ release" and links
a releases page that has none. `versions/bwapi_4.2.x/` keeps the 4.2 headers and the old
`AIModuleLoader` beside the tree, and the backend carries its own copy of the 4.x types under a
`BWAPI4` namespace (`bwapi4_include/`, `BWAPILIB_4_3_0/`) for its internal use; the develop
server does not expose 4.x shared memory (`CreateFileMapping` appears only in `SNP_DirectIP`),
so **a 4.4.0-built client and a 5.0 server cannot talk to each other in either direction.**

---

## 3. The declared surface, as the coverage audit sees it

`r12/audit-diff.py` parses the 24 BWAPI headers on `tools/abi/audited-headers.txt` under
`clang_flags.audit_flags()` against both include roots, with `check_coverage.py`'s own
`Declarations` walker, and diffs the universes after folding the five `XxxInterface` → `Xxx`
renames. Both trees parse with zero errors under the pinned flags. The pinned tree yields 796
keys; with BWEM's 163 that is the 959 `docs/pins.md` records, so the walker agrees with the
audit it is borrowed from. Develop yields 813.

| Header | Pinned | Develop | Kept | Removed | Added |
|---|---|---|---|---|---|
| `Game.h` | 142 | 160 | 135 | 7 | 25 |
| `Unit.h` | 238 | 236 | 236 | 2 | 0 |
| `Player.h`, `Force.h`, `Region.h`, `Bullet.h` | 85 | 81 | 81 | 4 | 0 |
| `UnitType.h` | 85 | 88 | 85 | 0 | 3 |
| `Position.h` | 7 | 5 | 5 | 2 | 0 |
| the other 16 (13 type classes, `Color`, `UnitCommand`, `Event`) | 151 | 151 | 151 | 0 | 0 |
| **total** | **708** | **721** | **693** | **15** | **28** |

**Removed (15).** Five are `getID` on `Unit`, `Player`, `Force`, `Region` and `Bullet`, which
moved to the `InterfaceDataWrapper` base in `IDs.h` and still resolve through it (decision 23
skips them anyway). `Point::isValid` and `Point::makeValid` moved to `Game`, because validity
now depends on the map the `Game` holds rather than a global. `GameWrapper::flush` went with
`GameWrapper` (`Game::flush` replaces it). The genuine removals are `Game::enableFlag`,
`getLatency`, `getReplayFrameCount`, `getRevision`, `indexToUnit`, `setLatCom` and
`Unit::getLastAttackingPlayer`.

**Added (28).** Twenty-five on `Game`, of which sixteen are the client-facing fill and lookup
surface (`addUnit`, `addForce`, `addRegion`, `addBullet`, `addEvent`, `updatePlayer`,
`initGameData`, `update`, `clearEvents`, `flushCommandOptimizer`, `getUnitData`,
`getPlayerData`, `getRegionData`, `getForceData`, `getBulletData`, `getInitialData`), five are
the moved or new cheats and helpers (`createUnit`, `killUnit`, `killUnits`, `removeUnit`,
`removeUnits`), and the rest are `flush`, `getUnits(Player)`, `isValid`, `makeValid`. Three on
`UnitType`: `attackUpgrade`, `attackRangeUpgrade`, `speedUpgrade` (upstream #842, #866, #867).

**Re-signatured (49).** Forty-four are one change: `Unit` → `UnitID` on every `UnitCommand`
static and on the `UnitCommand` constructors, and `int` → `UnitID`/`PlayerID`/`ForceID` on
`Game::getUnit`, `getPlayer`, `getForce`. Four are `Event`'s text taking `std::string` instead
of `char*`. `Game::setMap` loses its `char*` overload. `Unit::useTech`, `canUseTech` and
`isVisible` gain an overload each; nothing loses one.

**The spec.** Of the 248 entries with a `cpp:` (the four `Client::*` entries excluded, as the
audit excludes them), **55 fail to resolve on develop as written; 54 of those are
`PlayerInterface::…` and resolve once the class is spelled `Player`; the one left is
`GameWrapper::flush`.** The wildcard skip resolves. The 737-entry backlog would report the 15
removals as stale and the 28 additions as undecided, which is what it is for.

**BWEM.** All fourteen `BWEM/src/*.cpp` pass `clang++ -std=c++17 -fsyntax-only` against
develop's headers, without `-fdelayed-template-parsing`, and all ten symbols R11.4 found BWEM
needs are declared on develop. BWEM takes the game as a `BWAPI::Game*` argument to
`Map::Initialize` and never touches `Broodwar`, and `InterfaceDataWrapper`'s `operator->` and
`nullptr` comparisons carry its pointer idioms. The carried `ResetInstance` commit is untouched.

**What the audit cannot see.** Five public headers are new on develop — `IDs.h`,
`UnitFinder.h`, `CommandOptimizer.h`, `APMCounter.h`, `FPSCounter.h` — and none is on
`audited-headers.txt`, so the audit is silent about them by construction; seven headers the
list excludes by name (`Interface.h`, `InterfaceEvent.h`, `Latency.h`, `Streams.h`,
`BroodwarOutputDevice.h`, `ArithmaticFilter.h`, `Client/*.h` less the `Data` structs) no longer
exist, which the audit also does not report. An explicit universe was the right call (§9), but
step 6 of the pin-bump checklist should diff the header directory listing before it runs the
audit, or a header upstream adds is invisible until someone reads the tree.

---

## 4. Mechanism by mechanism

The plan's provisions for upstream motion, in the order §10.3 runs them, and what each would do
at a bump to develop. "Detects" means the mechanism fails loudly; "right-sized" means the
response it was built for is the response needed.

| Mechanism | Where | At a bump to develop | Right-sized? |
|---|---|---|---|
| Rebase `bwapi-c2-pin`; a carried commit that no longer applies is the first finding | §10.3, §15.2, `docs/pins.md` | Detects. Two of the three BWAPI commits vanish: the `va_list` fix is upstream, in a file that moved; `revisionUpdate.sh` and the committed `svnrev.h` have no target, the frontend no longer includes either | Yes, exactly as written. The finding is "carried commits retired", and the checklist says to expect it |
| `svnrev.h` regenerated at the upstream commit; `bwapi_revision()` reports `SVN_REV` | §10.3, §4 | Nothing to regenerate. `BWAPI_getRevision()` returns 9000 by fiat, and `Game::getRevision` is gone | No. The export's meaning ("the BWAPI a bot is talking to") has no upstream definition on develop |
| `bwapi_client_version()`; a release records `CLIENT_VERSION`; the client refuses a mismatch | §4, §10.4 | `CLIENT_VERSION` is 10002 on both ends of a handshake that sends `(0, "x", false)`. The value is a constant with nothing behind it | No, until upstream finishes the handshake. The one promise the export makes — a consumer can tell which server a DLL speaks to — cannot be kept from develop's tree |
| Explicit closure file list, never globs; `derive_closure` in CI | §10.1, `cmake/closure.cmake` | Detects on every path: `BWAPIClient/Source/*` (7 TUs) and `Shared/*` (6) do not exist, `BWAPILIB/Source/*.cpp` moved under `Library/BWAPILIB/Source/{Containers,Core,Interface,Types}/` (34 TUs, four new), and the client now needs `Network/BWAPIFrontendClient`, `BWAPINetworkCore`, `SCRAPINetworkCore`, the generated protobuf sources, SFML's nine network TUs and libprotobuf. The `-I` list changes entirely. The R6 Win32 shim and stub have nothing to shim | Detects, then the response is R6 again, not an edit. Also lifts a constraint: no `-fdelayed-template-parsing`, g++ allowed (every BWAPILIB TU passes clang++ and g++ syntax-only here; upstream CI builds them) |
| Layout dump against `baseline.json`; `check_no_pack.sh` | §10.2, `tests/layout_dump` | Detects: `GameData` is a different struct with `std::string` members. But there is no layout to freeze, because nothing maps it | No. The right response is to delete the row, not update the baseline. R5 is moot, and so is the x64 question: settled by the absence of a shared layout |
| Coverage audit against the backlog; every `cpp:` must resolve | §9, `check_coverage.py` | Detects and reports the numbers in §3 precisely | **Yes.** The one wrinkle is that a class rename shows as 54 unresolved entries rather than one finding; the fix is a `sed`, but the audit could say "rename" if the walker matched by method name within the base chain |
| `draft_spec.py --update-constants`; review `constants.yaml` | §10.3 step 6 | Three new `UnitType` accessors; every type-class header otherwise identical; the 848 constants unchanged | **Yes** |
| Spec is the source of truth; regen check; golden `.def`; `api.json` | §9, `tests/regen_check.sh` | No C signature changes: `Player`'s 54 entries and the 185 type accessors resolve after the rename, and the emitters do not care what the `cpp:` side is called. What changes is behind the signatures (next section) | **Yes**, and §4's rule that "a pin bump is the only event that can change semantics behind an unchanged signature" names this case exactly |
| `Client::*` entries covered by `client.gen.cpp`'s `static_assert`s | §9, `spec/client.yaml` | `Client::connect`, `disconnect`, `isConnected`, `update` all exist on develop's abstract `Client`; the assertions pass. The bodies do not compile: `BWAPI::BWAPIClient` and `BroodwarPtr` are gone, `update` takes a `Game&` | Detects, by compilation. Right-sized: `client.cpp` is hand-written and short |
| Synthetic `GameData` fixture; every suite over the real `GameImpl` | §11, `tests/fixture` (444 lines) | Every suite except `types_test`, `header_hygiene`, `exports` and the golden `.def` fails to compile: `GameImpl`, `UnitImpl`, `BWAPIClient.data` do not exist | No, but the replacement is better than what it replaces. `Game(Client&)` is concrete, `Client` is an interface, and `Game::addUnit`, `addForce`, `addRegion`, `addBullet`, `addEvent`, `updatePlayer` are public; upstream's own `Library/BWAPILIBUnitTest/` ships a gmock `MockClient` and a `GameFixture`. R7's invariants 1 (the global `BWAPIClient.data`), 2 (`-1` in every index field — `UnitData` now defaults every id to `None`) and 4 (neutrals only via the event stream) dissolve by construction |
| Divergence register §15 and decisions log §13 updated when research moves a decision | §13, §15 | Register entries that rest on 4.x internals: #6 (bullets: "no `Game::getBullet(id)` exists" — `getBulletData(BulletID)` now does), #17 (grouped commands: the wire message `UnitCommand` carries `repeated int32 unitID`, but `Server.cpp` still loops `unit->issueCommand` per id, so the rationale holds with a new footnote), the re-entrancy table's "shared-memory writes" category (now message-queue writes) | Yes, as a register: the entries are stated against mechanisms, and the mechanisms are what changed |
| §0: the DLL embeds BWAPI's object code, hence LGPL-3.0-only; Corresponding Source names the tagged pins | §0, `NOTICE` | It would also embed SFML (zlib), libprotobuf (BSD-3) and abseil (Apache-2.0). All permissive; the licence does not change; `NOTICE` grows by three, and the source pointer has to name protobuf's tag as well, since develop fetches it at configure time | Yes, with the addition |
| Appendix A: module mode as the v2 item, motivated by grouped commands | Appendix A | develop's backend has no module loading; the v2 item would have no upstream to build on | Not a mechanism, but a decision whose premise moved |
| Appendix B: Linux parked on a POSIX transport port and OpenBW's licence | Appendix B | Item 1 is delivered upstream: the transport is SFML sockets with a Unix backend and UDP-broadcast discovery, so a Linux client reaches a Windows StarCraft over the LAN with no OpenBW involved. Item 3 (no public CI substrate) is unchanged, and the process boundary is now a network boundary | The parking rationale shrinks to the engine; the transport half of it is gone |

**Semantics behind unchanged C signatures**, the case §4 reserves for a pin bump, and what
this bump would put in §15:

- `resolve()` (§6.1) becomes a `std::set` lookup, and §6.2's three outcomes become two: a
  unit that has left the set returns `nullptr` like a never-valid id, so "what BWAPI's own
  getters return on a dead `UnitImpl`, not latched" has no referent. The ABI would either latch
  on dead handles (a behaviour change for every bot that remembers ids across frames) or keep
  its own id-to-data map to preserve the outcome.
- `bwapi_revision()` returns 9000 forever; `bwapi_client_version()` returns 10002, which is
  *lower* than the 10003 it returns today.
- `Position` validity is per-`Game`, so an ABI-level `is_valid` on a packed position needs a
  connected game where it did not before.
- `Event` text is `std::optional<std::string>` rather than a heap `std::string*`; no ABI
  effect, §5.6's stated premise only.

---

## 5. What follows for the plan

**Nothing to revise now.** The pin tracks `main`, `main` has not moved past comments, and
develop is unreleased with a stubbed version gate, a hardcoded revision and a four-year gap in
its history. §10.3's "there is no scheduled drift canary" holds; a survey at each upstream
release is the canary, and this is the first.

**Three sentences in the plan are now false as written**, and a future revision should soften
them rather than let a reader find develop first: §4's "two libraries that have not moved since
2018 and 2021", §9's "a dependency that has not moved meaningfully in years", and §10.3's
"dependencies that do not move". The accurate statement is that the *released* library has not
moved, and that the branch upstream develops on has replaced the client architecture beneath
headers that are 98% stable.

**The missing mechanism is a name for a bump across `CLIENT_VERSION`.** Everything in §10.3 is
sized for a commit on `main`; a move to 5.x is a phase. From this survey, that phase would
contain: R6 again (a closure over `Library/BWAPILIB`, the network layer, SFML and protobuf, with
g++ allowed and the shim retired); R7 again (a fake `Client` and `Game::add*` replacing the
synthetic `GameData`, likely borrowing upstream's `GameFixture`); the layout-dump row deleted;
five class renames and one entry in the spec, then 15 removals and 28 additions decided; the
§6.2 dead-handle outcome redesigned; the re-entrancy table re-derived against message queues;
three notices added and protobuf pinned in the source pointer; §15 #6 and #17 amended; Appendix
A's premise and Appendix B's transport item rewritten. One line in §10.3 naming that phase and
its trigger (a `5.x` tag, or the handshake ceasing to be a stub) is enough today.

**Two cheap findings independent of develop.** Step 6 of the checklist should diff the header
listing, not only run the audit (§3, last paragraph). And `docs/pins.md` should say `main`.

**The strategic question the survey raises, not answers.** The 245 C signatures survive a 5.0
bump; their semantics do not all survive it, and §4 says 1.0 makes append-only binding. If 1.0
ships against 4.4.0 and 5.0 arrives later, a 5.0-speaking `bwapi_c2.dll` cannot connect to the
4.x servers every current tournament runs, and a 4.x-speaking one cannot connect to 5.0, so the
bump is a new major or a second build behind `bwapi_client_version()`, whatever the headers
say. The consumer base is 4.x today and 5.0 has been "in progress" since 2018; 1.0 should not
wait. But the plan should say which of those two shapes a 5.0 `bwapi_c2` takes, because the
answer decides whether §6's handle semantics are allowed to change at that bump or must be
preserved by a boundary-side map.
