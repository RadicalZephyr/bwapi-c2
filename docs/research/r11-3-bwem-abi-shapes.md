# R11.3 — Mapping BWEM's shapes onto the §4 conventions

Deliverable: [`r11/bwapi_c2_bwem.h.sketch`](r11/bwapi_c2_bwem.h.sketch) — **98 signatures, no
implementation.**

**Headline: §4 covers BWEM with no amendments. Every shape maps, most of them more naturally
than BWAPI's did, because BWEM already thinks in integer IDs. Three things need a decision rather
than a translation — a synthesised `Base` id, the `Neutral` handle space, and the three-phase
init — and all three have an obvious answer. The one place BWEM raises the bar is exceptions:
`BWEM::Exception` derives from `std::runtime_error` and is thrown by `bwem_assert_throw`, so
`noexcept` boundaries stop being good practice and become mandatory.**

---

## 1. The mapping

| BWEM shape | §4 answer | Notes |
|---|---|---|
| `Area::id` (`int16_t`, 1-based) | `int32_t` handle, **BWEM's own numbering kept** | `0` already means "not walkable" and `-1` "missing"; both become documented neutral values |
| `ChokePoint::index` (`int`) | `int32_t` handle, BWEM's own numbering | `Index()` is already an array index into the path matrix |
| **`Base` — no id at all** | **Synthesised flat `int32_t`, 0..`BaseCount()-1`** | *Decision below* |
| `Neutral*` / `Mineral*` / `Geyser*` / `StaticBuilding*` | **Addressed by their BWAPI unit id** | *Decision below* — this is the join between the two headers |
| `const Area*` / `const ChokePoint*` / `const Base*` returns | id, or `BWAPI_BWEM_ID_NONE` | Straight §4 |
| `Area::ChokePoints()` → `vector<const ChokePoint*>` | Caller buffer of ids + true-count, ID-sorted | Straight §4 |
| `Map::GetPath(a,b,int*)` → `const CPPath&` | Caller buffer of choke ids + true-count, **plus `int32_t* out_length`** | BWEM's own out-param survives as an out-param |
| `ChokePoint::GetAreas()` → `pair<const Area*, const Area*>` | Two `int32_t*` out-params | §4 forbids struct returns; a pair of ids is two out-params |
| `ChokePoint::Geometry()` → `deque<WalkPosition>` | Caller buffer of **packed `int64_t`** positions | First use of a packed-position *array* — consistent with §4, worth noting |
| `ChokePoint::Pos(node)`, `enum node{end1,middle,end2}` | `int32_t node` + three `#define`d constants | §4's no-enums-in-signatures rule |
| `Neutral::IsMineral()`/`IsGeyser()`/`IsStaticBuilding()` | One `bwapi_bwem_neutral_kind()` returning a discriminator | Three RTTI downcasts collapse to one accessor |
| `Tile` / `MiniTile` field reads | Scalar `(tx,ty)` / `(wx,wy)` accessors | The shape all three ports shipped (R11.2) |
| `Map::Tiles()` / `MiniTiles()` | **Optional** §5.5 bulk grids, six of them | Secondary, not primary — see §3 |
| `Map::Instance()` | Process-wide singleton | Same statement §4 already makes for `BroodwarPtr` |
| `altitude_t` (`int16_t`) | `int32_t` in signatures, `int16_t` in bulk grids | §4's uniform-scalar-width rule; grids follow §5.5's byte-economy rule |
| `BreadthFirstSearch`, `GridMap`, `Markable`, `UserData` | **Not exported** | Zero call sites (R11.1); hosts traverse the exported grids |
| `Graph`, `MapImpl`, `MapPrinter`, `MapDrawer`, `TempAreaInfo` | **Not exported** | Zero call sites |
| 27 mutators + 15 internals | **Not exported** | Not merely unused — *unnecessary*, since no host runs the analysis (R11.2) |

**No new §4 convention is required.** That is a real result: §4 was designed against one library
and holds unmodified against a second with a different object model.

---

## 2. The three decisions

### `Base` needs a synthesised id — flat, global, assigned at init

`Base` is the only bot-facing class with no identity of its own. It is stored **by value** inside
`Area::Bases()` (`std::vector<Base>`), so a pointer is not stable across anything and there is no
index to borrow.

Two candidates: `(area_id, index_within_area)` as a pair, or a flat global index.

**Recommend flat global**, 0..`BaseCount()-1`, assigned once during `bwapi_bwem_initialize()`:

- `Map::BaseCount()` already exists, so the space is well-defined and enumerable.
- A single `int32_t` matches every other handle in both headers; a pair does not, and would be
  the only two-part handle in the ABI.
- `Base::Location()` is the fourth most-used BWEM function (89 sites, all three bots), so bases
  are a first-class navigation target and deserve a first-class handle.

The cost is that the ABI owns a mapping table BWEM does not have. It is one `vector<const Base*>`
built at init, and it must be rebuilt on `bwapi_bwem_reset()`.

### `Neutral` is addressed by its BWAPI unit id

`Neutral::Unit()` returns a `BWAPI::Unit`, and every mineral, geyser and static building BWEM
knows about is a BWAPI unit. So the handle already exists — in the *other* header.

**Recommend using it directly.** `bwapi_bwem_neutral_pos(unit_id)`,
`bwapi_bwem_area_get_minerals(area_id, out_unit_ids, cap)`.

