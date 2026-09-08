# Implementation plan: the eight defects in ADR 0001 §2

> **Status: draft. Stages A through H are implemented and both tiers of the offsets smoke test
> are green.** The work ships as two pull requests merged in order — the retarget and the test
> suite first (PR 3, five commits), then the defect fixes (PR 2, seventeen commits) — for the
> reasons in [§0](#0-where-the-work-happens). Together they close all eight defects and both open
> questions in [§2.7](#27-still-open); [§6](#6-what-the-implementation-ran-into)
> records the forks that came up on the way, including one the plan as written would have shipped
> as a fault on every bot's first command. [§7](#7-the-offsets-smoke-test) is new: the retarget in
> stage A cannot move a StarCraft address but could move a layout, so tier 1 pins what the compiler
> believes and tier 2 checks what the linked DLL does against upstream's released v4.4.0. **Both
> tiers are green, and tier 2 found a defect on its first working run** — stage B's permission table
> was denying BWAPI its own match reset, which §6 records. The retarget itself moved nothing.
> [§4](#4-ripples-into-bwapi-c2) is still deferred and still accurate.

> This plan executes
> the defect table in [ADR 0001 §2](adr/0001-fork-and-invert.md) — the patch series the ADR says
> a fork is, as distinct from the referee the ADR says a fork is *for*. **It does not design the
> referee.** Where a fix needs a policy the referee would otherwise own, [§2](#2-the-decisions)
> records the call that was made instead and what it costs. Nothing here changes the ABI plan or
> the phase order in [implementation-plan.md](implementation-plan.md); it changes the tree those
> phases build against, and **nothing lands in `bwapi-c2` until the pin bump that follows a
> release** ([§2 decision 4](#decision-4--the-fork-diverges-and-bwapi-c2-does-not-move-yet)).

## Scope

Eight rows of ADR 0001 §2's table. Every one is verified in the tree at
`RadicalZephyr/bwapi` `main` (`d727fed`, upstream `master` at 2026-05-08) — §1 restates each
with the site as it actually reads there, because three of the eight are wider than the table
records and one is narrower.

Out of scope, and stated so the omissions are deliberate rather than forgotten: the referee
process; the OS privilege boundary between bot and game; Wine; cgroups; the match-record
bundle; grouped commands (ADR §5.2, plan §15 #17); and the two capabilities ADR 0001 §2 already
names as unreachable by a fork. Where a defect's honest fix stops short of the property the ADR
wants, §5 says where it stops.

**Module mode is deleted, not fixed.** Defect 2.4 is the anti-cheat veto living inside the
bot's address space; the instruction for this series is to remove the ability to run module-mode
bots rather than to relocate the enforcement point. That decision drives more of this plan than
any other — it is stage B, it runs first, and it is where three of §2's six decisions had to be
made. **What is deleted is the game process's ability to load a bot**: the two loaders, the
veto, and every entry point they call. `BWAPI::AIModule` itself survives as a client-side base
class (§2.7 item i), because after stage B nothing in the game process can reach it and every
existing module bot's event handlers port to a client bot unchanged.

---

## 0. Where the work happens

**The work ships as two pull requests, merged in order.** Stage A's retarget and the offsets
smoke test ([§7](#7-the-offsets-smoke-test)) are one concern — moving off a toolset nobody can
install, and establishing that doing so changed nothing in StarCraft's memory. Stages B through H
are another — changing what BWAPI does. They are reviewed for different things by different
reasoning, so they are separated, and the test suite lands first because that is the order in
which it is useful: PR 3 is what makes PR 2 safe to read.

| What | Where |
|---|---|
| Stage A's retarget, and tiers 1 and 2 of the smoke test | `RadicalZephyr/bwapi` **PR 3**, branch `claude/bwapi-toolset-and-patch-tests-gvul5m`, off `main` (`d727fed`) |
| Stages B through H — the eight defect fixes | `RadicalZephyr/bwapi` **PR 2**, branch `claude/bwapi-defect-fixes-gvul5m`, based on PR 3's branch |
| This plan and its revisions | `RadicalZephyr/bwapi-c2`, branch `claude/bwapi-defect-fixes-gvul5m`, beside the ADR it executes |
| Pin, layout baseline, fixture ripples | **Deferred**, to the pin bump after this series is released — see [§4](#4-ripples-into-bwapi-c2) |

Stage A.2 — the validation header and its Linux test — goes with the defect fixes rather than
with the retarget, because what it tests is defect 2.2 and not the toolset. Only A.1 is in PR 3.

**Two lineages exist on the fork and this series only touches one.** `main` tracks upstream
untouched; `bwapi-c2-pin` carries three commits on the same base (`revisionUpdate.sh`,
`svnrev.h`, the `va_list` fix — [`pins.md`](pins.md)). **The series lands on `main` and leaves
`bwapi-c2-pin` alone** (§2 decision 4). That makes the fork's `main` a permanent divergence from
upstream rather than a mirror of it — a different promise from the one `pins.md` currently makes,
and one the pin-bump work after this series' release is where it gets rewritten.

---

## 1. The defects, as the tree actually reads

Row numbers are ADR 0001 §2's table order. **Verified** means the site was read at `d727fed`;
**wider than recorded** means the ADR's fix line is correct but does not cover every instance.

| # | Defect | Sites verified at `d727fed` | Status |
|---|---|---|---|
| 1 | Server blocks on the client with no deadline | `Server.cpp:737-754` `callOnFrame`. The pipe is created `PIPE_NOWAIT` (`Server.cpp:197-205`) and switched to `PIPE_WAIT` by `setWaitForResponse(true)` on connect (`Server.cpp:313-333`), so the `ReadFile` genuinely blocks. `PIPE_TIMEOUT = 3000` is `CreateNamedPipe`'s *default client timeout*, which governs `WaitNamedPipe` and nothing on this path | **Verified** |
| 2 | Client-supplied `commandCount` used as an unclamped loop bound | `Server.cpp:757` (`commandCount`), `Server.cpp:839` (`unitCommandCount`), and **`GameDrawing.cpp:214` (`shapeCount`)**, which the table does not name and which runs on the trusted side every frame. Three more unchecked client-supplied indices: `data->strings[v1]` at `Server.cpp:778, 782, 815` and `data->strings[data->shapes[i].extra1]` at `GameDrawing.cpp:228`. The only bound anywhere is the `assert` in the untrusted process (`BWAPIClient/Source/GameImpl.cpp:38, 44, 55, 61`), compiled out under `NDEBUG`. The correct pattern is present once, at `Server.cpp:841` for `unitIndex` | **Wider than recorded** |
| 3 | The client maps the whole 33 MB state plane writable | `BWAPIClient/Source/Client.cpp:107`. The client also writes two *state* fields directly: `data->hasLatCom` (`BWAPIClient/Source/GameImpl.cpp:861`) and `data->hasGUI` (`:873`), each alongside the command that asks the server to do the same thing | **Wider than recorded** |
| 4 | Anti-cheat is a veto interface inside the bot's own address space | `GameInternals.cpp:273` `tournamentCheck`, 13 call sites across `GameImpl.cpp`, `GameDrawing.cpp`; loader at `GameUpdate.cpp:265` `initializeTournamentModule`; module loader at `GameUpdate.cpp:324` `initializeAIModule` | **Verified** |
| 5 | The meter's clock is `GetTickCount()` | `Server.cpp:241-243` (client mode) and `GameEvents.cpp:506-523` (module mode — deleted by stage B). Also `Server.cpp:93` and `:234`, the game-table keep-alive the client's connect logic sorts on | **Verified** |
| 6 | A client bot cannot read its own charged time | `BWAPIClient/Source/GameImpl.cpp:947` returns `0`. There is no field in `GameData` to return: `setLastEventTime` writes to a `GameImpl` member on the server side only (`GameImpl.h:362`) | **Verified** |
| 7 | Global unit-ID allocation is an information channel | `Server::getUnitID` (`Server.cpp:719-729`) allocates densely on first call. It is first called from `extractUnitData` (`GameUnits.cpp:228`) over `aliveUnits` — **every unit alive in the game**, not the accessible ones — so the handle a bot receives for a scouted enemy unit is its global creation ordinal, and the gap between two of the bot's own consecutive handles is the count of units created by everyone else in between. `BulletImpl::saveExists` (`BulletImpl.cpp:30-34`) leaks identically through a global `nextId`. **And `UnitImpl::updateData` calls `getUnitID` on *referenced* units** — `target`, `orderTarget`, `buildUnit`, `addon`, `nydusExit`, `powerUp`, `carrier`, `hatchery` (`UnitUpdate.cpp:296, 360, 365, 373, 378, 388, 395, 434, 437`) — which allocates a handle for, and hands the bot a handle to, a unit it has never seen | **Wider than recorded** |
| 8 | The C runtime RNG is seeded from the wall clock | `GameInternals.cpp:296` `srand(GetTickCount())`. Its only consumer is `getMenuRace` (`GameMenu.cpp:35-41`). A second unseeded generator exists — `AutoMenuManager`'s `mt19937`, seeded from `system_clock::now()` (`AutoMenuManager.cpp:21`) and *already* overridable by `seed_override` (`:26-28`). The game's own seed is already overridable, through the `GetSystemTimeAsFileTime` detour (`Detours.cpp:111-124`) | **Narrower than it reads, and half-solved** |

**One defect is smaller than the table implies and it is worth saying so.** Row 8's `srand` feeds
exactly one decision — which race `RANDOMTPZ` picks — and BWAPI already ships a `seed_override`
config key wired through a detour to the engine's own seed. The fix is to route all three
generators through that one key and to record the value, not to build a seeding mechanism.

**And one is larger.** Row 7's fix is not "renumber the handles". `getUnitID` is an
allocate-on-lookup function called from nine sites, and at least eight of them are asking a
question — *what is this unit's handle* — that must be answerable with "it has none".

---

## 2. The decisions

Six policies this series cannot derive from the defect table, each one a thing the referee would
otherwise own. All six are now settled. **The recommendation is recorded beside the call so the
reasoning is reviewable and so a later reversal is a change to a stated position rather than an
archaeology exercise.** Two smaller questions the decisions themselves opened are in
[§2.7](#27-still-open).

### Decision 1 — the deadline is a bounded wait; the adjudication rule is not ours

ADR decision 6 says the referee deadline **drops the frame** and never blocks. Drop-and-continue
is not implementable in this architecture without a change the ADR does not cost: **the state
plane is single-buffered.** `updateSharedMemory()` writes one `GameData`; if the server advanced
to frame N+1 while the client was still reading frame N, the bot would read a torn mix of two
frames. Drop-and-continue needs a double-buffered plane *and* ADR §6 fork 3 — what a late bot
sees when it comes back — answered first. Both are out of scope.

> **Ship the mechanism, not the policy.** On expiry, do what the tree already does when the pipe
> breaks (`Server.cpp:745-751`): disconnect, record the cause and the elapsed µs, and let the
> game play out. New `[game] frame_timeout_ms`, **defaulting to `0` — infinite, today's
> behaviour** — so the patch is behaviour-preserving until an operator configures it.

**Cost.** The defect closes — the runner can no longer be wedged forever by a hung bot — but the
property ADR §5.1 wants from the referee deadline, *never block and keep playing*, does not
arrive. A timed-out bot is a disconnected bot. `leaveGame()`-on-expiry, ADR decision 8's shape
without the referee, is deliberately not implemented: it is an adjudication rule, and adjudication
rules belong to the process that can attribute them.

**Where the cause goes.** There is no match record to write to, so it goes to the BWAPI log with
the elapsed µs beside it. When the match-record bundle exists it is one more producer.

### Decision 2 — a static permission table replaces the veto

The 13 `tournamentCheck` sites are the only thing standing between a client bot and
`enableFlag(Flag::CompleteMapInformation)`, `setLocalSpeed`, `setFrameSkip`, `setGUI`,
`pauseGame`, `setMap`, `leaveGame` and `setCommandOptimizationLevel`. Delete module mode with no
replacement and **the fork is strictly less noninterfering than upstream running a tournament
module** — a bot would simply ask for full map information on frame 0 and get it. That is a
regression against G1 introduced by a patch series whose purpose is G1.

> **A `[permissions]` section in `bwapi.ini`, read once on the trusted side, one key per
> `Tournament::ActionID`**, defaulting to deny for the eight above and allow for the rest.

This is not a referee: it is a config file the game process reads and a `switch` that refuses. It
is still strictly better than the veto it replaces, because the enforcement point is no longer a
DLL that the bot's own process loaded and can patch.

**`EnableFlag` has no nuance to preserve.** `Flag::Max` is 2 and both flags —
`CompleteMapInformation` and `UserInput` — are cheats, so the key is a plain deny with no
per-flag list under it.

**The knob has to be grantable, and B.5 is what proves it.** `TestAIModule` enables both flags in
nine files and calls `setLocalSpeed`, `setFrameSkip` and `setCommandOptimizationLevel` — five of
the eight denied actions — and `TestMap1.cpp:50-51` asserts the flags are *off* before enabling
them. Porting it to client mode is therefore the acceptance test for the table: it must run
green against a `bwapi.ini` that grants those five, and fail closed against one that does not.

### Decision 3 — no client at match start means the game runs unattended

Today, if no client is attached by the first in-game frame, `initializeAIModule` loads the `ai`
DLL; if that fails it installs a no-op module **and enables `CompleteMapInformation` and
`UserInput`** (`GameUpdate.cpp:373-384`), which is how a human plays a game with BWAPI installed.
Deleting module mode deletes that fallback, and `Server::checkForConnections` only runs while
`!startedClient` (`Server.cpp:252-253`), so a client that misses the menu window cannot attach.

> **Run unattended.** No bot, no flags, the game plays out and ends, and the log says so once.
> The connection window stays where it is; a client that misses it does not get a second chance.

**Cost.** The human-play and in-process-observer paths go. That is not collateral damage — it is
the same in-process trust defect 2.4 is about. `[config] shared_memory = OFF` (`Config.cpp:103`)
remains supported and now means the same thing as unattended.

### Decision 4 — the fork diverges, and `bwapi-c2` does not move yet

`pins.md` says the fork's default branch "tracks upstream untouched, so the diff between the two
branches is exactly what we carry". This series makes that false.

> **Land on `main`. Change nothing in `bwapi-c2` and nothing on `bwapi-c2-pin`.** The pin bump,
> the layout baseline, the fixture and `pins.md`'s own claims are a separate piece of work after
> this series is released.

**Cost, and it is the largest one in this document. Nothing in `bwapi-c2` validates any of this
until that bump**, so stage A's Windows job and the fork-local Linux tests are carrying the whole
verification burden. Two consequences follow, and both are real:

- **Stage A stops being hygiene and becomes the plan's foundation.** If `BWAPI.dll` is not built
  in CI, none of these changes are even compile-checked, because the fork builds nothing today.
- **F.3's two-world noninterference harness cannot be built here.** It needs `bwapi-c2`'s
  synthetic-`GameData` fixture. §2.7 says what happens to it.

The `bwapi-c2` ripples are enumerated in [§4](#4-ripples-into-bwapi-c2) anyway — as a deferred
list handed to that work, not as commits in this series. This document and the ADR pointer are
the only things this series writes into `bwapi-c2`.

### Decision 5 — the meter gets new accessors, not new semantics

`Game::getLastEventTime()` returns `int` milliseconds. Microseconds need either a new accessor or
a semantic change to an existing one, and `Game` is a surface `bwapi-c2` audits.

> **Keep `getLastEventTime()` with millisecond semantics; add `getLastFrameDurationMicros()` and
> `getLastIpcDurationMicros()`.**

**Cost.** Two new declarations on `BWAPI::Game`, which take the audit's `Game` row from 179 to
181 whenever the pin moves. Cheap to reverse.

### Decision 6 — the engine seed stops being published to the bot

`Server.cpp:497` writes `data->randomSeed = Broodwar->getRandomSeed()` — Brood War's own LCG
seed, handed to the client every frame and readable as `Game::getRandomSeed()`. ADR §4.3 lists
engine randomness under "no bot access" precisely because draw counts correlate with events.
Fixing defect 8 by deriving one recorded match seed and then publishing it would be hollow.

> **The seed goes to the log. It does not go into the plane.** Remove `data->randomSeed` and the
> client's `getRandomSeed`.

**Cost.** A small API removal on a surface the ABI plan has not specified yet, and one row that
was adjacent to ADR §2's table rather than in it now closes with the rest.

### 2.7 Still open

Two questions the decisions above opened. Neither blocks stage A, and the recommendation for each
is what the plan below assumes.

| # | Question | What the plan assumes |
|---|---|---|
| i | **Does `BWAPI::AIModule` survive as a client-side base class?** Keeping `TestAIModule` and porting it to client mode forces the question: `TestModule`, `TestMap1` and `MicroTest` all derive from `AIModule`, so deleting the class is a rewrite of forty test files rather than a port | **Yes.** What dies is `gameInit`/`newAIModule` as *entry points the game process loads*, and `TournamentModule` entirely. `AIModule` survives as a plain event-dispatch base a client bot may inherit — nothing in the game process touches it, so it costs nothing at the trust boundary, and it is what makes ADR §9's "bots compile and run" promise cheap for every existing module bot |
| ii | **Where does ADR §4.3's two-world noninterference harness live**, given decision 4? | Split it. The fork gets a Linux unit test for the handle allocator alone — issue-versus-lookup is where the logic is and it needs no `GameData`. The end-to-end two-world diff needs the fixture and is handed to the pin-bump work with the rest of §4 |

## 3. The steps

Eight stages, ordered by what each later stage would otherwise have to redo. Each step is one
commit unless it says otherwise. **Stage B runs first** because it deletes 13 veto call sites and
two loaders that every subsequent diff would otherwise touch twice.

### Stage A — make the tree compile-checked

Nothing in this repository or the fork builds `BWAPI.dll` today. `bwapi-c2` builds the *client*
closure on Linux; the fork ships an MSVC solution pinned to `v141_xp` and no CI. **Every step
below is otherwise unverifiable, so this stage is a prerequisite, not a nicety** — and §2
decision 4 makes it more than that, because with `bwapi-c2` frozen until the pin bump, stage A's
job plus the fork-local Linux tests are the only verification this series gets.

- **A.1** A Windows CI workflow on the fork building `BWAPI.dll` (x86) and `BWAPIClient`.
  Retarget `BWAPI.vcxproj` and its dependencies off `v141_xp` onto the current toolset — the
  call `implementation-plan.md` §0 already made for `bwapi_c2.dll`, for the same reason.
  *Check:* the workflow is green on an unmodified `main`.
- **A.2** Extract the trusted-side validation that stages C and F will grow into one
  dependency-free header (`BWAPI/Source/BWAPI/ClientInput.h`) plus a Linux-buildable test
  binary. No behaviour change: the header starts as the `unitIndex` bound already at
  `Server.cpp:841`, moved. *Check:* the test binary builds and passes with `clang++` on Linux;
  `BWAPI.dll` still builds on Windows.

### Stage B — remove module mode (defect 4; decisions 2 and 3, §2.7 item i)

- **B.1** The policy table. Add `[permissions]` to `bwapi.ini` and a trusted-side
  `permissionCheck(Tournament::ActionID)` reading it, with §2 decision 2's defaults. Replace all
  13 `tournamentCheck` call sites with it. No deletion yet — the tournament module still loads
  and still vetoes, and the policy table is a second gate, so this step is provably additive.
  *Check:* a bot denied `CompleteMapInformation` by config sees `Errors::Access_Denied` and the
  flag stays false; a bot granted it still gets it.
- **B.2** Delete the tournament module: the loader (`GameUpdate.cpp:265`), `tournamentAI`,
  `tournamentController`, `hTournamentModule`, `isTournamentCall`, `tournamentCheck`,
  `bTournamentMessageAppeared`, `getTournamentString`, the `Server.cpp:671-679` post-processing
  hook, the `ExceptionFilter.cpp:126` branch, `include/BWAPI/TournamentAction.h`,
  `TournamentModule` in `include/BWAPI/AIModule.h`, the `ai/tournament` config key, and
  `ExampleTournamentModule`. *Check:* the DLL builds; a client-mode game runs; §1 row 4's
  enforcement point is the config file.
- **B.3** Delete the AI module *loader*, not the base class (§2.7 item i): `initializeAIModule`,
  `hAIModule`, `client`, `externalModuleConnected`, `SendClientEvent`,
  `GameImpl::processEvents`, the `ai`/`ai_dbg` config keys, and the `AIModuleLoader`,
  `ExampleAIModule`, `DevAIModule` and `BWScriptEmulator` projects. **`include/BWAPI/AIModule.h`
  and `BWAPILIB/Source/AIModule.cpp` stay**, minus `TournamentModule`, as a client-side event
  base a bot may inherit; nothing in the game process references either after this step, and the
  header says so. *Check:* the DLL contains no `LoadLibrary` of a bot; `nm`/`dumpbin` shows
  `AIModule`'s vtable only in `BWAPILIB`.
- **B.4** The unattended path (§2 decision 3). `Server::update`'s disconnected branch keeps only
  the connection check; a match that starts with no client attached runs with no bot and no
  flags, and says so once in the log. *Check:* a game launched with `auto_menu` and no client
  reaches the end screen without loading anything.
- **B.5** Port `TestAIModule` to client mode. `Dll.cpp`'s `gameInit`/`newAIModule` exports become
  a `main()` that connects through `BWAPIClient` and drives `Broodwar->getEvents()` into the
  existing `AIModule` subclasses — `ExampleAIClient`'s loop around `TestModule`, `TestMap1` and
  `MicroTest` unchanged. Ships its own `bwapi.ini` granting the five permissions the suite needs:
  both flags, `SetLocalSpeed`, `SetFrameSkip`, `SetCommandOptimizationLevel`. *Check:* it builds
  in stage A's Windows job; against a granting `bwapi.ini` the suite reaches its first test, and
  against a default one it fails closed at `TestMap1.cpp:50-51` with `Access_Denied`.

**B.5 is two things at once, and the second is the more useful.** It preserves BWAPI's only
in-game regression suite, and it is the acceptance test for the permission table — the only
consumer in the tree that exercises both the grant and the deny path. What it is *not* is CI
coverage: it needs retail Brood War, a map and a launcher, so it compiles in CI and runs by hand,
like `implementation-plan.md`'s phase-3 exit criterion. **The fork's automated coverage after this
series is stage A.2's validator tests, C.5's fuzzer and F.1's allocator test, and nothing else**
until the pin bump reconnects `bwapi-c2`'s fixture (§2 decision 4).

### Stage C — validate everything the client writes (defect 2)

- **C.1** Clamp the four client-written counts to their `MAX_` on the trusted side —
  `commandCount`, `unitCommandCount`, `shapeCount`, `stringCount` — in `ClientInput.h`, applied
  at `Server.cpp:757`, `Server.cpp:839` and `GameDrawing.cpp:214`. *Check:* a test writes
  `commandCount = INT_MAX` and the server processes `MAX_COMMANDS`.
- **C.2** String indices. Every `data->strings[i]` read on the trusted side checks `i` against
  the clamped `stringCount`, and the string is NUL-terminated on the trusted side before use —
  a client can fill all 1024 bytes. Sites: `Server.cpp:778, 782, 815`, `GameDrawing.cpp:228`.
  *Check:* an unterminated string and an out-of-range index each produce a dropped command, not
  a read past the mapping.
- **C.3** Command payloads. Validate each `Command`'s `value1`/`value2` against what its handler
  will do with it — player ids, flag ids, speeds, frame-skip, optimisation level — and each
  `UnitCommand`'s `type` against `UnitCommandTypes::Enum::MAX` before constructing a
  `UnitCommand`. *Check:* a fuzzed command plane issues no command with an out-of-range field.
- **C.4** Bound `Server::getUnitID`'s return against `GameData::units`' 10,000 entries and drop
  the write rather than run off the array (`Server.cpp:493, 599`). This is a server-side
  overflow driven by game state rather than by the client, and ADR §8 records its reachability
  as never counted; stage F makes it far harder to reach, which is not the same as fixed.
  *Check:* a fixture with more than 10,000 issued handles does not write out of bounds.
- **C.5** A fuzz harness over `ClientInput.h` under ASan and UBSan, in the fork's CI. *Check:*
  a million random command planes produce no diagnostic.

### Stage D — the meter (defects 5 and 6; decision 5)

- **D.1** A monotonic microsecond clock: `QueryPerformanceCounter` / `QueryPerformanceFrequency`
  behind one small helper, replacing `GetTickCount` at `Server.cpp:241-243` and at the game-table
  keep-alive (`Server.cpp:93, 234`). *Check:* the reported span for a bot that sleeps a known
  duration is within a microsecond of it, not within three ticks of it.
- **D.2** Two timestamps, so a bot is not billed for the IPC. The server records the instant its
  `WriteFile` returns and the instant its `ReadFile` returns; the client records its own wake and
  reply instants into the command plane. The difference is the transport's cost and is reported
  separately. *Check:* on an idle client the bot-attributed span is a few microseconds and the
  IPC span carries the rest.
- **D.3** Publish the meter. New `GameData` fields for both spans; `BWAPIClient`'s
  `getLastEventTime` (`GameImpl.cpp:947`) returns the real number; decision 5's two new
  accessors. *Check:* a client bot reads back a value that matches what the server recorded.

### Stage E — the deadline (defect 1; decision 1)

- **E.1** Bounded wait. `CreateNamedPipe` gains `FILE_FLAG_OVERLAPPED`; `ConnectNamedPipe` and
  `callOnFrame`'s read become overlapped, waited with a timeout against D.1's clock, and
  cancelled with `CancelIoEx` on expiry. *Check:* with the timeout configured and a client that
  never replies, `callOnFrame` returns within the timeout instead of never.
- **E.2** The expiry rule (§2 decision 1): disconnect, log the cause and the elapsed µs, continue
  the game. New `[game] frame_timeout_ms`, default `0` = infinite. *Check:* a deliberately hung
  client ends the connection with a named cause and the game reaches its end screen.

### Stage F — per-bot handle namespaces (defect 7; §2.7 item ii)

- **F.1** Split `Server::getUnitID` into `issueUnitID(Unit)`, which allocates, and
  `lookupUnitID(Unit)`, which returns `-1` for a unit that has never been issued one. Issue
  **only where the bot is told the unit exists** — the accessible branch of
  `computePrimaryUnitSets` (`GameUnits.cpp:159-186`) and the published-units loop
  (`Server.cpp:598`). Every other call site becomes a lookup, including all nine in
  `UnitUpdate.cpp`. *Check:* the handles a bot holds after N discoveries are exactly `0..N-1`,
  and a visible enemy unit targeting a unit the bot cannot see reports `target == -1`.
- **F.2** The same split for bullets: `BulletImpl::saveExists` (`BulletImpl.cpp:30-34`) issues on
  first *visible* existence rather than on first existence. *Check:* bullet handles are dense in
  the order the bot observed them.
- **F.3** A Linux unit test for the handle allocator alone: issue, look up, look up an unissued
  unit, re-look-up after eviction. It lives beside stage A.2's validator, needs no `GameData`,
  and covers the logic F.1 and F.2 introduce. *Check:* N discoveries yield exactly the handles
  `0..N-1`; a lookup of a never-issued unit is `-1`; the test fails against `main`.

**ADR §4.3's two-world harness is not in this series** (§2.7 item ii). The end-to-end property —
construct two world-states differing only in what the bot cannot see, run them, diff the bot's
view — needs `bwapi-c2`'s synthetic-`GameData` fixture, and §2 decision 4 freezes that repository
until the pin bump. F.3 covers the mechanism; the property is verified when the fixture is
reconnected. **Saying which of the two we have is the point of separating them.**

**Two behaviour changes to state in the commit message.** `GameData::initialUnitCount` becomes a
count of *accessible* initial units rather than all of them, and a reference to a unit the bot
has never seen becomes `-1` where it used to be a valid handle. Both are the fix working. Both
will move a bot's behaviour, which is exactly what ADR §9 predicts and what G6's differential
test exists to measure.

### Stage G — the read-only state plane (defect 3)

The widest mechanical diff in the series, and last for that reason: every earlier stage touches
`Server.cpp` and `BWAPIClient/Source/GameImpl.cpp`, and doing this first would mean rewriting
each of them twice.

- **G.1** Split `GameData` into the state plane (server → client: everything except the four
  client-written regions) and a new `CommandData` (client → server: `strings`, `shapes`,
  `commands`, `unitCommands`, their counts, and D.2's client timestamps), in two named sections.
  Both mapped read/write by the server. *Check:* the DLL and the client build; a game runs.
- **G.2** The client maps the state section `FILE_MAP_READ` and the command section
  read/write. `setLatCom` and `setGUI` stop writing `data->hasLatCom` and `data->hasGUI` and
  cache locally until the server echoes the change back. *Check:* a write to the state plane
  from the client faults.
- **G.3** Tighten the section DACL. `CreateFileMappingA` currently passes `NULL` security
  attributes (`Server.cpp:103`) and the *pipe* is created with an explicit
  Everyone/`GENERIC_ALL` ACE (`Server.cpp:138-146`). Narrow both to the account the game runs
  as. *Check:* a process under a different account cannot open either.
- **G.4** `CLIENT_VERSION` bump, and the `bwapi-c2` ripples in §4. *Check:* an old client
  refuses to connect with the version mismatch message that already exists
  (`Client.cpp:120-129`).

**What G buys and what it does not, stated plainly.** A read-only view stops accidental
corruption and stops a *cooperative* bot from scribbling on state. It does not stop a hostile
one: the section name is derived from the server's pid, the client opens it by name today, and
nothing prevents a second `OpenFileMapping` with `FILE_MAP_WRITE`. G.3 raises that cost;
only a separate account does more, and ADR §2 already classes the OS privilege boundary as a
thing a fork cannot reach. The defect as written is fixed; the property it gestures at is not.

### Stage H — one recorded seed (defect 8; decision 6)

- **H.1** One `match_seed`: `seed_override` if set, otherwise a value drawn once at DLL init from
  a system CSPRNG, logged. It feeds `srand` (`GameInternals.cpp:296`), `AutoMenuManager`'s
  `mt19937` (`AutoMenuManager.cpp:21-28`) and the `GetSystemTimeAsFileTime` detour
  (`Detours.cpp:111-124`), which is already wired. *Check:* two runs with the same
  `seed_override` pick the same race and the same map from a rotation.
- **H.2** Decision 6: remove `data->randomSeed` (`Server.cpp:497`) and the client's
  `getRandomSeed`. *Check:* the engine seed is in the log and not in the plane.

---

## 4. Ripples into `bwapi-c2`

**None of these is a commit in this series** (§2 decision 4). They are the deferred list handed to
the pin-bump work that follows a release, recorded here so that work starts from an inventory
rather than from a broken build.

| Stage | What moves here |
|---|---|
| B.3 | Nothing moves in `closure.cmake` — `AIModule.cpp` stays (§2.7 item i). `tools/abi/audited-headers.txt:63` excludes `BWAPI/AIModule.h` "as a scoped v2 item"; the exclusion stands but its reason changes, because there is no module mode left to scope it against |
| F | ADR §4.3's two-world noninterference harness, which F.3 deliberately does not build (§2.7 item ii). It needs the `Fixture` builder and is the largest single item on this list |
| D.3, G.1 | `tests/layout_dump/baseline.json` changes: new fields, then a split struct. The `GameData` size in `implementation-plan.md`'s phase-0 exit criterion (33,017,048) changes with it |
| F.1 | Fixture invariant 4 — "`onMatchStart()` fills only `accessibleUnits` from `initialUnitCount`" — still holds, but `initialUnitCount` now means something narrower. The comment in `tests/fixture/fixture.h` says so |
| G.1 | The fixture builds one `GameData`; it now builds a pair. Every suite that reaches `data->unitCommands` to assert an emitted command reaches the command plane instead |
| D.3 | Two new `Game` declarations (decision 5); the audit's `Game` row goes 179 → 181 and `docs/pins.md`'s audit table is rerun |
| Throughout | `pins.md`'s carried-commits table and its "How the pins work" paragraph |

---

## 5. Where each fix stops short, and why that is honest

ADR §2 argues the review indicted a patch series rather than an architecture. That is right, and
this is the patch series — but four of the eight land short of the property their row names, and
the gap in each case is exactly the referee.

| Defect | What the patch gives | What still needs the referee |
|---|---|---|
| 1 | A bounded wait and an attributable end | Drop-and-continue, which needs a double-buffered plane and ADR §6 fork 3 answered |
| 3 | A read-only view and a narrower DACL | A privilege boundary. Same-account processes can still map it writable |
| 4 | Enforcement in the game process, from a config file the bot cannot load | Enforcement *outside* the process. A config file is still inside the trust domain the bot's game shares |
| 7 | Dense per-bot handles and no handle for an unseen unit; the mechanism unit-tested | The rest of ADR §4.3's residual surface — derived quantities, fog, response timing — and the two-world harness that would show the property rather than the mechanism (§2.7 item ii) |

Defects 2, 5, 6 and 8 are fixed outright, and decision 6 closes the engine-randomness leak that
sits beside 8. That is four of eight closed and four narrowed, which is a fair statement of what a
fork of the server side buys before anything is inverted.

---

## 6. What the implementation ran into

Twenty-six commits on `RadicalZephyr/bwapi` `claude/bwapi-defect-fixes-gvul5m`, off `main`
(`d727fed`). The stages above are as executed; this section records the forks the plan did not
anticipate and how each was resolved, because three of them were resolved *against* what the
stage said and one of them was a defect the plan would have shipped.

### Forks resolved during stage A

| Fork | Resolution |
|---|---|
| **`storm.h` under `/Zp1`.** `Storm.vcxproj` compiled at one-byte packing; the current Windows SDK's headers carry `static_assert`s about their own sizes that one-byte packing breaks | Removed `<StructMemberAlignment>1Byte</StructMemberAlignment>` (`37026b9`). It protected nothing: every other consumer of `storm.h` in the tree already compiled it at default packing, so the stub was the odd one out and its packing was never part of any ABI |
| **`std::experimental::filesystem` is gone** from MSVC 14.51 | `Util/Path.h`'s alias moved to `std::filesystem` (`8ea2627`). The retarget forced it; it is not optional and not scope creep |
| **The series stripped CRLF from 45 files.** A Python `write_text()` normalised line endings on every file it touched, turning small diffs into whole-file rewrites | Fixed forward in `490d4fa`, whitespace-only, rather than by rewriting history. The recurrence is prevented mechanically: every subsequent edit went through a helper that reads bytes, matches patterns against the file's own line ending, and asserts the replacement happened |

### A defect this plan would otherwise have shipped

**Stage G as written would have faulted on every bot's first command.** The plan said the client
gets a read-only view of `GameData` and writes commands to a second, writable plane. What it did
not account for is `CommandTemp.h`: BWAPI's client-side latency compensation *predicts* the effect
of a command by writing 274 unit fields and 36 player fields through `self` — pointers into the
state plane — so that `Unit::isMoving()` answers correctly before the server has seen the order.
Making that plane read-only turns every one of those writes into an access violation.

Resolved in `a8044be` by giving the client its own copy: `unitMirror` and `playerMirror` inside
`Client`, refreshed once per frame from the read-only plane, with `self` pointing at the copy.
Latency compensation keeps working, and what it writes is now private to the client rather than
being a write into memory the game trusts — which is closer to what defect 3 wanted than the plan
was.

**`0a2e395` is the second half of that.** `updateSharedMemory` set `data->unitCount` before the
frame's handles had been issued, so a unit discovered this frame would be named by a
`UnitDiscover` event and then fall outside the mirror copy. That fails silently — a wrong answer,
not a crash — and it only became reachable once the mirror existed. `unitCount` is now published
at the end of `updateSharedMemory`, after the handles it counts.

### The defect tier 2 found

**Stage B enforced the permission table in the wrong place, and the default table therefore denied
BWAPI its own match reset.** The shadow harness's first working run produced a report identical to
upstream v4.4.0's on every line but one:

```
v4.4.0       write 0x005124D4 60  01 00 00 00 A7 00 00 00 6F 00 00 00 ...
this build   write 0x005124D8 56              A7 00 00 00 6F 00 00 00 ...
```

`0x005124D4` is `FrameSkip` and `0x005124D8` is `GameSpeedModifiers`; they are adjacent, so the two
writes coalesce into one run when both happen. v4.4.0 resets the frame skip to 1 at every match
start and this build did not.

The cause was that stage B put `permissionCheck` inside the `GameImpl` method rather than at the
call that asks for it. Those methods are also how BWAPI does its own housekeeping —
`initializeData()` resets the frame skip and the GUI flag at every match start — and the default
table, which reproduces `ExampleTournamentModule`, denies `SetFrameSkip` and `SetGUI`. So BWAPI was
denying itself. Frame skip persists across matches within one process, so a match inherited whatever
the previous one left: a reproducibility defect introduced by the fix for a different one.

Resolved by asking the table where a request *arrives* rather than where it lands:
`Server::processCommands` for the client's twelve gated commands, which after stage B is the only
path from a bot into any of this, and `parseText` for the five console commands typed into the
game's chat box. No `GameImpl` method asks on its own behalf any more. Three latent faults went with
it — `setGUI` calling the gated `setFrameSkip` and half-applying under a table that allowed one and
not the other; the drawing code changing text size several times a frame through a gated setter that
a bot could never reach anyway, since the client's `setTextSize` never leaves the client; and stage
H's match-seed announcement going out through `sendText`, which a table denying `SendText` would
have suppressed, defeating the point of a seed that is *recorded*.

**This is the argument for tier 2 in one paragraph.** It was built to answer a question about the
toolchain retarget, it answered it (nothing moved), and the defect it actually caught was one this
series introduced in a stage that has nothing to do with offsets. Neither the compiler, the Linux
tests, nor review found it; it is only visible as a byte that a running BWAPI writes and ours did
not.

### Two bugs the new tests found in the new code

Recorded because they are the argument for stage A.2 having existed at all.

- **`reserveSlot` left a hostile count in place** when the buffer was full: it refused the slot but
  did not normalise the count it had just rejected, so the next caller saw the same out-of-range
  value. Found by the fuzzer in `b8a0c9c`, not by review.
- **`waitMillis(LLONG_MAX)` overflowed** in the rounding step before the clamp. Found by the unit
  test for the clock, which passed a saturated deadline precisely because nothing else would.

---

## 7. The offsets smoke test

Stage A retargeted the toolset off `v141_xp` and moved `_WIN32_WINNT` from `0x0501` to `0x0601`.
Neither can move a StarCraft address — `BW/Offsets.h` binds every one of them with an integer
literal through `IS_REF`, and no compiler rewrites `0x0057F0F0`. What either could move is the
*layout* of the types mapped onto those addresses, and that failure is silent: the DLL builds,
loads, and reads every field from the wrong bytes of a running game.

Nothing in this repository can run StarCraft, so the test is in two tiers, and the tiers answer
different questions.

| Tier | Where | What it asks | Status |
|---|---|---|---|
| 1 | `bwapi/tests/bw_layout` | What does the *compiler* believe? `sizeof` and `alignof` for the 59 types mapped onto game memory, and the address each of the 66 `BWDATA` references is bound to, pinned to `baseline.txt` | **Done.** Green; the baseline is committed |
| 2 | `bwapi/tests/shadow_bw` | What does the *linked binary* do? Which import slots it redirects and which bytes it writes into StarCraft's address range, compared against upstream's released v4.4.0 | **Done.** Green; it found a defect on its first working run ([§6](#the-defect-tier-2-found)) |

**Tier 1 is complete and cheap, and it says something.** The sizes it measured are the documented
StarCraft ones — `CUnit` 336, `CBullet` 112, a trigger 2400, a dialog 0x56, the replay header 633 —
which matters because a self-consistent dump of the wrong thing would look exactly like a correct
one. The SDK types the `_WIN32_WINNT` change would have reached first are in the baseline and
unmoved: `POINT` 8/4, `PALETTEENTRY` 4/1, `RECT` 16/4.

**Tier 2 exists because tier 1 structurally cannot see two things.** The `BW::BWFXN_*` patch sites
are plain integer constants used only in `CodePatch.cpp` — no `BWDATA` reference is bound to them,
so there is no address for the dump to take. And the import slots BWAPI redirects are decided by
the loader from the *host process's* import table, which is not in a header at all.

It works because BWAPI never checks it is inside StarCraft: `HackUtil::PatchImport` passes a null
source module, so `GetModuleHandleA(nullptr)` gives it whatever process loaded the DLL, and
`ApplyCodePatches` writes to StarCraft's addresses whatever is mapped there. A host executable that
declares itself version 1.16.1.1, reserves `0x00400000`, and imports the eleven `storm.dll`
ordinals and thirteen `kernel32`/`user32` names BWAPI detours is therefore enough to catch the
whole patch pass in the act. `bwapi/tests/shadow_bw/README.md` has the design and its limits; the
short version of the limit is that it does not call through the hooks, so what it proves is that
the same slots are redirected to the same hook graph and the same bytes are written at the same
addresses — not that the detour bodies behave.

**The reference is upstream's released v4.4.0, pinned by SHA-256.** It is the only build of these
offsets that predates the retarget and the only ground truth available without a copy of the game,
which is also why it is downloaded in CI rather than vendored: a binary in the tree is a binary
somebody could have edited, and a hash of a release asset is not.

**The answer on the retarget is: it moved nothing.** Tier 2's report — 24 import slots redirected,
27 writes into the game image, 41 distinct hook targets — is byte-identical between upstream's
v4.4.0 binary and this build, in the same order with the same hook-graph numbering. Every address
BWAPI patches, every byte it writes there, and every import it redirects survived the toolset and
`_WIN32_WINNT` change unchanged. That also settles the question of whether to keep a `v141_xp`
reference build around for comparison: it would answer a narrower version of what this answers.

Two of tier 2's numbers cross-check tier 1 rather than repeating it, which is worth recording
because they were not designed to. The speed table written at `0x005124D8` holds StarCraft's real
frame durations (167, 111, 83, 67, 56, 42 …), and the persistent-patch thread's two screen-layer
hooks land at `0x006CEF88` and `0x006CEFC4` — exactly `ScreenLayers[2]` and `[5]` given the
`sizeof(BW::layer) == 20` that tier 1's baseline pins. A wrong layout would have put them elsewhere.

**Neither tier compares against anything in `bwapi-c2`.** They are fork-local, they run in the
fork's Windows job, and they are the third leg of the verification §2 decision 4 left this series
standing on — beside stage A's build and the Linux validator tests.
