# R11.5 — Lifecycle, cost, and the frame loop

Measured with [`r11/bwem_fixture.cpp`](r11/bwem_fixture.cpp) — BWEM's full analysis driven from a
synthetic `GameData`, no StarCraft involved.

**Headline: BWEM's analysis costs ~450 ms on a 128×128 map and is entirely a match-start
operation — there is nothing per-frame. Re-initialisation across consecutive matches works
correctly in upstream BWEM (it placement-news itself), so JBWAPI #51 was a port bug rather than
an inherited one. The real lifecycle hazard is different and sharper: `OnMineralDestroyed`
asserts — and therefore throws — if handed a unit BWEM does not know about, which is exactly what
a host forwarding every `onUnitDestroy` event would do.**

---

## 1. Cost: ~450 ms, all of it at match start

Synthetic 128×128 tile map (512×512 walk tiles), two start locations, one wall with one gap:

```
Initialize()                       447.8 ms
EnableAutoPath + FindBases()         0.0 ms
TOTAL                              447.8 ms
re-Initialize (consecutive game)   475.1 ms
```

**All the cost is in `Initialize()`.** `EnableAutomaticPathAnalysis()` only sets a flag, and
`FindBasesForStartingLocations()` was free here because the fixture has no resources for bases
(§4). On a real map with eight bases it will do more, but the shape holds: this is
hundreds of milliseconds, once, at match start.

Where it goes, from `MapImpl::Initialize`: resize the tile and minitile vectors (65,536 and
262,144 entries at this size), `LoadData` (three nested scans over the walk grid),
`DecideSeasOrLakes`, `InitializeNeutrals`, then altitude computation and the area watershed.

**Consequences for the ABI:**

- `bwapi_bwem_initialize()` **blocks for roughly half a second** and the header must say so. A
  host calling it inside `onStart` will lose frames; BWAPI's match-start grace period absorbs it,
  but a host calling it mid-match will stutter.
- It is emphatically **not** a per-frame or lazy-init operation. No "initialise on first query"
  convenience.
- There is **nothing to add to §4.1's frame loop.** BWEM has no per-frame update at all — only
  the three event hooks below. That is a simpler answer than R11 anticipated.

---

## 2. Re-initialisation works, and JBWAPI #51 was a port bug

```cpp
void MapImpl::Initialize(BWAPI::Game *game)
{
    this->~MapImpl();
    new (this) MapImpl();
    ...
```

**BWEM resets itself.** Calling `Initialize` a second time on the same singleton destroys and
placement-news the implementation before rebuilding. Measured: the second call produced the same
two areas and one chokepoint, in 475 ms.

So JBWAPI #51 — `IllegalStateException` when playing consecutive games without restarting — is a
divergence bug in JBWAPI's port, not a defect inherited from BWEM. **That is a point in favour of
wrapping the real thing**, and belongs alongside R11.2's #27 finding.

One wrinkle worth recording: **`Map::ResetInstance()` does not exist in `BWEM-community`.**
Stardust calls it, and it is present in the BWEM variant Stardust vendors (`3rdparty/BWEM`). If
we ever want an explicit teardown separate from re-`Initialize`, it is a small upstream addition —
but `bwapi_bwem_reset()` can simply be "re-run `Initialize`", which is what the sketch should say.

---

## 3. The event hooks, and the hazard in them

BWEM exposes three, and the host must drive all three from BWAPI events:

```cpp
void MapImpl::OnMineralDestroyed(BWAPI::Unit u)
{
    auto iMineral = find_if(m_Minerals.begin(), m_Minerals.end(), ...);
    bwem_assert(iMineral != m_Minerals.end());     // <-- throws
    fast_erase(m_Minerals, ...);
}
```

**`bwem_assert` throws `BWEM::Exception` in release builds** (R11.3 §4). So calling
`OnMineralDestroyed` with a unit BWEM has never heard of — any non-mineral, or a mineral already
removed — **throws**.

That is precisely the mistake a host will make. The natural host code is "on every
`onUnitDestroy`, tell BWEM", and that throws on the first marine.

**Design consequence — the ABI should own this, not the host:**

