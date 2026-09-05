# R11.2 — The three BWEM ports as prior art

**Headline: the 2.7× size spread between the ports is not API surface — it is re-implemented
analysis. All three re-run BWEM's map analysis in the host language; none reads results out of
anything, because there is nothing to read them out of. Their exposed APIs are 45–129 functions,
bracketing R11.1's 91, and our cut covers 11 of the 11 names all three agree on. And JBWAPI's
tracker shows the divergence trap in its BWEM-specific form, which is nastier than the BWAPI
one: a hand-maintained checklist of individual maps that break.**

---

## 1. The size spread is analysis, not API

| Port | Total | Analysis machinery | Exposed API |
|---|---|---|---|
| **JBWAPI** (`bwem`) | 5,338 | **2,116 (40%)** — `Graph` 779, `BWMapInitializer` 669, `AreaInitializer` 542, `TempAreaInfo` 126 | 116 public methods |
| **rsbwapi** (`src/bwem`) | 2,496 | most of `map.rs` (1,053) and `area.rs` (581) | 129 `pub fn` |
| **gobwapi** (`pkg/bwem`) | 2,004 | **1,290 (64%)** — `analyze.go` 1,128, `bfs.go` 162 | 45 exported |

**JBWAPI is 2.7× rsbwapi because it carries more of BWEM's construction pipeline as separate,
faithfully-named classes, not because it exposes more.** Its API is *smaller* than rsbwapi's
(116 vs 129) while its total is more than double. gobwapi is the newest (started Feb 2026) and
the least complete; its 45 is a snapshot of a port in progress, not a considered cut.

**None of the three reads analysis results out of an existing implementation.** Every one
re-derives areas, chokepoints, altitudes and bases from the walkability grid, in its own
language. That is the entire justification for wrapping BWEM in one sentence: three teams have
now written the same 1,300–2,100 lines of graph analysis, and a fourth (a Python or C# port)
would write it again.

---

## 2. Our cut, validated against theirs

Normalising names across languages (`getArea`/`get_area`/`GetArea` → `area`):

| | Distinct exposed names |
|---|---|
| rsbwapi | 109 |
| JBWAPI | 94 |
| **R11.1's cut** | **90** |
| gobwapi | 33 |
| **In all three ports** | **11** |
| In at least two ports | 47 |

**R11.1's 91-function cut sits inside the range the ports settled on, and covers 11 of the 11
names all three agree are necessary** — `areas`, `bases`, `chokepoints`, `geysers`, `minerals`,
`isPseudo`, `miniTile`, `nearestArea`, `onMineralDestroyed`, `path`, `tile`. Nothing in the
common subset is missing from our cut.

That is the strongest available evidence for the v1 line: three independent teams, three
languages, and the intersection of what they all thought necessary is fully covered.

---

## 3. What they skipped, and what it cost

**All three expose per-tile accessors and none exposes bulk grids.** JBWAPI has
`TerrainData.getTile(TilePosition)` and `getMiniTile(WalkPosition)`; rsbwapi's `tiles.rs` is a
`MiniTile` struct with per-field methods; gobwapi's tile access lives inside `analyze.go` and is
barely exposed at all. **Nobody found bulk grid export necessary** — corroborating R11.1's
finding that grid access is ~2% of BWEM traffic, and arguing again that §5.5's bulk machinery is
not where BWEM's value is.

rsbwapi exposes mutators (`set_walkable`, `set_sea`, `set_altitude`) because its analysis lives
in the same module. Ours will not need to: the analysis stays in C++ behind the ABI. **That is a
concrete simplification the C-ABI approach buys** — the 27 mutators R11.1 identified as
zero-usage are not merely unused by bots, they are unnecessary to expose at all, because no host
ever runs the analysis.

---

## 4. The divergence trap, BWEM edition

R4 measured the trap for BWAPI: fourteen recorded divergence bugs in JBWAPI, three still open.
BWEM's version is worse in kind, because BWEM's bugs are **map-specific** and only surface on
maps you have not tested.

| Issue | Date | What |
|---|---|---|
| **#27** "BWEM Fixes" | 2019-03-17 | **A hand-maintained checklist of individual maps that break.** Sparkle — fixed by cherry-picking an upstream `BWEM-community` commit. Hitchhiker — **still unchecked.** Aztec (KSL) — fixed |
| **#34** "BWEM sometimes has no areas" | 2019-06-19 | Intermittent crash in **1 in 10 to 1 in 20 games**, always on the first pathfind attempt |
| **#51** "BWEM IllegalStateException on game start" | 2020-02-05 | Only when playing consecutive games without restarting — state leaking across matches |
| **#86** "Investigate BWEM issue with New Popular Fastest Map" | 2023-05-09 | **Still open.** A start location not assigned to a base |

**#27 is the finding.** A port maintainer keeping a to-do list of *maps* — cherry-picking
upstream BWEM commits one at a time, leaving one unfixed for six years — is exactly the failure
mode JBWAPI #88 describes for BWAPI ("a fix in BWAPI will not fix it for JBWAPI"), but with a
long tail of map-shaped triggers instead of a finite set of rules. A C ABI over the real BWEM
inherits upstream fixes on a pin bump instead.

**#51 is a design input, not just a bug.** State leaking across consecutive matches is precisely
the lifecycle question R11.5 is scoped to answer, and BWEM's own `Map::ResetInstance` (which
Stardust calls) exists because of it. Our ABI must make match-to-match reset explicit and hard to
skip.

---

## 5. Answers to the questions R11.2 asked

**Why is JBWAPI's port 2.7× rsbwapi's?** Duplicated construction pipeline, faithfully transcribed
as separate classes. Its exposed API is *smaller* than rsbwapi's.

**What did each port skip, and did anyone complain?** No complaints about missing API in any of
the three trackers. All the reported problems are correctness: missing areas, crashes on
consecutive games, and specific maps analysing wrongly. **Nobody wanted more BWEM surface; they
wanted the surface they had to be right.** That reinforces the R11.1 cut and shifts the risk to
R11.4 and R11.5.

**Do the ports re-run the analysis, or read results out?** All three re-run it. There is nothing
to read from, which is the gap this project closes.

**Does any port expose `Tile`/`MiniTile` grids in bulk?** No. All per-tile. Second independent
signal that bulk grid export is not where BWEM's value lies.

---

## 6. Feeds forward

| To | Finding |
|---|---|
| **R11.3** | Do not export mutators — not merely unused, but *unnecessary*, since no host runs the analysis. Design the header around 84 reads + 7 lifecycle only |
| **R11.3** | Per-tile accessors are what every port shipped and what bots use. Design `bwapi_bwem_tile_*` as scalar reads first; treat a bulk grid export as an optional §5.5 fast path, not the primary shape |
| **R11.5** | JBWAPI #51 (state leaking across consecutive games) and Stardust's use of `Map::ResetInstance` make match reset a first-class lifecycle concern, not an afterthought |
| **Plan §M / non-goal 1** | BWEM strengthens the "never port the rules again" argument more than BWAPI does. BWAPI's divergence bugs are a finite rule set; BWEM's are an open-ended list of maps, one of which JBWAPI has carried unfixed since 2019 |