This is the cross-API join R11 anticipated, and it is better than a separate neutral-id space:

- A host that has a `bwapi_unit_id` from the BWAPI header can immediately ask BWEM about it, and
  vice versa, with no lookup table on either side.
- `Map::GetMineral(BWAPI::Unit)` / `GetGeyser(BWAPI::Unit)` become unnecessary — they *are* the
  identity function under this scheme.
- It is the one place the two headers deliberately share a handle space, and the header must say
  so explicitly, because §4's general rule is that handle spaces are disjoint.

**Caveat to resolve in R11.5:** BWEM keeps `Neutral` objects alive after the unit is destroyed
until `OnMineralDestroyed` is called. So a BWEM neutral id can outlive the BWAPI unit id briefly.
Documented, and the reason `bwapi_bwem_neutral_exists()` is in the sketch.

### Three-phase init collapses to one call

BWEM requires `Initialize(game)`, then `EnableAutomaticPathAnalysis()`, then
`FindBasesForStartingLocations()`, in that order.

**Recommend one entry point with two flags:**

```c
int32_t bwapi_bwem_initialize(int32_t enable_path_analysis,
                              int32_t find_bases_for_start_locations);
```

An ABI should make ordering errors impossible rather than diagnosable, and there is no use case
for the intermediate states — all three bots in the corpus call all three in sequence. The flags
preserve the choice without exposing the ordering. `Initialize` also takes a `BWAPI::Game*`, which
we supply internally from our own `GameImpl*`; the host never sees it.

---

## 3. Grids are the secondary shape, not the primary

R11.1 measured grid access at ~2% of BWEM traffic; R11.2 found no port exposes bulk grids. So the
sketch leads with **14 scalar tile/minitile accessors** — the shape every port shipped — and adds
**six bulk grid exports as an optional fast path**:

```c
int32_t bwapi_bwem_grid_minitile_altitude(int16_t* out, int32_t cap);   /* 1024*1024 */
int32_t bwapi_bwem_grid_minitile_walkable(uint8_t* out, int32_t cap);
```

One grid per field rather than a struct-of-arrays, following §4's bulk-boolean rule (`uint8_t`
per element, not `int32_t`) and §5.5's byte economy. A host that wants terrain analysis pulls
three grids once at match start; a host that wants one tile calls one function.

**This is a correction to the R11 plan's expectation**, which assumed the grids would be the
interesting part. They are not, and over-investing in grid marshalling would be the wrong place
to spend the design budget.

---

## 4. Exceptions: the one place BWEM raises the bar

```cpp
class Exception : public std::runtime_error { ... };
#define bwem_assert_throw_plus(expr, message) \
    ((expr)?(void)0:detail::onAssertThrowFailed(__FILE__,__LINE__, #expr, message))
```

**BWEM throws, by design, from assertion macros compiled into release builds.**

R6 found BWAPI's client closure contains **zero** `try`, `catch` or `noexcept` — so for the BWAPI
header, exception safety at the boundary was a should. For BWEM it is a must:

- Every exported `bwapi_bwem_*` function gets a `noexcept` boundary with a `catch (const std::exception&)`
  that latches `BWAPI_ERR_*` and returns the documented neutral value.
- `BWEM::Exception::what()` carries a useful message; route it to §4's
  `bwapi_last_error_message()` snprintf channel rather than discarding it.
- The generator emits the wrapper, so this costs one template, not 98 hand-written try blocks.

JBWAPI's tracker shows what this protects against: **#34** (BWEM has no areas — an intermittent
crash in 1 in 10 to 1 in 20 games) and **#51** (`IllegalStateException` on consecutive games) are
both assertion failures escaping into the bot. Under this ABI they become a latched error code and
a neutral return, which is exactly §4's headline safety promise applied to a library that
genuinely needs it.

---

## 5. Answers to the questions R11.3 asked

**Does BWEM throw across our boundary, and where?** Yes — `BWEM::Exception` from
`bwem_assert_throw`, anywhere an invariant fails, including in release builds. Mandatory
`noexcept` boundaries with error-latch mapping.

**Does anything need a new §4 convention?** **No.** Every shape maps onto an existing rule. The
closest to novel is a caller buffer of *packed* `int64_t` positions (`Geometry`), which is a
combination of two existing rules rather than a new one.

**Are the two headers' handle spaces disjoint, and is that stated?** Mostly disjoint —
`bwapi_bwem_area_id`, `_choke_id`, `_base_id` are BWEM-only — with **one deliberate exception**:
neutrals are addressed by their BWAPI unit id, because that is what they are. The header states
it at the top; §4 should carry the same note so the general disjointness rule is not read as
absolute.

---

## 6. Feeds forward

| To | Finding |
|---|---|
| **R11.4** | The sketch is 98 functions over 12 classes. The `Base` id table and the neutral-id join are ABI-side state, so the wrapper has a little state of its own — check it does not complicate the link |
| **R11.5** | `bwapi_bwem_reset()` must rebuild the base-id table. Neutral ids outliving their units needs a documented lifetime |
| **R11.6** | 98 signatures is a small enough surface that a synthetic-terrain fixture can plausibly exercise most of it |
| **§4** | Add a note that handle spaces are disjoint *except* where a BWEM object is a BWAPI unit, where the unit id is shared deliberately |
| **§9 / generator** | The `noexcept` + error-latch wrapper is a per-function template the generator emits. Same machinery as the BWAPI layer, no new emitter |
