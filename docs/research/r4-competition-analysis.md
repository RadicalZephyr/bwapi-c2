# R4. rsbwapi and JBWAPI as the competition

Read `Bytekeeper/rsbwapi` @`1fc0ada` (2025-11-17) and `JavaBWAPI/JBWAPI` @`0680856` (2026-02-20),
with `OpenBW/BWAPI4J` as the JNI counter-example, against the BWAPI sources in this tree.

**Headline: the trap is real, it is bigger than non-goal 1 claims, and non-goal 1 still gets the
reasoning wrong. The protocol is not the cost — it is 926 lines in JBWAPI (4%) and 212 lines in
rsbwapi (0.6%). The cost is the game-rules layer and the static type database, which is 72% of
JBWAPI and 86% of rsbwapi. And the permanent cost is not the writing but the divergence: once
reimplemented, upstream fixes stop arriving, which JBWAPI's maintainers say in as many words on
a bug that is still open.**

**But the honest paragraph has a second half the plan will not like. A client-mode C ABI does
not escape everything. Some of what these projects cannot do, we cannot do either, for the same
reason — the BWAPI server, not the language.**

---

## 1. What "the shared game-rules code" actually is

Non-goal 1 names `Templates.h` (3,098 lines) and `CommandTemp.h`. The full set a pure-client
reimplementation must reproduce:

| File | Lines | Contains |
|---|---|---|
| `Shared/Templates.h` | 3,098 | the entire `canXxx` family (68 predicates, 453 call sites), `hasPower`, `iterateUnitFinder`, `canBuildHere`, `canMake` |
| `Shared/UnitShared.cpp` | 1,093 | derived unit state (`isGathering`, `isAttacking`, …) |
| `Client/CommandTemp.h` | 1,004 | latency compensation |
| `Shared/{Player,Game,Bullet,Region}Shared.cpp` | 466 | derived player/game state |
| **Total rules** | **5,661** | |
| `BWAPILIB/Source/{UnitType,WeaponType,UpgradeType,TechType,…}.cpp` | ~4,100 | the static type database |
| `BWAPIClient/Source/*` | 1,421 | the actual protocol + Impl classes |

So the ratio is already visible upstream: **~9,800 lines of rules and data versus 1,421 lines of
protocol.** The protocol is the small part, and both competitors' own line counts confirm it.

---

## 2. What each project actually spent

### JBWAPI — 28,697 lines (22,952 in `bwapi`, 5,338 BWEM, 407 BWTA)

| Category | Lines | Share of `bwapi` pkg |
|---|---|---|
| **Game-rules port** (`Unit`, `Game`, `Player`, `BuildingPlacer`, `UnitFilter`, `UnitCommand`) | **11,210** | 49% |
| **Static type data** | **5,297** | 23% |
| Generated shared-memory accessors (`ClientData.java`) | 2,005 | 9% |
| Geometry / events / misc | 1,443 | 6% |
| Async, caching, perf instrumentation | 1,039 | 5% |
| **Latency compensation** (`CommandTemp` + `UnitSelf` + `PlayerSelf` + `SideEffect*`) | **1,032** | 4% |
| **Protocol and plumbing** (`Client`, `ClientConnection*`, `GameTable`, `WrappedBuffer`, `BWClient`) | **926** | **4%** |

`Unit.java` alone is **6,361 lines** and declares **294 `can*` methods**. `ClientData.java` was
*generated* — JBWAPI issue #12, "Using output of CLANG dump-record-layouts to generate shared
memory a…".

### rsbwapi — 37,017 lines

| Category | Lines | Share |
|---|---|---|
| **Static type data** (`unit_type.rs` 20,341 + weapon/upgrade/tech) | **25,367** | 69% |
| **Game-rules port** (`can_do.rs` 2,551 + `unit` + `game` + `player` + `command`) | **6,143** | 17% |
| BWEM port | 2,631 | 7% |
| Geometry / types / misc | 1,346 | 4% |
| SMA | 1,083 | 3% |
| **Protocol and plumbing** (`client.rs` + `shm.rs` + `aimodule.rs`) | **212** | **0.6%** |

`can_do.rs` is 2,551 lines and 128 `can_*` functions. The shared-memory struct layouts are
*generated* by bindgen from BWAPI's own headers (`build.rs`, `allowlist_type("BWAPI::(GameTable|.*Enum|MouseButton|Key|UnitData|RegionData|GameData)")`).

**Both projects generated the one part that is mechanical (the memory layout) and hand-wrote the
6,000–11,000 lines of rules that are not.**

---

## 3. Feature by feature: ported, skipped, or substituted

