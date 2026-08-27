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
- The client static-lib CMake path is exactly where a new shared-library target belongs.

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
5. **Not** exposing `std::function` filters as first-class composable objects. See §7.4.

---

## 3. Recommended shape

> **A new `BWAPI_C` project producing `bwapi_c.dll`, wrapping `BWAPI::Game` behind
> `int32_t` handles, generated from a checked-in API spec, client-mode first, module mode
> in a later phase.**

Four decisions worth defending:

**Client mode first.** It needs no injection, no C++ vtable interop, isolates crashes, and
is what Rust/JS users actually want. Because both modes sit behind `BWAPI::Game`, ~95% of
the wrapper source is mode-agnostic and gets reused when module mode lands.

**ID handles, not pointers.** §1.3. Also means the ABI is trivially usable from a language
without raw-pointer types, and handles can be logged/replayed.

**Generated, not hand-written.** §1.8 and §11.

**Naming.** ⚠️ Do **not** name the project `BWAPIC` — `namespace BWAPIC` already exists in
`bwapi/include/BWAPI/Client/*.h` for the shared-memory PODs. Proposal:

| Thing | Name |
|---|---|
| VS project / CMake target | `BWAPI_C` |
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

**Types.** Only `int32_t`, `uint32_t`, `double`, `char`, `void*`, function pointers, and
PODs declared in `bwapi_c.h`. No `bool` (use `int32_t`, 0/1 — C++ `bool` is 1 byte but its
ABI treatment across FFIs is a recurring footgun). No enums in signatures — `int32_t` with
`#define`d constants, so an unknown future value can't be UB in a strict-enum language.
No structs by value in or out; pass `const T*` in, `T*` out.

**Naming.** `bwapi_<subject>_<verb>[_<disambiguator>]`, snake_case:
`bwapi_unit_get_hit_points`, `bwapi_unit_attack_position`, `bwapi_unit_attack_unit`,
`bwapi_unittype_max_hit_points`. Overloads get an explicit type-suffixed name — no
mangling, no defaulted arguments (C has none, so every parameter is explicit and the
per-language wrapper re-adds defaults).

**Booleans out.** `int32_t`, 0 or 1.

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
   typedef int32_t (BWAPI_C_CALL *bwapi_unit_pred)(int32_t unit_id, void* user);
   ```
   Wrap every invocation in `try{}catch(...){ return false; }` — a foreign callback
   throwing (a Rust panic unwinding across FFI, say) through BWAPI's stack would be
   catastrophic. Document that the callback must not call back into the ABI.

An enum-based mini-filter DSL (`BWAPI_FILTER_IS_WORKER`) is tempting and should be
**declined**: it re-encodes upstream semantics in a switch statement that silently rots.

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
In client mode a borrowed-pointer variant is possible (the data lives in mapped shared
memory), but a **copy** is the right v1 default: it works identically in module mode, and
a borrowed pointer's validity window across `update()` is a subtle contract to get wrong.
Revisit only with a measured need.

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
| `TournamentModule` | Niche; module-mode-only; after §8 |
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

## 7. Directory layout

```
bwapi/BWAPI_C/
  include/
    bwapi_c.h            # generated — the single public header
    bwapi_c_types.h      # generated — ~700 constants + PODs
  Source/
    abi.cpp              # version, thread-local error channel, log callback
    client.cpp           # connect / update / disconnect / is_connected
    handles.cpp          # resolve+validate helpers
    game.gen.cpp         # generated
    unit.gen.cpp         # generated
    player.gen.cpp       # generated
    force_region.gen.cpp # generated
    types.gen.cpp        # generated — static type data
    bulk.cpp             # hand-written — map data, collections, events
    commands.cpp         # hand-written — UnitCommand, unit-set broadcasts
    module_shim.cpp      # phase 4 — AIModule -> C callback table
  BWAPI_C.def
  BWAPI_C.vcxproj
tools/abi/
  spec/{game,unit,player,types,...}.yaml   # the source of truth
  emit_header.py  emit_source.py  emit_json.py
  check_coverage.py                        # libclang drift detector
  api.json                                 # generated — for downstream generators
