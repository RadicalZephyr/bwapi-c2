# R11.6 — Test substrate for a map-analysis library

Reproducible via [`r11/run-bwem-fixture.sh`](r11/run-bwem-fixture.sh) and
[`r11/bwem_fixture.cpp`](r11/bwem_fixture.cpp).

**Headline: synthetic terrain works completely. BWEM's full analysis runs from a hand-built
`GameData` — two areas, one chokepoint, two bases with four minerals each, a working `GetPath`
across the wall — with no StarCraft, no MPQs, no map file and no Blizzard data of any kind. R9's
provenance question disappears entirely. And building the fixture found a real upstream bug that
no amount of reading would have: BWEM crashes when re-initialised on any map that has neutrals,
which is every map.**

---

## 1. It works

```
units=12  events=12  neutralUnits=12  minerals=12  staticNeutrals=12
synthetic map 128x128 tiles (512x512 walk), 2 start locations
Initialize()                       443.3 ms
Initialized=1  areas=2  chokepoints=1  bases=2  maxAltitude=977
  area  1: group=1 miniTiles=121051 chokes=1 bases=1 top=(381,366)
  area  2: group=1 miniTiles=120997 chokes=1 bases=1 top=(130,295)
GetPath across the wall: 1 chokepoints, length=2816
minerals known to BWEM: 12   geysers: 0
  base in area 1 at (102,105) starting=0 minerals=4
  base in area 2 at (20,7) starting=0 minerals=4
```

The terrain is thirty lines of C: a walkable field, an unwalkable border, a full-height wall with
one narrow gap, and a raised plateau in each half. BWEM's watershed finds the two areas, places
the chokepoint in the gap, and `GetPath` routes through it.

**BWEM reads exactly six things from BWAPI**, and `GameData` carries all six:

| BWEM input | `GameData` field |
|---|---|
| `mapWidth()`, `mapHeight()` | `mapWidth`, `mapHeight` |
| `getStartLocations()` | `startLocations[8]`, `startLocationCount` |
| `isWalkable(w)` | `isWalkable[1024][1024]` |
| `isBuildable(t)` | `isBuildable[256][256]` |
| `getGroundHeight(t)` | `getGroundHeight[256][256]` |
| `getStaticNeutralUnits()` | `units[]` + the event stream (below) |

So the R7 harness extends to BWEM with no new mechanism — it is the same synthetic `GameData`,
with three grids and some units filled in.

---

## 2. Two things the fixture had to learn, both worth writing down

Neither is obvious from reading the headers, and both would trip anyone building this later.

**Neutral units arrive through the event stream, not the unit array.** `GameImpl::onMatchFrame`
builds `neutralUnits`, `minerals` and `geysers` by walking `data->events[]` for
`EventType::UnitDiscover` — *not* by scanning `data->units[]`. A fixture that populates units but
no events gets `staticNeutrals=0` and BWEM sees an empty map. The fix is three lines synthesising
one `UnitDiscover` per unit.

**`isNeutral()` reads a flag, not the player type.** `PlayerImpl::isNeutral()` is
`return self->isNeutral;` — a `PlayerData` boolean. Setting `players[11].type = PlayerTypes::Neutral`
is not enough; `players[11].isNeutral = true` is what matters. Without it every mineral is
classified as a player unit and BWEM builds no bases.

Both belong in whatever fixture-builder helper the test suite grows, so they are solved once.

---

## 3. The bug the experiment found

Adding neutrals turned the earlier "re-initialisation works" result (R11.5 §2) into a crash:

```
Program received signal SIGSEGV
#0  BWEM::Neutral::~Neutral()
#1  BWEM::detail::MapImpl::~MapImpl()
#2  BWEM::detail::MapImpl::Initialize(BWAPI::Game*)
```

**Diagnosis.** `MapImpl::Initialize` resets in place:

```cpp
void MapImpl::Initialize(BWAPI::Game *game) { this->~MapImpl(); new (this) MapImpl(); ... }
```

and `~MapImpl` destroys `m_Minerals` / `m_Geysers` / `m_StaticBuildings`, whose element
destructor reaches *back into the Map*:

```cpp
Neutral::~Neutral() { RemoveFromTiles(); if (Blocking()) ...OnBlockingNeutralDestroyed(this); }
void Neutral::RemoveFromTiles() { auto & tile = MapImpl::Get(GetMap())->GetTile_(...); }
```

By then the Map's tile storage is gone. **Use-after-free during the object's own destruction.**
It is invisible on a map with no neutrals — which is why R11.5, measured before minerals were
added, reported success.

**The same root cause bites a second way**: BWEM's singleton is destroyed by static destruction at
process exit, after the host has released its `GameData`. The fixture segfaults on a normal
`return 0` and exits cleanly only when static destruction is skipped.

