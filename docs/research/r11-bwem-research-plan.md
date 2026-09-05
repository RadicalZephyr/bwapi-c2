# R11. Research plan: wrapping BWEM

**Decision already taken, and this plan does not revisit it:** BWEM is in scope. A bot written
against `bwapi-c2` without map analysis starts behind an equivalent C++ bot on day one, and the
alternative — every host ecosystem wrapping or reimplementing BWEM separately — is the same
duplicated-effort trap §5.8 exists to close, and which R2/R3 measured five times over. BWEM is
fixed, small, permissively licensed, and universally needed. It gets wrapped.

**What this plan settles is *how*:** which subset, in what shape, linked how, and what it costs.

Shape constraints fixed in advance, so the research does not relitigate them:

- **A second header**, `bwapi_c2_bwem.h`, with its own `.def`-listed export block. Not a
  separate DLL unless R11.4 finds a reason.
- **The same §4 conventions.** Integer handles, `int32_t` booleans, packed `int64_t` positions,
  caller-provided buffers with true-count returns, ID-sorted output, size-prefixed PODs, the
  sticky error latch, plain `const char*`. No exceptions for BWEM.
- **Symbol prefix `bwapi_bwem_`**, consistent with R10's reasoning: the prefix names the API being
  wrapped, not the project wrapping it.

---

## What is already known

Gathered while scoping this plan, so the experiments start from facts rather than from zero.

| | |
|---|---|
| Canonical repo | **`N00byEdge/BWEM-community`** — the maintained fork; last commit 2021-06-01 |
| Size | **5,222 lines** C++ across 15 headers and 9 sources |
| **License** | **MIT/X11**, Igor Dimitrijevic, 2015/2017 — *permissive, LGPL-compatible, vendorable* |
| Declarations | ~308 across the headers; 21 classes, of which roughly half are internal |
| Bot-facing classes | `Map`, `Area`, `ChokePoint`, `Base`, `Neutral`/`Mineral`/`Geyser`/`StaticBuilding`, `Tile`, `MiniTile` |
| Internal / tooling | `Graph`, `MapImpl`, `MapPrinter`, `MapDrawer`, `TempAreaInfo`, `GridMap`, `Markable`, `BMP` |
| Existing ports | JBWAPI `bwem` **5,415 lines**; rsbwapi `src/bwem` **2,496**; gobwapi `pkg/bwem` **2,004**; JBWAPI also carries a 409-line `bwta` shim |

**Three properties make this much more tractable than BWAPI was**, and the plan should exploit
all three:

1. **BWEM already uses integer handles.** `Area::id` is an `int16_t`, `Map::GetArea(Area::id)`
   exists, and `ChokePoint::Index()` is an array index into `m_PathsBetweenChokePoints`. §4's
   handle convention is BWEM's own model, not an imposition.
2. **`Map::Initialize(BWAPI::Game* game)` takes the game explicitly** — no `Broodwar` global
   reads anywhere in the sources. It composes with our closure, which already owns a `GameImpl*`.
3. **The heavy data is already grid-shaped.** `Tiles()` returns `const std::vector<Tile>&` over a
   256×256 grid and `MiniTiles()` a 1024×1024 one. That is exactly §5.5's bulk-grid case, already
   designed.

**And three that will cost work:**

- `Map::Instance()` is a singleton with a **multi-phase init** (`Initialize`, then
  `EnableAutomaticPathAnalysis`, then `FindBasesForStartingLocations`), and analysis is expensive.
- `GetPath(a, b, int* pLength)` returns `const CPPath&` = `std::vector<const ChokePoint*>`.
- `BreadthFirstSearch` is a template over two predicates, and `GridMap`/`Markable`/`UserData` are
  templates. Some of the surface is not wrappable and should not be.

---

## Ordering

| # | Item | Timebox | Blocks |
|---|---|---|---|
| R11.1 | Size the real BWEM surface from the same bot corpus | 3–4 h | The v1 cut for `bwapi_c2_bwem.h` |
| R11.2 | Study the three existing ports as prior art | 2–3 h | Which subset is load-bearing; what they got wrong |
| R11.3 | Map BWEM's shapes onto the §4 conventions | 3 h | The header design |
| R11.4 | Build and link BWEM against our closure | half day | Whether one DLL or two; the CMake story |
| R11.5 | Lifecycle, cost and the frame loop | 2–3 h | §4.1 integration; what a host must call and when |
| R11.6 | Test substrate for a map-analysis library | 2 h | Whether R7's synthetic-fixture approach reaches BWEM |
| R11.7 | Licensing and vendoring | 1 h | Vendor vs submodule; notice obligations |
| R11.8 | BWTA2: settle it and close it | 1 h | Whether a second map library is even a question |

