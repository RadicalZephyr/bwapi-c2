# R2. Sizing the real API surface from bot corpora

Method, corpus, and raw counts below; the ranked list of all 347 observed entry points is in
[r2-frequency-ranked-api.md](r2-frequency-ranked-api.md).

**Headline: the 95% line is ~195 entry points and each individual competitive bot needs
160–215 — so the plan's 550–600 completeness target is roughly 3× what any bot uses. But the
generator does not become optional, because the part of the surface bots reach for most
densely is exactly the part §1.8's count omits: static type data. Every independent
implementation examined re-derived it by hand, at a cost of 1,119 to 25,367 lines each.**

---

## 1. Corpus

| Bot | Source scanned | Lines | HEAD |
|---|---|---|---|
| Steamhammer | `Steamhammer/Source` | 55,236 | 2022-12-08 |
| UAlbertaBot | `UAlbertaBot/Source` | 31,773 | 2022-03-01 |
| OpprimoBot | `SCProjects/OpprimoBot/Source` | 15,476 | 2015-04-19 |
| RazeAndPlunder | `src` | 15,430 | 2017-04-07 |
| ZZZKBot | `ZZZKBot/Source` | 6,335 | 2025-10-23 |
| ExampleAIModule | (this tree) | 349 | — |
| ExampleAIClient | (this tree) | 273 | — |
| **C++ total** | | **124,872** | |
| Styx2 (Rust, via rsbwapi) | `src` | 7,722 | 2023-04-24 |
| rsbwapi (the binding itself) | `src` + `bwapi_wrapper/src` | 36,782 | 2025-11-17 |

Vendored dependencies were excluded: `Steamhammer/BWAPILIB`, `UAlbertaBot/BOSS`,
`UAlbertaBot/SparCraft`, `RazeAndPlunder/deps`, `OpprimoBot/SCProjects/include`. (BOSS and
SparCraft reappear in §5 as evidence, not as call sites.)

### Method and its error bars

An authoritative name inventory was extracted from the BWAPI headers in this tree —
**574 distinct method names** across `Game`, `Unit`, `Player`, `Region`, `Force`, `Bullet`, the
fourteen type classes (including the `Type<>` CRTP base), the `*set` containers, `Position`,
`UnitCommand` and `Event`. Bot sources were then stripped of comments and string literals and
scanned for `->name(`, `.name(`, and `::name(`, with each hit recorded as a distinct
`(file, line)` call site.

The obvious failure mode is a bot method that shares a BWAPI method's name. Two counts bracket
it:

- **Conservative** (used throughout below): for each bot, drop any name that bot also *defines*.
  347 distinct names, 8,408 call sites.
- **Liberal** (count everything): 354 distinct names, 12,776 call sites.

The two agree on the shape — 95% at 195 vs. 176 — so the conclusion does not turn on the choice.
Only **7 names** appear exclusively in shadowed form and are therefore missing entirely from the
conservative figures: `canBuildHere`, `cancelUpgrade`, `getColor`, `height`, `isAccessible`,
`isValid`, `width`.

As a control, **ZZZKBot has zero shadowed names** — it is a single-file bot with no wrapper
classes, so its 168-name profile is measured, not estimated. Its list is spot-checked in the
appendix and reads correctly.

---

## 2. The coverage curve

Conservative count, 8,408 call sites ranked by frequency:

| Coverage of all call sites | Distinct entry points needed |
|---|---|
| 50% | **24** |
| 75% | 69 |
| 80% | 87 |
| 90% | 143 |
| **95%** | **195** |
| 98% | 251 |
| 100% (everything observed) | 347 |

**Answer to the question as posed: the 95% line is 195, not 150 and not 250.**

Per-bot, the number is smaller and strikingly consistent:

| Bot | Distinct BWAPI entry points | Call sites |
|---|---|---|
| Steamhammer | 215 | 2,233 |
| RazeAndPlunder | 181 | 1,785 |
| ZZZKBot | 168 | 1,104 |
| UAlbertaBot | 165 | 1,859 |
| OpprimoBot | 161 | 1,194 |
| ExampleAIModule | 54 | 100 |
| ExampleAIClient | 42 | 133 |

