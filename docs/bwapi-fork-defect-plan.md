# Implementation plan: the eight defects in ADR 0001 §2

> **Status: draft, and blocked on six decisions.** This plan executes the defect table in
> [ADR 0001 §2](adr/0001-fork-and-invert.md) — the patch series the ADR says a fork is, as
> distinct from the referee the ADR says a fork is *for*. **It does not design the referee.**
> Where a fix needs a policy the referee would otherwise own, the policy is named as a blocker
> in [§2](#2-the-blockers) with a recommendation, and the step that depends on it is marked.
> Nothing here changes the ABI plan or the phase order in
> [implementation-plan.md](implementation-plan.md); it changes the tree those phases build
> against.

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
any other — it is stage B, it runs first, and it is the source of three of the six blockers.

---

## 0. Where the work happens

| What | Where |
|---|---|
| Every code change | `RadicalZephyr/bwapi`, branch `claude/bwapi-defect-fixes-gvul5m`, off `main` (`d727fed`) |
| This plan and its revisions | `RadicalZephyr/bwapi-c2`, same branch name, beside the ADR it executes |
| Pin, layout baseline, fixture ripples | `bwapi-c2` — see [§4](#4-ripples-into-bwapi-c2) |

**Two lineages currently exist on the fork and this plan makes them collide.** `main` tracks
upstream untouched; `bwapi-c2-pin` carries three commits on the same base (`revisionUpdate.sh`,
`svnrev.h`, the `va_list` fix — [`pins.md`](pins.md)). The series lands on `main`'s lineage;
`bwapi-c2-pin` then rebases onto it and `bwapi-c2` bumps the pin in one commit. That makes the
fork's `main` a permanent divergence from upstream rather than a mirror of it, which is a
different promise from the one `pins.md` currently makes. Blocker 4.

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

## 2. The blockers

Six. Each is a policy the referee would own, or a compatibility promise this repository has
already made; each has a recommendation, and each names the step it blocks. **None of them
blocks stage A, and stages B through H can start against the recommendations if they are
accepted as written.**

### Blocker 1 — what happens when the deadline expires (blocks E.2)

ADR decision 6 says the referee deadline **drops the frame** and never blocks. Drop-and-continue
is not implementable in this architecture without a second change the ADR does not cost:
**the state plane is single-buffered.** `updateSharedMemory()` writes one `GameData`; if the
server advanced to frame N+1 while the client was still reading frame N, the bot would read a
torn mix of two frames. Drop-and-continue needs a double-buffered state plane, and it needs ADR
§6 fork 3 — what a late bot sees when it comes back — answered first. Both are out of scope.

**Recommendation: ship the mechanism, not the policy.** The bounded wait is the defect; the
adjudication rule is the referee's. On expiry, do what the tree already does when the pipe
breaks (`Server.cpp:745-751`): disconnect, record the cause and the elapsed µs, and let the
game play out. Default the timeout to `0` — infinite, today's behaviour — so the patch is
behaviour-preserving until an operator configures it.

**What I need:** confirmation that "bounded wait, disconnect on expiry, off by default" is the
v1 rule, and that a `leaveGame()`-on-expiry variant (ADR decision 8's shape, minus the referee)
is not wanted yet.

### Blocker 2 — what replaces the tournament veto (blocks B.1 and B.3)

The 13 `tournamentCheck` sites are the only thing standing between a client bot and
`enableFlag(Flag::CompleteMapInformation)`, `setLocalSpeed`, `setFrameSkip`, `setGUI`,
`pauseGame`, `setMap`, `leaveGame` and `setCommandOptimizationLevel`. Delete module mode with no
replacement and **the fork is strictly less noninterfering than upstream running a tournament
module** — a bot would simply ask for full map information on frame 0 and get it. That is a
regression against G1 introduced by a patch series whose purpose is G1.

**Recommendation: a static policy table, on the trusted side, read from `bwapi.ini`.** A
`[permissions]` section, one key per `Tournament::ActionID`, defaulting to deny for the eight
listed above and allow for the five cosmetic ones (`Printf`, `SendText`, `SetTextSize`,
`SetLatCom`, plus `EnableFlag` for the non-cheat flags). This is not a referee: it is a config
file the game process reads once and a `switch` that refuses. It is also strictly better than
the veto it replaces, because the enforcement point is no longer a DLL the bot's process loaded.

**What I need:** a choice between static config policy (recommended), unconditional refusal with
no knob, and leaving them open on the grounds that policy is the referee's job.

### Blocker 3 — the unattended path (blocks B.4)

Today, if no client is attached by the first in-game frame, `initializeAIModule` loads the `ai`
DLL; if that fails it installs a no-op module **and enables `CompleteMapInformation` and
`UserInput`** (`GameUpdate.cpp:373-384`), which is how a human plays a game with BWAPI
installed. Deleting module mode deletes that fallback, and `Server::checkForConnections` only
runs while `!startedClient` (`Server.cpp:252-253`), so a client that misses the menu window
cannot attach at all.

**Recommendation: run unattended.** No bot, no flags, the game plays out and ends. Keep the
connection window where it is. Losing "a human plays with BWAPI installed and full map
information" is not collateral damage — it is the same in-process trust that defect 2.4 is about.

**What I need:** confirmation that losing the human-play and in-process-observer paths is
acceptable, and whether a client should be able to attach mid-match (a small change, and it
interacts with blocker 1's disconnect-on-expiry rule).

### Blocker 4 — how the fork's branches relate after this (blocks G.4, and the pin bump)

`pins.md` says the fork's default branch "tracks upstream untouched, so the diff between the two
branches is exactly what we carry". This series makes that false. Splitting the state plane also
changes `sizeof(GameData)`, which moves `tests/layout_dump/baseline.json` (33,017,048) — a file
`CLAUDE.md` says changes **only at a pin bump**.

**Recommendation:** land the series on `main`; rebase `bwapi-c2-pin` onto it; bump the pin, the
baseline and the fixture in one `bwapi-c2` commit; rewrite `pins.md`'s "How the pins work" to
say the fork now carries a divergence rather than a patch set.

**What I need:** confirm the fork's `main` is now a hard fork, or say the series belongs on a
named branch that `bwapi-c2-pin` merges instead.

### Blocker 5 — the meter's unit and the public interface (blocks D.3)

`Game::getLastEventTime()` returns `int` milliseconds. Microseconds need either a new accessor
or a semantic change to an existing one, and `Game` is a surface `bwapi-c2` audits (179
declarations on the backlog).

**Recommendation:** keep `getLastEventTime()` with millisecond semantics, and add
`getLastFrameDurationMicros()` and `getLastIpcDurationMicros()`. Two new declarations, decided
at the next audit rather than appearing in it.

**What I need:** agreement that adding to `BWAPI::Game` is acceptable here. Cheap to reverse.

### Blocker 6 — the engine seed is published to the bot today (blocks H.1, and is adjacent to the table)

`Server.cpp:497` writes `data->randomSeed = Broodwar->getRandomSeed()` — Brood War's own LCG
seed, handed to the client every frame, readable as `Game::getRandomSeed()`. ADR §4.3 lists
engine randomness under "no bot access" precisely because draw counts correlate with events.
Fixing defect 8 by deriving one recorded match seed and then publishing it would be hollow.

**Recommendation:** the seed goes to the log and to whatever carries the match record; it does
not go into `GameData`. Remove `data->randomSeed` and make `Game::getRandomSeed()` return `0`
on the client, or deny it through blocker 2's policy table.

**What I need:** this row is *adjacent to* ADR §2's table rather than in it, so it is yours to
accept or defer. Deferring is coherent — it just means defect 8 is fixed for reproducibility and
not for noninterference.

---

## 3. The steps

Eight stages, ordered by what each later stage would otherwise have to redo. Each step is one
commit unless it says otherwise. **Stage B runs first** because it deletes 13 veto call sites and
two loaders that every subsequent diff would otherwise touch twice.

### Stage A — make the tree compile-checked (no blockers)

Nothing in this repository or the fork builds `BWAPI.dll` today. `bwapi-c2` builds the *client*
closure on Linux; the fork ships an MSVC solution pinned to `v141_xp` and no CI. **Every step
below is otherwise unverifiable, so this stage is a prerequisite, not a nicety.**

- **A.1** A Windows CI workflow on the fork building `BWAPI.dll` (x86) and `BWAPIClient`.
  Retarget `BWAPI.vcxproj` and its dependencies off `v141_xp` onto the current toolset — the
  call `implementation-plan.md` §0 already made for `bwapi_c2.dll`, for the same reason.
  *Check:* the workflow is green on an unmodified `main`.
- **A.2** Extract the trusted-side validation that stages C and F will grow into one
  dependency-free header (`BWAPI/Source/BWAPI/ClientInput.h`) plus a Linux-buildable test
  binary. No behaviour change: the header starts as the `unitIndex` bound already at
  `Server.cpp:841`, moved. *Check:* the test binary builds and passes with `clang++` on Linux;
  `BWAPI.dll` still builds on Windows.

### Stage B — remove module mode (defect 4; blockers 2, 3)

- **B.1** The policy table. Add `[permissions]` to `bwapi.ini` and a trusted-side
  `permissionCheck(Tournament::ActionID)` reading it, with blocker 2's defaults. Replace all 13
  `tournamentCheck` call sites with it. No deletion yet — the tournament module still loads and
  still vetoes, and the policy table is a second gate. *Check:* a bot denied
  `CompleteMapInformation` by config sees `Errors::Access_Denied` and the flag stays false.
- **B.2** Delete the tournament module: the loader (`GameUpdate.cpp:265`), `tournamentAI`,
  `tournamentController`, `hTournamentModule`, `isTournamentCall`, `tournamentCheck`,
  `bTournamentMessageAppeared`, `getTournamentString`, the `Server.cpp:671-679` post-processing
  hook, the `ExceptionFilter.cpp:126` branch, `include/BWAPI/TournamentAction.h`,
  `TournamentModule` in `include/BWAPI/AIModule.h`, the `ai/tournament` config key, and
  `ExampleTournamentModule`. *Check:* the DLL builds; a client-mode game runs; §1 row 4's
  enforcement point is the config file.
- **B.3** Delete the AI module: `initializeAIModule`, `hAIModule`, `client`,
  `externalModuleConnected`, `SendClientEvent`, `GameImpl::processEvents`,
  `include/BWAPI/AIModule.h`, `BWAPILIB/Source/AIModule.cpp`, the `ai`/`ai_dbg` config keys, and
  the `AIModuleLoader`, `ExampleAIModule`, `DevAIModule`, `TestAIModule` and `BWScriptEmulator`
  projects. *Check:* the solution builds with only `BWAPI`, `BWAPILIB`, `BWAPIClient`,
  `ExampleAIClient`, `Util`, `Storm` and the installer projects.
- **B.4** The unattended path (blocker 3). `Server::update`'s disconnected branch keeps only the
  connection check; a match that starts with no client attached runs with no bot and no flags,
  and says so once in the log. *Check:* a game launched with `auto_menu` and no client reaches
  the end screen without loading anything.

**Consequence to record, not fix:** B.3 deletes `TestAIModule`, which is BWAPI's only functional
test suite. After this series the fork's testing story is stage A.2's validator tests plus
`bwapi-c2`'s synthetic-`GameData` fixture, and the fork has no in-game regression coverage. That
is a real loss and it should be stated in the commit message rather than discovered later.

### Stage C — validate everything the client writes (defect 2; no blockers)

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

### Stage D — the meter (defects 5 and 6; blocker 5)

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
  `getLastEventTime` (`GameImpl.cpp:947`) returns the real number; blocker 5's two new
  accessors. *Check:* a client bot reads back a value that matches what the server recorded.

### Stage E — the deadline (defect 1; blocker 1)

- **E.1** Bounded wait. `CreateNamedPipe` gains `FILE_FLAG_OVERLAPPED`; `ConnectNamedPipe` and
  `callOnFrame`'s read become overlapped, waited with a timeout against D.1's clock, and
  cancelled with `CancelIoEx` on expiry. *Check:* with the timeout configured and a client that
  never replies, `callOnFrame` returns within the timeout instead of never.
- **E.2** The expiry rule (blocker 1): disconnect, record the cause and the elapsed µs, continue
  the game. New `[game] frame_timeout_ms`, default `0` = infinite. *Check:* a deliberately hung
  client ends the connection with a named cause and the game reaches its end screen.

### Stage F — per-bot handle namespaces (defect 7; no blockers)

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
- **F.3** A noninterference regression test, in the shape ADR §4.3 describes: two synthetic
  world-states differing only in what the bot cannot see, diffed through the published plane.
  Runs on Linux against `bwapi-c2`'s fixture. *Check:* the diff is empty for handles; the test
  fails against `main`.

**Two behaviour changes to state in the commit message.** `GameData::initialUnitCount` becomes a
count of *accessible* initial units rather than all of them, and a reference to a unit the bot
has never seen becomes `-1` where it used to be a valid handle. Both are the fix working. Both
will move a bot's behaviour, which is exactly what ADR §9 predicts and what G6's differential
test exists to measure.

### Stage G — the read-only state plane (defect 3; blocker 4)

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

### Stage H — one recorded seed (defect 8; blocker 6)

- **H.1** One `match_seed`: `seed_override` if set, otherwise a value drawn once at DLL init from
  a system CSPRNG, logged. It feeds `srand` (`GameInternals.cpp:296`), `AutoMenuManager`'s
  `mt19937` (`AutoMenuManager.cpp:21-28`) and the `GetSystemTimeAsFileTime` detour
  (`Detours.cpp:111-124`), which is already wired. *Check:* two runs with the same
  `seed_override` pick the same race and the same map from a rotation.
- **H.2** Blocker 6, if accepted: remove `data->randomSeed` (`Server.cpp:497`) and the client's
  `getRandomSeed`. *Check:* the engine seed is in the log and not in the plane.

---

## 4. Ripples into `bwapi-c2`

Each of these is a `bwapi-c2` commit, all of them in the pin bump of blocker 4.

| Stage | What moves here |
|---|---|
| B.3 | `cmake/closure.cmake:15` lists `AIModule.cpp`; it goes. `tools/abi/audited-headers.txt:63` excludes `BWAPI/AIModule.h` "as a scoped v2 item" — the exclusion becomes permanent and its comment changes |
| C, F | New noninterference and validator suites are Linux-native and belong beside `tests/`; F.3 needs the `Fixture` builder |
| D.3, G.1 | `tests/layout_dump/baseline.json` changes: new fields, then a split struct. The `GameData` size in `implementation-plan.md`'s phase-0 exit criterion (33,017,048) changes with it |
| F.1 | Fixture invariant 4 — "`onMatchStart()` fills only `accessibleUnits` from `initialUnitCount`" — still holds, but `initialUnitCount` now means something narrower. The comment in `tests/fixture/fixture.h` says so |
| G.1 | The fixture builds one `GameData`; it now builds a pair. Every suite that reaches `data->unitCommands` to assert an emitted command reaches the command plane instead |
| Blocker 5 | Two new `Game` declarations; the audit's `Game` row goes 179 → 181 and `docs/pins.md`'s audit table is rerun |
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
| 7 | Dense per-bot handles and no handle for an unseen unit | The rest of §4.3's residual surface: derived quantities, fog, engine randomness, response timing |

Defects 2, 5, 6 and 8 are fixed outright. That is four of eight closed and four narrowed, which
is a fair statement of what a fork of the server side buys before anything is inverted.
