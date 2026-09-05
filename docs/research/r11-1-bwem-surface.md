# R11.1 — Sizing the real BWEM surface

Same method as R2, on a corpus rebuilt for BWEM. Ranked list in
[`r11/bwem-frequency-ranked.md`](r11/bwem-frequency-ranked.md).

**Headline: BWEM's exportable surface is 91 functions — 84 read accessors plus 7 lifecycle
entry points — and that is the whole v1 cut. Of the 279 declarations in the headers, 91 are
bot-facing reads, 42 are mutators and internals that no bot touches, 49 belong to classes that
should not be exported at all, and the rest are helpers. This is a much smaller and much cleaner
job than the BWAPI layer: one header, ninety-odd functions, no percentile judgement required.**

---

## 1. The corpus had to be rebuilt

The R2 corpus is a **BWTA** corpus, not a BWEM one:

| Bot | Files referencing BWEM | Files referencing BWTA |
|---|---|---|
| UAlbertaBot | 0 | 16 |
| OpprimoBot | 0 | 12 |
| Steamhammer | 0 | 1 |
| ZZZKBot | 0 | 0 |
| RazeAndPlunder | **21** | 6 |

So R11.1 uses a different corpus, chosen for actual BWEM use and for currency:

| Bot | Source scanned | Lines | Note |
|---|---|---|---|
| **Stardust** | `src` | 62,398 | SSCAIT #1 (ELO 3455, R3); active 2026-08 |
| **McRave** | `Source` | 41,184 | active 2026-05 |
| **RazeAndPlunder** | `src` | 15,430 | bundles `deps/BWEM-1.3.1` |
| | | **119,012** | |

Vendored BWEM copies (`Stardust/3rdparty/BWEM`, `McRave/Source/BWEM`,
`RazeAndPlunder/deps/BWEM-1.3.1`) are excluded from the scan — otherwise BWEM's own internals
would count as usage.

**A methodology note worth recording**, because it nearly produced a wrong answer: my first
extraction pass missed every `const`-qualified accessor, because the declaration regex allowed
`virtual`/`static`/`inline` as leading qualifiers but not `const`. That silently hid
`ChokePoint::Center`, `Area::Top`, `Area::ChokePoints`, `Base::Location` and `Map::GetPath` — the
five most-used functions in the library — and made the surface look 40% smaller than it is. The
tell was that a grep for literal call sites found `choke->Center(` all over Stardust while the
inventory said `Center` did not exist. **Cross-check any generated inventory against a literal
grep before trusting it.**

---

## 2. What is actually in BWEM

279 declarations across 15 headers. Classified by who they are for:

| Bucket | Declared | Used in corpus | Call sites |
|---|---|---|---|
| **Read accessors** (bot-facing) | **84** | 40 | 1,365 |
| **Lifecycle / events** | **7** | 6 | 15 |
| Mutators (`Set*`, `Add*`, `Remove*`, `Replace*`, `Create*`, `Compute*`, `Update*`) | 27 | **0** | 0 |
| Internals (`RawFrontier`, `PathBackTrace`, `InternalData`, `GetGraph`, `BreadthFirstSearch`, …) | 15 | **0** | 0 |
| Non-exported classes (`Graph`, `GridMap`, `UserData`, `Markable`, `MapDrawer`, `MapPrinter`) | 49 | — | — |
| Free helpers (`utils.h`, `bwapiExt.h`, `defs.h`) | 35 | — | — |

**The mutator and internal buckets are empty of usage — zero call sites across 119,000 lines.**
They are BWEM's own construction API, public only because C++ made them so. That confirms by
measurement what §R11 assumed by inspection: `Graph`, `MapImpl`, `MapPrinter`, `MapDrawer`,
`TempAreaInfo` and `BMP` are not bot-facing, and neither are the `Set*`/`Add*` families on the
classes that are.

---

## 3. The usage curve

1,380 call sites across the 91 export candidates.

| Coverage | Entry points |
|---|---|
| 50% | **3** |
| 80% | 8 |
| 90% | 15 |
| 95% | **22** |
| 99% | 36 |
| everything observed | 46 |

Top of the list, and it is a very short head:

| | sites | bots | class |
|---|---|---|---|
| `Center` | 372 | 3 | `ChokePoint`, `Base`, `Map` |
| `Get` | 263 | 1 | `MapImpl` |
| `GetArea` | 175 | 2 | `Map`, `Base` |
| `Location` | 89 | 3 | `Base` |
| `Instance` | 88 | 3 | `Map` |
| `GetAreas` | 64 | 3 | `ChokePoint` |
| `ChokePoints` | 41 | 3 | `Area` |
| `Areas` | 24 | 3 | `Map` |
| `GetNearestArea` | 21 | 2 | `Map` |
| `Geometry` | 17 | 3 | `ChokePoint` |

