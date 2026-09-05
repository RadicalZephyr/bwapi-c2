# R7. OpenBW as the test and CI substrate

Built OpenBW headless from source, traced how every downstream project handles the MPQ problem,
and built a working alternative. Reproducible via
[`r7/run-fixture-harness.sh`](r7/run-fixture-harness.sh).

**Headline: OpenBW still builds and still cannot be used in public CI, because it needs Blizzard's
MPQ files and nobody in the ecosystem has solved that — JBWAPI wrote both end-to-end workflows and
disabled them, and the URL they downloaded StarCraft from is now a 404. But the mock server should
not be built either, because JBWAPI already demonstrated the better answer and I reproduced it in
C++: a real BWAPI client driven against a synthetic `GameData` with no server, no shared memory,
no pipe and no StarCraft. Read state, run BWAPI's real rule engine, emit commands — 48 lines of
harness in JBWAPI's case, ~60 in mine. §11.5's line item should be deleted, not re-estimated.**

---

## 1. Does OpenBW still build and run on a current toolchain?

**Yes.** `OpenBW/bwapi@48124ba` (2020-06-11) against `OpenBW/openbw@4b046d5` (2026-08-13), CMake
3.22, GCC 11, `-DOPENBW_ENABLE_UI=0`:

```
[100%] Built target BWAPILauncher
EXIT=0        errors: 0
```

Artifacts: `bin/BWAPILauncher`, `lib/libBWAPI.so` (743 KB), `lib/libBWAPILIB.so` (648 KB),
`lib/libOpenBWData.so` (1.9 MB), `lib/libBWAPIClient.a` (**4,778 bytes** — the throwing stub from
R6 §8), plus `ExampleAIModule.so` and `TestAIModule.so`.

Two caveats worth recording, because they cost time:

- The **official** pairing builds clean headless. The **`basil-ladder` pairing does not** (R6 §13): its `ui_wrapper` headless stub is missing `play_sound`, `screen_pos_x`, `screen_pos_y` and a constructor overload, so `-DOPENBW_ENABLE_UI=0` fails there. The documented build for client mode uses `-DOPENBW_ENABLE_UI=1`, which drags in SDL2 **and SDL2_mixer**.
- So "headless" in OpenBW means *no window*, not *no graphics stack*. The client-mode path needs SDL2 + SDL2_mixer installed even in CI.

---

## 2. Does it need the retail MPQ files? Yes, and precisely three

Running the launcher with nothing else present:

```
$ ./BWAPILauncher
Error: file_reader: failed to open ./Patch_rt.mpq for reading
```

`bwgame::data_loading::data_files_directory` (`data_loading.h:1322-1324`) hardcodes
`Patch_rt.mpq`, `BrooDat.mpq`, `StarDat.mpq`, and `OpenBWData/BW/BWData.cpp` points it at
`$OPENBW_MPQ_PATH` (default `.`). There is no loose-file loader wired in. `data_files_loader` is a
template, so a custom one is *possible*, but the BWAPI fork never exposes it.

**What it actually reads out of them is small.** The headless simulation loads eleven files:

```
arr/units.dat  arr/weapons.dat  arr/upgrades.dat  arr/techdata.dat  arr/orders.dat
arr/flingy.dat arr/sprites.dat  arr/images.dat    arr/images.tbl
scripts/iscript.bin   triggers/Melee.trg
```

(`staredit/scenario.chk` comes from the map, not the MPQs.) That is a handful of small binary
tables, not the art assets. It does not change the legal position — they are Blizzard's — but it
does mean the CI problem is "obtain eleven copyrighted tables", not "download a 1.5 GB game".

---

## 3. How JBWAPI and BWAPI4J CI handle it: they don't

This is the question the plan asked, and the answer is unambiguous.

**JBWAPI wrote two end-to-end workflows and disabled both.** The files are still in the repo as
`.github/workflows/e2e-linux.yml.disabled` and `e2e-wine.yml.disabled`, renamed in commit
`58cf01e` — *"disable e2e test for now"* — on **2022-04-13**.

`e2e-linux` did exactly what R7 hoped for: `apt install cmake libsdl2-dev libsdl2-mixer-dev`,
clone `basil-ladder/openbw` + `basil-ladder/bwapi@linux-client-support`, build with
`OPENBW_ENABLE_UI=1`, then:

