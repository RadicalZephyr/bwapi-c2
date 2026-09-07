# ADR 0001. Fork and invert BWAPI, not a greenfield rebuild

> **Status: Draft.** Nothing here is scheduled and nothing supersedes
> [the implementation plan](../implementation-plan.md); phases 2–4 proceed on client mode
> unchanged. This ADR records the goals of a tournament-grade successor to BWAPI and the
> architecture chosen for it, so that the question is not re-opened from scratch. Five open forks
> (§6) and four experiments (§8) block a final decision. Supersedes nothing; extends
> [`possible-alternatives.md`](../possible-alternatives.md) and
> [`proxy-protocol-design.md`](../proxy-protocol-design.md), whose §6.2 this ADR answers.

## Context

The opening question was the maximal one: throw out BWAPI and rebuild from the StarCraft
memory-injection hacks up, keeping a stable C ABI, aimed at automated fair tournaments, with
unlimited time and budget. Four primary goals were proposed — tournament-runner robustness
against malicious or buggy bots, fairness within a match, minimum latency, and any host language.

A fifteen-agent review measured the premises against BWAPI's source, the three live competitions'
rule sets and code, seven comparable game-AI competition APIs, and the relevant case law. **Three
of the four proposed goals turned out to be mechanisms rather than properties, the latency goal
optimised a quantity the enforcing clock cannot resolve, and the rebuild's stated motivation —
version fragility in the injection layer — inverts on inspection.** What survived is a smaller
project with a sharper property.

The material conclusions are recorded here. The measurements are in
[R12](../research/r12-proxy-transport.md) and in §7 below; the numbers that are *modelled* rather
than measured are marked as such, because two of them were load-bearing in earlier drafts of this
argument and should not be again.

---

## Decision

> **Fork BWAPI, invert the trust boundary, and put a referee outside the game. Leave
> `BW/Offsets.h` alone.**
>
> The bot never shares an address space with the game. A referee process owns the only
> authoritative view, publishes a per-bot projection over a per-bot handle namespace, meters
> CPU-time via cgroups, enforces deadlines, adjudicates, and emits a replayable match record. The
> game stays retail Brood War 1.16.1 under a patched Wine on Linux. The engine, the injection
> layer and the reverse-engineered memory model are inherited, not rewritten.

---

## 1. The goals

Seven, each stated so that it can be checked rather than felt. Where a goal replaces one of the
four originally proposed, the original is named.

**G1. Noninterference.** Nothing observable by bot A may depend on anything A is not entitled to
see. *(Replaces "all bots have the same capabilities available to them".)* Symmetry is the wrong
predicate: the most symmetric arrangement available is to give both bots in-process module mode,
which hands each of them the opponent's minerals and the full 1,700-entry unit table at addresses
the library publishes in its own headers. What is wanted is enforceable boundedness, and
noninterference is its name. It is also testable — §4.3.

**G2. A bounded, attributed, published resource envelope.** Every bot gets the same cores, memory,
disk quota and device access; no bot obtains compute the referee did not grant; every charge is
attributed to the party that incurred it; and **the envelope is published to the bot**, because a
bot cannot structure itself against a budget it cannot read. *(Replaces "all bots run locally" and
"the same time budget for compute per-frame".)* Locality is a mechanism and the wrong one — two
bots co-tenant on one host contend for L3 and memory bandwidth, so co-tenancy is itself a fairness
bug, while a bot on a dedicated cpuset is more equal than two bots sharing a machine.

**How a bot spends its envelope is its own business.** Threads and subprocesses are a legitimate
architecture — and for any interpreter with a GIL, a subprocess is the *only* way to use a second
core. They carry coordination costs and are not a free win. **No rule restricts them, which is
also what every current ruleset does** (§7). What is bounded is the envelope, and it is bounded
structurally by `cpuset.cpus`, `memory.max`, `pids.max`, a device cgroup and an empty network
namespace — not by a rule anyone has to police.

**G3. Every match is a replayable, auditable artifact.** Seed, order streams, per-frame budget
charges, every referee decision, and — recorded but not enforced (§5 decision 2a) — per-frame
CPU-µs and retired instructions, in a bundle that re-simulates bit-exactly without the bots.
*(New. Academia is the stated audience and fairness that cannot be demonstrated is worth little
when a competitor disputes a result.)* Note the honest bound: this is *replay* reproducibility.
*Re-run* reproducibility — same bots, same seed, same game — is not a goal (§5, decision 4).