| BWAPI shared behaviour | JBWAPI | rsbwapi |
|---|---|---|
| `canXxx` family | **Ported.** 294 methods, `Unit.java` | **Ported.** 128 fns, `can_do.rs` |
| `canBuildHere` | Ported (`Game`, `Unit`, `BuildingPlacer`) — **known incorrect, issue #88 open since 2023** | Ported |
| `canMake` | Ported | Ported |
| `hasPower` / `hasPowerPrecise` | Ported | Ported |
| `getBuildLocation` | Ported — `BuildingPlacer.java`, 465 lines | **Absent** |
| `iterateUnitFinder` (spatial index) | **Skipped.** Has `unitFinder` accessors in `ClientData` and does not use them; `getUnitsInRectangle` is a linear stream over `getAllUnits()` | **Substituted.** Builds an `rstar` R-tree per frame |
| Latency compensation | **Ported after 11 months.** 1,032 lines, 26+ per-unit frame-keyed caches | **Absent.** Only `is_lat_com_enabled()` (reads the server flag) and `set_lat_com()` (sends the toggle). No `CommandTemp` equivalent, no predicted state |
| `setClientInfo` / `getClientInfo` | Present | **Absent** |
| `registerEvent` | — | **Absent** |
| Grouped commands | **Absent** — issue #70, and *unfixable* (see §6) | **Absent** |
| Module mode | Absent (client only) | Absent (client only) |

The latency-compensation row is the sharpest comparison available. **The same feature, in two
pure-client reimplementations of the same library: one took eleven months and 1,032 lines, the
other still does not have it after seven years.** JBWAPI's arc:

- **#6** (2018-11-05) *"implement client side latcom"* — reference given as a pinned upstream commit
- **#15** (2018-12-27) *"As long as LatCom isn't implemented disable latcom at start"* — they had to turn the feature off
- **#42** (2019-09-29) PR *"Implement LatCom"* — merged, closing #6 on 2019-10-05
- **#48** (2019-11-26) *"isGatheringMinerals issue with LatCom on"* — a real bot (MadMix) breaks on conversion from BWMirror

Two spatial strategies for one query is the other sharp one. BWAPI does a binary search over
X/Y-sorted arrays; JBWAPI streams over every unit and takes `min` by distance; rsbwapi queries an
R-tree it rebuilt this frame. All three give *a* nearest unit. They do not agree on ties, and
JBWAPI's is O(n) per call in a function bots call constantly (R2: `getClosestUnit`, 55 sites).

---

## 4. The divergence cost, measured

JBWAPI's tracker is the field report the plan wanted. Filtering 96 issues to those where the
reimplementation disagreed with BWAPI's own behaviour:

| # | Date | Bug | Time to close |
|---|---|---|---|
| #7 | 2018-11-24 | `getDistance(Unit, Unit)` gives wrong values | 6 weeks |
| #10 | 2018-12-05 | `bwta.Chokepoint` is wrong | — |
| #11 | 2018-12-05 | MarineHell's marines never load into the bunker | — |
| #21 | 2019-01-20 | "You are your own Ally" | 1 day (was consistent) |
| #22 | 2019-01-20 | PurpleWave works but gateways are not being built | — |
| #23 | 2019-02-26 | Neutral `isEnemy` returns true | — |
| #29 | 2019-03-17 | `getBuildTile` doesn't seem to work | — |
| #35 | 2019-08-06 | `getBuildLocation` results wrong (with screenshots) | same day |
| #38 | 2019-08-09 | `TechType.Nuclear_Strike.energyCost()` throws AIOOBE | — |
| #52 | 2020-02-06 | Renders wrong colors | 0 days |
| #61 | 2020-09-08 | Training with a full queue throws IndexOutOfBounds | **open** |
| #76 | 2022-03-20 | Train issued twice in command center | — |
| **#88** | **2023-05-10** | **`canBuildHere` mineral vs depot check is not correct** | **open** |
| **#96** | **2026-08-21** | **Unpowered buildings can't cancel production** | **open** |

Fourteen divergence bugs over eight years, three still open, the newest filed two weeks ago. Two
of them were found by *bots being ported* — #11 (MarineHell), #22 (PurpleWave), #48 (MadMix) —
which is the expensive kind: the bot author debugs someone else's binding.

rsbwapi's tracker has two issues total, which proves nothing about correctness — it has roughly
one user. Its commit log carries the same class of bug:

- `1279c41` **"Added PlayerId; fixed WeaponTypes being totally off"** — the hand-transcribed weapon table was wrong
- `7ead762` "Fixed UpgradeType missing '18'" — a missing enum entry
- `066f2aa` "Fixed distance-matrix"
- `b0d8a1d` "fixed unit visibility (none are visible after start match event)"
- `e3fe077` "Fixed BWAPI depot placing bug", `7799366` "Fixed selected units", `a49123d` "Fixed shape drawing"