**Three functions cover half of all BWEM traffic.** The shape of the library in practice is:
find the area for a position, walk its chokepoints, ask a chokepoint where its centre is and
which areas it joins, and ask a base where it is. Everything else is long tail.

---

## 4. The v1 cut

**Ship all 91.** Read accessors plus lifecycle, exclude the 42 mutators/internals, exclude the
six non-exported classes.

Applying the rule established for draw calls — *usage frequency tells you what is safe to merge,
not what is safe to omit* — the 45 read accessors with zero observed call sites still ship. Three
reasons specific to BWEM:

1. **The corpus is three bots.** For BWAPI, R2 had seven and the union was still growing. Three
   C++ BWEM users is a thin sample and the tail is not evidence of absence.
2. **Much of the unused tail is obviously useful and cheap**: `Tiles()`, `MiniTiles()`, `Valid()`,
   `Crop()`, `GroundHeight()`, `Buildable()`, `Doodad()`, `GetPathTo()`, `DistanceFrom()`,
   `Index()`, `GroupId()`, `BaseCount()`, `ChokePointCount()`, the `*GroundPercentage` family.
   These are the primitives a *new* bot in a *new* language would reach for first, and the reason
   the corpus does not use them is partly that C++ bots have their own terrain layers already.
3. **91 functions is not a number worth optimising.** The whole point of a cut line is to avoid
   hand-writing 600 wrappers. At 91 the analysis costs more than the code.

Two items need a decision rather than a default, and both go to R11.3:

- **`Tiles()` and `MiniTiles()`** return `const std::vector<Tile>&` over 65,536 and 1,048,576
  elements. These are not one function each — they are §5.5 bulk grid exports, and *which fields*
  of `Tile`/`MiniTile` to export is a design question. `Tile` has 23 declarations, `MiniTile` 18,
  mostly one-line field reads.
- **`Get`, at 263 sites, is a false positive** — it is `MapImpl::Get`, but the vast majority of
  those hits are bot-local `Get(...)` calls that happen to share the name. The liberal count does
  not disambiguate. Excluded from the design; noted so the ranked table is not read naively.

---

## 5. Answers to the questions R11.1 asked

**Which of the declarations are ever called?** 46 of 133 bot-facing names, 1,380 sites. But the
right denominator is 91, not 133 — 42 of the 133 are mutators and internals with zero usage.

**Which classes are bot-facing and which are internal?** Confirmed by measurement:
bot-facing are `Map`, `Area`, `ChokePoint`, `Base`, `Neutral`/`Mineral`/`Geyser`/`Ressource`/
`StaticBuilding`, `Tile`, `MiniTile`. Not exported: `Graph`, `MapImpl` (an implementation detail
of `Map`), `GridMap`, `UserData`, `Markable`, `MapDrawer`, `MapPrinter`, `TempAreaInfo`, `BMP` —
49 declarations, zero usage.

**Graph traversal versus grid queries?** Overwhelmingly graph traversal: `Center`, `GetArea`,
`Location`, `GetAreas`, `ChokePoints`, `Areas`, `GetNearestArea`, `Geometry`, `Top`, `Bases`,
`AccessibleNeighbours` are the working set. Grid access (`GetMiniTile` 14, `GetTile` 2,
`Altitude` 12, `Walkable` 6, `Tiles`/`MiniTiles` 0) is comparatively rare — **which is itself a
finding for §5.5**: the bulk-grid export matters less for BWEM than expected, and per-tile
handle access may be adequate. R11.3 should not over-invest in grid marshalling.

**Is `GetPath` used, and how?** Yes — 10 sites across all three bots. Low frequency, high value:
it is called for route planning, not per frame. A caller-buffer of chokepoint IDs is the right
shape and the cost is irrelevant at ten call sites.

**Does anyone use `BreadthFirstSearch`, `GridMap`, or `Markable`?** **No — zero call sites for
all three.** The template surface can be dropped without a compatibility story, exactly as R11
proposed.

---

## 6. Feeds forward

| To | Finding |
|---|---|
| **R11.2** | The three ports should be checked against this 91-function cut. If JBWAPI's 5,415 lines expose materially more, find out what and why |
| **R11.3** | `Tile`/`MiniTile` grid export is *less* load-bearing than assumed — design it, but do not lead with it. `Center`/`GetArea`/`Location`/`GetAreas`/`ChokePoints` are the hot path and deserve the ergonomic attention |
| **R11.3** | `Base` has no native integer ID (`Area::id` and `ChokePoint::Index()` do). `Location` at 89 sites makes `Base` a first-class handle type, so an ID has to be synthesised |
| **R11.4** | The export set is 91 functions over 12 classes — small enough that the link closure, not the surface, is the risk |
| **§5.5** | Second workload, and it argues the opposite way from BWAPI's walkability grid. Worth noting when §5.5 is finalised |