Roughly two days. R11.1 and R11.4 are the ones that can change the shape of the answer.

---

## R11.1 Size the real BWEM surface

Same method as R2, same corpus, so the numbers are comparable.

**Method.** The R2 corpus already contains heavy BWEM users — Steamhammer, UAlbertaBot (BWTA),
RazeAndPlunder (bundles `deps/BWEM-1.3.1`), plus Styx2 and the JBWAPI-based bots. Extract the
BWEM API inventory from `BWEM-community`'s headers the way R2 did for BWAPI, then count call
sites. Add the three ports as a cross-check: what a port chose to expose is itself usage data.

**Answer these:**

- Which of the ~308 declarations are ever called? R2's prior for BWAPI was that 60% are used and
  the top 195 cover 95%; BWEM is smaller and more focused, so expect a higher hit rate and a
  smaller tail.
- **Which classes are bot-facing and which are internal?** My reading is that `Graph`, `MapImpl`,
  `MapPrinter`, `MapDrawer`, `TempAreaInfo` and `BMP` are implementation or tooling and should not
  be exported at all. Confirm against call sites rather than by inspection.
- How much of the usage is `Area`/`ChokePoint`/`Base` graph traversal versus `Tile`/`MiniTile`
  grid queries? That decides how much of the design is §5.5 bulk grids and how much is handle
  navigation.
- Is `GetPath` used, and how? Per-frame or cached at match start?
- Does anyone use `BreadthFirstSearch`, `GridMap`, or `Markable` — the template surface we would
  have to drop?

**Deliverable.** A frequency-ranked BWEM function list and a proposed v1 cut, in the same format
as `r2-frequency-ranked-api.md`.

**Note the scope rule from the BWAPI side applies here too:** usage frequency tells us what is
safe to *merge*, not what is safe to *omit*. Expect the cut line to be "everything bot-facing"
with the internal classes excluded, not a percentile.

---

## R11.2 Study the three existing ports

JBWAPI (5,415 lines), rsbwapi (2,496) and gobwapi (2,004) have each already answered "what does a
non-C++ consumer need from BWEM." The three-way size spread is itself the interesting datum.

**Answer these:**

- **Why is JBWAPI's port 2.7× rsbwapi's?** Completeness, idiom, or duplicated internals? If the
  smaller ports are usable, the smaller surface is the right target.
- What did each port **skip**, and did anyone complain? JBWAPI's tracker has BWEM issues (#34
  "BWEM sometimes has no areas", #51 `IllegalStateException` on game start, #86 "Investigate BWEM
  issue with New Popular Fastest Map") — read them. They are the divergence-bug evidence R4 found
  for the BWAPI layer, applied to BWEM.
- Do the ports **re-run the analysis** in the host language, or read results out? All three
  re-implement — which is the whole reason this project exists — so the question is what
  *interface* they settled on, not their internals.
- Does any port expose `Tile`/`MiniTile` grids in bulk, or one call per tile? That is a direct
  test of §5.5's design against a second workload.

**Deliverable.** A comparison table and a statement of the common subset all three expose — which
is the strongest available evidence for the v1 cut.

---

## R11.3 Map BWEM's shapes onto the §4 conventions

The design work. Each BWEM shape gets a §4 answer, and the ones that do not fit get named.

**Work through at minimum:**