```bash
curl http://www.cs.mun.ca/~dchurchill/starcraftaicomp/files/Starcraft_1161.zip -o starcraft.zip
unzip starcraft.zip patch_rt.mpq BROODAT.MPQ STARDAT.MPQ
```

launch two `BWAPILauncher` instances, run two Java bots, grep stdout for `"Hello from JBWAPI!"`.

**That URL is now HTTP 404.** So the workflow would not work even if re-enabled — the MPQ source
was one person's university web directory, and it has gone.

`e2e-wine` used `Bytekeeper/sc-docker` and asserted `Winner is BotPlayer:jbwapibot:T`. Its
`game.dockerfile` says `# Get Starcraft game from ICCUP` and then `COPY starcraft.zip` — a file
that is **not in the repository**. The user supplies their own copy of StarCraft. That is the
ecosystem's actual answer, and it is not available to a public CI runner.

**JBWAPI's live CI (`maven.yml`) is `mvn package` + `mvn test` on four JDKs. It never runs a game.**
**BWAPI4J's CI is `./gradlew clean build buildOpenBWBridgeForLinux` on Travis — build only, no
game, and Travis has been dead since 2021.**

**So: nobody runs BWAPI games in public CI. Not one project in the corpus.** Plan on not doing it
either.

---

## 4. Does client mode work against OpenBW?

Settled in R6 §8 and §13, restated here because it is R7's question:

- **OpenBW's own repositories: no.** `BWAPIClient/Source/Client.cpp` is 477 bytes and
  `Client::connect()` is `throw std::runtime_error("Client not supported :(")`. Still true at
  `OpenBW/bwapi@develop-openbw` (2020-06-11). bwapi-c's 2017 README was right and remains right.
- **The ecosystem: yes.** `basil-ladder/bwapi@linux-client-support` implements it in ~344 lines
  across 8 files — POSIX `shm_open`/`mmap` plus an `AF_UNIX` socket at `/tmp/bwapi_socket_<pid>`.
  Unmerged, five years idle, needs one `#include <string>` to build on GCC 11, and requires the
  paired `basil-ladder/openbw` fork.
- **Windows client mode against OpenBW: no.** The branch is entirely `#ifndef _WIN32`-guarded.

---

## 5. Pricing "support both retail and OpenBW"

OpenBW's README, line 6, is explicit: *"This fork has significant changes and it no longer works
with regular StarCraft: Brood War."* So the two are genuinely separate targets. Measured:

| | Upstream BWAPI 4.4.0 | OpenBW fork |
|---|---|---|
| `GameData.h` | — | **byte-identical** |
| `CLIENT_VERSION` | **10003** | **10002** |
| Public headers differing | — | 13 files, **180 changed lines** |
| `Client/CommandTemp.h` | present | **absent** |
| Platform | Windows only, x86, MSVC | Linux/Windows, CMake, GCC/clang |
| Client mode | yes | no (see §4) |

Two things follow, and they pull in opposite directions.

**Cheaper than feared: the wire format is the same.** `GameData.h` is identical, so R5's layout
matrix and any generated accessors serve both. The ABI does not fork.

**More expensive than it looks: the version gate.** `BWAPIClient/Source/Client.cpp:120` refuses to
connect on mismatch:

```cpp
if (BWAPI::CLIENT_VERSION != BWAPI::Broodwar->getClientVersion())
{ std::cerr << "Error: Client and Server are not compatible!"; disconnect(); return false; }
```

A C ABI built against upstream headers (10003) **will refuse to talk to an OpenBW server** (10002).
gobwapi hardcodes 10003 and fails the same way; JBWAPI does not check at all, which is why it
works against both. So supporting both means either relaxing the check, making the constant
configurable, or pinning two submodules and shipping two binaries.

**Honest price: two pinned dependencies, two CMake configurations, two release artifacts, and one
deliberate decision about the version gate.** That is real but bounded — perhaps a week of build
engineering, plus permanent duplication in the release matrix. It is not justified by CI value,
because §3 shows CI cannot run either of them.

---

## 6. `mini-openbwapi`: smaller, and gone

Covered in R6 §12 as part of the Snowstorm audit; the R7-relevant summary:

- 2,657 lines total (`openbwapi.h` 1,138 + `openbwapi.cpp` 1,474 + an 8-line `BWAPI.h`), exposing ~263 functions.
- **Removed from `OpenBW/openbw` on 2026-07-16** (commit `8265ec4`, "Remove mini-openbwapi."). The only maintained copy is inside `awest813/OpenSnowstorm---Brood-War`, an AI-agent-driven campaign-client fork with six stars.
- Its `Client` is a façade — `connect()` sets a bool, `update()` calls `g.update()` in-process — and `mini-openbwapi/BWAPI/Client.h` is a **zero-byte file**. No client protocol.
- It still needs the MPQs; it is a smaller *API surface*, not a smaller *dependency*.

**Not a viable substrate.** Worth citing as evidence of how small an OpenBW-backed BWAPI façade
can be, nothing more.

---

## 7. The answer to §11.5: don't build the mock server

JBWAPI faced exactly this problem and solved it without a mock server. Their approach, in two
pieces:

1. **`GameStateDumper`** — a bot you run *once* against a real game. On `onStart()` it grabs the
   whole 33 MB shared-memory buffer, deflates it, and writes `<map>_frame0_buffer.bin`.
2. **`GameBuilder`** — **48 lines** that inflate a fixture, hand the bytes to a `WrappedBuffer`,
   and call `game.init()`.

`src/test/resources/` holds **15 fixtures totalling 1.2 MB** — real frame-0 state from Benzene,
Destination, Fighting Spirit, Python, Andromeda, Circuit Breaker and the rest. The 33 MB buffer
deflates to ~70–80 KB because it is mostly zeros. Five test classes consume them
(`GameTest`, `PointTest`, `BWEMTest`, `DrawTest`, `SynchronizationEnvironment`).

### I reproduced it in C++, against our closure

[`r7/fixture_harness.cpp`](r7/fixture_harness.cpp) builds R6's 44-TU archive, `calloc`s a
`GameData`, fills in a handful of fields by hand, constructs `GameImpl(data)` and drives the real
API. No server, no shared memory, no pipe, no MPQs, no StarCraft:

```
sizeof(GameData) = 33017048
clientVersion = 10003 (expected 10003)
frameCount    = 123
map           = Fixture Map (128x128)
self          = FixtureBot, minerals=350 gas=75 race=Terran
allUnits      = 1
  unit 0: Terran_SCV at (1000,2000) hp=60/60  isWorker=1  mineralPrice=50
  canMove=1 err=None
  canIssueCommandType(Move)=1 err=None
move() returned 1; unitCommandCount=1; lastError=None
  cmd[0]: type=Move unitIndex=0 x=1500 y=2500
shapeCount=1 stringCount=1
```

All three paths work:

- **Read** — game state, player, unit, and the static type data (`isWorker`, `mineralPrice`, `maxHitPoints`) that R2 identified as the product, served out of `UnitType.cpp` inside the closure.
- **Rules** — BWAPI's real `canCommand` / `canMove` / `canIssueCommandType` run and return real `Errors::` codes. Nothing is faked.
- **Write** — `move()` lands a `{Move, unitIndex 0, x 1500, y 2500}` entry in `data->unitCommands`, and `drawTextScreen` produces a shape and a string. That is exactly what a mock server would have to assert on, available directly.

### Three things the exercise taught, which a hand-written mock would have got wrong

1. **`UnitImpl`'s constructor reads a global.** `UnitImpl::UnitImpl(int id) : self(&(BWAPI::BWAPIClient.data->units[id]))` — the *singleton*, not the `GameData*` handed to `GameImpl`. A fixture must set `BWAPI::BWAPIClient.data` before constructing `GameImpl`, or it segfaults.
2. **Zero is not the empty value.** `calloc` leaves every unit-index field at `0`, which is a *valid* unit index. The SCV therefore believed it was loaded inside unit 0, `isLoaded()` returned true, and `canCommand()` failed with `Unit_Busy`. BWAPI's "none" is `-1`, across `transport`, `target`, `orderTarget`, `buildUnit`, `addon`, `carrier`, `hatchery`, `rallyUnit`, `lastAttackerPlayer`.
3. **Commandability needs more flags than are obvious** — `isPowered` and `isInterruptible` both gate `canMove`, and both default false.

Each of those is a place a hand-written mock server would have silently produced a *plausible but
wrong* game state, and the tests would have passed against a fiction. **Recording a real buffer
avoids all three by construction.** That is the strongest argument against §11.5, and it is
JBWAPI's argument, arrived at independently.