"WeaponTypes being totally off" is R2's transcription risk realised: 25,367 lines of hand-copied
static data, and one whole table was wrong until someone noticed.

### The cost that does not end

JBWAPI #88 states the permanent cost in one sentence:

> **"Since a fix in BWAPI will not fix it for JBWAPI."**

That is the trap, correctly named. It is not the 5,661 lines — a determined person writes those
once. It is that after writing them you have forked the game rules, and every upstream fix has to
be noticed, understood, and re-applied by hand, forever, by a project with one or two maintainers.
JBWAPI has 506 commits and 14 contributors in 7.5 years; rsbwapi has 186 commits and is
effectively one person. Neither has the capacity to track BWAPI's rules indefinitely, and #88 is
the proof — a known-wrong `canBuildHere` sitting open for three years because fixing it upstream
does not help.

---

## 5. Per-frame data strategy: the split is architectural, not incidental

The plan cites BWAPI4J as having reached §5.10's conclusion independently. Confirmed, and the
reason matters.

- **BWAPI4J** (JNI over the real C++ BWAPI) does exactly what the plan describes:
  `private native int[] getAllUnitsData();` — one JNI call per category per frame returning a flat
  `int[]`, decoded Java-side. It batches because JNI crossings are expensive.
- **JBWAPI** (pure client) does **not** copy. `ClientData.java` is generated offset-based getters
  reading a mapped `WrappedBuffer` via `sun.misc.Unsafe`, in place.
- **rsbwapi** (pure client) does **not** copy either. `Shm<BWAPI_GameData>` derefs the mapped
  region directly; field reads are ordinary struct loads.