| BWEM shape | Candidate §4 answer | Open question |
|---|---|---|
| `Area::id` (`int16_t`), `ChokePoint::Index()` | Already integer handles — widen to `int32_t`, keep BWEM's numbering | Do we keep BWEM's IDs verbatim so a host can cross-reference, or renumber? Keep, presumably |
| `const Area*`, `const ChokePoint*`, `const Base*` returns | Integer handles | `Base` has no native ID — do we synthesise one, and is it stable across the match? |
| `GetPath(a,b) -> const CPPath&` | Caller buffer of chokepoint IDs + true-count return, plus the `int* pLength` out-param | Is the path stable enough to hand out IDs, or must it be copied? |
| `Tiles()` / `MiniTiles()` — 256×256 and 1024×1024 | §5.5 bulk grid export, `uint8_t` per element per field | Which `Tile`/`MiniTile` fields does a bot actually read (R11.1)? One grid per field, or one size-prefixed struct array? |
| `Minerals()`/`Geysers()`/`StaticBuildings()` (`vector<unique_ptr<>>`) | Index handles; ownership stays in BWEM | These alias BWAPI units — expose the BWAPI unit ID alongside so a host can join the two APIs |
| `Area::AccessibleNeighbours()`, `ChokePoints()` | Caller buffer + count, ID-sorted per §4 | — |
| `GetMineral(BWAPI::Unit)` / `GetGeyser(BWAPI::Unit)` | Takes a BWAPI unit handle, returns a BWEM handle | **The cross-API join.** Confirms the two headers must share the handle space |
| `Map::Instance()` | Process-wide singleton — same statement as §4 | Consistent; just document it |
| `BreadthFirstSearch`, `GridMap`, `Markable`, `UserData` | **Not exposed** | Host does its own traversal over the exported grids. Confirm R11.1 shows nobody needs them |
| `MapDrawer` / `MapPrinter` | **Not exposed** | Debug tooling; the host draws via `bwapi_game_draw_*` |
| `Exception` | Caught at the boundary, mapped to the sticky error latch | BWEM throws — R6 found BWAPI's closure has no `try`/`catch` at all. **BWEM makes exception safety mandatory, not optional** |

**Answer these:**

- **Does BWEM throw across our boundary, and where?** `bwem::Exception` exists. Every exported
  BWEM function needs a `noexcept` boundary and a mapping into the error latch. This is the one
  place BWEM raises the bar above the BWAPI layer.
- Does anything need a *new* §4 convention, or does the existing set cover it? If BWEM needs a
  new convention, that is a finding about §4, not just about BWEM.
- Are the two headers' handle spaces disjoint, and is that stated? A `bwapi_unit_id` and a
  `bwapi_bwem_area_id` are both `int32_t` and must not be confusable.

**Deliverable.** A draft `bwapi_c2_bwem.h` sketch — signatures only, no implementation — plus a
list of any §4 amendments BWEM forces.

---

## R11.4 Build and link BWEM against our closure

The question the decision actually hangs on: **does BWEM link cleanly against the R6 closure, or
does it drag in the parts of BWAPI we excluded?**

**Method.** Extend `r6/derive-closure.sh`. Compile `BWEM/src/*.cpp` against the closure's include
set, archive both, and let the linker enumerate what is undefined. BWEM is MIT and 5,222 lines,
so vendoring or submoduling are both open.

**Answer these:**

- **What BWAPI surface does BWEM use?** `bwapiExt.h` is 153 lines and is the whole coupling
  point. Is it inside R6's 44-TU closure, or does it need `Util/`, Storm, or the injected-DLL
  side? *This is the finding that matters* — if BWEM needs something the client closure excludes,
  the whole shape changes.
- Does BWEM compile under the same non-MSVC workarounds R6 needed (`-fdelayed-template-parsing`,
  the `va_list` fix), or does it introduce more MSVC-isms?
- Does it build x64? R5 settled BWAPI's layout; BWEM has its own structs and its own
  `int16_t`/`altitude_t` types.
- **One DLL or two?** Default to one — `bwapi_c2.dll` exporting both headers' symbols — unless
  BWEM's analysis cost or dependency set argues for a separate `bwapi_c2_bwem.dll` a host can
  choose not to load.
- Vendor as a subdirectory or pin as a submodule? BWEM's last commit is 2021-06-01, so pinning is
  nearly free either way; MIT means vendoring is permitted.

**Deliverable.** A working link, an undefined-symbol report, and a one-DLL-or-two recommendation
with the reason.

---

## R11.5 Lifecycle, cost, and the frame loop

BWEM is not stateless and its analysis is not cheap. §4.1's frame loop currently knows nothing
about it.

**Answer these:**

- **How long does `Initialize` + `FindBasesForStartingLocations` take** on a standard ladder map?
  Measure it. If it is seconds, the host needs to know when to call it and that the call blocks.