A 55,000-line tournament bot and a 6,300-line one differ by 47 entry points. **A competitive
BWAPI bot uses about 200 distinct calls.** Of the 574 names BWAPI declares, 347 (60%) appear
anywhere in 125,000 lines of bot code, and 227 appear nowhere at all.

The union does keep growing as bots are added — `+82` for OpprimoBot, `+36` for UAlbertaBot,
`+36` for Steamhammer — so the tail is real and five bots have not exhausted it. But growth is
in the tail, not the core: **only 6 entry points are used by all 7 corpora, 68 by five or more,
and 175 by three or more.**

### Proposed v1 cut line

**Used by ≥ 2 corpora, or ≥ 10 call sites: 263 entry points, covering 97.7% of observed call
sites.** Split: 201 interface methods, 58 type-data accessors, 4 other.

This is the defensible cut. It is not the 95% line (195) because the frequency ranking is
dominated by a handful of hot accessors, and a rule based on *breadth of adoption* rather than
raw count is more robust to corpus composition — 263 is what survives when you ask "did more
than one team need this?"

---

## 3. The `canXxx` family: the prior was right

**BWAPI declares 74 `canXxx` predicates on `Unit` and `Game`. 29 are called anywhere in the
corpus; 45 are never called once.**

Nine account for the bulk:

| | sites | corpora |
|---|---|---|
| `canAttack` | 52 | 4 |
| `canMake` | 35 | 5 |
| `canMove` | 14 | 2 |
| `canUpgrade` | 11 | 4 |
| `canBurrow` | 10 | 1 |
| `canTrain` | 9 | 2 |
| `canCancelMorph` | 9 | 2 |
| `canUseTech` | 7 | 2 |
| `canMorph` | 6 | 2 |

