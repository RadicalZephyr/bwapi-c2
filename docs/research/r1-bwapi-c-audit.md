# R1. Audit of `RnDome/bwapi-c`

Executed against a local clone at `/home/zefs/prog/bwapi/bwapi-c`, HEAD `99450f2` (v1.0.0,
last commit 2018-07-11; repo `pushed_at` 2019-02-19). Compared against the BWAPI headers in
this tree (`bwapi/include/BWAPI/`).

**Headline: the fork option is foreclosed on licensing, and the codebase is roughly half the
asset the plan assumed.** Detail below; the §4 convention table is at the end.

---

## 1. Size and provenance

| Measure | Value |
|---|---|
| Total source | 4,536 lines (881 header, 3,320 impl, 335 example) |
| Exported entry points | **530** |
| Commits / contributors | 148 / 4 |
| Tags | v0.1.0 … v1.0.0 |
| Tests | **none** — AppVeyor sets `test: off`, Travis builds only |
| Open issues / PRs | 2 issues (#9, #61), 1 PR (#62, open since 2019-01-22) |
| Unmerged branches | `safe_cast`, `32_64_win_libs` |

**Hand-written, not generated.** There is no generator, spec file, or script anywhere in the
tree or in the 148-commit history. Every function is a literal one-liner:

```cpp
int Unit_getHitPoints(Unit* self) {
    return reinterpret_cast<BWAPI::Unit>(self)->getHitPoints();
}
```

Type marshalling goes through a hand-written `Cast<BW, TX>` trait in `src/Cast.hpp` (694 lines,
~40 explicit specializations plus `CastFwd`/`CastRev` dispatch aliases). It is competent
template work, and it is exactly the boilerplate a spec-driven emitter would remove.

Declaration/definition parity on master is clean: 528 prefixed declarations, all defined;
one function (`Force_registerEvent`) is defined but missing from `Force.h`. Issue #52 — an
external user hitting `undefined reference to Game_setTextSize` on master — was a transient
state of an older master, since fixed.

**The headers still compile.** All twelve public headers compile standalone under
`gcc -std=c99 -Wall -Wextra -Wpedantic` in 2026 with zero diagnostics, and both examples pass
`-fsyntax-only`. The C-facing surface is genuinely portable and has not rotted.

---

## 2. Coverage: near-total on interfaces, zero on type data

This is the finding that most changes the picture.

**Dynamic interfaces — essentially complete:**

| Class | bwapi-c entry points | Upstream distinct methods | Gap |
|---|---|---|---|
| `Game` | 143 | 142 | `flush` only |
| `Unit` | 281 | 238 | none found (overloads expanded with `_Position`/`_Unit` suffixes) |
| `Player` | 55 | 53 | none |
| `Region` | 16 | 14 | none |
| `Bullet` | 14 | 13 | none |
| `Force` | 3 (+1 undeclared) | 3 | none |

**Static type data and the bulk/filter layer — entirely absent:**

| Upstream | Count | bwapi-c |
|---|---|---|
| `UnitTypes::`/`Orders::`/`WeaponTypes::`… enum constants | **848** | 0 |
| `UnitType`/`TechType`/`WeaponType`/… accessor methods | **185** | 0 |
| `Filters.h` predicate constants and combinators | 147 | 0 |
| `Unitset` bulk command methods | 40 | 0 |

`Types.h` says so explicitly:

> These are value-only structs with no API. You should implement API on your own.

Every type is `struct UnitType { int id; };` and nothing else. The consequence is visible in
their own example bot, which hardcodes raw integers with comments:

```c
case 7:  // SCV
case 41: // Drone
case 64: // Probe
...
return type_id == 176 || type_id == 177 || type_id == 178; // mineral fields
CoordinateType CoordinateType_None = { .id = 0 };
```

A bot in a downstream language cannot ask `UnitType_mineralPrice`, `UnitType_getName`,
`UnitType_isBuilding`, or `TechType_energyCost`. It must either hardcode magic numbers or
re-derive the entire static database in the host language. The maintainers knew: in #28,
2017-05-23, `dkashitsyn` writes that they "need to think how to organize the constants… Currently
all interface structures operate on raw values only." They never did.

**So the plan's premise is wrong in both directions.** A fork does not inherit "~900 wrapped
declarations"; it inherits 530, and inherits *nothing* of the ~1,030 constants and type
accessors that make up rev 3 §5.8 — the block the plan itself calls the highest-value part
of the ABI. The remaining hand-written work after a fork is larger than the fork saves.

Also absent: `Game::getGameTable` / multi-instance client, `Regionset`/`Playerset` helpers,
and all but four methods of `BWAPI::Client`.

---

## 3. Object model: raw C++ pointers, unvalidated

`Unit*`, `Player*`, `Region*`, `Force*`, `Bullet*`, `Game*` are opaque C typedefs whose values
are **the raw `BWAPI::UnitInterface*` etc.**, passed through `reinterpret_cast` in both
directions. `BWAPI::Unit` is literally `typedef UnitInterface *Unit;` (`bwapi/include/BWAPI/Unit.h:26`).

**Nothing validates them.** There is not a single null check in `Unit.cpp` (1,140 lines),
`Game.cpp` (652), `Player.cpp`, `Region.cpp`, `Bullet.cpp`, `Force.cpp`, or `Cast.hpp`.
`assert()` appears only in `Iterator.cpp`, `BwString.cpp`, and three places in `Game.cpp` — and
those compile out under `NDEBUG`, which is how releases ship. A null or stale handle from a
foreign-language binding dereferences directly into StarCraft's address space.

Two contributors tried to fix this and neither landed:

- Branch `safe_cast` (PR #32, 2017) adds `template<class Out, class In> Out safe_cast(In in) { assert(in); … }`
  and converts exactly one function, `Player_getName`. Never merged to master.
- PR #62 (2019, still open) adds a `checkPointer` that `printf`s and `abort()`s — applied only
  to the `AIModule` vtable at construction.

Handle-validation is the single largest behavioural divergence between bwapi-c and rev 3 §4,
and the record shows the project recognised it, attempted it twice, and abandoned it twice.

---

## 4. Collections: heap-allocated stateful iterator objects

Neither our caller-buffer convention nor a snapshot. `src/IteratorImpl.hpp` (233 lines) defines
an `IteratorBase` with virtual `valid()`/`get()`/`next()` and four concrete templates —
`OwningIterator`, `BorrowingIterator`, and `Value*` variants that additionally materialise each
element into a member and hand back `&current`.

The C surface is four untyped functions:

```c
bool  Iterator_valid(const Iterator* self);
void* Iterator_get(const Iterator* self);          /* untyped */
void  Iterator_next(Iterator* self);
void  Iterator_release(Iterator* self);            /* caller must free */
```

`Game_getAllUnits` returns `UnitIterator*`, but `Iterator_valid` takes `Iterator*`, so **every
call site casts**, and `Iterator_get` returns `void*` the caller casts again. From their own
example:

```c
Iterator* const units = (Iterator*) Player_getUnits(ai);
for (; Iterator_valid(units); Iterator_next(units)) {
    Unit* const unit = (Unit*) Iterator_get(units);
```

There is a runtime type tag (`IteratorType id()`) but nothing consults it — the comment says
"Can be used for runtime checks" and no check exists.

**Cost.** One heap allocation per collection call, then three exported-function calls per
element, each a PLT/import-thunk hop plus a virtual dispatch. Enumerating 200 units is 600
boundary crossings and 200 virtual calls before a single accessor runs. That is the concrete
measurement behind rev 3 §5.10.

**Credit where due:** ownership discipline is correct. All 18 `as_iter`/`as_value_iter`
(borrowing) call sites borrow from genuine `const T&` returns; every by-value BWAPI return
(`getLoadedUnits`, `getLarva`, `getTrainingQueue`, `getUnitsInRadius`, …) uses the owning
`into_iter` with a `std::move`. No dangling was found.

**Ordering is nondeterministic and passed straight through.** `Unitset` is
`SetContainer<BWAPI::Unit, std::hash<void*>>` over `std::unordered_set` — hashed on pointer
value, so ASLR changes iteration order run to run. bwapi-c adds no ordering. A bot that takes
the first match from `Game_getAllUnits` is irreproducible through no fault of its own.

---

## 5. Strings, positions, structs, filters

**Strings.** `BwString*` — a heap-allocated `struct { std::string data; }`, returned from every
name/path accessor, released by `BwString_release`. One allocation per call; a per-frame
`Player_getName` costs a malloc. Allocation and free both happen inside `BWAPIC.dll`, so the
cross-CRT bug is avoided — but only because the caller is trusted to call the release function.

`Event.text` is worse: the C `Event` struct carries a bare `void*` that points at the
`std::string` **living inside BWAPI's own event object**. `Event_getText` dereferences it. The
C caller holds a raw pointer into C++ internals whose lifetime is undocumented.

**Positions.** `struct Position { int x; int y; }` returned **by value**. On i386 this is the
classic MSVC-vs-System-V divergence: MSVC `__cdecl` returns an 8-byte POD in `EAX:EDX`, the
i386 System V ABI returns it in memory via a hidden pointer. Their README works around it by
telling MinGW users to pass `-mabi=ms` — an undocumented-in-the-header requirement that every
non-C consumer must independently discover. No calling convention is declared anywhere; the
project relies on the compiler default.

**Booleans.** `<stdbool.h>` `bool` throughout, i.e. 1-byte `_Bool`, in both parameters and
returns.

**Structs.** `Position`, `TilePosition`, `WalkPosition`, `UnitCommand`, `Event` and the fourteen
`struct T { int id; }` type wrappers all cross by value with **no size prefix and no version
field**. `UnitCommand` and `Event` are layout contracts baked into every consumer.

**Filters.** Yes, there is one, and it is a plain C function pointer:

```c
typedef bool (*UnaryUnitFilter)(Unit* unit);
typedef Unit* (*BestUnitFilter)(Unit* left, Unit* right);
```

Implemented by `reinterpret_cast`ing the C function pointer to `bool(*)(BWAPI::Unit)` and
handing it to BWAPI's `UnitFilter`. **There is no `void* user` parameter.** A binding for any
language with closures — Rust, Python, JS, C# — cannot carry state into a filter without a
global or a runtime-generated thunk. For the stated goal ("provide a minimalistic C API which
can be used to create bindings for high-level languages") this is a design defect, not a
detail. None of the 147 `Filters.h` combinators is exposed.

---

## 6. Errors, exceptions, re-entrancy

**There is no error story.** `Game_getLastError` / `Game_setLastError` are pass-throughs of
BWAPI's own per-call error. There is no wrapper-level channel, no way to report a bad handle,
no way to report a failed allocation.

**No exception handling anywhere.** Zero `try`, `catch`, or `noexcept` in the entire source.
`BwString_new`, `into_iter`, and `createAIModuleWrapper` all call `new`; `std::string::assign`
allocates. A `std::bad_alloc` — or any exception escaping a BWAPI call — unwinds straight
through an `extern "C"` frame into a C caller. That is undefined behaviour, and it is the most
likely explanation for issue #52's report of "strange linker behavior when cross linking…
missing unwind symbols."

**Re-entrancy.** `BWAPIC_setGame` writes the global `BWAPI::BroodwarPtr`; there is no context
handle. No locks, no thread-affinity documentation. Single game per process, single thread
assumed but never stated.

**AIModule lifetime is broken.** `createAIModuleWrapper` heap-allocates; `destroyAIModuleWrapper`
exists but nothing ever calls it and there is no hook telling a host when to. Their own example
`malloc`s an `ExampleAIModule` and never frees it. Issue #61 (2018-07-12) and PR #62
(2019-01-22) address exactly this by adding a `drop` vtable slot. The PR has sat open for seven
years. It is the clearest single indicator that the project is not merely dormant but unowned.

---

## 7. Client mode

Compiles; four functions (`isConnected`, `connect`, `disconnect`, `update`) plus
`BWAPIC_getClient`. The example connects, polls, and reads game state, and the README
transcript shows it working under Wine against retail BW.

**It is implemented with an ODR violation.** `src/Client.cpp` does not include BWAPI's
`Client.h`. It re-declares the class from scratch:

```cpp
namespace BWAPI {
class Client {
public:
    bool isConnected() const;
    bool connect();
    void disconnect();
    void update();
};
extern Client BWAPIClient;
}
```

The real `BWAPI::Client` (`bwapi/include/BWAPI/Client/Client.h`) has a constructor, destructor,
a public `GameData* data`, and five private members including three `HANDLE`s. The fake works
only because the four methods are non-virtual (so calls resolve by mangled name to definitions
compiled against the true layout) and because `&BWAPIClient` is a link-time address.

The motivation is legitimate: the real header `#include <Windows.h>` unconditionally, which
breaks the Linux/OpenBW build. **That is a finding for R6 and R7** — the upstream client path
is Windows-header-coupled at the top level, and OpenBW's fork is presumably where that was
solved. The workaround chosen here is not one we would repeat.

Per the README, client mode does not work against OpenBW ("you may compile it, but it is not
supported by OpenBW for now", and `example/Client.c` says the same of openbw/bwapi v4.1.12).

---

## 8. Licensing — the decisive finding

**`RnDome/bwapi-c` has no license.** Not in the tree, not in any of the four tags, not in any
of the 148 commits (`git rev-list --all --objects` finds no license blob ever), not in the
README, and no copyright header in any source file. GitHub's own API returns `"license": null`.

This is not an oversight of an unlicensed org. The same owner ships:

| Repo | License |
|---|---|
| `RnDome/bwapi` (their BWAPI fork) | LGPL-3.0 |
| `RnDome/bwapi-rs` | MIT |
| `RnDome/bwapi-sys` | MIT (`Unlicense/MIT` in `Cargo.toml`) |
| **`RnDome/bwapi-c`** | **none** |

They licensed the fork, the Rust FFI crate, and the Rust wrapper, and left the C layer — the
only piece worth forking — unlicensed. `bwapi-sys` consumes bwapi-c as a **git submodule**, not
vendored source, so its MIT grant covers `build.rs` and the Rust declarations and does not
reach bwapi-c's code.

Under default copyright that is all-rights-reserved. Forking, modifying, and redistributing it
is not permitted. Relicensing needs sign-off from four contributors — Roman Proskuryakov, Nick
Linker, Dmitry Kashitsyn, and the `0x7CFE` account — on a project none of them has touched in
seven years, and the research plan's constraint rules out contacting them.

**Direction A is foreclosed unless that constraint is lifted and all four respond.**

---

## 9. Field reports from the tracker

The plan asked for these specifically. 62 issues and PRs, read in full for the eight with
substantive discussion.

- **#61 / #62 — AIModule destructor.** Open since 2018 and 2019. The leak is real, the fix is
  written, nobody merged it.
- **#9 — const qualifiers in wrappers.** Open since 2017-05-09, empty body, never addressed.
  Related dead branch `safe_cast`.
- **#52 — Mac OS X cross-compile.** The only external user in the tracker. Hit a broken master
  (declared-but-undefined `Game_setTextSize`), was told to use the release tag instead. In the
  same thread `dkashitsyn`: "we're currently experiencing strange linker behavior when cross
  linking… missing unwind symbols and for the moment I have no clue what causes them."
  Never resolved. This is the C++-runtime-crossing-the-C-boundary problem showing up in the
  field, and it is an argument for `noexcept` boundaries and a static-CRT release policy.
- **#28 — "How can I help?"** The maintainers' own statement of the gaps: constants
  unorganised, and "we would be really interested in testing of the whole API." No tests were
  ever written.
- **#35 — GOAL language support.** *The demand data point.* TU Delft students, 40–50 teams
  building StarCraft bots in GOAL, asked about tournament entry. `kpp` pitched bwapi-c. The
  logged IRC follow-up shows the students actually shipped on JNIBWAPI/BWMirror. `kpp`,
  2017-06-13: "I guess @matisup10 is not interested in our library … :cry:". **Feed this to R3
  verbatim.** A concrete, sizeable, genuinely unserved-language audience found the library,
  and it still lost to the Java bindings.
- **#44 / branch `32_64_win_libs` — x64.** Attempted, abandoned, with the comment left in the
  CI file: `#  - x64 BWAPI is a 32-only project =(`. **Feed to R5, but discount it:** they were
  linking the retail BWAPI 4.2.0 release archive, which ships only a 32-bit `.lib`. That is
  "no 64-bit BWAPI to link against," which is not the same claim as "the client protocol cannot
  be 64-bit" — and JBWAPI's production 64-bit shared-memory reads contradict the stronger claim.

**CI is dead in a way that also destroys reproducibility.** Travis (`travis-ci.org`, shut down
2021) fetched a prebuilt OpenBW BWAPI 4.1.12 Linux tarball from
`s3.amazonaws.com/bwapic/…` — a bucket the maintainer owned. AppVeyor fetched
`BWAPI_420.old.7z` from the upstream GitHub release. Neither pipeline ran a test, and the
Linux dependency is not reconstructible from the repo.

---

## 10. Deliverable: rev 3 §4 conventions vs. bwapi-c

| §4 convention | What bwapi-c does | What we do differently, and why it is worth it |
|---|---|---|
| **Handles** — `int32_t` IDs, validated, neutral value + latched error on bad input | Raw `BWAPI::UnitInterface*` etc. via `reinterpret_cast`. Zero validation in 3,320 lines; `safe_cast` and `checkPointer` both abandoned unmerged | **Diverge.** A foreign-language wrapper bug must not crash StarCraft mid-tournament. Integer IDs also make handles serialisable, comparable, and hashable in every host language, and let a stale handle be *detected* rather than dereferenced. This is the largest divergence and the one bwapi-c's own history says was needed |
| **Linkage / calling convention** — explicit `__cdecl` on every export, `.def` file, no ordinals | `extern "C"` only. No convention declared; relies on compiler default. `CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON` auto-generates the export set | **Diverge.** JNA-style loaders guess the convention; auto-exported symbol sets are unstable across compiler versions. Explicit is nearly free |
| **Types** — `int32_t`/`int64_t`/`double`/`char`/pointers only; no C++ `bool`, no enums in signatures | `bool` (`<stdbool.h>`, 1 byte) throughout; `int` rather than `int32_t`; `double` for angles and velocities | **Diverge on `bool` and fixed widths.** 1-byte returns leave upper register bits unspecified and FFI layers disagree about zero-extension. `int` is fine everywhere in practice but costs nothing to pin |
| **Positions** — packed `int64_t` on return, unpacked `int32_t x, y` as parameters and struct fields | `struct Position { int x; int y; }` by value, both directions | **Diverge.** The 8-byte-POD return is precisely where MSVC and i386 System V disagree; their README papers over it with a `-mabi=ms` instruction that lives outside the header. A packed `int64_t` is one register and has no ABI ambiguity |
| **Strings out** — snprintf convention, caller buffer, no allocation crosses the boundary | `BwString*`: a heap `std::string` wrapper with a mandatory `BwString_release`. `Event.text` is a bare `void*` into BWAPI's own `std::string` | **Diverge.** One malloc per name read per frame, an object lifetime every binding must model, and an interior pointer that can dangle. Caller buffers remove all three, and remove the free function entirely |
| **Collections out** — caller buffer + true-count return, `cap == 0` size query, first `cap` in ID order on truncation | Heap iterator object; `Iterator_valid`/`_get`/`_next`/`_release`; `void*` element type; caller casts twice per loop; ~3 boundary crossings + 1 virtual dispatch per element | **Diverge.** 600 crossings to enumerate 200 units versus one call. This is the §5.10 argument, and the measured cost is bwapi-c's own. Caller buffers also delete the untyped `void*` and the release call |
| **Ordering** — sorted ascending by ID, documented | Passes `std::unordered_set` hashed on pointer straight through. Nondeterministic run to run under ASLR | **Diverge.** Genuine nondeterminism, not merely unspecified order. A bot taking the first match is irreproducible through no fault of its own. Sorting is cheap and the alternative is unfixable downstream |
| **Errors** — sticky first-error latch, never cleared implicitly, plus `bwapi_last_error_message` | Nothing. `Game_getLastError`/`setLastError` are BWAPI pass-throughs. No wrapper channel. No `try`/`catch`/`noexcept` anywhere; exceptions can unwind through `extern "C"` | **Diverge, and add what does not exist.** Issue #52's unresolved "missing unwind symbols" is this failure mode reported from the field. `noexcept` boundaries are mandatory, not optional |
| **Structs** — every crossing POD begins with `int32_t size`; element zero's size is the array stride | No size or version field on `Position`, `UnitCommand`, `Event`, or the fourteen `{int id;}` wrappers | **Diverge.** ctypes and koffi consumers hardcode layouts in script and never recompile. Four bytes buys the ability to add a field without breaking them |
| **Re-entrancy** — process-wide singleton, stated explicitly; `connect()` when connected latches `ALREADY_CONNECTED` | Global `BroodwarPtr` via `BWAPIC_setGame`. No locks, no documentation, no statement of thread rules | **Same model, state it.** The singleton is inherited from BWAPI and is not worth fighting. The divergence is documenting it and making the error paths explicit |
| **Filters** — (not in §4; recorded because bwapi-c has one) | Bare C function pointer, **no `void* user`**; `reinterpret_cast` into BWAPI's `UnitFilter` | **Diverge: add `void* user` to every callback.** Without it no language with closures can pass a stateful filter, which defeats the library's own stated purpose. Also expose the `Filters.h` combinators — 147 constants currently unreachable |
| **AIModule lifetime** — (not in §4; recorded as a gap) | `createAIModuleWrapper` allocates, `destroyAIModuleWrapper` is never called, no `drop` hook. Open since 2018 | **Add a destructor slot**, as PR #62 proposes. Cheap, and required for any host with a GC or RAII |

---

## 11. Verdict

**Direction A (fork) is not available.** The blocker is licensing, not code quality, and no
amount of engineering judgement moves it. Nothing in this repository can be copied into a
project we distribute.

That is decisive on its own, but the technical case would have been marginal anyway. Set the
license aside and ask what a fork would actually buy:

- **Keeps:** near-complete method-name coverage of the six dynamic interfaces (530 entry
  points), a `Cast<>` marshalling layer, a working CMake + `FindBWAPI.cmake` build, and a
  proven Linux/OpenBW module-mode path.
- **Rewrites anyway:** handles (every one of 530 signatures), collections (every function
  returning a set — the whole iterator subsystem), strings (every name accessor), positions
  (every geometry return), error channel (does not exist), exception safety (does not exist),
  struct versioning (does not exist), callback `user` pointers (do not exist), ordering
  (does not exist).
- **Still has to be written from scratch:** ~848 constants, ~185 type accessors, 147 filter
  combinators, 40 bulk commands. Around 1,030 entry points of §5.8 work — **more than the
  entire existing surface** — plus every test, since there are none.

Under rev 3's §4 conventions, a fork retains the *names* and discards the *implementation*.
That is worth something as a reference: bwapi-c is a checked, complete inventory of which BWAPI
methods are wrappable, which overloads need disambiguating suffixes, and what each one's C
signature looks like — and reading it costs nothing legally.

**Recommendation: direction B, with `RnDome/bwapi-c` as a read-only reference for coverage and
naming, and zero code reuse.** Cite it in the spec as prior art; do not depend on it.

Two prior-art positives worth adopting outright, both cheap:

1. **Overload disambiguation by type suffix** — `Unit_attack_Position` / `Unit_attack_Unit`,
   `Unit_canBuild_UnitType_TilePosition`. It matches rev 3's naming rule and their 530-name
   inventory is a free, complete list of exactly which methods need it.
2. **The `AIModule` vtable-struct pattern** for module mode. It is clean C, it maps naturally
   onto every host language, and it needs only the `void* user` and `drop` additions above.

---

## 12. Feeds into other experiments

| To | Finding |
|---|---|
| **R2** | The static-type-data question is already answered from the other side: bwapi-c ships **0 of ~1,030** constants and type accessors, and its example bot is reduced to `case 7: // SCV`. Whatever R2's usage frequency says, §5.8 cannot be cut — its absence is what made this library unusable without a hand-written companion layer |
| **R3** | Issue #35 is a hard demand data point: 40–50 TU Delft teams writing bots in GOAL, aware of bwapi-c, who shipped on JNIBWAPI/BWMirror anyway. Also: 20 stars, 6 watchers, 1 fork, exactly one external user in 62 issues over eight years |
| **R5** | #44 abandoned x64 with "BWAPI is a 32-bit library" — but they were linking the retail 4.2.0 release archive, which ships only a 32-bit `.lib`. Treat as evidence about *artifact availability*, not about the protocol |
| **R6 / R7** | Upstream `BWAPI/Client/Client.h` `#include <Windows.h>` at top level, which is why bwapi-c re-declared the class rather than including it. The client path is Windows-header-coupled upstream; check how OpenBW's fork handles this before assuming a clean client-only source set |
| **R9** | `RnDome/bwapi` (their BWAPI fork) is LGPL-3.0 — independent corroboration of BWAPI's license, and of the fact that these authors licensed deliberately elsewhere |
| **R10** | `bwapi-c` the GitHub name is taken by an unlicensed, unmaintained repo whose README is the top hit for "BWAPI C bindings." Reusing the name inherits that confusion with none of the code |