### Recommended test strategy

| Layer | Substrate | Cost |
|---|---|---|
| Type data, geometry, enums, string marshalling | pure unit tests, no `GameData` at all | ~0 |
| Read path, rules, command emission | **recorded `GameData` fixtures + a ~60-line harness** | small, and the harness already exists |
| Error paths, bad handles, truncation, sticky-error latch | **synthetic** `GameData` — deliberately malformed | small |
| Protocol handshake (game table, mapping, pipe/socket) | a thin fake of the *transport only* — ~150 lines, no game semantics | small |
| End-to-end against a live server | **manual, gated, not in CI** — needs MPQs (§2, §3) | occasional |

**§11.5's mock server collapses into rows 2–4.** The 300 lines the plan budgets (or the 800–1500
I estimated) buys nothing that a recorded buffer does not buy better, because the recorded buffer
is *correct by construction* and a mock server is correct only insofar as its author understood
BWAPI's invariants — which §7's three findings suggest is optimistic.

**One caveat to check before adopting fixtures: provenance.** JBWAPI ships its `.bin` fixtures
under MIT, but they are dumps of `GameData` populated from a running game, and that state is
derived from Blizzard's unit tables. Whether a frame-0 buffer is redistributable is a licensing
question I have not resolved and R9 should take. The fallback — a `GameStateDumper` equivalent
that contributors run locally against their own StarCraft copy, with fixtures gitignored — costs
CI coverage but no legal risk.

---

## 8. Answers to the questions as asked

**Does it still build and run on a current toolchain?** Builds: yes, cleanly, GCC 11 / CMake 3.22,
zero errors. Runs: only with the MPQs present; without them it exits immediately on `Patch_rt.mpq`.

**Does it need the retail MPQ files?** Yes — three archives, of which the headless sim reads
eleven small tables. No loose-file path is wired in.

**How do JBWAPI and BWAPI4J CI handle it?** They don't. JBWAPI wrote both e2e workflows and
disabled them on 2022-04-13; the StarCraft download URL is now 404. BWAPI4J's Travis CI builds
only. Neither project's live CI runs a game.

**Does client mode work against OpenBW, or only module mode?** Only module mode on OpenBW's own
repos — `connect()` throws. A working POSIX client mode exists on `basil-ladder/bwapi@linux-client-support`
(R6 §13): ~344 lines, unmerged, five years idle.

**Price supporting both retail and OpenBW.** `GameData` is byte-identical so the ABI does not
fork; 13 public headers differ by 180 lines; `CLIENT_VERSION` differs (10003 vs 10002) and the
client refuses to connect on mismatch. Two submodules, two configs, two artifacts, one decision
about the version gate.

**Is `mini-openbwapi` a smaller target?** Smaller API (2,657 lines, ~263 functions) but the same
MPQ dependency, a façade client with a zero-byte `Client.h`, and **deleted from upstream OpenBW on
2026-07-16**. Not a substrate.

---

## 9. What this does to the plan

**Delete §11.5's mock server.** Replace it with recorded fixtures plus the ~60-line harness in
this directory, a small synthetic-`GameData` suite for error paths, and a transport-only fake of
~150 lines if the handshake needs covering. That is the single largest line-item reduction the
research has produced, and unlike the others it comes with working code.

**Do not adopt OpenBW as the CI substrate.** It builds, and that is the end of the good news: it
cannot run in public CI without Blizzard's files, the one public source of those files is gone,
and the two projects that tried both gave up in 2022. Use it — if at all — as a *local,
developer-run* integration target, the same way JBWAPI's disabled workflow would have.

**Non-goal 3 ("not cross-platform") survives, and is now better argued.** The reason is not that
Linux is hard — R6 built the whole closure on Linux and §7 ran a bot against it. The reason is
that a Linux target implies OpenBW, OpenBW implies MPQs and SDL2_mixer, client mode on OpenBW
implies two five-year-old unmerged forks, and none of it can be exercised in CI. The cost is real
and the payoff is untestable.

**One upside worth keeping.** R6 §1 and R7 §7 together show the closure builds and *runs* on Linux
with no StarCraft at all. Whatever the shipping target is, **the test suite can be Linux-native and
free of Windows, Wine, and Blizzard's files.** That is worth more to this project than OpenBW is.