- What is the required call order, and what happens if a host gets it wrong? Three phases with an
  ordering dependency is exactly the kind of thing an ABI should make hard to misuse — a single
  `bwapi_bwem_initialize()` that does all three, or explicit phases with the latch catching
  out-of-order calls?
- **What must be called per frame or on events?** `OnMineralDestroyed` and
  `OnStaticBuildingDestroyed` exist and must be driven by the host from BWAPI events. Does §4.1's
  frame loop grow a BWEM section, and can we drive those internally from our own event pump
  instead of making the host do it?
- Is any BWEM state invalidated mid-match, and do handles stay stable across it? If an `Area::id`
  can change, the handle convention needs a statement.
- Is BWEM re-entrant or thread-affine? §4's process-wide-singleton statement should extend to it.

**Deliverable.** A BWEM section for §4.1, and a statement of the handle-stability guarantee.

---

## R11.6 Test substrate

R7 established that the BWAPI layer is testable from a synthetic `GameData` with no StarCraft.
BWEM is harder: it needs real terrain to analyse.

**Answer these:**

- Can BWEM be initialised from a **synthetic** `GameData` with hand-built walkability grids?
  R7's harness already populates `isWalkable[1024][1024]`. A small artificial map — two plateaus
  and a ramp — would exercise areas, chokepoints and a path, with no Blizzard data at all.
- If synthetic terrain works, that is the whole test story and it inherits R9's clean provenance.
  If it does not, what is the minimum real input — and does that reintroduce the map-copyright
  question R9 §7 identified?
- Does `BWEM-community` ship its own tests (`tests/`, `Tests/`) and are they usable as ours?

**Deliverable.** Either a synthetic-terrain fixture that produces a non-trivial area graph, or a
statement of why not and what it costs.

---

## R11.7 Licensing and vendoring

Short, because the answer looks easy and should be confirmed rather than assumed.

- BWEM is **MIT/X11** — compatible with LGPL-3.0 and vendorable. Confirm the fork's license is
  unmodified from the original and that no contributed file carries different terms.
- Note the MIT attribution requirement: the copyright notice must ship in the release asset.
  Extend R9 §5's file table with `LICENSE.BWEM`.
- Confirm the *combined* distribution story: an LGPL-3.0 DLL containing MIT code is fine; the
  reverse would not be. State it once so it is not re-derived.
- Check `BWEM/external/` for anything with its own terms.

---

## R11.8 BWTA2: settle it and close it

BWTA2 is the older map-analysis library, and JBWAPI still carries a 409-line `bwta` shim.

**Answer these:** is BWTA2 still used by any active bot; is it maintained; does anything it
provides have no BWEM equivalent? Expected answer is that BWEM superseded it and BWTA2 is a
compatibility shim. **If so, record it as an explicit non-goal and stop.** The point of the
experiment is to close the question, not to open a second one.

---

## Decision framework

| Direction | What it means | Findings that support it |
|---|---|---|
| **A. One DLL, full bot-facing surface** | BWEM vendored, `bwapi_c2_bwem.h` exports the `Map`/`Area`/`ChokePoint`/`Base`/`Neutral` graph plus §5.5 grids, one `bwapi_c2.dll` | R11.4 links cleanly against the R6 closure; R11.1's cut is the bot-facing classes; R11.6 gives synthetic terrain |
| **B. Two DLLs** | `bwapi_c2_bwem.dll` separate, optional to load | R11.4 finds BWEM drags in dependencies the client closure excludes, or R11.5 finds the analysis cost warrants making it opt-in |
| **C. Grids only** | Export `Tile`/`MiniTile` grids and BWAPI's own `Region` data; let hosts run their own analysis | R11.1 shows the area/chokepoint graph is lightly used, or R11.4 shows the link is bad. **Weak** — it recreates the duplicated-effort problem this decision exists to avoid |

**Bias to record:** direction A is the expected answer and the one the decision to wrap BWEM
already implies. The research exists to find the link-closure or lifecycle surprise that would
force B, and to fix the surface and the shapes. **If R11.4 comes back clean and R11.1's cut is
"the bot-facing classes", this is a straightforward extension of work already designed, and the
plan should say so rather than manufacture a third phase.**

**What is not in question**, whichever direction wins: BWEM is in scope, it uses the §4
conventions, it ships as a second header, and its symbol prefix is `bwapi_bwem_`.
