# R11.9. BWEM teardown re-tested: the R11.6 crash is a fixture artifact

Executed against the pinned submodules in this repository (BWAPI `d727fed`, BWEM `9a63141`)
with the two §15.2 patches applied, on Linux with clang 18 and g++ 13, while landing
implementation-plan step 0.3. Reproduction: [r11/run-bwem-teardown.sh](r11/run-bwem-teardown.sh),
[r11/bwem_teardown.cpp](r11/bwem_teardown.cpp).

**Headline: `Map::ResetInstance()` does not fix the crash R11.6 attributed to BWEM's in-place
reset, because that crash is not caused by the in-place reset.** It is caused by the synthetic
fixture placing 2×1 mineral fields one tile apart, so adjacent fields partially overlap on a
shared tile — a layout BWEM does not support and, in a release build, silently accepts. With
the minerals spaced, upstream's in-place re-`Initialize`, static destruction after the
`GameData` is freed, and `ResetInstance` **all survive**. With R11.6's layout, **all three
crash**, `ResetInstance` included. The patch stays, as a teardown API BWEM lacks; its rationale
in the plan was wrong and is corrected in revision 4.3.

## 1. What was run

R11.6's fixture, with the tail replaced by three teardown protocols and the mineral spacing made
a flag. Six cells:

| Layout | `--in-place` (upstream `Initialize` twice) | `--no-reset` (free `GameData`, `return 0`, static destruction) | `--reset` (`ResetInstance`, re-`Initialize`, `ResetInstance`) |
|---|---|---|---|
| `--overlap` — R11.6's `addMineral(20 + k, 13)`, six fields one tile apart | **SIGSEGV** | **SIGSEGV** | **SIGSEGV** |
| `--spaced` — `addMineral(20 + 2*k, 13)`, one free tile between fields | exit 0, areas=2 | exit 0 | exit 0, areas=2 chokes=1 bases=2 minerals=12 |

The build compiled BWAPI's 44-TU closure and BWEM's 14 TUs **directly from the patched
submodules**, with no shadow copy of `Convenience.h`, which is also the first end-to-end
exercise of the `va_list` patch.

## 2. The mechanism

The backtrace is the same in all three crashing cells:

```
#0 BWEM::Neutral::RemoveFromTiles   neutral.cpp:107   while (pPrevStacked->NextStacked() != this)
#1 BWEM::Neutral::~Neutral          neutral.cpp:47
#8 std::vector<unique_ptr<Mineral>>::~vector
#9 BWEM::detail::MapImpl::~MapImpl  mapImpl.cpp:56
```

A `Resource_Mineral_Field` is 2×1 tiles. Placed at `tx*32 + 32`, field *k* covers tiles
`20+k` and `21+k`, so fields *k* and *k+1* share a tile. `Neutral::PutOnTiles()` handles a
shared tile as a **stack**, and guards the cases it supports with `bwem_assert_plus`: same
type, **same `TopLeft()`**, and stacking only at `dx == 0 && dy == 0`. Here the second and third
fail. But `bwem_assert` is `bwem_assert_plus(expr, "")`, and **`bwem_assert_plus` expands to
nothing** unless `BWEM_ASSERTS` or `BWEM_TRACE` is defined (`defs.h:28-39`). Only the
`bwem_assert_throw` family throws in release. So the misaligned stack is built silently:
field *k*'s `m_pNextStacked` points at field *k+1*, which does not have the same tiles.

On destruction, `m_Minerals` is destroyed in order. Field 0 removes itself from tiles 20 and 21;
at tile 21 it is the head, so it installs its `m_pNextStacked` (field 1) as the new head — but
field 1 is also already on tile 22 as head, and at tile 21 it never expected to be first. Field
1 then removes itself: at tile 21 it is the head, fine; at tile 22, the head is field 1 too.
Field 2, however, finds tile 22's head to be field 1 — **already destroyed** — and
`RemoveFromTiles` walks `pPrevStacked->NextStacked()` through freed memory. The exact sequence
matters less than the shape: **a partially overlapping stack leaves a dangling `m_pNextStacked`
during teardown, on every teardown path**, because every path destroys the same vector.

R11.6 read the crash as "`~Neutral` reaches back into the Map's already-destroyed tile storage",
reasoned that dropping the `unique_ptr` would destroy members in a defined order, and did not
run the fix. The tile storage was never the problem: `m_Tiles` is a member of the base `Map`,
destroyed after every `MapImpl` member, in every protocol.

## 3. What this changes

1. **The §15.2 rationale for `Map::ResetInstance()` was wrong.** It is not a crash fix. It is
   still the right patch: BWEM has no teardown API — `Initialize` both resets and re-analyses,
   and the singleton otherwise lives until static destruction — and `bwapi_bwem_reset()` needs
   a reset that leaves `Initialized()` false and returns the memory, while
   `bwapi_client_disconnect()` needs a deterministic teardown before the `GameData` goes away.
   Stardust's vendored BWEM carries the identical one-liner (verified against
   `3rdparty/BWEM/src/map.cpp` at its current `master`). The patch header, §8.2 and §15.2 are
   restated on that footing.
2. **`bwapi_bwem_initialize()` need not reset first for safety.** In-place re-`Initialize` is
   sound. §8.2's "resets first if already initialised" stands on its own merits — one call,
   ordering cannot be got wrong, memory returned — not on a crash.
3. **The fixture builder gains a fifth invariant** (§11): neutrals occupy their own tiles unless
   the scenario deliberately stacks them, and a deliberate stack has identical `TopLeft()` and
   type. R11.6's terrain is corrected to spaced minerals when it is ported in step 0.10.
4. **A real hazard for real maps, and a cheap guard.** Map editors can place partially
   overlapping neutrals, and BWEM's release build will accept them and crash later, at the first
   teardown — in a bot, at match end. `bwapi_bwem_initialize()` should check every neutral's
   footprint against every other's before handing the game to BWEM, and latch `BWAPI_ERR_BWEM`
   with a message naming the units rather than initialise. That is the §4 principle — a
   third-party assertion becomes a latched error, not a crash — applied to an assertion the
   third party compiled out. Recorded as an implementation-plan item in phase 3.
5. **JBWAPI #51 is no longer attributed to this.** R11.5 and R11.6 suggested JBWAPI's
   `IllegalStateException` on consecutive games was this bug inherited through the port. With
   the in-place reset shown sound, that attribution has no evidence behind it and is withdrawn.

## 4. What still stands from R11.6

Everything except the crash diagnosis: the synthetic-`GameData` substrate drives BWEM's full
analysis with no server and no Blizzard data; the four fixture invariants; the `UnitDiscover`
event-stream requirement for neutrals; the ~450 ms initialisation cost; the decision to carry
`ResetInstance` as a patch rather than fork. R11.6's own §3 consequences are re-derived here
with the corrected reason and reach the same three conclusions.