**The pure-client designs have no FFI boundary, so they have no batching problem.** Their caching
(`Cache.java`, `UnitSelf`'s 26 frame-keyed caches) exists to serve *latency compensation*, not to
amortise crossings.

This is an honest cost of our approach that the plan should state. **A C ABI reintroduces the
boundary that JBWAPI and rsbwapi designed away.** §5.10's snapshots are not a clever optimisation
we thought of and they did not; they are compensation for a per-call cost we are choosing to
take on, and which they do not have. What we buy with it is BWAPI's real implementation — see
next section — but the trade should be written down as a trade.

---

## 6. What a C ABI gets for free — and what it does not

**Gets for free**, by calling the real `BWAPI::Game`:

- All 68 `canXxx` predicates, exactly as BWAPI computes them, including `canBuildHere`'s
  mineral/depot check that JBWAPI has had wrong since 2023.
- `iterateUnitFinder` and therefore BWAPI's actual spatial semantics and tie-breaking, rather
  than a linear scan (JBWAPI) or a substituted R-tree (rsbwapi).
- `getBuildLocation` — rsbwapi has none; JBWAPI wrote 465 lines and got it wrong once.
- `CommandTemp` latency compensation — rsbwapi has none; JBWAPI spent 11 months and 1,032 lines.
- The static type database from `UnitType.cpp` et al., instead of a fifth hand transcription
  (R2, R3).
- Upstream fixes, on a pin bump instead of a re-port.
- `registerEvent`, `setClientInfo`/`getClientInfo` — absent from rsbwapi.

**Does not get for free — the half the plan should not claim:**

- **Grouped commands.** JBWAPI #70, from `dgant`: *"Module bots can issue grouped commands. But
  client bots can't, because BWAPI's server implementation doesn't support them… There's no way
  to fix this on JBWAPI's end."* A **client-mode** C ABI inherits this identically. Only module
  mode escapes it, and rev 3 puts module mode in an appendix.
- **Server-side bugs.** JBWAPI #96: *"Fixing it on the JBWAPI side isn't enough — the BWAPI server
  will reject the command too."* #48's `isGatheringMinerals`/LatCom bug likewise turned out to be
  server-side, fixed in `bwapi/bwapi#845`. We inherit these exactly as they do.
- **BWEM / BWTA.** JBWAPI ships 5,338 lines of BWEM, rsbwapi 2,631. A C ABI over `BWAPI::Game`
  exposes none of it, because upstream BWAPI does not contain it. Every consumer still needs map
  analysis, and a C ABI does not answer that at all. Neither does the current plan.

So the honest framing is: **a C ABI removes the game-rules fork and the type-database
transcription — the 72–86% — and removes nothing else.** That is a large, real win. It is not
"everything they can't do."

---

## 7. Does a good Rust option mean Rust should be dropped?

**Yes, and R3 already argued half of it.** rsbwapi is maintained through Feb 2026, has a
competitive bot behind it (Styx2, SSCAIT ELO 2437), and has four downstream users. It is the one
non-C++/non-JVM language that is genuinely served.

Three qualifications, because the answer is not unconditional:

1. **rsbwapi is one person.** 186 commits, effectively a single author since 2019. Its bus factor
   is 1, and it lacks latcom, `getBuildLocation`, `clientInfo`, `registerEvent`, and grouped
   commands. "Served" means "served well enough for its author's bot."
2. **The gap is exactly the ported-rules gap.** Everything rsbwapi lacks is something a C ABI
   would provide for free. So a Rust binding over the C ABI would be *better* than rsbwapi on
   correctness — while being worse on ergonomics (`Vec<Unit>` and `Position` structs versus
   caller buffers and packed `int64_t`), and worse on deployment (a second DLL to ship).
3. **Rust remains the best proof-of-concept target regardless.** Keeping a Rust binding as a
   *test consumer* — the thing that proves the ABI is bindable, and the thing R2's Styx2 numbers
   give us a usage baseline for — is different from shipping it as a product competing with
   rsbwapi. The plan should keep the former and drop the latter.

Combined with R3's finding that C# has six dead attempts and no living binding, the in-scope
binding set should probably be **C# and Python, with Rust demoted to a test harness** — not
"Rust and one other."

---

## 8. The paragraph that replaces non-goal 1

> Two maintained projects reimplement BWAPI's client protocol in another language, so the claim
> that doing so is a trap has to be argued rather than asserted. Measured, the trap is real but
> mislocated. The protocol itself is cheap — 926 lines in JBWAPI, 212 in rsbwapi, under 5% of
> either codebase. What is expensive is everything the protocol does not carry: BWAPI's shared
> game rules (`Templates.h`, `UnitShared.cpp`, `CommandTemp.h` — 5,661 lines) and its static type
> database (~4,100 lines). JBWAPI spent 11,210 lines on the rules and 5,297 on the data, 72% of
> its `bwapi` package; rsbwapi spent 6,143 and 25,367, 86% of its total. Both got parts of it
> wrong — JBWAPI has fourteen recorded divergence bugs, three still open, including a
> `canBuildHere` defect known since 2023; rsbwapi shipped a weapon table that was, in its own
> commit message, "totally off." Both skipped parts: rsbwapi has no latency compensation and no
> `getBuildLocation`; JBWAPI ignores BWAPI's spatial index and scans every unit instead. And the
> cost does not end at the port. JBWAPI's own issue #88 states the permanent term: *"a fix in
> BWAPI will not fix it for JBWAPI."* Reimplementing forks the game rules, and a two-person
> project then owns that fork forever.
>
> A C ABI over the real `BWAPI::Game` avoids all of that, and it is worth being precise about
> what it does not avoid. Client-mode limitations are the server's, not the language's: grouped
> commands are unavailable to any client bot in any language, and server-side bugs reach a C ABI
> unchanged. Map analysis (BWEM) is outside BWAPI entirely and stays a consumer problem. And a C
> ABI reintroduces a per-call FFI boundary that the pure-client designs simply do not have —
> which is why §5.10's snapshots exist, and which is a trade, not a free lunch. The trap is the
> 72–86%. That is what we are buying out of, and it is enough.

---

## 9. Feeds into other experiments

| To | Finding |
|---|---|
| **R2** | rsbwapi's `1279c41` "fixed WeaponTypes being totally off" is direct evidence that hand-transcribed static tables get transcribed wrong. Fifth data point for shipping §5.8. |
| **R3** | Confirms Rust is genuinely served, but by a one-person project missing latcom, `getBuildLocation`, `clientInfo` and `registerEvent`. "Served" is weaker than R3 assumed. |
| **R5** | rsbwapi's README: *"Windows: You should be fine to just compile your bot. The resulting **x64 executable** should run fine in all current tournaments/ladders."* A second production x64-client-against-32-bit-server existence proof alongside JBWAPI. |
| **R6** | **Both competitors generated the shared-memory layout rather than hand-writing it** — JBWAPI from `clang -Xclang -fdump-record-layouts` (issue #12), rsbwapi from bindgen with an explicit allowlist. That is the layout-dump tooling §10.2 wants, already proven twice. |
| **§5.10** | BWAPI4J's `native int[] getAllUnitsData()` confirms the batching conclusion — but only for FFI-boundary designs. JBWAPI and rsbwapi read the mapped region in place and need no batching. State §5.10 as compensation for a boundary we choose, not as a universal truth. |
| **Scope** | Nothing in the plan addresses BWEM/map analysis, which both competitors ship (5,338 and 2,631 lines) because bots need it. Worth an explicit non-goal rather than silence. |