**This is upstream, and Stardust already worked around it.** `BWEM-community` has no
`Map::ResetInstance`; the BWEM variant Stardust vendors adds exactly one:

```cpp
void Map::ResetInstance() { m_gInstance = nullptr; }
```

Dropping the whole `unique_ptr` destroys members in a defined order instead of in place.

**Three consequences for the ABI:**

1. **`bwapi_bwem_reset()` must drop the singleton, not re-run `Initialize`.** `BWEM-community`
   lacks the method, so we either carry a one-line patch on the pinned submodule or upstream it.
   Given MIT and a dormant upstream, a pinned patch is the pragmatic answer — recorded in the
   divergence register (§15).
2. **The wrapper must tear BWEM down explicitly**, before the `GameData` mapping goes away and
   before static destruction. `bwapi_client_disconnect()` grows a BWEM teardown step.
3. **R11.5's retraction stands**: JBWAPI #51 (`IllegalStateException` on consecutive games) is
   plausibly *inherited* rather than a port bug. R11.2 counted it as evidence for wrapping the
   real thing; it is weaker evidence than it looked, and R11.2's §4 should be read with that
   caveat. The #27 map-checklist finding is unaffected and remains the strong one.

---

## 4. What BWEM ships as tests — and it reached both of our conclusions

`BWEM-community` carries **two** test approaches, and between them they independently validate
this experiment and R7's.

**`tests/DummyBWAPIMap.h` — 231 lines subclassing `BWAPI::Game` and stubbing the whole
interface.** Same idea as our fixture, different mechanism: it implements the abstract `Game`
directly (`int mapWidth() const { return 64; }`, `bool isWalkable(int,int) const { return true; }`)
rather than filling a `GameData`. So BWEM's own authors also concluded that synthetic input is the
way to unit-test map analysis.

**Which mechanism should we use? Ours.** Subclassing `BWAPI::Game` is simpler — it sidesteps both
gotchas in §2 — but it **bypasses `GameImpl` entirely**, and `GameImpl` is the code our ABI
actually wraps. A `GameData` fixture exercises the real client path: our closure, the real
accessors, the real event pump. `DummyBWAPIMap` would test BWEM against a mock of BWAPI; we need
to test *our wrapper* against the real client. Worth knowing the alternative exists, and worth not
taking it.

**`Tests/` — googletest against real maps through OpenBW.** `external/googletest`,
`external/openbw`, `external/openbw-bwapi`, and **twelve `.scx` files committed in
`Tests/data/maps`** including `(4)Sparkle 1.1.scx` and `(2)Showdown.scx`. `Tests/MapTest.hpp`
carries `sscaitMaps` lists and runs BWEM over each.

That is the heavyweight path R7 ruled out for our CI, and it is also **R9 §7's provenance question
in the wild**: an MIT-licensed repository shipping twelve community ladder maps as test data. We
should not copy that pattern, and it is a second reason to prefer synthetic terrain — not merely
convenient, but the only option that carries no third-party map data.

The `examples/` tree (`examples.cpp`, `exampleWall.cpp`) is BWAPI-module scaffolding and expects a
running game. Not reusable.

---

## 5. Answers to the questions R11.6 asked

**Can BWEM be initialised from a synthetic `GameData` with hand-built walkability?** Yes,
completely — areas, chokepoints, altitudes, neutrals, bases and paths.

**If synthetic terrain works, is that the whole test story?** Yes, and it inherits R9's clean
provenance: no map file, no Blizzard tables, nothing derived from anything. **R9's open question
about recorded-fixture provenance does not arise for BWEM at all.**

**What is the minimum real input?** None.

**Does BWEM ship usable tests?** Partly, and instructively. `tests/DummyBWAPIMap.h` is the same
synthetic conclusion reached independently, but via a `BWAPI::Game` subclass that bypasses
`GameImpl` — the wrong mechanism for us, since `GameImpl` is what our ABI wraps. `Tests/` runs
googletest against twelve real `.scx` maps through OpenBW, which is both the CI path R7 ruled out
and an instance of R9 §7's map-provenance problem.

---

## 6. Feeds forward

| To | Finding |
|---|---|
| **R11.3 sketch** | `bwapi_bwem_reset()` is `ResetInstance`-shaped, not `Initialize`-shaped. Add an explicit teardown to the disconnect path |
| **R11.7** | We will carry a one-line patch (`Map::ResetInstance`) on the pinned BWEM. MIT permits it; note the modification in the release notice |
| **R11.5** | Corrected in place |
| **R11.2** | JBWAPI #51 downgraded from "port bug" to "plausibly inherited" |
| **§11 testing** | The BWAPI and BWEM fixtures are the same mechanism. One `GameData` builder serves both, and the two gotchas in §2 belong in it |
| **§15 divergence register** | First entry: a pinned patch adding `Map::ResetInstance` to BWEM |
