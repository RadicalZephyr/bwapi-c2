# Plan: a C ABI for BWAPI

## Purpose

BWAPI today is consumable only from C++. Everything a bot touches is a C++ class with
virtual dispatch, `std::string`/`std::unordered_set` return values, `std::function`
predicates, template types, and printf-style varargs. None of that crosses an FFI
boundary, so Rust, JavaScript, Python, C#, Zig, and friends currently have to either
re-implement the client protocol from scratch or maintain a per-language C++ shim.

This document plans a **stable, flat C ABI** (`bwapi_c.dll` + `bwapi_c.h`) that wraps the
existing C++ implementation, so any language with a C FFI can drive BWAPI without writing
a line of C++.

This is a design/roadmap document. No production code is included.

> **Revision 2**, incorporating review feedback on
> [PR #1](https://github.com/RadicalZephyr/bwapi/pull/1). The largest change: the C ABI
> lives in a **separate repository** that consumes BWAPI as a dependency, not in this tree.
> Module mode is deferred indefinitely. All five open questions have been answered and are
> recorded in §13. Reviewer decisions are marked **[Review]** throughout.

---

## 1. Ground truth: what the codebase actually looks like

These facts drive every decision below.

### 1.1 There are two integration modes, behind one interface

| | Module mode | Client mode |
|---|---|---|
| Bot artifact | 32-bit Windows DLL, injected into `StarCraft.exe` | Separate `.exe` process |
| Contract | Export `newAIModule()` → `BWAPI::AIModule*` and `gameInit(BWAPI::Game*)` (`bwapi/ExampleAIModule/Source/Dll.cpp`); loaded by `GetProcAddress` in `bwapi/BWAPI/Source/GameUpdate.cpp:389` | `BWAPIClient.connect()` / `.update()` (`bwapi/BWAPIClient/Source/Client.cpp`) |
| Transport | In-process virtual calls | `Local\bwapi_shared_memory_<pid>` + `\\.\pipe\bwapi_pipe_<pid>`, discovered via `Local\bwapi_shared_memory_game_list` (`bwapi/BWAPI/Source/BWAPI/Server.cpp:40,99,194`) |
| Events | Virtual overrides on `BWAPI::AIModule` | Poll `Broodwar->getEvents()` |
| Crash blast radius | Takes StarCraft down | Isolated |

Both modes implement the *same* abstract `BWAPI::Game` (`bwapi/include/BWAPI/Game.h`):
`BWAPI/Source/BWAPI/GameImpl` in-process, `BWAPIClient/Source/GameImpl` out-of-process.
`bwapi/Shared/` (≈4,700 lines, of which `Templates.h` is 3,098) is compiled into *both*
and holds the game-rules logic — `canBuildHere`, `canMake`, the ~130 `canXxx` predicates,
spatial `unitFinder` queries, `hasPower`.

**Consequence: a wrapper written against `BWAPI::Game` serves both modes from one source
tree.** That is the single most important structural fact here — it means we do not need
two ABIs.

### 1.2 The type system is already almost FFI-shaped

`BWAPI::Type<T, UnknownId>` (`bwapi/include/BWAPI/Type.h`) has exactly **one** data
member — `int tid` — no virtuals, plus `constexpr operator int()`. So all of
`UnitType`, `Race`, `TechType`, `UpgradeType`, `WeaponType`, `Order`, `UnitCommandType`,
`UnitSizeType`, `GameType`, `PlayerType`, `BulletType`, `DamageType`, `ExplosionType`,
`Error`, `Color` are **`int32_t` at the ABI level**. No conversion code, no wrapper
objects, no lifetime questions.

`Position` / `TilePosition` / `WalkPosition` are `Point<int, Scale>`
(`bwapi/include/BWAPI/Position.h`) — two `int`s, distinguished only by a compile-time
scale. At the ABI level they are a 2×`int32_t` POD; the scale becomes a naming convention
(`_position` = pixels, `_tile` = 32px, `_walk` = 8px).

Sentinels worth exporting as constants: `Positions::Invalid/None/Unknown/Origin`
(`Position.h:399-408`) and their tile/walk equivalents.

### 1.3 Object identity maps cleanly onto integer IDs

`Unit`, `Player`, `Force`, `Region`, `Bullet` are typedefs for pointers to interface
classes. But every one exposes `getID()`, and `Game` already has ID-keyed lookups:

- Client mode: `GameImpl::getUnit(id)` is a direct index into a 10,000-entry vector
  (`bwapi/BWAPIClient/Source/GameImpl.cpp:382`, populated in the constructor at `:29`) —
  **O(1)**. `getPlayer` (12), `getForce` (5), `getRegion` likewise.
- Module mode: `Game::getUnit(id)` → `Server::getUnit(id)`, an index into `unitVector`
  (`bwapi/BWAPI/Source/BWAPI/Server.cpp:730`). IDs are assigned lazily by
  `Server::getUnitID()` (`:719`), but `extractUnitData()` assigns every alive unit an ID
  before AI callbacks run (`bwapi/BWAPI/Source/BWAPI/GameUnits.cpp:228`), so by the time a
  bot can observe a unit it has a stable ID.
- `UnitImpl::getID()` returns that same `id` field in both modes
  (`bwapi/Shared/UnitShared.cpp:31`).

**Consequence: use `int32_t` IDs as ABI handles, not raw pointers.** They are stable
across frames, serializable, cheap to validate, cannot dangle, are already what the
shared-memory protocol speaks, and let the host language store them in plain arrays.
Exposing `UnitInterface*` would be faster by a pointer chase and vastly more dangerous.

One exception: **bullets have no `Game::getBullet(id)`**. Client-side they live in a
fixed 100-entry `bulletVector`; `BulletInterface::getID()` returns the game's bullet ID,
not the index. Bullets are transient and few, so expose them as a **POD snapshot array**
rather than handles (see §6.3).

### 1.4 `GameData` is pointer-free

`bwapi/include/BWAPI/Client/GameData.h` and its members (`UnitData`, `PlayerData`,
`ForceData`, `RegionData`, `BulletData`, `BWAPIC::{Event,Command,Shape,UnitCommand,Position}`,
`unitFinder`) contain **only** `int`, `unsigned`, `bool`, `double`, `char[]`,
`unsigned short`, plain enums, and one `BWAPI::UnitCommandType` (which is the 4-byte
`Type` above). No pointers, no `size_t`, no `long`.

Since MSVC x86 and x64 agree on all of those sizes *and* on `alignof(double) == 8`, the
`GameData` layout should be **byte-identical between 32- and 64-bit builds**. That opens
the door to a **64-bit client-mode DLL** — which matters enormously, because Node.js is
effectively 64-bit-only and the default Rust Windows toolchain is `x86_64`. It must be
*proved*, not assumed: see §10.2.

### 1.5 Build reality

- Every C++ project in `bwapi/bwapi.sln` (24 projects) is **`Win32` only**, MSVC toolset
  `v141_xp`, `stdcpp17`.
- CMake exists but only builds **static libs** for the client path
  (`CMake/BWAPI/CMakeLists.txt`, `CMake/Client/CMakeLists.txt`). The BWAPI DLL target is
  commented out with the note *"Actually it doesn't. It freezes and never starts."* — the
  injected DLL must stay MSBuild.
- The CMake tree has **no client-only equivalent of `BWAPILIB`**: those sources are
  compiled only as part of `BWAPI-Static`, which also drags in the injected-DLL sources and
  a `cscript.exe`-generated `svnrev.h`. That shapes how an out-of-tree consumer has to
  build — see §10.1.

### 1.6 Prior art in-tree

`bwapi/include/swig.i` (167 lines) plus `bwapi/include/swig_lib/java/` is an existing,
unfinished SWIG attempt targeting Java. It is genuinely useful as a triage record: its
`%ignore` list enumerates precisely the constructs that don't survive a binding
(`operator=`, `operator<<`, `operator int`, `operator bool`, arithmetic operators), and its
`position_template` / `type_template` macros confirm that `Point` and `Type` are the two
shapes needing special handling. Worth reading before writing the spec; not worth
resurrecting (SWIG generates a per-language C++ layer, which is the thing we're trying to
stop needing).

### 1.7 No exceptions to worry about

`grep -rn "throw "` across `bwapi/BWAPILIB`, `bwapi/Shared`, `bwapi/BWAPIClient`, and
`bwapi/include` returns **nothing**. `Util/Exceptions.h` types are used only by the
injected DLL's config/XML paths. So the boundary needs a `catch(...)` for safety
(especially around host callbacks re-entering us) but not an error-translation layer.

### 1.8 API surface size

Approximate public declaration counts (grep-derived, includes overloads and
convenience wrappers):

| Header | ~Decls | Notes |
|---|---|---|
| `Unit.h` | 280 | ~130 are the regular `canXxx` family |
| `Game.h` | 183 | ~90 are `drawX{Map,Mouse,Screen}` convenience triples |
| `UnitType.h` | 86 | pure functions of an `int` — no game needed |
| `UnitCommand.h` | 58 | mostly static factories |
| `Player.h` | 54 | |
| `Unitset.h` | 48 | broadcast helpers over a set |
| `WeaponType.h` | 27 | |
| others | ~90 | Bullet, Region, Force, Position, Color, Error, Tech/Upgrade/Race/… |

Call it **~900 declarations, collapsing to roughly 550–600 distinct ABI entry points**
once overloads are given distinct names and the `drawX{Map,Mouse,Screen}` triples collapse
into one function taking a `coordinate_type` parameter.

That number is the whole reason for §11: hand-writing 600 wrappers is a mistake.

---

## 2. Goals and non-goals

### Goals

1. **One flat `extern "C"` header**, valid C99 *and* C++, no C++ types, no macros required
   at the call site.
2. **Stable ABI** across BWAPI patch releases: append-only, versioned, symbol list under
   test.
3. **Zero C++ toolchain required for consumers** — a `.dll` + `.h` (+ `.lib` for those who
   want it) is the whole delivery.
4. **Cheap for binding generators**: emit a machine-readable API description alongside the
   header so `bindgen`, `napi`, `ctypes`, and JNA-style wrappers can be generated, not
   hand-typed.
5. **Complete enough to write a real bot** in v1 — not a demo subset.
6. **No fork of the game logic.** Everything routes through `BWAPI::Game` /
   `bwapi/Shared/`. If a rule changes upstream, the ABI inherits it.
7. **Purely additive.** **[Review]** The ABI lives in its own repository and *depends on*
   BWAPI. Landing it must require no change to this tree — no fork, no vendored copy, no
   upstream patch as a precondition. Any upstream improvement it wants (§10.1) is proposed
   separately, on its own merits, and the ABI must build without it.

### Non-goals

1. **Not** a re-implementation of the shared-memory protocol per language. That's the
   JBWAPI/BWAPI4J approach; it means re-implementing `Templates.h` (3,098 lines of game
   rules) and `CommandTemp.h` (latency compensation) in every language. Explicitly
   rejected.
2. **Not** an idiomatic high-level API. That belongs in per-language wrappers
   (`bwapi-sys` → `bwapi` in Rust terms). The C ABI is the thin, boring, mechanical layer.
3. **Not** cross-platform. BWAPI hooks a 32-bit Windows binary. Windows-only is inherent.
4. **Not** exposing `Interface<T>::registerEvent`, `get/setClientInfo`, or the
   `Broodwar << ...` stream. Host languages have closures and hash maps; these exist to
   paper over C++ ergonomics.
5. **Not** exposing `std::function` filters as first-class composable objects. See §5.4.

---

## 3. Recommended shape

> **A separate `bwapi-c` repository producing `bwapi_c.dll`, consuming BWAPI as a
> dependency, wrapping `BWAPI::Game` behind `int32_t` handles, generated from a checked-in
> API spec, client mode only.**

Four decisions worth defending:

**Its own repository, BWAPI as a dependency.** **[Review]** The ABI is a strictly additive
layer; nothing about it requires editing BWAPI, so nothing about it should. That also lets
it version, release, and iterate on its own cadence, and pin a known-good BWAPI revision
rather than chasing `main`. §7 covers the layout and §10.1 the mechanics.

**Client mode only.** **[Review]** No injection, no C++ vtable interop, crashes stay
isolated, and it is what Rust/JS users actually want. Module mode is deferred indefinitely
(§8) — its design sketch is kept only so a future revival does not start from zero. Since
both modes sit behind `BWAPI::Game`, ~95% of the wrapper source would be reusable if it
ever is revived.

**ID handles, not pointers.** §1.3. Also means the ABI is trivially usable from a language
without raw-pointer types, and handles can be logged/replayed.

**Generated, not hand-written.** §1.8 and §9.

**Naming.** ⚠️ Do **not** name it `BWAPIC` — `namespace BWAPIC` already exists in
`bwapi/include/BWAPI/Client/*.h` for the shared-memory PODs. Proposal:

| Thing | Name |
|---|---|
| Repository | `bwapi-c` |
| CMake target | `BWAPI_C` |
| Output | `bwapi_c.dll`, `bwapi_c.lib`, `bwapi_c.def` |
| Public headers | `bwapi_c.h`, `bwapi_c_types.h` |
| Symbol prefix | `bwapi_` |
| Internal C++ namespace | `BWAPI::CApi` |

---

## 4. ABI conventions

These are the rules the generator enforces. Locking them down first is most of the work.

**Linkage and calling convention.** Everything `extern "C"`. Explicit
`#define BWAPI_C_CALL __cdecl` on every exported function — never rely on the project
default, since consumers (notably JNA-style loaders) guess. Export via a `.def` file so
names are undecorated even in x86 builds.

**Types.** Only `int32_t`, `uint32_t`, `uint8_t`, `double`, `char`, `void*`, function
pointers, and PODs declared in `bwapi_c.h`. Never C++ `bool` — its width is 1 byte but its
ABI treatment across FFIs is a recurring footgun. No enums in signatures — `int32_t` with
`#define`d constants, so an unknown future value can't be UB in a strict-enum language.
No structs by value in or out; pass `const T*` in, `T*` out.

**Booleans, scalar vs. bulk.** **[Review]** These are two different rules and were
previously conflated:

- **Scalar** boolean parameters and return values are `int32_t`, 0 or 1. `int32_t` rather
  than `int8_t` because a narrow integer return leaves the upper bits of the register
  unspecified, and FFI layers disagree about whether the callee zero-extends — `ctypes`,
  JNA, koffi and hand-written `extern` blocks all model an `int`-width return most
  reliably. It also means the header has exactly one integer width for every value in the
  ABI, since all `Type` ids are already `int32_t`.
- **Bulk** boolean grids (§5.5) are `uint8_t` per element, 0 or 1. Four bytes per tile
  would quadruple a 1 MB walkability copy for nothing, and the destination is a typed
  array in the host language, not a call argument — so none of the register-width
  reasoning applies.

**Naming.** `bwapi_<subject>_<verb>[_<disambiguator>]`, snake_case:
`bwapi_unit_get_hit_points`, `bwapi_unit_attack_position`, `bwapi_unit_attack_unit`,
`bwapi_unittype_max_hit_points`. Overloads get an explicit type-suffixed name — no
mangling, no defaulted arguments (C has none, so every parameter is explicit and the
per-language wrapper re-adds defaults).

**Strings out.** snprintf convention:
```c
int32_t bwapi_game_map_name(char* buf, int32_t buf_len);
```
Writes at most `buf_len` bytes including NUL; returns the length the string *would*
need (excluding NUL), so callers can size a second call. `buf` may be `NULL` when
`buf_len == 0`, purely to query length. Never returns an interior pointer — `mapName()`
returns `std::string` **by value** (`Game.h`), so any borrowed pointer would dangle
immediately.

**Strings in.** `const char*`, UTF-8-ish (StarCraft is codepage-bound; document as
"pass-through, no transcoding").

**Collections out.** Caller-provided buffer + true-count return:
```c
int32_t bwapi_game_get_all_units(int32_t* out_ids, int32_t cap);
```
Fills up to `cap`, returns the *total* available. `cap == 0` with `out_ids == NULL` is the
size query. No allocation crosses the boundary, so there is no free function and no
allocator mismatch — the classic MSVC-CRT-vs-host-CRT bug is designed out.

⚠️ **Order.** `Unitset` is a `SetContainer` over `std::unordered_set`
(`bwapi/include/BWAPI/SetContainer.h`), so iteration order is unspecified *and* can differ
run to run. Bots that iterate and take the first match would become nondeterministic.
**The ABI must sort collection output ascending by ID** and document it. This is a small
per-frame cost and buys reproducibility, which matters for a competitive-AI framework.

**Invalid handles.** Never dereference. Validate, then return a documented neutral value
(`0` / `-1` / `Positions::None` / empty) and record it in the boundary error channel.
Rationale: a wrapper bug in a foreign language must not crash StarCraft mid-tournament.

**Two error channels, kept separate.**
- *Game* errors: BWAPI's own `Errors::Enum`, via `bwapi_game_get_last_error()` — an
  unchanged pass-through of `Game::getLastError()`.
- *ABI* errors: `bwapi_last_error()` / `bwapi_last_error_message()`, thread-local,
  for invalid handles, buffer-too-small, not-connected, callback-threw. Distinct enum,
  never mixed with game errors.

**Diagnostics.** `Client::connect()` writes directly to `std::cout`/`std::cerr`
(`bwapi/BWAPIClient/Source/Client.cpp`), which is useless-to-harmful for an embedded
consumer. Add `bwapi_set_log_callback(void(*)(int32_t level, const char* msg, void* user), void* user)`
and route wrapper diagnostics there. Leave the underlying C++ prints alone in v1 (changing
them is an upstream behaviour change), but document them.

**Threading.** BWAPI is single-threaded and frame-synchronous. Document: *all* calls on
the thread that calls `bwapi_client_update()`. Note explicitly that
`Client::update()`'s pipe `ReadFile` **blocks** — a Node consumer must run it on a worker
thread or it will stall the event loop.

**Versioning.**
```c
uint32_t bwapi_abi_version(void);      /* semver of this ABI, bumped by the generator */
int32_t  bwapi_client_version(void);   /* BWAPI::CLIENT_VERSION, 10003 today */
int32_t  bwapi_revision(void);
int32_t  bwapi_is_debug(void);
```
Policy: **append-only**. New functions get new names; existing signatures never change
meaning. A removal is a major bump.

---

## 5. What is mechanical vs. what needs design

Roughly **85% of the surface is mechanical** — a getter returning `int`, `double`, `bool`,
a `Type` (→ `int32_t`), a `Position` (→ 2 out-params or a POD), or a handle. That includes
the entire `canXxx` family (~130 on `Unit` alone), all of `UnitType`/`WeaponType`/
`TechType`/`UpgradeType` static data, every `Player` accessor, every `Unit` state query,
and every command method. These want a generator, not a human.

The other 15% is the interesting part. Everything below needs a decision.

### 5.1 Varargs

`Game::printf`, `sendText`, `sendTextEx`, `drawText*` are printf-style, and the
implementation `vsnprintf`s into a **256-byte** buffer
(`bwapi/BWAPIClient/Source/GameImpl.cpp:622-635`).

Expose **non-format** functions only:
```c
void bwapi_game_printf(const char* text);
void bwapi_game_send_text(const char* text);
void bwapi_game_send_text_ex(int32_t to_allies, const char* text);
void bwapi_game_draw_text(int32_t ctype, int32_t x, int32_t y, const char* text);
```
implemented as `Broodwar->printf("%s", text)`. This is not just convenience — passing a
foreign string straight into a format position is a **format-string vulnerability**, and a
unit name containing `%s` would be enough to trigger it. Host languages already have
better formatting. Document the 256-byte truncation.

### 5.2 The `drawX{Map,Mouse,Screen}` triples

`Game.h` has ~90 draw declarations, but only 8 primitives (`text`, `box`, `triangle`,
`circle`, `ellipse`, `dot`, `line`) × 3 coordinate spaces × `int`/`Position` overloads. The
virtual ones already take `CoordinateType::Enum`. Emit **one function per primitive** with
an explicit `ctype` parameter: 90 → 8.

### 5.3 `UnitCommand`

`BWAPI::UnitCommand` holds raw `Unit` pointers. `BWAPIC::UnitCommand`
(`bwapi/include/BWAPI/Client/UnitCommand.h`) is already the ID-based mirror used over the
wire. Mirror *that*:
```c
typedef struct bwapi_unit_command {
  int32_t type; int32_t unit_id; int32_t target_id;
  int32_t x; int32_t y; int32_t extra;
} bwapi_unit_command;

int32_t bwapi_unit_issue_command(int32_t unit_id, const bwapi_unit_command* cmd);
int32_t bwapi_game_issue_command(const int32_t* unit_ids, int32_t n,
                                 const bwapi_unit_command* cmd);
```
Also expose the ~40 convenience methods directly (`bwapi_unit_attack_unit`,
`bwapi_unit_train`, `bwapi_unit_build`, …) — that's what bots actually call, and going
through a struct for `move()` is a needless ergonomic tax on every wrapper author.

### 5.4 Filters and predicates

`UnitFilter` is `UnaryFilter<Unit>` over `std::function<bool(Unit)>`; `Filter::IsWorker`
et al. are composable objects with operator overloads. **None of that crosses C.**

Three tiers, in this order:

1. **v1: no filters.** Every query gets an unfiltered form; the host language filters the
   returned ID array. For `getUnitsInRadius`-style calls this is essentially free.
2. **v1: keep the spatial index.** `getUnitsInRectangle` uses `Templates::iterateUnitFinder`
   over the shared-memory `xUnitSearch`/`yUnitSearch` arrays. That optimisation must not be
   lost — expose the rectangle/radius queries natively, and let filtering happen after.
3. **v2: callback predicates** for the three calls where filtering must happen *inside*
   (`getClosestUnitInRectangle`, `getBestUnit`, and the filtered rectangle query), because
   they early-exit:
   ```c
   typedef int32_t (BWAPI_C_CALL *bwapi_unit_pred)(bwapi_unit unit, void* user);
   ```
   Wrap every invocation in `try{}catch(...){ return 0; }` — a foreign callback throwing
   (a Rust panic unwinding across FFI, say) through BWAPI's stack would be catastrophic.

An enum-based mini-filter DSL (`BWAPI_FILTER_IS_WORKER`) is tempting and should be
**declined**: it re-encodes upstream semantics in a switch statement that silently rots.

#### What a predicate may call

**[Review]** Revision 1 said only "the callback must not call back into the ABI". That was
both unjustified and, as written, unusable: it would force the caller to pre-fetch state
for every candidate unit *before* the query, which defeats the point of filtering inside.
Reading the three call sites shows the real constraint is much narrower.

All three queries — `GameImpl::getUnitsInRectangle`, `getClosestUnitInRectangle`,
`getBestUnit` (`bwapi/BWAPIClient/Source/GameImpl.cpp:475-543`) — hold only
**function-local** state: a local `Unitset`, a local `bestDistance`/`pBestUnit`. They
delegate to `Templates::iterateUnitFinder` (`bwapi/Shared/Templates.h:69-152`), whose only
scratch state is a **local** `std::unordered_map` and which iterates
`data->xUnitSearch`/`yUnitSearch` — shared-memory arrays the server rewrites only during
`update()`. There is no global or static scratch buffer, and no `GameImpl` member container
is mutated. So re-entering the ABI is not inherently unsafe; only *specific* things are.

| Class | Verdict | Reason |
|---|---|---|
| Every read-only accessor — `bwapi_unit_*` getters, `bwapi_player_*`, `bwapi_game_map_*`, static type data, scalar tile queries | **Allowed** | Touch only shared memory the server won't rewrite mid-frame, plus function-local state. This is the whole point of the callback and covers essentially every predicate anyone would write |
| Unit commands (`bwapi_unit_attack_unit`, `train`, `move`, …) | **Forbidden** | Not a style rule. Client-mode `UnitImpl::issueCommand` runs `Command{command}.execute()` (`BWAPIClient/Source/UnitImpl.cpp:59`), and `CommandTemp::execute()` writes straight into `unit->self->order`, `->target`, `->isConstructing` … (`include/BWAPI/Client/CommandTemp.h:196-214`) — i.e. into the very `UnitData` the enclosing query is filtering on. The query's answer would depend on iteration order |
| Drawing, `printf`, `sendText`, `enableFlag`, `setLocalSpeed` | **Forbidden** | Append to `data->shapes[]` / `commands[]` / `unitCommands[]`, bounded only by `assert` — no runtime check in release builds |
| `bwapi_client_update` / `connect` / `disconnect` | **Forbidden** | `Client::disconnect()` deletes `BroodwarPtr` while the in-flight query still holds `this`. Immediate use-after-free |
| Nested queries (`bwapi_game_get_units_in_rectangle` inside a predicate) | **Allowed, discouraged** | Memory-safe — each call builds fresh local state — but it is O(n) inside O(n) |
| Anything that calls `setLastError` | **Allowed, with a caveat** | `GameImpl` holds a single `mutable Error lastError`; a predicate can clobber what the outer query would have left. Observable, not unsafe |

**Enforce it, don't just document it.** The spec gains a `mutates: true` flag, and the
generator emits a guard on every mutating wrapper:

```c
if (bwapi_capi_in_predicate()) {   /* thread-local depth counter */
    bwapi_capi_set_error(BWAPI_ERR_REENTRANT_MUTATION);
    return 0;                      /* no side effect */
}
```

One thread-local read on mutating calls only, and none at all on the read-only calls a
predicate actually uses. That converts an undefined-behaviour footgun into a deterministic,
reportable error a binding author can hit in a unit test — which is the difference between
a rule people follow and a rule people discover in a tournament.

### 5.5 Bulk map data

`isWalkable` is a `bool[1024][1024]`; `isBuildable`/`isVisible`/`isExplored`/`hasCreep` are
`bool[256][256]`; `getGroundHeight` is `int[256][256]` (`GameData.h`). Per-cell FFI calls
mean up to **1,048,576 calls per frame** for a walkability sweep. That would make the ABI
look slow when the underlying library is not.

Give every one of these both forms:
```c
int32_t bwapi_game_is_walkable(int32_t wx, int32_t wy);                 /* scalar */
int32_t bwapi_game_copy_walkability(uint8_t* out, int32_t cap,
                                    int32_t* out_w, int32_t* out_h);    /* bulk */
```
Boolean grids are `uint8_t` per element, one byte per tile, per the bulk half of the
§4 boolean rule — the scalar `int32_t` convention deliberately does **not** apply here.
`getGroundHeight` stays `int32_t` per element, matching its source array.

A borrowed-pointer variant is possible (the data lives in mapped shared memory), but a
**copy** is the right v1 default: a borrowed pointer's validity window across `update()`
is a subtle contract to get wrong, and it would hand a foreign language a writable view of
the server's own memory. Revisit only with a measured need.

### 5.6 Events

Client mode already polls (`Broodwar->getEvents()` → `std::list<Event>`; `BWAPI::Event`
holds a heap `std::string*`). Flatten to indexed access:
```c
typedef struct bwapi_event {
  int32_t type;                    /* EventType::Enum */
  int32_t unit_id, player_id;
  int32_t x, y;                    /* NukeDetect target */
  int32_t is_winner;
  int32_t text_len;                /* fetch via bwapi_game_event_text() */
} bwapi_event;

int32_t bwapi_game_event_count(void);
int32_t bwapi_game_get_event(int32_t index, bwapi_event* out);
int32_t bwapi_game_event_text(int32_t index, char* buf, int32_t buf_len);
```
Polling, not callbacks, for v1: it's the natural client-mode shape, avoids re-entrancy
entirely, and every host language can build its own callback dispatch on top. The
module-mode callback table (§8) reuses the same POD.

### 5.7 Strings, sets, and the `Unitset` broadcast helpers

`Unitset` has 48 broadcast methods (`unitset.attack(pos)` → all units). Rather than
reproducing them, expose the ID-array form:
`bwapi_units_attack_position(const int32_t* ids, int32_t n, int32_t x, int32_t y, int32_t queued)`.
Same for the ~10 that matter; the rest are host-language `for` loops.

### 5.8 Static type data

`UnitType::maxHitPoints()` and friends (~86 on `UnitType`, ~27 on `WeaponType`, plus
Tech/Upgrade/Race/UnitSize) are **pure functions of an `int`** — no game instance needed.
These are the highest-value, lowest-risk part of the whole ABI: a host language can build
its complete static type table at startup with zero game connection, and they're testable
without StarCraft.

The few that return containers need out-buffers:
`requiredUnits()` → `std::map<UnitType,int>` → parallel `(type[], count[])` arrays;
`whatBuilds()` → `std::pair` → two out-params; `abilities()`/`upgrades()`/`buildsWhat()`
→ ID arrays.

Also generate the full **constant set** into `bwapi_c_types.h` — every `UnitTypes::Enum`,
`Orders::Enum`, `TechTypes::Enum`, `UpgradeTypes::Enum`, `WeaponTypes::Enum`,
`Races::Enum`, `Errors::Enum`, `EventType::Enum`, `Flag::Enum`, `CoordinateType::Enum`,
`Text::{Enum,Size::Enum}`, `MouseButton`, `Key`, `Latency::Enum`, `Colors`, position
sentinels. ~700 constants, pure codegen, and it's what makes the ABI pleasant rather than
magic-number soup. Names come from `Type::getName()` / the enum identifiers, so they can't
drift from upstream.

### 5.9 Explicitly excluded from v1

| Excluded | Why |
|---|---|
| `Interface<T>::registerEvent` | Host closures do this better |
| `get/setClientInfo` | Host hash maps do this better |
| `GameWrapper` / `Broodwar << …` | C++ streams |
| `Type::getType(string)` name lookup | Nice-to-have; add later as `bwapi_unittype_from_name` |
| `TournamentModule` | Module-mode-only, and module mode is deferred indefinitely (§8) |
| `getBestUnit` with `BestFilter` | Needs the §5.4 tier-3 callback |

---

## 6. Handle model in detail

```c
typedef int32_t bwapi_unit;    /* Game::getUnit(id)   — O(1) */
typedef int32_t bwapi_player;  /* Game::getPlayer(id) — 0..11 */
typedef int32_t bwapi_force;   /* Game::getForce(id)  — 0..4 */
typedef int32_t bwapi_region;  /* Game::getRegion(id) */
#define BWAPI_NONE (-1)
```

Distinct typedefs (even though all are `int32_t`) so generated Rust/TypeScript wrappers
can newtype them and catch a unit ID passed where a player ID belongs.

### 6.1 Resolution

One internal helper per kind:
```cpp
inline BWAPI::Unit resolve(bwapi_unit id) {
  if (id < 0 || !BWAPI::BroodwarPtr) return nullptr;
  return BWAPI::BroodwarPtr->getUnit(id);
}
```
Every generated wrapper begins with a resolve-and-guard. Cost is a bounds check plus a
vector index — negligible next to the virtual call that follows.

### 6.2 Validity

`bwapi_unit_exists(id)` maps to `UnitInterface::exists()`. Handles do **not** need explicit
release: they are indices into game-owned storage, nothing is retained, nothing leaks.
Document that a handle from frame *N* may resolve to a dead unit at frame *N+1* — exactly
the existing C++ semantics, unchanged.

### 6.3 Bullets

No ID-keyed lookup exists. Snapshot instead:
```c
typedef struct bwapi_bullet {
  int32_t id, player_id, type, source_id, target_id;
  int32_t x, y, target_x, target_y, remove_timer, exists;
  double angle, velocity_x, velocity_y;
} bwapi_bullet;

int32_t bwapi_game_get_bullets(bwapi_bullet* out, int32_t cap);
```
Max 100 (`GameData::bullets[100]`), fully transient, no identity needed across frames.

---

## 7. Repository and layout

**[Review]** The C ABI lives in its own repository, `bwapi-c`, and consumes BWAPI as a
pinned dependency. It is a purely additive layer: nothing here requires a change to the
BWAPI tree, so nothing here belongs in it. Practically, that also means it can pin a
known-good BWAPI revision instead of tracking `main`, and release on its own cadence.

```
bwapi-c/                            # separate repository
  third_party/bwapi/                # git submodule, pinned revision
  include/
    bwapi_c.h            # generated — the single public header
    bwapi_c_types.h      # generated — ~700 constants + PODs
  src/
    abi.cpp              # version, thread-local error channel, log callback,
                         #   the in-predicate depth counter (§5.4)
    client.cpp           # connect / update / disconnect / is_connected
    handles.cpp          # resolve+validate helpers
    game.gen.cpp         # generated
    unit.gen.cpp         # generated
    player.gen.cpp       # generated
    force_region.gen.cpp # generated
    types.gen.cpp        # generated — static type data
    bulk.cpp             # hand-written — map data, collections, events
    commands.cpp         # hand-written — UnitCommand, unit-set broadcasts
  tools/abi/
    spec/{game,unit,player,types,...}.yaml   # the source of truth
    emit_header.py  emit_source.py  emit_json.py
    check_coverage.py                        # libclang drift detector
  api.json               # generated — for downstream binding generators
  tests/
    header_hygiene/      # C99 + C++ compile checks
    layout_assert.cpp    # GameData offsets, x86 vs x64 (§10.2)
    types_test.cpp       # ~500 static-type assertions, no game needed
    mock_server/         # fake BWAPI server for CI (§11)
  bindings/
    rust/bwapi-sys/      # raw FFI only
    node/                # raw FFI only
  examples/{rust-example-bot,node-example-bot}/
  CMakeLists.txt
  BWAPI_C.def
```

### What lives here, and what doesn't

**[Review]** `bindings/` in this repo holds the **raw FFI layer only** — `bwapi-sys` in
Rust terms: `extern` declarations, constants, build glue, nothing more. Keeping it here
means it is generated from the same `api.json`, versioned with the ABI, and regression-
tested in one CI run against the mock server.

The **idiomatic, safe, host-language wrapper** for each language lives in its own
repository, released on that ecosystem's cadence: a `bwapi` crate published from a Rust
repo, an npm package published from a JS repo. Those are where `Unit` newtypes, `Result`,
iterators, and async integration belong, and they should not be gated on this repo's
release process.

**Rust and JavaScript are the only two bindings in scope for in-tree `bindings/`.** Others
are welcome downstream — `api.json` exists precisely so they need nothing from here.

`*.gen.cpp` and `api.json` are **checked in**, not built by a codegen step at compile time.
Contributors without Python still get a buildable tree, diffs are reviewable, and CI
verifies that regenerating produces no change.

---

## 8. Module mode — deferred indefinitely

**[Review] Not in scope.** Deferred until someone presents a concrete reason it is
necessary. Client mode covers the non-C++ audience, and module mode carries real costs:
x86-forever, no crash isolation, and a much harder test story. The sketch below is kept
only so a future revival does not start from a blank page — nothing in the roadmap (§12)
depends on it.

Module mode requires the bot DLL to export `newAIModule()` returning a pointer to an object
with an **MSVC C++ vtable** matching `BWAPI::AIModule`. A Rust `cdylib` *can* fake that, but
it's a hand-laid vtable pinned to one compiler's ABI — exactly the fragility this project
exists to remove.

The way out would be to **make `bwapi_c.dll` itself the loadable AI module**: it exports
`newAIModule`/`gameInit`, and on `gameInit` loads a *host* DLL named by config, resolving
pure-C entry points against a vtable of function pointers —

```c
typedef struct bwapi_bot_vtable {
  void (BWAPI_C_CALL *on_start)(void* bot);
  void (BWAPI_C_CALL *on_end)(void* bot, int32_t is_winner);
  void (BWAPI_C_CALL *on_frame)(void* bot);
  void (BWAPI_C_CALL *on_unit_create)(void* bot, bwapi_unit u);
  /* … one per AIModule virtual … */
  void (BWAPI_C_CALL *destroy)(void* bot);
} bwapi_bot_vtable;

/* the host DLL would export exactly this: */
void* BWAPI_C_CALL bwapi_bot_create(const bwapi_bot_vtable** out_vtable);
```

— with one C++ class deriving `BWAPI::AIModule` forwarding every override to the table,
each guarded by `catch(...)`. There is precedent for the indirection:
`bwapi/AIModuleLoader/Source/AIModuleLoader.cpp:69` already does load-and-`GetProcAddress`.

Note that dropping module mode also removes this project's only dependency on
`CMake/BWAPI` — see §10.1, where that turns out to matter more than expected.

---

## 9. Codegen: spec-driven, with drift detection

**Rejected: parse the headers and emit directly (libclang/SWIG).** The headers use MSVC
extensions, `#pragma warning`, and heavy templates; a parser-driven generator is brittle
and, worse, would silently change the public ABI when an upstream header is edited. ABI
stability has to be a deliberate act.

**Rejected: hand-write 600 wrappers.** Guaranteed inconsistency in exactly the details
(buffer semantics, invalid-handle behaviour) that matter most.

**Recommended: a checked-in YAML spec + a small Python emitter + a libclang coverage
check.**

Spec entries are terse because the patterns are few:
```yaml
- cpp: "UnitInterface::getHitPoints"
  c:   "bwapi_unit_get_hit_points"
  self: unit
  returns: int32

- cpp: "UnitInterface::attack(Position, bool)"
  c:   "bwapi_unit_attack_position"
  self: unit
  params: [{name: x, type: int32}, {name: y, type: int32},
           {name: queued, type: bool32}]
  returns: bool32
  body: "return self->attack(BWAPI::Position(x, y), queued != 0);"

- cpp: "Game::mapName"
  c:   "bwapi_game_map_name"
  returns: string_out

- cpp: "UnitInterface::registerEvent"
  skip: "host-language closures supersede this"
```

Handful of return kinds cover nearly everything: `int32`, `bool32`, `double`, `type`
(→`int32`), `position`(→two out-params), `handle`, `string_out`, `id_array`, `void`.
Anything unusual carries an inline `body:`.

Emitters produce: `bwapi_c.h`, `bwapi_c_types.h`, the `*.gen.cpp` files, `BWAPI_C.def`,
and **`api.json`** — the machine-readable description that downstream Rust/TS/Python
generators consume, so binding authors never re-parse C.

**`check_coverage.py` is the piece that keeps this honest.** It parses `Game.h`, `Unit.h`,
`Player.h`, `Bullet.h`, `Region.h`, `Force.h`, and the `*Type.h` headers with libclang and
fails CI if a public declaration has **neither** a spec entry **nor** an explicit
`skip:` with a reason. Adding a method upstream then forces a conscious decision instead
of silent divergence. This inverts the usual binding-rot failure mode.

---

## 10. Build and packaging

### 10.1 Building against BWAPI from outside its tree

**[Review]** No `.vcxproj` in `bwapi.sln`. **CMake is the only build system**, which is
also what Rust `build.rs`, `node-gyp`, and CI actually want. BWAPI comes in as a pinned
submodule at `third_party/bwapi`.

`CMake/README.md` documents the supported consumption path — `ADD_SUBDIRECTORY` of
`CMake/BWAPI/` and `CMake/Client/`, then link `BWAPI-Static` and `BWAPIClient`. Following
it exactly is the obvious first move, and it has two frictions worth planning around
rather than discovering in phase 3:

**`CMake/Client` alone is not enough to link a client bot.** It builds only
`BWAPIClient/Source` + `Util` + `Shared`. The type tables and the non-virtual convenience
methods live in `BWAPILIB/Source/*.cpp` (`UnitType.cpp`, `Game.cpp`, `Unit.cpp`,
`Position.cpp`, …), which the CMake tree compiles **only** as part of `BWAPI-Static`. The
MSBuild path confirms the real dependency set: `ExampleAIClient.vcxproj` references both
`BWAPIClient.vcxproj` *and* `BWAPILIB.vcxproj`. So a CMake client consumer is pushed into
`BWAPI-Static`, which drags in `BW/*`, `Detours.cpp`, `CodePatch.cpp`, `Storm`, and the
whole injected-DLL source set it will never call.

**`BWAPI-Static` needs a generated `svnrev.h`.** `BWAPILIB/Source/BWAPI.cpp` includes it
for `BWAPI_getRevision()`; it is gitignored (`.gitignore:4-5`) and produced by
`CMake/BWAPI`'s `ADD_CUSTOM_COMMAND` running `cscript.exe revisionUpdate.vbs` — a Windows
Script Host step in a build that otherwise needs only CMake and MSVC.

Two options, and the second is better:

1. Depend on `BWAPI-Static` as documented, and make the `bwapi-c` CMake generate a minimal
   `svnrev.h` when one is absent. Works today, no upstream change, but links a large
   amount of dead code into `bwapi_c.dll`.
2. Define a **client-only static target inside `bwapi-c`'s own CMakeLists** — compiling
   `BWAPILIB/Source/*.cpp` + `BWAPIClient/Source/*.cpp` + `Shared/*.cpp` + `Util/Source` +
   `Storm` from the submodule, with a generated `svnrev.h`. That is the actual dependency
   set, it needs nothing from upstream, and dropping module mode (§8) means `CMake/BWAPI`
   is not needed at all.

Take option 2, and *separately* offer upstream a small purely-additive
`CMake/BWAPILIB/CMakeLists.txt` (or an option on `CMake/Client`) so any CMake client
consumer gets the same thing without hand-rolling it. That is a genuine improvement to
BWAPI on its own merits — but per the additive goal (§2), `bwapi-c` must build whether or
not it is ever accepted.

Two settings to carry over from the existing CMake: `ADD_DEFINITIONS(/DNOMINMAX=1)`, and
`BWAPI_CUSTOM_COMPILE_FLAGS` as the documented hook for matching compile flags across the
boundary. Statically link the CRT (`/MT`) so consumers don't need a matching VC++
redistributable — safe precisely because no allocation crosses the boundary (§4).

### 10.2 The x64 question

Prove or disprove §1.4 **early** — it changes what's possible for Node and for default Rust
toolchains, and it's cheap to test:

1. Add a `layout_assert.cpp` with `static_assert(sizeof(GameData) == …)` plus
   `static_assert(offsetof(GameData, <field>) == …)` for **every** `GameData`, `UnitData`,
   `PlayerData`, `RegionData`, `BulletData`, and `BWAPIC::*` field, with values captured
   from the x86 build.
2. Compile that file for x64. If it passes, ship an **x64 client-mode-only** variant
   (`bwapi_c64.dll`); if not, the asserts pinpoint exactly which field diverges.
3. Either way the asserts stay in the build as regression protection — they're valuable
   independent of the x64 outcome, since a careless upstream edit to `GameData` silently
   breaks server/client compatibility today.

Module mode would have stayed x86-forever (it's injected into a 32-bit process); with it
deferred (§8), x86 is no longer a floor the project has to respect — if x64 works, x64 can
be the primary target.

### 10.3 Distribution

**[Review]** `bwapi-c` ships its own artifacts; it does not add anything to BWAPI's
`Release_Binary/` or installer.

- A `bwapi-c` GitHub Release asset per platform — `bwapi-c-<version>-win32.zip` and, if
  §10.2 permits, `-win64.zip` — containing `.dll`, `.lib`, `.def`, headers, and
  `api.json`. This is what a `bwapi-sys` `build.rs` or an npm postinstall downloads.
- Each release records the **pinned BWAPI revision** it was built against, so a consumer
  can tell at a glance which server versions a given `bwapi_c.dll` speaks to. Pair it with
  `BWAPI::CLIENT_VERSION` (10003 today), which the server already version-checks at
  connect time (`BWAPIClient/Source/Client.cpp`).
- `bindings/rust/bwapi-sys` published to crates.io from this repo; the safe `bwapi` crate
  is published from its own repo (§7), as is the idiomatic npm package.

---

## 11. Testing

Ordered by value per unit of effort.

1. **Header hygiene.** Compile `bwapi_c.h` standalone as C99 (`/TC`), as C++, and twice in
   one TU (include-guard check). Catches C++-isms leaking into the public header — the most
   common failure in hand-maintained C ABIs.
2. **Symbol golden file.** Check in `bwapi_c.symbols`; CI diffs `dumpbin /exports`. Any
   accidental ABI change fails the build loudly. This is the mechanism that makes the
   stability promise in §4 real rather than aspirational.
3. **Layout asserts.** §10.2.
4. **Coverage check.** §9.
5. **Static-type-data tests, no game required.** ~500 assertions
   (`bwapi_unittype_mineral_price(BWAPI_UNIT_TERRAN_MARINE) == 50`) validating the largest
   generated block with zero infrastructure. `bwapi/BWAPILIBTest` is a good model for the
   style, though the tests live in `bwapi-c/tests/`.
6. **Mock server — the high-value one.** Client mode only needs a process that creates
   `Local\bwapi_shared_memory_game_list`, a `Local\bwapi_shared_memory_<pid>` mapping, and
   `\\.\pipe\bwapi_pipe_<pid>`, then drives the frame handshake
   (`bwapi/BWAPI/Source/BWAPI/Server.cpp:40,99,194` documents the whole protocol; ~300
   lines to reproduce the client-facing half). With it, the **entire client-mode ABI is
   testable on Windows CI with no StarCraft installation** — hand-populate `GameData` with
   a synthetic map and units and assert the ABI reports them. This is the difference
   between a binding that's tested and one that's merely compiled, and it's reusable by
   every future language binding.
7. **Re-entrancy tests.** Assert that a predicate calling a read-only function works and
   returns correct results, and that one calling a mutating function gets
   `BWAPI_ERR_REENTRANT_MUTATION` with no side effect (§5.4). The guard is only worth
   having if it is covered.
8. **Upstream-drift canary.** A scheduled CI job that bumps the `third_party/bwapi`
   submodule to upstream `main` and runs the coverage check and layout asserts. It does not
   gate merges — it just tells us early when BWAPI changes something the ABI cares about.
   This is the tax for living in a separate repo, and it is small.
9. **End-to-end smoke.** The Rust and Node example bots run against real StarCraft as a
   pre-release manual gate.

---

## 12. Roadmap

Sizes are rough order-of-magnitude, assuming one developer with a Windows/MSVC setup.
Module mode is gone (§8); the old phase 6 becomes phase 5.

| Phase | Deliverable | Size |
|---|---|---|
| **0. Bootstrap & conventions** | `bwapi-c` repo created with BWAPI pinned as a submodule; client-only CMake target per §10.1 building an empty DLL; `bwapi_c.h` skeleton with the §4 conventions, version/error/log functions; layout asserts (§10.2) answering the x64 question | S |
| **1. Generator** | YAML spec format, `emit_*.py`, `check_coverage.py`, `api.json`; hand-written spec for `Player` (54 decls) as the proving ground; symbol golden file + header hygiene in CI | M |
| **2. Static types** | Full `bwapi_c_types.h` (~700 constants) + all `*Type` static data (~150 functions); ~500 unit tests. **Independently useful even before the game layer** — a language can ship a complete type table now | M |
| **3. Client mode, complete** | `Game` (~120 entry points after the §5.2 collapse), `Unit` (~250), `Force`/`Region`, commands, events, bulk map data, collections; the mock server and its test suite. **This is the milestone at which a real bot can be written in another language** | L |
| **4. Consumers** | `bwapi-sys` crate + Node raw FFI in `bindings/`, plus example Rust and JS bots (with the blocking-`update()` worker-thread pattern documented). Safe/idiomatic wrappers get spun out to their own repos here. Each one shakes out real ABI ergonomics — expect to revise phase 3 | M |
| **5. Filters & polish** | Callback predicates with the §5.4 safe subset and its re-entrancy guard, `Type::getType` name lookup, borrowed-pointer bulk access if measurement justifies it | S |

Phases 0–3 are the real project. Phase 4 is what makes it credible. Phase 5 is a follow-on.

---

## 13. Risks, and decisions taken

| Risk | Mitigation |
|---|---|
| **x86-only shuts out Node and default Rust** | Settle §10.2 in phase 0. If x64 client mode works this evaporates — and with module mode dropped, x64 can be the primary target. If not, document `i686-pc-windows-msvc` / 32-bit Node as hard requirements, up front |
| **Living in a separate repo means silently drifting from BWAPI** | The pinned submodule makes drift a deliberate act, not an accident; the scheduled drift canary (§11.8) reports it early; the coverage check names exactly which declarations changed |
| **`unordered_set` iteration order leaks nondeterminism into bots** | Sort by ID at the boundary (§4). Decide once, document loudly |
| **~600 entry points is a lot of surface to keep correct** | Codegen + coverage check + symbol golden file. The generator is the deliverable; the wrappers are output |
| **A predicate mutates the state the enclosing query is reading** | The §5.4 safe subset, enforced by a thread-local guard that fails mutating calls deterministically, plus tests that cover both sides of it |
| **A foreign callback unwinds into BWAPI's stack** | `catch(...)` at every callback site; document "must not throw / must not panic across FFI"; callbacks are phase 5 regardless |
| **256-byte text truncation surprises users** | Document on every text function; worth proposing upstream separately |
| **The ABI drifts from its own past releases** | `bwapi_abi_version()` + append-only policy + golden symbols; every release records the BWAPI revision it was built against |
| **Someone builds language bindings on the shared-memory layout instead** | The mock server and `api.json` make the C ABI the path of least resistance. Worth saying plainly in the README: re-implementing `Templates.h` per language is a maintenance trap |

### Decisions from the PR #1 review

All five open questions from revision 1 are answered; they are recorded here rather than
left open.

| # | Question | Decision |
|---|---|---|
| 1 | Is x64 client mode in scope? | **Yes, if phase 0 shows it is technically feasible.** Settled by the layout asserts in §10.2 before anything else is built |
| 2 | Is `bindings/` in-tree or in separate repos? | **Both, split by layer.** The raw FFI core — `bwapi-sys` in Rust terms — lives in `bindings/` in the `bwapi-c` repo. The idiomatic, safe, host-language package lives in its own repo per language. **Rust and JavaScript are the only two in scope** for in-tree bindings today (§7) |
| 3 | Is a Python dependency in CI acceptable for the generator? | **Yes.** Generated sources stay checked in, so it remains a CI-only dependency and never a contributor one |
| 4 | Does module mode justify a phase? | **No — deferred indefinitely**, until someone presents a compelling reason it is necessary. The design sketch is retained in §8; nothing in the roadmap depends on it |
| 5 | Should `swig.i` / `swig_lib/` be removed from BWAPI? | **No.** `BWAPI_C` lives in a separate repository, so it does not displace anything in this tree and has no standing to propose removals here |

The structural consequence of Q2 and Q5 together is goal 7 in §2: **the ABI is purely
additive.** It depends on BWAPI, proposes improvements to BWAPI separately and on their own
merits (§10.1), and must build regardless of whether any of them are accepted.

---

## 14. What a consumer sees

**Rust** (`bwapi-sys`, generated from `api.json`):
```rust
extern "C" {
    fn bwapi_client_connect() -> i32;
    fn bwapi_client_update();
    fn bwapi_game_is_in_game() -> i32;
    fn bwapi_game_get_all_units(out: *mut i32, cap: i32) -> i32;
    fn bwapi_unit_get_type(unit: i32) -> i32;
    fn bwapi_unit_attack_unit(unit: i32, target: i32, queued: i32) -> i32;
}
```
…wrapped into a safe `bwapi` crate with `Unit(i32)` newtypes, `Result`, and iterators —
published from its own repository, per §7.

**JavaScript** (koffi, if x64 client mode lands):
```js
const lib = koffi.load('bwapi_c.dll');
const connect = lib.func('int32_t bwapi_client_connect()');
const allUnits = lib.func('int32_t bwapi_game_get_all_units(_Out_ int32_t *out, int32_t cap)');
```
with `bwapi_client_update()` on a worker thread, since it blocks on the pipe.

Neither consumer compiles a line of C++. That is the whole point.