**G4. The runner never blocks on, and never trusts, bot-controlled data.** Every input validated
on the trusted side; every unbounded resource behind a deadline the runner enforces and can
attribute; a hang distinguishable from a crash; and an adjudication rule that names a cause.
*(Sharpens "a malicious or buggy bot must not be able to bring down the tournament runner or its
opponent's bot" — "must not bring down" is the symptom, not the property.)*

**G5. Any host language, without gratuitous penalty.** Bulk snapshot reads, batched capability
queries, a schema published as a first-class artifact, and a budget shape that does not punish a
managed runtime for being one. *(Keeps goal 4 as written and narrows its non-goal — see below.)*

**G6. Migration comparability, stated falsifiably.** A tournament can run a full round with N of
the current top 20 bots either unmodified or mechanically ported, and a differential run — same
bots, same maps, both stacks — produces outcome distributions that are statistically
indistinguishable. *(New.)* Without that test the new stack may have silently changed the game,
and every fairness property is then being asserted about a different sport.

**G7. Bounded maintained surface, and survivable knowledge.** Everything derivable is generated —
type data from `units.dat`/`weapons.dat` rather than hand-transcribed constants, offsets from
signature scanning rather than absolute addresses, bindings from the schema — and there exists a
written, *executed* procedure for re-deriving the memory model from the game binary. *(New.)*
BWAPI has had one functional commit since 2019-03-07 and one effective maintainer. The system
specified here is strictly larger than BWAPI. Unlimited budget is a premise about year one; year
six is one part-time maintainer, and that is how projects of this shape actually die.

### The non-goal, narrowed

**Equalising the fundamental suitability of a language to meet a deadline is not a goal.** It
remains correct as originally stated, and it is narrowed here because as written it excuses too
much: it must not license a design choice that *gratuitously* punishes a language whose
fundamental suitability is fine. A hard per-frame wall-clock deadline and a per-field read API are
both cases where the design, not the language, is the deciding factor.

### And what latency is now

Not a goal. A constraint inside G2 and G5, **anchored to the shortest supported frame and to
games-per-hour, not to 42 ms.** The supported frame set is {0 unthrottled, ~1 ms headless, 20 ms
SSCAIT, 42 ms Fastest}. At 42 ms a 28 µs handoff is 0.07% and invisible to a `GetTickCount` clock
whose granularity is ~15.6 ms. At ~1 ms headless — the regime BASIL uses to play a million games,
and the regime self-play training needs — the same handoff is 6% and library overhead is the
throughput determinant. An earlier draft of this argument deleted the goal by dividing by 42 ms
every time. That was wrong.

---

## 2. Why not a greenfield rebuild

**The stated motivation inverts.** The reason to go down to the injection layer is its fragility:
101 hardcoded absolute addresses in `BW/Offsets.h`, four more embedded in `__declspec(naked)`
inline assembly under a comment reading *"PLEASE LOOK AWAY. THIS IS SO BAD AND GROSS AND
DISGUSTING AND SHAMEFUL"* (`bwapi/BWAPI/Source/Assembly.cpp:6-7`), sixteen version-gated code
patches, eight `storm.dll` detours bound by *ordinal* rather than name
(`CodePatch.cpp:61-68`), and a `DllMain` that performs ~19 `VirtualProtect`+`memcpy` patches under
the loader lock. All true, and all inert: **1.16.1 is a frozen artifact.** SC:R shipped as a
separate 1.18+ line that BWAPI has never targeted, and every organised competition mandates
1.16.1 — SSCAIT by rule, sc-docker by baked image, AIIDE by install. The risk that the address
table becomes wrong cannot materialise. The rebuild would rewrite the one layer that provably
never has to change, and the reason that layer is ugly is that it is finished.

**What the review actually indicted is a patch series.** Every defect found is an implementation
defect in a permissively-licensed tree with no merge-conflict risk:

| Defect | Location | Fix |
|---|---|---|
| The server blocks on the client with no deadline — a hung bot wedges the game forever | `Server.cpp:737-754` (`callOnFrame`; `PIPE_TIMEOUT=3000` governs `WaitNamedPipe`, not this read) | A deadline, and an adjudication rule |
| Client-supplied `commandCount` used as an unclamped loop bound over `commands[20000]` | `Server.cpp:757`, `GameData.h:136,155-156`; the only bound is an `assert` in the *untrusted* process, compiled out under `NDEBUG` (`BWAPIClient/Source/GameImpl.cpp:55`) | Clamp on the trusted side — the correct pattern is already present at `Server.cpp:839` for `unitIndex` |
| The client maps the whole 33 MB state plane writable | `BWAPIClient/Source/Client.cpp:107` — `MapViewOfFile(..., FILE_MAP_WRITE \| FILE_MAP_READ, ...)` | `FILE_MAP_READ` on the state half |
| Anti-cheat is a veto interface inside the bot's own address space | `GameInternals.cpp:273` (`tournamentCheck`), 13 `Tournament::ActionID` values | Move the enforcement point out of the process |
| The meter's *clock* is `GetTickCount()` at ~10–16 ms granularity against a 55 ms threshold — a resolution of three to four ticks | `GameEvents.cpp:523` (module) and `Server.cpp:243` (client) | A monotonic sub-µs clock, and a second timestamp so a bot is not billed for the referee's IPC. **The unit stays wall clock** (§5 decision 2); only the clock is wrong |
| A client bot cannot read its own charged time | `BWAPIClient/Source/GameImpl.cpp:947` returns `0` | Publish the meter |
| Global unit-ID allocation is an information channel | Documented 2018; never banned | Per-bot handle namespaces (§4.2) |
| The C runtime RNG is seeded from the wall clock | `GameInternals.cpp:296` — `srand(GetTickCount())` | Seed from the match record |

Two goals a fork genuinely cannot reach — an OS privilege boundary between bot and game, and a
read-only state plane — are changes to the *server side of the client protocol*: a fork of
`Server.cpp` plus a new client. Still not a rebuild from the injection layer up.

**The series is planned.** [`bwapi-fork-defect-plan.md`](../bwapi-fork-defect-plan.md) sequences
all eight rows into commit-sized steps against the fork's `main`, restates each site as the tree
actually reads it — three rows are wider than this table records — and collects the six decisions
the series cannot make for itself. Four rows close outright; four narrow, and the gap in each is
the referee.

**And the legal asymmetry runs the wrong way for a rebuild.** *MDY Industries v. Blizzard*, 629
F.3d 928 (9th Cir. 2010), held that botting a Blizzard game is breach of contract rather than
copyright infringement, with the DMCA §1201 hook attaching only to circumventing the Warden
anti-cheat. 1.16.1 has no anti-cheat, so injecting into a user-supplied copy has no circumvention
leg. *Davidson & Associates v. Jung*, 422 F.3d 630 (8th Cir. 2005) — the bnetd case — held
Blizzard's EULA enforceable and that agreeing to it waives the DMCA §1201(f) interoperability
exception for reverse engineering; bnetd was shut down. **Reimplementing a Blizzard engine is the
one leg with adverse precedent, and porting injection forward to SC:R would move the project from
the first category into the second.** (Secondary sources; the opinions were not read directly.
This is the decision that should buy an hour of actual counsel before it buys a line of code.)

### The concession

A rebuild is not refuted, it is deferred, and the argument that defers it is an argument about
evidence. Fork-and-invert delivers G1–G5 and G7 against the ecosystem that exists, with the
results record intact. If it ships and the operators adopt it, the case for going lower is then
made with data rather than with a critique of `Assembly.cpp`'s comments.

---

## 3. Why not our own engine, and why OpenBW is not a substrate

**OpenBW is foreclosed as a shipped dependency and remains available as a runtime target.** The
engine has no `LICENSE`, no statement in `README.md`, zero copyright headers, and GitHub reports
`license = NONE`. Issue #17, "Add a LICENSE.md", has been open since 2018-02-26; four people have
asked; the author has not replied and publishes only a GitHub-anonymised address.
[R9](../research/r9-licensing.md) already ruled this a blocker and named outreach as the
precondition. **Outreach is now judged to have been attempted by others and to have failed**, so
the blocker stands. Note the distinction that R9 did not draw: *"we do not distribute OpenBW"* and
*"we refuse to run against OpenBW"* are different promises, and only the first is required. A user
who supplies their own engine process gets one; we ship nothing unlicensed.

**Reimplementing it is worse on every axis.** The silence is probably not apathy — the same author
released `bwheadless` under CC0-1.0, so the habit exists. The likelier reason is that OpenBW is not
clean-room: it reproduces Brood War's exact LCG (`lcg_rand_state * 22695477 + 1`) and carries a
named state field `consider_collision_with_unit_bug` beside a literal `// This is an original bug.
Don't fix`. If OpenBW is a derivative work of `StarCraft.exe`, its author has nothing to grant —
and a reimplementation inherits the problem rather than escaping it, into the *Davidson* category.

Three further costs, each independently sufficient:

- **Fidelity.** OpenBW is the state of the art — nine years, by the person who reverse-engineered
  the binary, deliberately reproducing original engine bugs — and still carries four open
  replay-desync issues (#22 2018, #27 2019, #28 2019, #32 2025). No competition has adopted it in
  nine years. A greenfield engine starts at zero on that axis and has to climb past OpenBW to be
  *usable*. **This is in direct conflict with G6**: an engine that is not bit-accurate invalidates
  every existing bot's tuning.
- **Contamination.** A clean-room defence requires showing that expression was not copied. R6 and
  R7 audited OpenBW's source. Choosing to reimplement would require a specification team who may
  read it and an implementation team who may not — and would *revoke* R9's "read-only reference"
  position rather than preserve it.
- **The MPQs.** OpenBW hardcodes `Patch_rt.mpq`, `BrooDat.mpq` and `StarDat.mpq` and reads eleven
  tables out of them. A reimplementation needs the same data. **A redistributable turnkey stack is
  unreachable on every substrate**, which removes one of the main things a rebuild would buy.

**What the engine would have bought, and what replaces it.** Seed control, whole-state
snapshot/restore, and no Wine. Seed control is dropped with re-run reproducibility (§5, decision
4). Snapshot/restore is a genuine capability — OpenBW's `copy_state` proves it well-defined — but
it is an ML-research goal that is not on this list, and it is left as a capability-namespaced
extension the ABI must not be shaped to forbid. **And Wine is not a liability but the answer**:
production already runs 1.16.1 under Wine on Linux, and Wine is open source. A patched Wine or
winelib host gives, by construction, a virtualised game clock (`GetTickCount`,
`QueryPerformanceCounter`, `timeBeginPeriod`), syscall interception without seccomp,
page-protection control over the game's address space, clean teardown without `DllMain` under the
loader lock, and separate UIDs for bot and game. **The injection layer's replacement is Wine, not
a new engine.** One caution against over-claiming: Wine gives us the *game's* clock, not the
bot's, so it does not make a CPU-time budget deterministic and does not reopen §5 decision 2.

---

## 4. What "invert" means

### 4.1 A referee outside the game — defined by the boundary it owns, not by instance count

**The referee is defined by one property: it is the process that mediates between an untrusted bot
and the game, and the bot reaches the game through nothing else.** How many `StarCraft.exe`
instances sit behind it is a separate, orthogonal question, and this ADR does not settle it.

**v1 topology: one game instance per bot, which is what already runs.** AIIDE and SSCAIT run two
StarCraft instances — one per bot, on separate VMs or containers — playing a LAN game over UDP,
each maintaining its own copy of the lockstep simulation; sc-docker and BASIL do the same with
containers on a bridge network. Every goal in §1 is reachable in this topology. The leak that G1
addresses was never cross-instance — each bot already has its own machine and its own game — it is
that the *local* instance hands its own bot more than it should. The referee fixes that locally,
once per side.

**Which forces a constraint that a single-instance framing would hide.** In this topology the
referee is a *pair* of referee sides, and **no side holds a monopoly on truth**: each game instance
already contains the full state for both players (§4.1.1). So noninterference is enforced
independently on each side, and the coordination path between them — forfeit, adjudication, match
end — is itself a potential channel and must carry only referee decisions, never game state, in
one direction.

#### 4.1.1 Single-instance is an open optimisation, not a claimed capability

An earlier draft of this ADR asserted a single game instance serving both competitors, and with it
halved game-side CPU and the elimination of desync. **That was a capability claim about retail
Brood War stated as a settled consequence, and it is withdrawn.** It splits into a verified half
and an open one.

**The read half is feasible and verified.** BW's tile data carries fog for every player
simultaneously as bitmasks; BWAPI narrows it to the local player and nothing else does:

```cpp
const u32 playerFlag = 1 << BroodwarImpl.BWAPIPlayer->getIndex();
data->isVisible[x][y]   = !(tileData.bVisibilityFlags & playerFlag);
data->isExplored[x][y]  = !(tileData.bExploredFlags  & playerFlag);
```
— `bwapi/BWAPI/Source/BWAPI/Map.cpp:79-86`

So one instance already holds ground truth *and* both players' fog at tile granularity, and
projecting two per-bot views from it is `playerFlag` with a different index. This is also why
`GameData` carries a singular `isVisible[256][256]` (`GameData.h:99-100`): the narrowing is
BWAPI's, not the engine's.

**The write half is open.** Issuing orders on behalf of a non-local player. BWAPI's command path is
bound to `BWAPIPlayer`, and in live lockstep play the opponent's turn arrives over the wire from
another client. Replay playback proves the engine will *consume* a multi-player command stream,
but that is a different code path, and nobody has traced whether a turn can be injected for a
second player from one process. On OpenBW this is `execute_command(player, cmd)` and trivial;
on retail it is R13.5 (§8).

**What it would buy, and therefore how much it matters: throughput and the removal of desync.**
Neither is a goal in §1. It is an optimisation to be decided on evidence, and the ADR's analysis
does not rest on it.

The referee is also the only place a supervisor can stand. This is the strongest structural
argument in the design and it is confirmed by prior art: Sc2LadderServer can enforce anything only
because SC2's transport is a socket it can interpose on. BWAPI client mode is a mapped 33 MB view
plus a named pipe, which is far harder to interpose. **The referee is not a faster transport; it
is the supervision point.**

### 4.2 Per-bot projections over per-bot handle namespaces

**Handles are capabilities, not identities.** This directly contradicts §1.3 of
[the plan](../c-abi-plan.md), which is correct for a wrapper and wrong for a referee. The unit-ID
exploit — subtract two of your own consecutive unit IDs to learn how many units the opponent
produced in between, recognising a 4-pool without scouting — exists because BWAPI hands bots the
engine's global IDs, and the *gaps* are the channel. Dense per-bot allocation makes it structurally
impossible. A `(slot, generation)` pair leaks identically if generations are global. **Unit IDs are
one instance of a class**: bullets, sprites, images and orders leak the same way, and the plan's
§6.3 already exposes bullets.

### 4.3 Noninterference is testable, and that is the point

For every value in A's view, ask whether it could differ between two world-states that are
equivalent from A's perspective. Mechanically: construct two states differing only in what A
cannot see, run the referee, diff A's view. Any difference is a leak. That is a fuzzing harness
and a regression test for every future ABI addition, which is what matters — leaks are added one
convenience function at a time.

The residual surface it has to cover, none of which is closed by out-of-process plus handle
virtualisation alone:

- **Derived quantities.** A path query answered against ground truth reveals whether an unscouted
  building blocks it. SC2 hit this: `RequestQueryPathing` and `RequestQueryBuildingPlacement` must
  be evaluated against the asking player's knowledge. BWAPI's 68-predicate `canXxx` family is full
  of it.
- **Fog of war.** Vision radius, detection for cloaked and burrowed units, building snapshots that
  persist after vision is lost, terrain-height blocking, spider mines. Too permissive leaks; too
  strict breaks bots. BWAPI implements this today so it is a known quantity, but the referee now
  owns it, and it is the largest correctness surface behind G1.
- **Engine randomness.** No bot access. OpenBW instruments `random_counts` per call site precisely
  because draw counts correlate with events.
- **Response timing.** Lockstep — advance only when both have submitted, release both views
  together. The referee must expose nothing derived from the opponent's timing, including a
  remaining-latency or frame-skip signal.
- **Persistent storage as a mediated capability, not a directory.** ~100 MB, path-fenced,
  `read/` repopulated from last round's `write/`. If the bot has a filesystem, G1 has a hole the
  size of the filesystem.
- **A network namespace with no interfaces**, or "no external compute" is unenforced.
- **Shared-hardware side channels.** `cpuset` pinning and separate NUMA nodes get most of it. The
  remainder is scoped out explicitly rather than assumed away.

### 4.4 Partition XOR meter

An earlier draft of this ADR called background-thread work a loophole and chose a CPU-time budget
to close it, "along with a spinning waiter paying for the core it burns, and a garbage collector
being billed to the bot that chose the runtime." **That was wrong in both halves, and the rule
that replaces it is one line:**

> **Partition the cores and meter wall clock, or share the cores and meter CPU-time. Never both.**

**If a bot has N cores for T seconds, its compute is already bounded at N×T.** Charging CPU-seconds
on top is a second, redundant constraint that taxes a bot 4× for using the four cores it was
given, and rewards the single-threaded bot that leaves three idle. That is not fairness; it is a
tax on using your own allocation, and it falls hardest on exactly the runtimes G5 exists to serve,
since a GIL interpreter must spend a subprocess to use a second core at all.

**All three "exploits" that argument closed were artifacts of the shared-core model.** On a
dedicated cpuset, background threads are engineering rather than laundering; a spinning waiter
burns its own core; a stop-the-world GC pause is wall time and is charged either way, while a
*concurrent* collector on the bot's own core is not something to tax. CPU-time is a fair-share
mechanism, and fair-share is what you need when cores are contended. Partitioning removes the
contention instead of accounting around it, which is the better fix.

**Two of the objections to wall clock dissolve with it.** R12's 4–8 ms preemption tail came from
contention — its own conclusion is that "with a core available per process, spin: 0.25 µs and a
bounded tail." And re-run reproducibility, the other objection, is already not a goal (§5,
decision 4).

**What remains genuinely enforceable is the envelope, not the schedule**: cores by `cpuset.cpus`,
memory by `memory.max`, no GPU by a device cgroup, no second machine by an empty network
namespace. Each of those is structural. None of them is a rule about how the bot arranges its own
threads.

---

## 5. Decisions closed

| # | Question | Decision | What it costs |
|---|---|---|---|
| 1 | Greenfield rebuild, or fork? | **Fork and invert.** §2 | The rebuild's freedoms; the argument is deferred, not refuted |
| 2 | Budget unit: wall clock, CPU-time, or deterministic? | **Wall clock, on a dedicated cpuset** — modelling what tournaments already enforce, and making it sound by removing the contention that undermines it. §4.4's partition-XOR-meter rule. *(Reversed: an earlier draft chose CPU-time to close a "background-thread loophole" that is not a loophole once cores are partitioned. Reverting also deletes the G5 penalty that draft had to name as its cost — wall clock does not charge a Python bot for interpreter dispatch any differently than it charges a C++ bot for its work.)* | Wall clock is not machine-independent and not reproducible. Both are accepted: §5 decision 4 already dropped re-run reproducibility, and G3's artifact carries the charges |
| 2a | What is recorded, versus what is enforced? | **Meter everything, enforce one thing.** CPU-µs and retired instructions per frame go into G3's match artifact as *observations*. A season of records then answers empirically whether multi-threaded bots dominate in a way that tracks core usage — which is the evidence that would justify changing the unit, and it is cheaper than guessing now | Two numbers to carry that nothing currently consumes |
| 3 | Deterministic (bytecode/instruction) budget? | **Foreclosed as a guarantee.** Battlecode's totality is bought by owning the JVM: an instrumenting classloader, `RobotMonitor.incrementBytecodes()`, and a hand-maintained cost table (`System.arraycopy` = 1/element, `NEWARRAY` = length) because bytecode counts are not proportional to real cost. A multi-language C ABI owns no runtime. Hardware instruction counting needs no cost table and is *not* strictly foreclosed, but it is microarchitecture-dependent and penalises JIT warmup, so it buys far less | The strongest available fairness guarantee |
| 4 | Seeded re-run reproducibility? | **Not a goal.** `Game::getRandomSeed()` exists with no setter anywhere in BWAPI; academia lives without it today. G3's replay artifact carries the weight | "Same bots, same seed, same game" is never claimable without a caveat |
| 5 | Budget shape | **Three mechanisms, not one number** — see §5.1. A previous draft welded liveness, throughput and fairness into a single per-frame budget, which is why it kept producing rules that punish a runtime for being one | Three things to explain instead of one |
| 6 | Deadline expiry: drop-and-continue or forfeit? (**answers `proxy-protocol-design.md` §6.2**) | **Both, at different layers** (§5.1): the referee deadline **drops the frame**, the excess-time bank **forfeits**. §6.2 posed this as one question with two answers; it is two questions with one answer each | Drop-and-continue makes match outcome depend on host load every frame rather than at one rarely-binding cliff — a real, accepted regression against G3, mitigated by recording the charges |
| 7 | Deadline shape | **Two numbers, not one.** Every system supervising untrusted bots uses an init deadline 10–150× the steady-state one: Halite II 60,000/2,000 ms; Sc2LadderServer 300,000/20,000 ms; CodinGame 1,000/100 ms; RLBot gates match start on the bot's own `InitComplete`. The ABI needs a ready signal distinct from "finished frame 0" | Two knobs to misconfigure |
| 8 | The forfeit path | **Surrender on the bot's behalf**, so the game ends cleanly, the result is recorded and the replay is flushed — Sc2LadderServer's `ExitCase::BotStepTimeout`. Not merely "stop waiting" | — |
| 9 | Substrate | **Retail 1.16.1 under a patched Wine on Linux**, engine a runtime seam. §3 | Wine is now a maintained component |
| 10 | Own engine / OpenBW as a shipped dependency? | **No.** §3. OpenBW stays supportable as a user-supplied runtime target, and is valuable as a differential *oracle* (§8) | Snapshot/restore deferred to a capability namespace |
| 11 | Transport | **Not the lever, and settled three times independently.** RLBot had shared memory under DLL injection and deliberately replaced it with sockets + FlatBuffers, citing language bindings; Blizzard never considered shared memory and shipped protobuf over a WebSocket; R12 measures a socket round trip at 0.05–0.08% of a 42 ms frame. Choose for supervisability, not speed. If a framed protocol ships, **a 32-bit length prefix** — RLBot's 16-bit prefix caps a message at 65,535 B, and 1,700 units is 571 KB | The last microseconds |
| 12 | Read API shape | **Bulk snapshots and batched capability queries.** SC2 collapses the whole `canXxx` problem into one `RequestQuery` carrying repeated pathing, ability and placement questions, with the rules staying inside the game. The one-shot/per-frame partition is *forced*, not chosen — 33 MB whole-state is arithmetically impossible per frame, and SC2, RLBot and BWAPI's `GameData` (82.9% one-shot, R12 §1) converged on it independently. Make it a documented ABI rule so future additions land on the correct side | — |
| 13 | Error channels | **Two, not one.** SC2 returns `ActionResult` synchronously from `RequestAction` *and* `action_errors` on the next observation. "The game rejected the order you issued last frame" is a different thing with different timing from an ABI error code | A second surface to drain |
| 14 | End conditions | **Never conflated.** Game-over, bot-disconnected, left-game and supervisor-forfeit must be distinguishable at the ABI. Gymnasium's most consequential API change was splitting `done` into `terminated` and `truncated` after shipping a magic `info['TimeLimit.truncated']` workaround | — |
| 16 | Action rate: is unbounded APM within G1? | **A tuned knob, defaulting to unbounded** (§5.2). Unbounded is a valid setting; a human-achievable cap is a valid setting and a difficulty slider for bot-vs-human; the interesting middle is a cap that changes the question from *how much micro can I do* to *what are the highest-value commands to spend a scarce budget on* | The default is **forced by G6**: existing bots are tuned for unbounded APM, so the differential test is only meaningful at that setting |
| 17 | Opponent identity: do bots learn who they are playing? | **A tuned knob, defaulting to the name being visible** (§5.2), with **three settings, not two**: named, stable-pseudonymous, and blind. Named mirrors pro-level scouting and preparation; blind forces the uniquely bot-shaped problem of recognising a build programmatically rather than keying it to a name | Results are only comparable within a setting, so the setting belongs in G3's artifact |
| 15 | Divergence policy | **libmelee's rule.** Fix the game's *inconsistencies*; refuse to abstract away load-bearing *weirdness*, and state the failure case that proves it. Turns §15 from a register into a policy | — |

### 5.1 Three mechanisms, three jobs

The single per-frame budget that every current tournament runs is doing three unrelated jobs at
once, which is why it keeps producing rules that punish a runtime for being one. Separated:

| Mechanism | Protects | Expiry |
|---|---|---|
| **Referee deadline** — bounded wait, substitute empty orders, never block | The runner: liveness | **Drop the frame.** Subsumes AIIDE's 1000 ms × 10 and 10000 ms × 1 tiers entirely |
| **Excess-time bank** — microseconds over a per-frame allowance, game-long | The operator's season: throughput (§7) | **Forfeit.** Exhausting a game-long bank is never transient |
| **Dedicated cpuset and envelope** — cores, memory, devices, network | Fairness | **—** Structural, not temporal. There is nothing to expire: it is an allocation, not a budget |

**The third row is the point.** With cores partitioned and no realtime clock to be late against,
fairness needs no budget at all — it needs an allocation that is impossible to exceed. That is the
same structural-versus-policed distinction G1 rests on, applied to compute. It also means **§6 fork
5 is load-bearing**: if operators will not pin cores, fairness loses its mechanism and the argument
for wall clock (§5 decision 2) goes with it.

**What this buys the runtimes G5 serves.** A GC pause costs a bot a few frames of micro rather than
a resignation, because the referee simply advances without it. The bank bites only the bot that is
*uniformly* slow, which is the operator's legitimate complaint and not a language penalty.

**And the honest cost.** Missing frames in Brood War is not the benign thing it is in a game where
inputs persist harmlessly: BW orders are sticky, so a late bot keeps executing its last orders
rather than idling, and because many bots re-issue commands every frame — the reason
`getLastCommandFrame` exists — a missed frame can occasionally *improve* execution. So degradation
is noisy rather than proportional, and "a slow bot loses on merit" is a tendency, not a guarantee.
A bot getting a decision every third frame is also at a permanent 3× action-rate disadvantage on
top of BW's existing 2–3 frame order latency. **This is better-shaped punishment, not the absence
of punishment**, and it should not be sold as the latter.

### 5.2 Two knobs the tournament owns

Decisions 16 and 17 are not defaults dressed as choices. They are settings an operator picks per
tournament, and they share three properties: **symmetric** — both bots in a match always get the
same setting; **recorded** — the setting is part of G3's artifact, because results are only
comparable within it; and **defaulting to today's behaviour**, which is forced rather than
conservative, since G6's differential test is only meaningful at the setting existing bots were
built for.

#### Action rate

**The mechanism is a token bucket on *actions*, held by the referee, with overflow rejected as a
readable error** — not throttled observations. The distinction matters and the prior art makes it
concrete: DeepMind's `step_mul` throttles *observations* (20 ≈ 50 APM, 5 ≈ 200 APM) because PySC2
wanted human-like **reaction time** as well as human-like rate. Capping actions alone forces
**choice** while leaving reactivity intact, and choice is the property that makes the knob
interesting. A bucket with burst capacity also matches how humans actually play — bursting in a
fight — and it is the same allowance-plus-bank shape as §5.1's middle row. The bucket refills on
**game frames, not on bot replies**, or a bot that misses frames under §5.1 is punished twice.

**The ripple: an action-rate cap promotes grouped commands from a documented gap to a blocker.** A
human moves twelve units with one action — select, then click. A client-mode bot must issue twelve
individual commands, because BWAPI's server does not implement grouped commands for client bots
(`GameImpl::issueCommand(const Unitset&, ·)` is annotated `//FIX FIX FIX naive implementation`; the
seventeen `canXxxGrouped` declarations are suppressed for this reason). Calibrate a cap against a
human baseline and measure it per command, and every client bot pays a ~12× penalty for a
limitation that is an implementation gap rather than a game rule. **So the knob is not meaningful
until the referee owns the command path and can represent a grouped command as one action.**

**One correction to the argument for the knob.** Capping APM does not, by itself, make expressive
languages more viable. It *removes* a pure-throughput advantage, which is a real reduction in the
edge a fast language has. But it also raises the value of deliberation per action, and deeper
deliberation rewards whoever computes more per action — which cuts the other way. What actually
helps a Python or C# bot is the **pair**: a capped action rate together with §5.1's treatment of a
slow frame as lost micro rather than a resignation. Neither knob does it alone.

#### Opponent identity

**Three settings, because the middle one is the research-interesting case.** *Named* is today's
behaviour and mirrors pro-level preparation. *Blind* forces programmatic build recognition. But
*stable-pseudonymous* — a per-opponent identifier that is consistent across a season and carries no
information about who the opponent is — lets a bot adapt online while making it impossible to ship
hardcoded counters against a named rival. That is a different experiment from either neighbour and
probably the sharpest one.

**Identity leaks through channels the knob does not close**, and one of them is load-bearing. Race
is visible and many bots are single-race; starting position and map leak weakly; and the build
order is observable, which is the entire point. **The coupling that matters is persistent
storage** (§4.3): a bot in blind mode can fingerprint an opponent from observed play and key its
own `write/` directory on that fingerprint, rebuilding per-opponent learning from scratch over a
season. In blind mode that is precisely the intended research problem. But it means the knob is
under-specified until storage semantics are decided — under *stable-pseudonymous* the referee
supplies the key, under *blind* the bot must derive it, and under *named* it is free. §6 fork 7.

---

## 6. Open forks

| # | Fork | Why it is open |
|---|---|---|
| 1 | **Co-tenancy scope.** One bot per machine, per cpuset, or per NUMA node? | §4.3's last bullet. Decides how much of the side-channel surface is in scope and what a rack costs |
| 2 | **Realtime rendering, and SSCAIT.** | SSCAIT exists because it is watchable: `LocalSpeed 20` with rendering on, and its first timeout tier raised from 55 ms to 85 ms *because* it renders. A headless non-realtime referee cannot serve it. Three competitions, roughly three operators — and the project's no-outreach constraint is currently a background condition rather than what it is: **the largest risk to a system whose entire purpose is adoption by three named people** |
| 3 | **What a late bot sees when it comes back.** | Choosing drop-and-continue (§5.1) makes this live rather than hypothetical: after missing *k* frames, does the bot resume at *N* and fall progressively further behind, or skip to the latest state? Skip-to-latest is almost certainly right — it makes each bot observe the game at whatever rate it can sustain — but it has to be stated, because "never block" without it degrades unboundedly rather than gracefully. Battlecode answers the same question with pause-and-resume; out-of-process we answer it with a staleness rule |
| 4 | **Is the excess-time bank needed at all?** — *for the tournament operators* | §5.1's middle row is the one held loosely. It is the only thing standing between a season and a bot that takes 500 ms per frame forever, never stalls and never trips the deadline. But **BASIL has run without any per-frame limit for over a million games**, which is real evidence against it, and it needed only a discretionary anti-gaming rule to cope. An operator settles this in one message |
| 5 | **Does each bot get a dedicated cpuset?** — *for the tournament operators* | Decision 2 rests on it and §5.1 makes it fairness's whole mechanism, and **no current tournament does it**: AIIDE and SSCAIT give a VM per bot but run two VMs unpinned on one host, and BASIL bounds CFS *bandwidth* via Docker `nano_cpus` rather than pinning. It changes what a rack costs and how many games run in parallel. This is an operator question, not a design question, and it should be asked rather than assumed |
| 6 | **What counts as one action, and what is the cap's number?** — *for bot authors* | Decision 16's knob has no unit yet. BW pros run roughly 200–400 raw APM, but Brood War is a spam-heavy game and effective APM is far lower, so "twice a pro" is ambiguous by about 2× before anyone argues about the multiplier. And "one action" needs defining against redundant re-issues and against grouped commands (§5.2) |
| 7 | **Persistent storage semantics under blind play.** | Decision 17's blind and stable-pseudonymous settings are under-specified until §4.3's storage capability is: whether `read/`/`write/` is keyed per opponent by the referee, derived by the bot from a self-computed fingerprint, or global. Blind play without an answer here is a half-measure |

---

## 7. The numbers this rests on

Marked, because two modelled numbers were load-bearing in earlier drafts of this argument.

| Quantity | Value | Status |
|---|---|---|
| AIIDE per-frame tiers | 55 ms × 320, 1000 ms × 10, 10000 ms × 1; cumulative over the game; frames 0–9 exempt; breach calls `leaveGame()` | **Verified** — tournament module source |
| Game length cap | 85,714 frames (AIIDE/CoG), 86,400 (SSCAIT); winner at cap by kills + buildings + razings + gathered minerals + gas | **Verified** |
| The enforcing clock | `GetTickCount()`, ~10–16 ms granularity, against a 55 ms threshold | Mechanism **verified**; the granularity figure is **recalled**. Whether `StarCraft.exe` or Wine calls `timeBeginPeriod(1)` is **untested**, and if either does the clock is 1 ms |
| AIIDE game speed | `localSpeed 0`, `frameSkip 256` — as fast as the machine allows. **The 55 ms is an absolute compute budget, not a keep-up-with-realtime deadline** | **Verified** |
| BASIL limits | 1.2 CPUs, 2 GB memory per container; 1800 s realtime timeout; **no per-frame limit at all**, "due to technical limitations" | **Verified** for the container limits; the rules text via search snippet |
| Crash detection | `gameState.txt` written every 360 frames; the Java client declares a crash at 60,000 ms of staleness. Timeouts above 60,000 ms are therefore inert | **Verified** |
| Client-mode frame handoff | 0.25–28 µs | **Verified** (R12 §2), Linux, 4-core VM |
| One bulk snapshot, 400 units | 2.8–2.9 µs | **Verified** (R12 §2) |
| Per-field FFI reads, 400 units × 20 fields, ctypes | 2.70–3.49 ms | **Modelled.** The read pattern is invented and ctypes is the pessimistic binding. The ~1000× *ratio* survives any workload; the "6–8% of a frame" share does not, and it was promoted to a fairness property on that basis |
| Scheduler preemption tail under contention | 4–8 ms | **Verified** (R12 §3) but **disclaimed by its own author** as a scheduler property that may not carry to Windows, on a VM with a ±25% noise floor |
| `GameData` partition | 33 MB, 82.9% one-shot, 12.5% per-frame, 4.6% upstream | **Verified** (R12 §1) |
| Bots / games at stake | AIIDE 2017: 28 bots, 41,580 games, 14 VMs, two weeks. SSCAIT 2022/23: 57 bots, 3,192 games. BASIL: >1M games | **Likely** — papers and operator statements |

**What the three AIIDE tiers are actually for, which is not one thing.** The 10000 ms × 1 and
1000 ms × 10 tiers are anti-stall, and under a referee that never blocks they are subsumed by the
deadline. The 55 ms × 320 tier is neither anti-stall nor fairness: AIIDE runs `localSpeed 0`, so
there is no realtime clock to be late against and no opponent being made to wait. **It is operator
throughput.** A bot averaging 50 ms/frame over 85,714 frames spends 71 minutes of pure bot compute
per game, and AIIDE 2017 ran 41,580 games on 14 machines in two weeks. That is the constraint the
tier protects, and naming it changes what should replace it.

**And its currency is incidents, not time, which is the mis-quantisation.** A 56 ms frame and a
999 ms frame each cost exactly one of 320. Against a `GetTickCount` clock resolving 55 ms to three
or four ticks, a frame genuinely at 45 ms can be *measured* over the line. So a bot sitting near
the threshold is charged full price for measurement noise, while a bot with rare large pauses is
charged the same price for real overruns. **Hypothesis, testable against experiment 1: bots with
unexplained cumulative-quota failures have frame-time distributions clustered just below 55 ms
rather than long tails** — in which case the failure is in the meter, not the bot. A currency of
*microseconds of excess over an allowance* prices both cases correctly. Against a 60 s bank over a
42 ms allowance, versus AIIDE's 320 incidents:

| Bot shape | Cost under 320 incidents | Cost under a 60 s excess bank |
|---|---|---|
| One 300 ms pause | 1 incident = **0.31%** | 258 ms = **0.43%** |
| Frames sitting at 56 ms | 1 each → dead after **320 frames** | 14 ms each → dead after **4,285 frames** |
| Frames sitting at 60 ms | 1 each → dead after **320 frames** | 18 ms each → dead after **3,333 frames** |

Rare large pauses cost about the same under either currency. **Marginal frames are 10–13× more
tolerated under a time currency** — and marginal frames are exactly what a quantised clock
misreports.

---

## 8. Experiments this ADR is blocked on

Numbered as R13. Two need nothing but a download; one is source reading; none needs a
running StarCraft to begin.

0. **Ask three people.** One operator and two bot authors, in an afternoon, settle §6's forks 4
   (whether the excess-time bank is needed), 5 (dedicated cpusets — which §5.1 makes load-bearing
   for fairness), 6 (the action-rate cap's unit and value) and 7 (storage under blind play). **Forks 1 and 2 came back settled the moment one
   was asked**, which is the argument: this ADR has blocked on community judgement four times, and the no-outreach constraint has stopped being an adoption
   risk and started costing design decisions. It is cheaper than every experiment below and it
   gates more of them. Do it first.

   One question is already specific enough to ask verbatim: **do the bots that fail the cumulative
   quota have frame times clustered just below 55 ms, or long tails?** §7's hypothesis says the
   former, which would make those failures a measurement artefact of `GetTickCount` rather than a
   property of the bot — and would mean the fix is a clock, not a budget.
1. **Parse BASIL's `frames.csv`.** Over a million games of per-frame times are public and the
   review cited the file's existence three times without opening it. Every budget number in §5 —
   tier heights, ceiling, bank size, whether 55 ms is generous, what p99.9 looks like — is
   currently designed against a distribution nobody has looked at. **Cheapest experiment
   available; do it first.**
2. **Instrument a real bot's read pattern per frame.** Retires the one modelled number in §7 and
   decides whether mandating a bulk path in every binding is a fairness property or
   over-engineering.
3. **Logical frames per second, headless, same machine, same map, same bot:** retail 1.16.1 under
   Wine via `bwheadless` versus a user-supplied OpenBW `BWAPILauncher`; plus per-match startup
   cost for each. No public benchmark exists. R12 assumes "~1 ms frames" with no citation, and
   that number is load-bearing and unowned.
4. **Can a turn be injected for a non-local player on retail 1.16.1?** (§4.1.1's open half.) Trace BWAPI's command path from `Server::processCommands` down to BW's turn queue
   and establish whether the player index is a parameter or a premise. Decides single-instance,
   and the read half is already verified, so this is the whole question. On OpenBW it is
   `execute_command(player, cmd)`; the experiment is only about retail.
5. **OpenBW as a differential oracle.** It reads real `.rep` files natively and ships
   `std::array<int,0x100> random_counts` — per-call-site RNG draw counters designed for
   frame-by-frame comparison. Run the SSCAIT/BASIL replay archive through both engines and diff.
   This converts "OpenBW is 99.x% accurate" from an adjective into a number at zero licensing
   exposure, and it is **the same harness G6's differential test needs** — so it is worth building
   under either substrate decision.

Also unverified and decision-relevant, recorded so it is not quoted as settled: the cross-instance
shared-memory read was never demonstrated; the `units[10000]` overflow's mechanism is verified but
its reachability was never counted; patching the tournament module's `onAction` from a module bot
is asserted and never tried, and it is the sole basis for "module mode is honor-system by
construction"; and the claim that a bot can bank a frame lead and crash to win **is wrong** — BW is
lockstep, both instances execute the same frames. What exists there is a quantisation race on
`gameState.txt`'s 360-frame write interval, a coin flip nobody can steer. BASIL's
mutual-crash-is-a-draw incentive is genuine and survives.

---

## 9. Consequences

**What this buys.** Every goal in §1 except G3's re-run caveat, against the ecosystem that exists,
without touching a hardcoded offset. The supervision gap that neither BWAPI mode can close — a
hang distinguishable from a crash, with a cause — closes. The background-thread hole closes for
free. The unit-ID channel closes by construction.

**What it costs.** A fork of `Server.cpp` and a new client, a referee, a patched Wine, an OS-level
resource envelope, a match-record bundle, and a conformance suite — on top of the ABI already
planned. That is a strictly larger maintained surface than BWAPI, which is why G7 is a goal and
not a footnote.

**What it breaks, and this should be said plainly.** Bots that thread work off the callback, that
read global unit IDs, or that touch the filesystem outside `bwapi-data/` will behave differently
or lose. That is not a defect in the migration path; it is G1 doing its job. **"As easy as
possible" therefore means *bots compile and run*, not *bots score the same*** — and G6's
differential test is what makes the difference measurable rather than arguable.

**What survives from the existing plan.** The §4 ABI conventions, unchanged — they held across two
libraries with different object models and nothing here disturbs them. The spec-driven generator
and `api.json`, whose status as *the contract* should get louder: both greenfield systems in the
prior art (`s2client-proto`, RLBot's `flatbuffers-schema`) separated schema from implementation
into their own repositories and both got community bindings in five-plus languages their
maintainers did not write. The static type data. The licensing analysis. And the synthetic-fixture
test substrate (R7, R11.6), which is now the only substrate that needs neither Blizzard's MPQs,
nor Wine, nor an unlicensed engine, nor a redistributed game image — **promoted from a testing
convenience to a stated goal under G7.**

**What it contradicts.** §1.3's identity-is-the-ID handle model (§4.2). §2's non-goal 3, which
makes Windows the v1 target on market grounds — the ground is now also that 1.16.1 under Wine on
Linux is where the volume, the cgroups and the legal safety are. And **§15 #17's status**: the
plan records grouped commands as a documented gap to revisit with module mode, and decision 16's
action-rate knob promotes it to a blocker, because a per-command cap calibrated against a human
baseline charges a client bot twelve actions for what a human does in one (§5.2). That is a second
reason the referee must own the server-side command path, independent of the one in
`proxy-protocol-design.md` §1.1.