CMake/BWAPI_C/CMakeLists.txt
bindings/rust/{bwapi-sys,bwapi}/
bindings/node/
examples/{rust-example-bot,node-example-bot}/
```

`*.gen.cpp` are **checked in**, not built by a codegen step at compile time. Contributors
without Python still get a buildable tree, diffs are reviewable, and CI verifies that
regenerating produces no change.

---

## 8. Module mode (later phase)

Module mode requires the bot DLL to export `newAIModule()` returning a pointer to an object
with an **MSVC C++ vtable** matching `BWAPI::AIModule`. A Rust `cdylib` *can* fake that, but
it's a hand-laid vtable pinned to one compiler's ABI — exactly the fragility this project
exists to remove.

**Better: make `bwapi_c.dll` the loadable AI module.** It exports `newAIModule`/`gameInit`
itself, and on `gameInit` loads a *host* DLL named by config, resolving pure-C entry points:

```c
typedef struct bwapi_bot_vtable {
  void (BWAPI_C_CALL *on_start)(void* bot);
  void (BWAPI_C_CALL *on_end)(void* bot, int32_t is_winner);
  void (BWAPI_C_CALL *on_frame)(void* bot);
  void (BWAPI_C_CALL *on_unit_create)(void* bot, bwapi_unit u);
  /* … one per AIModule virtual … */
  void (BWAPI_C_CALL *destroy)(void* bot);
} bwapi_bot_vtable;