1. Drive the three hooks **internally** from our own event pump, not from the host. We already
   process BWAPI's event stream; dispatching `UnitDestroy` to BWEM with the right filter is
   something the wrapper can do correctly once, instead of every binding in every language doing
   it wrongly.
2. Keep the three explicit entry points in the header anyway, for hosts that want control — but
   make them **filtered and idempotent**: check membership before forwarding, return 0 rather
   than throwing for a unit BWEM does not track.
3. That makes `bwapi_bwem_on_*_destroyed()` safe to call with any unit id, which is the only
   contract a host can actually satisfy.

This is a case where §4's "never dereference an invalid handle, return a neutral value and latch"
rule extends naturally to a third-party library's assertions.

---

## 4. Handle stability

| Handle | Stable across | Notes |
|---|---|---|
| `area_id` | the whole match | BWEM's own `Area::id`, assigned during `Initialize` and never renumbered. **Not** stable across a re-`Initialize` |
| `choke_id` | the whole match | `ChokePoint::Index()`, likewise |
| `base_id` (synthesised, R11.3) | the whole match | Our table, built at init. **Must be rebuilt on `bwapi_bwem_reset()`** |
| neutral (= BWAPI unit id) | the unit's lifetime | Outlives the unit briefly: BWEM keeps the `Neutral` until `OnMineralDestroyed` is called. `bwapi_bwem_neutral_exists()` covers the gap |

**Nothing is invalidated mid-match** by the event hooks — `OnMineralDestroyed` erases a mineral
from lists but does not renumber areas, chokepoints or bases. Areas keep their ids even when a
blocking neutral is destroyed and two areas become accessible; that is a `GroupId` change, not an
`Id` change.

**The statement for the header:** area, chokepoint and base ids are stable from
`bwapi_bwem_initialize()` until the next `bwapi_bwem_initialize()`. Nothing carries across a
match boundary.

---

## 5. Re-entrancy

`grep -rn 'mutex\|thread\|atomic'` across BWEM's sources and headers: **nothing**. `Map::Instance()`
lazily constructs a `unique_ptr<MapImpl>` with no guard, so even the first call is not thread-safe.

BWEM is a single-threaded, process-wide singleton — **identical to the statement §4 already makes
about `BroodwarPtr`**. No new convention; the existing sentence covers both, and the BWEM header
should repeat it rather than imply anything different.

---

## 6. Answers to the questions R11.5 asked

**How long does initialisation take?** ~450 ms on a 128×128 map, essentially all in `Initialize()`.

**Required call order, and what if a host gets it wrong?** `Initialize` → `EnableAutomaticPathAnalysis`
→ `FindBasesForStartingLocations`. R11.3's single `bwapi_bwem_initialize(int32_t, int32_t)` removes
the question by removing the choice, which §1 above confirms is the right call: there is no use
case for the intermediate states and the whole thing is one blocking operation anyway.

**What must be called per frame or on events?** **Nothing per frame.** Three event hooks —
mineral destroyed, static building destroyed, blocking neutral destroyed — which the wrapper
should drive internally and also expose in filtered, non-throwing form.

**Is BWEM state invalidated mid-match, and do handles stay stable?** No, and yes. Ids are stable
for the match; only a re-`Initialize` renumbers.

**Is BWEM re-entrant or thread-affine?** Neither — a plain unguarded singleton. §4's existing
statement applies unchanged.

---

## 7. Feeds forward

| To | Finding |
|---|---|
| **R11.3 sketch** | Make the three `on_*_destroyed` entry points filtered and idempotent; document `bwapi_bwem_initialize` as blocking for ~half a second; state the id-stability window |
| **R11.6** | The fixture works — full analysis, two areas, one chokepoint, a working `GetPath` — on entirely synthetic terrain. One gap remains: BWEM's *neutral* path is fed from `GameImpl`'s `UnitDiscover` **event stream**, not from `data->units`, so a fixture that wants bases must synthesise events too. Not yet working; R11.6 owns it |
| **§4.1** | No BWEM section needed in the frame loop. Only a match-start note |
| **§9 / generator** | The filtered event hooks are hand-written, not generated — three functions with real logic. Worth flagging as the first genuinely non-mechanical wrapper in either header |