The remaining 20 called predicates average 3 sites each, and 15 of those come from a single bot.
**Not one bot in the corpus calls the `*Grouped` variants, the `checkCommandibility` overloads,
or the fine-grained `canRightClickUnitGrouped`-class functions** — which is most of what makes
the family 74 wide (and 130 wide once bwapi-c's overload expansion is counted; see R1).

Styx2, independently, in Rust: `can_attack` (10), `can_move` (7), `can_build_here` (1). Three.

**Implication.** The `canXxx` family is the single largest block that can be cut from v1 without
anyone noticing. Ship the nine above plus `canBuildHere`, defer the other ~64, and add on
request. §1.8's 550–600 shrinks by roughly a fifth on this finding alone.

---

## 4. Draw calls: 8 of 28, and the coordinate-space answer is "two"

**28 draw methods declared. 8 used. 20 never called by any bot in the corpus.**

| | sites | corpora |
|---|---|---|
| `drawTextScreen` | 230 | 7 |
| `drawTextMap` | 145 | 6 |
| `drawLineMap` | 95 | 5 |
| `drawBoxMap` | 91 | 5 |
| `drawCircleMap` | 78 | 5 |
| `drawBoxScreen` | 25 | 4 |
| `drawLineScreen` | 23 | 2 |
| `drawDotMap` | 2 | 1 |

By coordinate space: **Map 411 sites, Screen 278, Mouse 0, explicit `CoordinateType` parameter 0.**

Never used by anyone: every `*Mouse` variant, every `drawEllipse*`, every `drawTriangle*`,
`drawDot{Screen,Mouse}`, `drawCircleScreen`, and all eight generic `draw*(CoordinateType, …)`
forms.

**Implication — corrected.** *This recommendation is withdrawn; see
[research-vs-rev4-review.md](research-vs-rev4-review.md) §4.* The original text argued for
shipping 8 draw functions in Map and Screen space only, and dropping the generic
`CoordinateType`-taking forms because nobody passes a runtime coordinate space.

That was the wrong inference. §5.2 of the plan already collapses ~90 declarations to **8 functions
by taking `ctype` as a parameter** — reaching the same count while keeping all three coordinate
spaces and the runtime choice. Dropping the generic forms would have traded away Mouse space for
nothing.

**Draw calls are development and debugging tooling, so tournament-bot usage systematically
undercounts them**: a bot author wants more drawing during development than survives into a
submitted binary. The numbers above are therefore evidence that the §5.2 collapse is *safe* — no
bot depends on the 90-way spelling — and not an argument for omitting anything. The general form
of the correction: **usage frequency tells you what is safe to merge, not what is safe to omit.**

---

## 5. Static type data — the finding that saves the generator

This is the question the plan flagged as open ("how much of the surface is static type data,
and is it accessed as functions or as lookup tables the bot builds once?"). Both halves have
sharp answers, and they point opposite ways from the rest of R2.

### It is accessed heavily, as functions

**80 distinct type-class accessors, 1,671 call sites** — 20% of all measured BWAPI traffic,
from a block that is 122 methods wide. The density is far higher than the interface classes:

| Bucket | Entry points used | Call sites | Sites per entry point |
|---|---|---|---|
| Interface (`Game`/`Unit`/`Player`/…) | 270 | 6,683 | 24.8 |
| **Type data (`UnitType`/`WeaponType`/…)** | **70** | **1,604** | **22.9** |

`groundWeapon` (132), `isFlyer` (96), `airWeapon` (83), `maxRange` (73), `isWorker` (70),
`tileHeight`/`tileWidth` (116), `isResourceDepot` (46, in **all seven** corpora),
`maxHitPoints` (35), the four `dimension*` (91). These are not conveniences; they are how a bot
decides what to build and what to shoot.

Named constants: **279 of 888 referenced** (31%), concentrated where you would expect —
`UnitTypes` 107/235, `Orders` 49/192, `TechTypes` 26/39, `UpgradeTypes` 30/54 — and near-zero
where you would also expect: `WeaponTypes` 3/104, `ExplosionTypes` 0/27, `GameTypes` 0/20,
`PlayerTypes` 0/14, `Latency` 0/7, `CoordinateType` 0/4.

### And when a binding does not provide it, the binding writes it out by hand

Three independent reimplementations of the same StarCraft static database turned up in the
corpus without being looked for:

| Reimplementation | Size | Why |
|---|---|---|
| `rsbwapi/bwapi_wrapper/src/unit_type.rs` | **20,341 lines**, 236 `UnitTypeData` entries | Shared memory carries no type data |
| `rsbwapi` weapon/upgrade/tech tables | 5,026 lines | same |
| `UAlbertaBot/SparCraft/bwapidata/UnitType.cpp` | 1,119 lines | Simulator must not link BWAPI |
| `UAlbertaBot/BOSS/…/bwapidata/UnitType.cpp` | 1,119 lines | same |

**25,367 of rsbwapi's 36,782 lines — 69% of the entire binding — are transcribed static type
tables.** Its actual client/API code is 8,212 lines. The form is a compile-time table
(`struct UnitTypeData { mineral_price, tile_width, ground_weapon, … }`, one entry per unit)
exposed through accessor methods — so the answer to "functions or lookup tables" is *a table
built once, read through functions*, and either shape is fine for the ABI as long as the data
crosses at all.

**This is the strongest single argument in R2, and it cuts against the rest of the findings.**
R1 established that bwapi-c ships 0 of ~1,030 constants and type accessors and that its example
bot is reduced to `case 7: // SCV`. R2 establishes what happens next: the consumer writes the
table itself, at 20,000 lines a language. A C ABI that omits §5.8 does not save 1,030 entry
points — it exports them to every downstream binding, one copy each, and guarantees they drift.

---

## 6. Filters

**144 `Filter::` references, 26 distinct, of 134 declared.** `GetType` (37), `IsEnemy` (28),
`IsOwned` (15), `IsCompleted` (8), `IsBuilding` (7), then a long thin tail. Roughly 20% used.

Worth noting against R1: bwapi-c exposes none of these and instead takes a bare C function
pointer with no `void* user`. The corpus says the combinators are used, but lightly and by a
narrow set — a C ABI can reasonably ship the ~26 observed predicates as `int32_t` filter
constants plus a general callback, and skip the arithmetic/comparison combinator machinery.

---

## 7. Cross-check: a Rust bot on a non-C++ binding

Styx2 — 7,722 lines, a real competitive bot — calls **115 distinct rsbwapi entry points**
(423 sites), with 50% of traffic in the top 11 and 95% in the top 94. rsbwapi's own hand-written
API is **419 public functions** (77 `game`, 172 `unit`, 52 `player`, 53 `can_do`, 46 `command`,
13 `region`, 12 `bullet`, 2 `force`, 2 `client`).

Two things follow. First, the C++ and Rust numbers agree: ~120–215 entry points per bot, ~420
for a binding aiming at completeness — which is close to bwapi-c's 530 and well under §1.8's
550–600 *plus* the 1,030 it omits. Second, `can_do.rs` is 2,551 lines — the `canXxx` family
ported by hand into Rust, for the three predicates Styx2 actually calls. That is R4's territory;
recorded here as a measurement.

---

## 8. Answers to the questions as asked

**What is the smallest set covering 95% of call sites?** 195. Per bot, 160–215. The recommended
v1 cut — ≥ 2 corpora or ≥ 10 sites — is 263 entry points at 97.7% coverage.

**How many of the ~130 `canXxx` predicates are ever called?** 29 of 74 base predicates; 9 carry
the load; 45 are never called. The prior ("a handful") was right.

**How many of the ~90 draw calls are used, and in which coordinate spaces?** 8 of 28. Map and
Screen only; zero Mouse, zero explicit `CoordinateType`.

**How much of the surface is static type data, and how is it accessed?** 122 accessors and 888
constants declared; 80 accessors (1,671 sites, 20% of all traffic) and 279 constants used. It is
accessed as functions over a table the *implementation* builds once — and every binding that
omits it has been forced to rebuild that table by hand, at 1,119–25,367 lines a copy.

---

## 9. What this means for the plan

**Cut, but not where the plan would cut.**

§1.8's "~900 declarations collapsing to 550–600 entry points" is a completeness target derived
from the interface classes. Against usage:

| Block | §1.8 / R1 sizing | Observed usage | Recommendation |
|---|---|---|---|
| `canXxx` family | ~130 (bwapi-c expands to 130) | 29 used, 9 hot | **Ship 10, defer ~120** |
| Draw calls | ~90 | 8 used | **Ship 8 via §5.2's `ctype` collapse — all three spaces kept, nothing omitted** |
| `Filters.h` combinators | 134 | 26 used | **Ship ~26 as constants** |
| Interface methods (Game/Unit/Player/…) | ~450 | 270 used, 201 in v1 cut | **Ship ~200, defer the rest** |
| **Static type data (§5.8)** | **1,030 (0 in bwapi-c)** | **80 accessors + 279 constants used, 20% of traffic** | **Ship it all. This is the product.** |

That reshapes the roadmap without deleting the generator, and the reasoning is worth stating
plainly because it inverts the plan's own expectation.

The plan asked whether "a spec DSL, emitters, `api.json`, and a coverage audit are load-bearing
or are consequences of a completeness goal nobody requested," and proposed that a 95% line under
~250 makes the generator optional. **The 95% line is 195, and the generator is still worth
having — but for the other half of the surface.** Roughly 200 hand-written interface functions
is very manageable; nobody needs a DSL for that. What nobody should hand-type is 235 `UnitType`
entries × ~50 fields, and that is precisely the block a generator is good at and the block whose
absence has cost three separate projects five figures of transcription.

**So: the generator's justification moves from "550–600 entry points is too many to hand-write"
to "the static type database must be emitted from a machine-readable source, or every consumer
pays for it again."** The emitters, the coverage audit, and `api.json` follow from that narrower
claim and survive. The declaration-hash and divergence-register machinery does not — it is
insurance on the hand-written interface layer, which at ~200 functions can be maintained by
reading a diff.

Two smaller items the corpus surfaced:

- **`Game::flush` is used by 4 of 7 corpora (17 sites)** and is the single Game method bwapi-c
  does not wrap. Include it.
- **`setClientInfo`/`getClientInfo` is used 24 times by ZZZKBot** — BWAPI's per-object user-data
  slot, `std::map<int, void*>` on every interface. A C ABI needs an equivalent (bots attach
  their own state to units), and the templated accessor's `(CT)(int)void*` cast in
  `Interface.h:72` **truncates a pointer to 32 bits**, which is a live finding for R5 and R6.