/* the host DLL exports exactly this: */
void* BWAPI_C_CALL bwapi_bot_create(const bwapi_bot_vtable** out_vtable);
```

`module_shim.cpp` holds one C++ class deriving `BWAPI::AIModule` whose every override
forwards to the table (guarded by `catch(...)`). There is precedent in-tree:
`bwapi/AIModuleLoader/Source/AIModuleLoader.cpp:69` already does load-and-`GetProcAddress`
indirection.

Deferred to phase 4 because it is x86-only, harder to test, and client mode covers the
demand.

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

### 10.1 Targets

1. **`BWAPI_C.vcxproj`** in `bwapi/bwapi.sln`, `Win32`, `v141_xp`, `stdcpp17`, matching the
   existing projects. Links `BWAPIClient` + `BWAPILIB` + `Util` + `Shared` statically, so
   consumers get exactly one DLL.
2. **`CMake/BWAPI_C/CMakeLists.txt`**, mirroring `CMake/Client/CMakeLists.txt` but
   `ADD_LIBRARY(... SHARED ...)`. This is the path non-MSBuild consumers (Rust `build.rs`,
   `node-gyp`, CI) will actually use. Note the existing warning in
   `CMake/BWAPI/CMakeLists.txt` about the *injected* DLL not working — that does not apply
   here; the client-side link has no injection or code-patching.
3. Statically link the CRT (`/MT`) so consumers don't need a matching VC++ redistributable.
   Since no allocation crosses the boundary (§4), a private CRT is safe.

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

Module mode stays x86-forever (it's injected into a 32-bit process).

### 10.3 Distribution

- `Release_Binary/` gains `bwapi_c.dll` + headers, so the installer ships them.
- A GitHub Release asset `bwapi-c-<version>-win32.zip` (`.dll`, `.lib`, `.def`, headers,
  `api.json`) — the artifact a `bwapi-sys` `build.rs` or an npm postinstall downloads.
- `bindings/rust/` published as `bwapi-sys` (raw) + `bwapi` (safe), following the
  ecosystem's `-sys` convention.

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
   generated block with zero infrastructure. Fits `bwapi/BWAPILIBTest`'s existing pattern.
6. **Mock server — the high-value one.** Client mode only needs a process that creates
   `Local\bwapi_shared_memory_game_list`, a `Local\bwapi_shared_memory_<pid>` mapping, and
   `\\.\pipe\bwapi_pipe_<pid>`, then drives the frame handshake
   (`bwapi/BWAPI/Source/BWAPI/Server.cpp:40,99,194` documents the whole protocol; ~300
   lines to reproduce the client-facing half). With it, the **entire client-mode ABI is
   testable on Windows CI with no StarCraft installation** — hand-populate `GameData` with
   a synthetic map and units and assert the ABI reports them. This is the difference
   between a binding that's tested and one that's merely compiled, and it's reusable by
   every future language binding.
7. **End-to-end smoke.** The Rust and Node example bots run against real StarCraft as a
   pre-release manual gate.

---

## 12. Roadmap

Sizes are rough order-of-magnitude, assuming one developer with a Windows/MSVC setup.

| Phase | Deliverable | Size |
|---|---|---|
| **0. Conventions** | This doc reviewed and agreed; `bwapi_c.h` skeleton with §4 conventions, version/error/log functions; `BWAPI_C.vcxproj` + CMake target building an empty DLL; layout asserts (§10.2) answering the x64 question | S |
| **1. Generator** | YAML spec format, `emit_*.py`, `check_coverage.py`, `api.json`; hand-written spec for `Player` (54 decls) as the proving ground; symbol golden file + header hygiene in CI | M |
| **2. Static types** | Full `bwapi_c_types.h` (~700 constants) + all `*Type` static data (~150 functions); ~500 unit tests. **Independently useful even before the game layer** — a language can ship a complete type table now | M |
| **3. Client mode, complete** | `Game` (~120 entry points after the §5.2 collapse), `Unit` (~250), `Force`/`Region`, commands, events, bulk map data, collections; the mock server and its test suite. **This is the milestone at which a real bot can be written in another language** | L |
| **4. Consumers** | `bwapi-sys` + safe `bwapi` crate + example Rust bot; Node binding (koffi or N-API) + example JS bot, with the blocking-`update()` worker-thread pattern documented. Each one shakes out real ABI ergonomics — expect to revise phase 3 | M |
| **5. Module mode** | `module_shim.cpp`, the `bwapi_bot_vtable` loader (§8), x86-only; `TournamentModule` if wanted | M |
| **6. Filters & polish** | Callback predicates (§5.4 tier 3), `Type::getType` name lookup, borrowed-pointer bulk access if measurement justifies it | S |

Phases 0–3 are the real project. 4 is what makes it credible. 5–6 are follow-ons.

---

## 13. Risks and open decisions

| Risk | Mitigation |
|---|---|
| **x86-only shuts out Node and default Rust** | Settle §10.2 in phase 0. If x64 client mode works, this evaporates. If not, document `i686-pc-windows-msvc` / 32-bit Node as hard requirements, up front |
| **`unordered_set` iteration order leaks nondeterminism into bots** | Sort by ID at the boundary (§4). Decide once, document loudly |
| **600 entry points is a lot of surface to keep correct** | Codegen + coverage check + symbol golden file. The generator is the deliverable; the wrappers are output |
| **Foreign callback unwinds into BWAPI's stack** | `catch(...)` at every callback site; document "must not throw / must not panic across FFI"; defer callbacks to phase 6 |
| **256-byte text truncation surprises users** | Document on every text function; consider an upstream fix separately |
| **ABI drifts from BWAPI across releases** | `bwapi_abi_version()` + append-only policy + golden symbols + coverage check |
| **Someone builds language bindings on the shared-memory layout instead** | The mock server and `api.json` make the C ABI the path of least resistance. Also worth saying plainly in the README: re-implementing `Templates.h` per language is a maintenance trap |

**Open questions for the maintainers:**

1. **x64 client mode: in scope?** Determines the Node story. (Phase 0 answers the technical
   half; the shipping decision is yours.)
2. **Is `bindings/` in-tree or separate repos?** In-tree keeps them versioned with the ABI
   and testable in one CI run; separate repos suit the crates.io/npm release cadence better.
   Recommendation: `bindings/rust` and `bindings/node` in-tree as the reference consumers,
   published from here.
3. **Is a Python dependency in CI acceptable** for the generator? (Generated sources are
   checked in, so it's a CI-only dependency, not a contributor one.)
4. **Does module mode matter enough** to justify phase 5, or is client mode sufficient for
   the non-C++ audience?
5. **Should `bwapi/include/swig.i` and `swig_lib/` be removed** once this lands? They are an
   unfinished parallel attempt at the same goal; leaving both invites confusion.

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
…wrapped into a safe `bwapi` crate with `Unit(i32)` newtypes, `Result`, and iterators.

**JavaScript** (koffi, if x64 client mode lands):
```js
const lib = koffi.load('bwapi_c.dll');
const connect = lib.func('int32_t bwapi_client_connect()');
const allUnits = lib.func('int32_t bwapi_game_get_all_units(_Out_ int32_t *out, int32_t cap)');
```
with `bwapi_client_update()` on a worker thread, since it blocks on the pipe.

Neither consumer compiles a line of C++. That is the whole point.
