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

> **Revision 3.** Two facts now shape the plan. **StarCraft is 28 years old and BWAPI is
> 14; neither is going to churn**, so effort priced for a moving dependency (continuous
> drift detection) is redirected toward initial completeness and a deliberate pin-bump
> procedure. And **BWAPI is LGPL-3.0**, which `bwapi-c` inherits — §0.
>
> Substantive changes from revision 2: licensing (§0); positions packed into one `int64_t`
> (§4); a sticky error channel (§4); size-prefixed PODs (§4); the normative frame loop
> (§4.1); per-frame snapshots (§5.10); determinism extended to the closest-unit queries
> (§5.4, §H); the link closure derived rather than declared, which also answers the x64
> gating question (§10.1); a generated layout dump replacing hand-written offset asserts
> (§10.2); the pin-bump checklist replacing the drift canary (§10.3, §11); an eight-phase
> roadmap with testable exit criteria (§12); a divergence register (§15); module mode
> moved to Appendix A.

---

## 0. Licensing

`bwapi-c` is **LGPL-3.0**, inherited from BWAPI (`LICENSE`, `LICENSE.md`). Three
consequences, two of which constrain the design and one of which constrains distribution.

**1. `bwapi_c.dll` must be consumed dynamically, never statically linked into a consumer.**
`bwapi-sys` links the import library; the Node binding `dlopen`s. This is what keeps a
closed-source bot clean under LGPL §4 — the user must be able to relink against a modified
`bwapi_c.dll`. Revision 2 already arrived at dynamic consumption by accident of
convenience; it is now a requirement with a reason attached. A static-library variant of
`bwapi_c` must not be offered, however often it is asked for.

**2. The static CRT (`/MT`, §10.1) is fine.** LGPL-3.0's System Libraries exception covers
the MSVC runtime. Keep `/MT`; the no-allocation-crosses-the-boundary argument in §4 stands
on its own merits, independent of licensing.

**3. Every release asset ships `COPYING.LESSER`, `COPYING`, and a link to the source at the
exact tagged commit.** The crates.io and npm packages carry the same. The README states
that the ABI's license does not propagate to bots that merely call it — that question will
otherwise be asked in every issue tracker downstream.

---

## 1. Ground truth: what the codebase actually looks like

These facts drive every decision below.

### 1.1 One interface, one wrapper

`bwapi-c` targets **client mode**: the bot is a separate process that finds a server via
`Local\bwapi_shared_memory_game_list`, opens `\\.\pipe\bwapi_pipe_<pid>` and maps
`Local\bwapi_shared_memory_<pid>` onto `BWAPI::GameData`
(`bwapi/BWAPIClient/Source/Client.cpp`; server side at
`bwapi/BWAPI/Source/BWAPI/Server.cpp:40,99,194`).

Module mode — the injected 32-bit DLL — is deferred indefinitely; see **Appendix A**. The
one fact worth keeping in the live document is that *both* modes implement the same
abstract `BWAPI::Game` (`bwapi/include/BWAPI/Game.h`), with `bwapi/Shared/` (≈4,700 lines,
of which `Templates.h` is 3,098) compiled into both. That is what would make a revival
cheap, and what lets this wrapper be written against one interface rather than a transport.

### 1.2 The type system is already almost FFI-shaped

`BWAPI::Type<T, UnknownId>` (`bwapi/include/BWAPI/Type.h`) has exactly **one** data
member — `int tid` — no virtuals, plus `constexpr operator int()`. So all of
`UnitType`, `Race`, `TechType`, `UpgradeType`, `WeaponType`, `Order`, `UnitCommandType`,
`UnitSizeType`, `GameType`, `PlayerType`, `BulletType`, `DamageType`, `ExplosionType`,
`Error`, `Color` are **`int32_t` at the ABI level**. No conversion code, no wrapper
objects, no lifetime questions.

`Position` / `TilePosition` / `WalkPosition` are `Point<int, Scale>`
(`bwapi/include/BWAPI/Position.h`) — two `int`s, distinguished only by a compile-time
scale. §4 packs them into a single `int64_t` on return; the scale becomes a naming
convention (`_position` = pixels, `_tile` = 32px, `_walk` = 8px).

Sentinels: `Positions::Invalid/None/Unknown/Origin` (`Position.h:399-408`) and their
tile/walk equivalents, exported in both packed and unpacked form (§4).

### 1.3 Object identity maps cleanly onto integer IDs

`Unit`, `Player`, `Force`, `Region`, `Bullet` are typedefs for pointers to interface
classes. But every one exposes `getID()`, and `Game` already has ID-keyed lookups:
`GameImpl::getUnit(id)` is a direct index into a 10,000-entry vector
(`bwapi/BWAPIClient/Source/GameImpl.cpp:382`, populated in the constructor at `:29`) —
**O(1)**; `getPlayer` (12), `getForce` (5), `getRegion` likewise.

**Consequence: use `int32_t` IDs as ABI handles, not raw pointers.** They are stable
across frames, serializable, cheap to validate, cannot dangle, are already what the
shared-memory protocol speaks, and let the host language store them in plain arrays.
Exposing `UnitInterface*` would be faster by a pointer chase and vastly more dangerous.

One exception: **bullets have no `Game::getBullet(id)`**. Client-side they live in a
fixed 100-entry `bulletVector`; `BulletInterface::getID()` returns the game's bullet ID,
not the index. Bullets are transient and few, so expose them as a **POD snapshot array**
rather than handles (§6.3).

### 1.4 `GameData` is pointer-free

`bwapi/include/BWAPI/Client/GameData.h` and its members (`UnitData`, `PlayerData`,
`ForceData`, `RegionData`, `BulletData`, `BWAPIC::{Event,Command,Shape,UnitCommand,Position}`,
`unitFinder`) contain **only** `int`, `unsigned`, `bool`, `double`, `char[]`,
`unsigned short`, plain enums, and one `BWAPI::UnitCommandType` (which is the 4-byte
`Type` above). No pointers, no `size_t`, no `long`.

Since MSVC x86 and x64 agree on all of those sizes *and* on `alignof(double) == 8`, the
`GameData` layout should be **byte-identical between 32- and 64-bit builds**.

That claim depends on default 8-byte struct member alignment, and the tree bears it out:
the only project setting `StructMemberAlignment` is **Storm** (`Storm.vcxproj:36`,
`1Byte`), and there is no `#pragma pack` anywhere under `include/`, `Shared/`, or
`Util/Source`. Storm is excluded from the client build for independent reasons (§10.1),
which removes the one setting that could have invalidated this. Verify with a grep in
phase 0 and then stop worrying about it.

### 1.5 Build reality

- Every C++ project in `bwapi/bwapi.sln` (24 projects) is **`Win32` only**, MSVC toolset
  `v141_xp`, `stdcpp17`.
- CMake exists but builds **static libs** only (`CMake/BWAPI/`, `CMake/Client/`); the
  BWAPI DLL target is commented out as non-functional.
- The CMake tree has **no client-only equivalent of `BWAPILIB`**: those sources are
  compiled only into `BWAPI-Static`, which also drags in the injected-DLL sources and a
  `cscript.exe`-generated `svnrev.h`. §10.1 derives what a client build actually needs,
  and the answer is smaller than either existing target.

### 1.6 Prior art in the BWAPI tree

`bwapi/include/swig.i` (167 lines) plus `bwapi/include/swig_lib/java/` is an existing,
unfinished SWIG attempt targeting Java. Useful as a triage record: its `%ignore` list
enumerates precisely the constructs that don't survive a binding (`operator=`,
`operator<<`, `operator int`, `operator bool`, arithmetic operators), and its
`position_template` / `type_template` macros confirm that `Point` and `Type` are the two
shapes needing special handling. Worth reading before writing the spec; not worth
resurrecting — SWIG generates a per-language C++ layer, which is the thing this project
exists to stop needing.

### 1.7 No exceptions to worry about

`grep -rn "throw "` across `bwapi/BWAPILIB`, `bwapi/Shared`, `bwapi/BWAPIClient`, and
`bwapi/include` returns **nothing**. `Util/Exceptions.h` types are used only by the
injected DLL's config/XML paths — which are outside the client closure (§10.1). So the
boundary needs a `catch(...)` for safety but not an error-translation layer.

### 1.8 API surface size

Approximate public declaration counts (grep-derived, including overloads and convenience
wrappers):

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

Call it **~900 declarations**. The estimate that followed — "collapsing to roughly 550–600
distinct ABI entry points" — has since been **measured, and it was low**. The six interface
headers alone declare 542 member functions; after §5.2's draw collapse, fork 3's `canXxx` rule
and two mechanical merges, and with §5.8 shipped whole, v1 is **~671 exported functions plus 848
constants in `bwapi_c2.h`, and 98 more in `bwapi_c2_bwem.h`** — about 30% above the guess. The
arithmetic is in [research/research-vs-rev4-review.md](research/research-vs-rev4-review.md) §3.2.

That number is the whole reason for §9 — but the reason is not "hand-writing 670 wrappers is a
mistake", which is arguable: `RnDome/bwapi-c` hand-wrote 530 and they work. It is that **542
declarations map to ~457 interface exports through five documented exclusion rules**, and the
rules are the thing that can be wrong. Only a coverage audit run off a machine-readable spec can
show the mapping is complete. §5.8's 848 constants and 185 accessors are the part nobody should
type at all.

---

## 2. Goals and non-goals

### Goals

1. **One flat `extern "C"` header**, valid C99 *and* C++, no C++ types, no macros required
   at the call site.
2. **Stable ABI**: append-only, versioned, symbol list under test — from 1.0 onward (§4).
3. **Zero C++ toolchain required for consumers** — a `.dll` + `.h` (+ `.lib`) is the whole
   delivery.
4. **Cheap for binding generators**: emit a machine-readable API description alongside the
   header so `bindgen`, `napi`, `ctypes`, and JNA-style wrappers can be generated, not
   hand-typed.
5. **Complete enough to write a real bot** in v1 — not a demo subset. The phase 5 exit
   criterion (§12) is a working bot in plain C.
6. **No fork of the game logic.** Everything routes through `BWAPI::Game` /
   `bwapi/Shared/`. Where the ABI's semantics deliberately differ, §15 records it.
7. **Purely additive.** The ABI lives in its own repository and *depends on* BWAPI.
   Landing it requires no change to the BWAPI tree — no fork, no vendored copy, no
   upstream patch as a precondition. Improvements it wants (§10.1) are proposed
   separately, on their own merits, and the ABI builds without them.
8. **LGPL-clean for closed-source bots** (§0): dynamic consumption only.

### Non-goals

1. **Not** a re-implementation of the shared-memory protocol per language. That's the
   JBWAPI/BWAPI4J approach; it means re-implementing `Templates.h` (3,098 lines of game
   rules) and `CommandTemp.h` (latency compensation) in every language. Explicitly
   rejected. Latency compensation is exposed and controllable (§4.1), not reproduced.
2. **Not** an idiomatic high-level API. That belongs in per-language wrappers
   (`bwapi-sys` → `bwapi` in Rust terms). The C ABI is the thin, boring, mechanical layer.
3. **Not** cross-platform. BWAPI hooks a 32-bit Windows binary. Windows-only is inherent.
4. **Not** exposing `Interface<T>::registerEvent`, `get/setClientInfo`, or the
   `Broodwar << ...` stream. Host languages have closures and hash maps; these exist to
   paper over C++ ergonomics. (Excluding the streams also removes the Boost dependency —
   §10.1.)
5. **Not** exposing `std::function` filters as first-class composable objects. See §5.4.

---

## 3. Recommended shape

> **A separate `bwapi-c` repository producing `bwapi_c.dll`, consuming BWAPI as a pinned
> submodule, wrapping `BWAPI::Game` behind `int32_t` handles, generated from a checked-in
> API spec, client mode only, LGPL-3.0 and dynamically consumed.**

Four decisions worth defending:

**Its own repository, BWAPI as a dependency.** The ABI is a strictly additive layer;
nothing about it requires editing BWAPI, so nothing about it should. That also lets it
version, release, and iterate on its own cadence, and pin a known-good BWAPI revision
rather than chasing `main` — which, for a dependency that has not moved meaningfully in
years, is close to free. §7 covers layout, §10.1 the mechanics, §10.3 the pin-bump.

**Client mode only.** No injection, no C++ vtable interop, crashes stay isolated, and it
is what Rust/JS users actually want. Module mode is deferred indefinitely (Appendix A).

**ID handles, not pointers.** §1.3. Also means the ABI is trivially usable from a language
without raw-pointer types, and handles can be logged and replayed.

**Generated, not hand-written.** §1.8 and §9.

**Naming.** ⚠️ Do **not** name it `BWAPIC` — `namespace BWAPIC` already exists in
`bwapi/include/BWAPI/Client/*.h` for the shared-memory PODs.

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
names are undecorated even in x86 builds, and **assign no ordinals**: binding is by name
only, so a reordered `.def` can never silently rebind a consumer.

**Types.** Only `int32_t`, `int64_t`, `uint32_t`, `uint8_t`, `double`, `char`, `void*`,
function pointers, and PODs declared in `bwapi_c.h`. Never C++ `bool`. No enums in
signatures — `int32_t` with `#define`d constants, so an unknown future value can't be UB
in a strict-enum language. No structs by value in or out; pass `const T*` in, `T*` out.

### Booleans: scalar vs. bulk

Two different rules, and each rests on its own reasoning.

- **Scalar** boolean parameters and return values are `int32_t`, 0 or 1.
  - *For returns*: a narrow integer return leaves the upper bits of the register
    unspecified, and FFI layers disagree about whether the callee zero-extends — `ctypes`,
    JNA, koffi and hand-written `extern` blocks all model an `int`-width return most
    reliably.
  - *For parameters*: the register argument is different — under `__cdecl` on x86,
    arguments occupy word-aligned stack slots, so a narrower type saves nothing at all.
    Uniformity is therefore free, and it means the header has exactly one integer width
    for every scalar in the ABI, since all `Type` ids are already `int32_t`.
- **Bulk** boolean grids (§5.5) and flag words (§5.10) do not follow that rule. Grids are
  `uint8_t` per element; snapshot booleans are bits in a `uint32_t`. Four bytes per tile
  would quadruple a walkability copy for nothing, and the destination is a typed array in
  the host language, not a call argument, so none of the above applies.

### Positions: one packed `int64_t` on return

A `Position` return value is a **single `int64_t`**: x in the low 32 bits, y in the high
32, two's complement.

```c
typedef int64_t bwapi_position;   /* also used for tile and walk positions */
#define BWAPI_POS_X(p)      ((int32_t)(uint32_t)((uint64_t)(p) & 0xFFFFFFFFu))
#define BWAPI_POS_Y(p)      ((int32_t)(uint32_t)((uint64_t)(p) >> 32))
#define BWAPI_POS_MAKE(x,y) ((bwapi_position)(((uint64_t)(uint32_t)(y) << 32) \
                                              | (uint32_t)(x)))
```

Out-params would cost an extra argument, a pointer write, and a host-side allocation on
every call to something bots invoke constantly. A packed `int64_t` is one register, no
pointer, and trivially decoded everywhere. The macros are a convenience, not a
requirement — the encoding is documented, so a host language decodes it however it likes,
which preserves goal 1.

**Position *parameters* stay as separate `int32_t x, y`.** Packing exists to solve
returning two values without a pointer; parameters have no such problem, and unpacked
arguments are cheaper to build and easier to read in every host language. Do not pack for
symmetry. **Positions inside structs likewise stay as separate `int32_t x, y` fields** —
packing buys nothing there and an `int64_t` member complicates the layout story in §10.2.

Packing is lossless, so the sentinels survive unchanged: emit `Positions::Invalid`,
`None`, `Unknown`, `Origin` and their tile and walk equivalents in **both** unpacked
(`BWAPI_POSITION_NONE_X` / `_Y`) and packed (`BWAPI_POSITION_NONE`) forms. Do not invent
a new invalid bit pattern. **The neutral return for a position-returning function given a
bad handle is packed `Positions::None`** — one rule, matching the invalid-handle policy
below.

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

**When `cap < total`, the returned elements are the first `cap` in ID order.** So a retry
with a larger buffer is coherent with the truncated result, and a caller can page. Leaving
this unspecified would make truncation a silent random sample.

**Order.** `Unitset` is a `SetContainer` over `std::unordered_set` hashing **pointers**
(`Unitset.h`: `SetContainer<BWAPI::Unit, std::hash<void*>>`). ASLR moves those pointers,
so iteration order varies *run to run* — this is genuine nondeterminism, not merely an
unspecified order. A bot that iterates and takes the first match would be irreproducible
through no fault of its own. **The ABI sorts collection output ascending by ID** and
documents it (§15). Sorting alone does not fix the closest/best queries, which return one
unit — see §5.4.

**Struct evolution: every POD that crosses the boundary begins with `int32_t size;`.**
The append-only policy covers function signatures and says nothing about struct layout,
but `bwapi_event`, `bwapi_bullet`, `bwapi_unit_command` and the §5.10 snapshots are layout
contracts compiled into every consumer — and koffi and ctypes consumers hardcode those
layouts in script and never recompile.

- The caller sets `size` on input structs. The callee validates it, reads only the prefix
  it understands, and ignores the rest.
- The callee sets `size` on output structs, writes the fields it knows, zero-fills the
  remainder up to the caller's `size`, and never writes past it.
- **For array-out functions the caller sets `size` on element zero, and that value is the
  uniform stride for the whole array.** No separate `elem_size` parameter — one mechanism,
  no redundancy.

Applied uniformly, including to `bwapi_unit_command`. The per-call cost is setting one
field on a struct most bots never touch (§5.3's ~40 convenience functions are the real
command path), and uniformity is worth more than four bytes.

**Invalid handles.** Never dereference. Validate, then return a documented neutral value
(`0` / `-1` / packed `Positions::None` / empty) and latch it in the ABI error channel.
Rationale: a wrapper bug in a foreign language must not crash StarCraft mid-tournament.

### Two error channels, with different lifetimes

```c
int32_t bwapi_last_error(void);                            /* does not clear */
int32_t bwapi_last_error_message(char* buf, int32_t len);  /* snprintf convention */
void    bwapi_clear_last_error(void);
```

**`bwapi_last_error()` is sticky and latches the *first* error. It is never cleared
implicitly.** A failing call sets the code only if the current code is `BWAPI_ERR_NONE`.
Successful calls do not touch it. Reading does not clear it.

Two reasons, and the first is the one that matters. **Clear-on-success would force a second
FFI crossing after every call** — a bot reading 3,000 values a frame would double its
crossing count just to find out whether any of them was a lie, which in Node and Python is
the difference between comfortable and unusable. Sticky lets the host clear once at the top
of the frame and check once at the bottom (§4.1). Second, **the first error is the causal
one**; a last-write-wins channel reports whichever downstream call happened to fail next,
which is the less useful of the two.

`bwapi_game_get_last_error()` remains an unchanged pass-through of `Game::getLastError()`,
with BWAPI's own per-call semantics. **The header states plainly that the two channels have
different lifetimes** — that is the thing a binding author will otherwise assume wrong.

**Process-wide singleton.** `BroodwarPtr` is a global; there is no context handle and there
will not be one. `bwapi_client_connect()` when already connected latches
`BWAPI_ERR_ALREADY_CONNECTED` and returns 0.

**Diagnostics.** `Client::connect()` writes directly to `std::cout`/`std::cerr`
(`bwapi/BWAPIClient/Source/Client.cpp`), which is useless-to-harmful for an embedded
consumer. Add
`bwapi_set_log_callback(void(*)(int32_t level, const char* msg, void* user), void* user)`
and route wrapper diagnostics there. Leave the underlying C++ prints alone in v1 (changing
them is an upstream behaviour change), but document them.

**Threading.** BWAPI is single-threaded and frame-synchronous. All calls happen on the
thread that calls `bwapi_client_update()` — see §4.1, which is normative.

**Versioning and the stability timeline.**
```c
uint32_t bwapi_abi_version(void);      /* semver of this ABI */
int32_t  bwapi_client_version(void);   /* BWAPI::CLIENT_VERSION, 10003 today */
int32_t  bwapi_revision(void);         /* SVN_REV from upstream's generator, §10.3 */
int32_t  bwapi_is_debug(void);
```

**The ABI is `0.x` and explicitly unstable until the consumers phase completes.** Real
bindings always shake out ergonomics, and promising append-only stability while also
planning to revise on consumer feedback cannot both hold — either the promise breaks or
the mistakes freeze. **Append-only begins when `bwapi_abi_version()` returns 1.0**, and
reaching 1.0 is the exit criterion of phase 6 (§12). After that: new functions get new
names; existing signatures never change meaning; a removal is a major bump.

---

## 4.1 The frame loop

The ABI's only stateful protocol is connect → update → poll → repeat. It is normative:
without it written down, every binding author reconstructs it independently and at least
one gets the reconnect path wrong.

```c
while (!bwapi_client_connect()) { sleep(1000); }
for (;;) {
    while (!bwapi_game_is_in_game()) {
        bwapi_client_update();
        if (!bwapi_client_is_connected()) goto reconnect;
    }
    while (bwapi_game_is_in_game()) {
        bwapi_clear_last_error();
        /* poll events, take snapshots, issue commands */
        bwapi_client_update();                    /* blocks on the pipe */
        if (!bwapi_client_is_connected()) break;
    }
}
```

Alongside it, state:

- `bwapi_client_update()` **blocks** on `ReadFile` against the pipe. A Node consumer must
  run it on a worker thread or it will stall the event loop.
- Every other call must happen on that same thread.
- Event indices (§5.6) and snapshot contents (§5.10) are valid until the next `update()`.
- A handle from frame *N* may resolve to a dead unit at frame *N+1* — the existing C++
  semantics, unchanged.
- Clear the ABI error at frame start; check it before `update()`.

**Latency compensation belongs here, not in a footnote.**

```c
void    bwapi_game_set_lat_com(int32_t enabled);
int32_t bwapi_game_is_lat_com_enabled(void);
```

Latcom changes what getters return *within* a frame: `CommandTemp::execute()` writes
predicted state straight into the local `UnitData` when a command is issued. That is a
first-order fact about what the read surface means, and it is the mechanism behind §5.4's
argument that issuing a command mutates the data an enclosing query is reading.

---

## 5. What is mechanical vs. what needs design

Roughly **85% of the surface is mechanical** — a getter returning `int`, `double`, `bool`,
a `Type` (→ `int32_t`), a `Position` (→ packed `int64_t`), or a handle. That includes the
entire `canXxx` family (~130 on `Unit` alone), all of `UnitType`/`WeaponType`/`TechType`/
`UpgradeType` static data, every `Player` accessor, every `Unit` state query, and every
command method. These want a generator, not a human.

The other 15% is the interesting part.

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
better formatting. Document the 256-byte truncation (§15).

### 5.2 The `drawX{Map,Mouse,Screen}` triples

`Game.h` has ~90 draw declarations, but only 8 primitives (`text`, `box`, `triangle`,
`circle`, `ellipse`, `dot`, `line`) × 3 coordinate spaces × `int`/`Position` overloads. The
virtual ones already take `CoordinateType::Enum`. Emit **one function per primitive** with
an explicit `ctype` parameter: 90 → 8.

### 5.3 `UnitCommand`

`BWAPI::UnitCommand` holds raw `Unit` pointers. `BWAPIC::UnitCommand`
(`bwapi/include/BWAPI/Client/UnitCommand.h`) is already the ID-based mirror used over the
wire. Mirror *that*, with the §4 size prefix:
```c
typedef struct bwapi_unit_command {
  int32_t size;
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

### 5.4 Filters, predicates, and re-entrancy

`UnitFilter` is `UnaryFilter<Unit>` over `std::function<bool(Unit)>`; `Filter::IsWorker`
et al. are composable objects with operator overloads. **None of that crosses C.**

1. **No filters in the exposed queries.** Every query has an unfiltered form; the host
   language filters the returned ID array — or, better, the §5.10 snapshot it already has.
2. **Keep the spatial index.** `getUnitsInRectangle` uses `Templates::iterateUnitFinder`
   over the shared-memory `xUnitSearch`/`yUnitSearch` arrays. That optimisation must not be
   lost — expose the rectangle and radius queries natively and let filtering happen after.
3. **Closest-unit queries are computed at the ABI boundary** over the sorted candidate set,
   tie-breaking on lowest ID. See "Determinism" below.
4. **Callback predicates are a conditional follow-on**, not a planned phase (§12 phase 7).

An enum-based mini-filter DSL (`BWAPI_FILTER_IS_WORKER`) is tempting and should be
**declined**: it re-encodes upstream semantics in a switch statement that silently rots.

#### Determinism, and why sorting is not enough

Sorting collection output (§4) fixes the set-returning queries. It cannot reach the
**closest/best** queries, which return one unit and break ties by iteration order *inside*
`Templates::iterateUnitFinder` — so the exact calls a bot uses to pick a target would stay
nondeterministic while everything around them was fixed. That is the worst of both worlds.

**Implement the closest queries at the ABI boundary.** Keep the native rectangle and radius
queries so the `unitFinder` spatial index is preserved, then compute the minimum ABI-side
over the sorted candidate set, tie-breaking on lowest ID. Distance comparison is not game
rules, so this does not violate goal 6 — and there is no cleverness being discarded:
`Game::getClosestUnit` is literally `getClosestUnitInRectangle` over a bounding box
(`bwapi/BWAPILIB/Source/Game.cpp:717-725`), with no expanding-ring search. One extra pass
over a small candidate list buys reproducibility, which in a competitive-AI framework is
worth more. Recorded in §15.

**`getBestUnit` is not exposed at all.** It is meaningless without a `BestFilter`, and the
callback mechanism that would supply one is a conditional follow-on. This is a removal,
not a deferral.

#### What a predicate may call, if callbacks ever land

The rule is derived, not asserted. All three filtered queries —
`GameImpl::getUnitsInRectangle`, `getClosestUnitInRectangle`, `getBestUnit`
(`bwapi/BWAPIClient/Source/GameImpl.cpp:475-543`) — hold only **function-local** state, and
`Templates::iterateUnitFinder` (`bwapi/Shared/Templates.h:69-152`) keeps its scratch in a
**local** `std::unordered_map` while iterating shared-memory arrays the server rewrites only
during `update()`. No global or static scratch, no `GameImpl` member container mutated. So
re-entering the ABI is not inherently unsafe; three specific categories are.

The spec flag is **`reentrant: forbidden`**, not `mutates:` — because what is being gated is
not "mutates game state". Drawing does not mutate game state; it appends to
`data->shapes[]`. `bwapi_client_disconnect` mutates nothing; it frees `BroodwarPtr`. The
three forbidden categories are:

| Category | Members | Why |
|---|---|---|
| **Shared-memory writes** | drawing, `printf`, `send_text`, `enable_flag`, `set_local_speed` | Append to `data->shapes[]` / `commands[]` / `unitCommands[]`, bounded only by `assert` — no runtime check in release |
| **Command-queue writes** | every unit command | `UnitImpl::issueCommand` runs `Command{cmd}.execute()` (`BWAPIClient/Source/UnitImpl.cpp:59`), and `CommandTemp::execute()` writes into `unit->self->order`, `->target`, `->isConstructing` … (`include/BWAPI/Client/CommandTemp.h:196-214`) — the very `UnitData` the enclosing query is filtering on. The answer would depend on iteration order |
| **Lifecycle** | `connect`, `update`, `disconnect` | `Client::disconnect()` deletes `BroodwarPtr` while the in-flight query still holds `this`. Use-after-free |

Everything else — every read-only accessor, static type data, scalar tile queries — is
**allowed**. Nested queries are allowed but discouraged (memory-safe, each builds fresh
local state, but O(n) inside O(n)).

Two refinements over revision 2:

- **Save and restore `lastError` around every predicate invocation.** Revision 2 filed this
  as "allowed, with a caveat", but most of BWAPI's read-only surface calls `setLastError` —
  every `canXxx` does — so a predicate clobbering the enclosing query's error is the normal
  case, not a corner. Three lines in the dispatcher makes the caveat disappear.
- **Fire the log callback at warn level on every rejected re-entrant call**, in addition to
  latching the ABI error. A silently no-op'd attack command in a language where nobody
  checks error codes is a miserable afternoon.

The guard itself is a thread-local depth counter; mutating wrappers check it and fail with
`BWAPI_ERR_REENTRANT_MUTATION` and no side effect. **Carry the `reentrant` flag in the spec
from phase 1 regardless of whether callbacks land** — it is free metadata, it feeds
`api.json` and the generated docs, and retrofitting a per-entry flag across 600 entries
later is not free. Emit the guard only if callbacks land.

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

- **Row-major, index `y * width + x`.**
- **Cropped to the live map**, not the full backing array: `mapWidth()*4 × mapHeight()*4`
  for walk tiles, `mapWidth() × mapHeight()` for tile grids. On a 128×128 map that is 16×
  less data than copying the whole 1024×1024. `out_w`/`out_h` report the cropped extent.
- Boolean grids are `uint8_t` per element (§4); `getGroundHeight` stays `int32_t` per
  element, matching its source array.

A borrowed-pointer variant is possible (the data lives in mapped shared memory), but a
**copy** is the right default: a borrowed pointer's validity window across `update()` is a
subtle contract to get wrong, and it would hand a foreign language a writable view of the
server's own memory. Revisit only with a measured need (§12 phase 7).

### 5.6 Events

`Broodwar->getEvents()` returns a **`std::list<Event>`**, and `BWAPI::Event` holds a heap
`std::string*`. Indexing a `std::list` directly would be O(n²) over a frame's events, so
**the event list is snapshotted into a vector during `bwapi_client_update()`, and indices
are stable until the next one.**

```c
typedef struct bwapi_event {
  int32_t size;
  int32_t type;                    /* EventType::Enum */
  int32_t unit_id, player_id;
  int32_t x, y;                    /* NukeDetect target */
  int32_t is_winner;
} bwapi_event;

int32_t bwapi_game_event_count(void);
int32_t bwapi_game_get_event(int32_t index, bwapi_event* out);
int32_t bwapi_game_event_text(int32_t index, char* buf, int32_t buf_len);
```

No `text_len` field — `bwapi_game_event_text()` already returns the needed length under the
§4 string convention, and a second source for the same number is a second thing to get out
of sync.

Polling rather than callbacks: it is the natural client-mode shape, avoids re-entrancy
entirely, and every host language can build its own callback dispatch on top.

### 5.7 Unit-set broadcasts

`Unitset` has 48 broadcast methods (`unitset.attack(pos)` → all units). Rather than
reproducing them, expose the ID-array form for the ~10 that matter:
`bwapi_units_attack_position(const int32_t* ids, int32_t n, int32_t x, int32_t y, int32_t queued)`.
The rest are host-language `for` loops.

### 5.8 Static type data

`UnitType::maxHitPoints()` and friends (~86 on `UnitType`, ~27 on `WeaponType`, plus
Tech/Upgrade/Race/UnitSize) are **pure functions of an `int`** — no game instance needed.
These are the highest-value, lowest-risk part of the whole ABI: a host language can build
its complete static type table at startup with zero game connection, and they're testable
without StarCraft.

The few that return containers need out-buffers: `requiredUnits()` → `std::map<UnitType,int>`
→ parallel `(type[], count[])` arrays; `whatBuilds()` → `std::pair` → two out-params;
`abilities()`/`upgrades()`/`buildsWhat()` → ID arrays.

**Shipped three ways, not one** (fork 1). The 185 accessors ship as functions, because that is
what the 1,671 measured call sites are written against and it is the only form that carries
upstream's semantics rather than a snapshot of them. The constants ship generated into the
header. And **one size-prefixed bulk table per type class ships as an optional §5.10-style fast
path**, so a host that would rather pay one crossing at startup than 185 can have it. What is
*declined* is shipping only the table: that is what rsbwapi does because it has no ABI to ask,
and it forces every host to re-derive accessor semantics the ABI already knows.

Also generate the full **constant set** into `bwapi_c_types.h` — every `UnitTypes::Enum`,
`Orders::Enum`, `TechTypes::Enum`, `UpgradeTypes::Enum`, `WeaponTypes::Enum`, `Races::Enum`,
`Errors::Enum`, `EventType::Enum`, `Flag::Enum`, `CoordinateType::Enum`,
`Text::{Enum,Size::Enum}`, `MouseButton`, `Key`, `Latency::Enum`, `Colors`, and the position
sentinels in both forms. **848 constants** (measured in R1), pure codegen, and it's what makes
the ABI pleasant
rather than magic-number soup. Names come from the enum identifiers, so they can't drift.

### 5.9 Explicitly not exposed

| Not exposed | Why |
|---|---|
| `Interface<T>::registerEvent` | Host closures do this better |
| `get/setClientInfo` | Host hash maps do this better. Also the one place in the client closure with pointer↔`int` casts (§10.1) |
| `GameWrapper` / `Broodwar << …` | C++ streams — and excluding them removes the Boost dependency (§10.1) |
| `getBestUnit` | Meaningless without a `BestFilter`; the callback mechanism is a conditional follow-on. A removal, not a deferral (§5.4) |
| `Type::getType(string)` name lookup | Conditional follow-on, §12 phase 7 |
| `TournamentModule` | Module-mode-only, and module mode is deferred indefinitely (Appendix A) |

### 5.10 Per-frame snapshots

§5.5 refuses per-cell FFI for map data — and then per-unit getters push the call count back
up, for exactly the languages that pay most per crossing. Apply the same fix where it
matters most.

```c
int32_t bwapi_game_snapshot_units(bwapi_unit_snapshot* out, int32_t cap);
int32_t bwapi_game_snapshot_players(bwapi_player_snapshot* out, int32_t cap);
```

Same convention as every other collection (§4): fills up to `cap`, returns the total,
sorted ascending by ID, **existing units only**, `cap == 0` with `NULL` is the size query,
and element zero's `size` is the uniform stride. `UnitData` is already pointer-free, so
this is a field-select copy loop, not new logic. (`last_command_frame` is the one field
that comes from the interface rather than `UnitData` — it is a client-side `UnitImpl`
member.)

**Booleans in the snapshot are bits in a `uint32_t flags`, not fields**: `exists`,
`is_completed`, `is_constructing`, `is_idle`, `is_moving`, `is_attacking`, `is_cloaked`,
`is_burrowed`, `is_stuck`, `is_under_attack`, `is_morphing`, `is_selected`, `is_powered`,
`is_visible_to_self`. This is not the §4 scalar-bool rule — that governs parameters and
returns — and a bit is also how a future boolean gets added without disturbing the layout,
which pairs with the size prefix.

v1 unit fields: `size`, `id`, `player_id`, `type`, `x`, `y`, `hit_points`, `shields`,
`energy`, `resources`, `resource_group`, `order`, `order_target_id`, `secondary_order`,
`target_id`, `build_type`, `remaining_build_time`, `remaining_train_time`,
`training_queue_count`, `addon_id`, `transport_id`, `carrier_id`, `hatchery_id`,
`ground_weapon_cooldown`, `air_weapon_cooldown`, `spell_cooldown`, `last_command_frame`,
`flags`, then `double angle, velocity_x, velocity_y`.

Player snapshot stays scalar-only: `id`, `race`, `type`, `color`, `start_x`, `start_y`,
`minerals`, `gas`, `gathered_minerals`, `gathered_gas`, `supply_used[3]`,
`supply_total[3]`, `flags`. **Upgrade and tech levels stay as scalar calls** — 63 upgrades
× 12 players copied every frame, to serve values that change a handful of times per game,
is the wrong trade.

The snapshots are **additive**. Per-unit getters remain, because the `canXxx` family and
the derived queries have no snapshot form.

The consequence for §5.4 is the important one: once the host has the frame's units as a
typed array, "filter inside the query" stops being interesting except for genuinely
early-exiting spatial searches — and §5.4 computes those at the boundary. Between the two,
the remaining case for running foreign code inside a BWAPI query is empty.

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

### 6.2 Validity, and legitimate `BWAPI_NONE`

`bwapi_unit_exists(id)` maps to `UnitInterface::exists()`. Handles need no explicit
release: they are indices into game-owned storage, nothing is retained, nothing leaks.

**The header enumerates the functions that legitimately return `BWAPI_NONE`** — among them
`bwapi_game_self` and `bwapi_game_enemy` in a replay or an observer slot,
`bwapi_unit_get_target` when idle, `bwapi_unit_get_addon` on a building without one,
`bwapi_unit_get_transport` when not loaded, `bwapi_unit_get_build_unit` when not building.
Without that list, `BWAPI_NONE` is indistinguishable from a rejected handle unless the
caller reads the error channel after every single call — which is precisely the second
crossing §4 designed away.

### 6.3 Bullets

No ID-keyed lookup exists. Snapshot instead, **existing bullets only, sorted by ID**:
```c
typedef struct bwapi_bullet {
  int32_t size;
  int32_t id, player_id, type, source_id, target_id;
  int32_t x, y, target_x, target_y, remove_timer;
  double  angle, velocity_x, velocity_y;
} bwapi_bullet;

int32_t bwapi_game_get_bullets(bwapi_bullet* out, int32_t cap);
```
There is no `exists` field: filtering on the way out makes it dead weight. Max 100
(`GameData::bullets[100]`), fully transient, no identity needed across frames.

---

## 7. Repository and layout

The C ABI lives in its own repository, `bwapi-c`, and consumes BWAPI as a pinned submodule.
It is a purely additive layer: nothing here requires a change to the BWAPI tree, so nothing
here belongs in it. Practically, it also pins a known-good BWAPI revision instead of
tracking `main`, and releases on its own cadence.

```
bwapi-c/                            # separate repository, LGPL-3.0
  COPYING  COPYING.LESSER           # §0
  third_party/bwapi/                # git submodule, pinned revision
  include/
    bwapi_c.h            # generated — the single public header
    bwapi_c_types.h      # generated — 848 constants + PODs
  src/
    abi.cpp              # version, sticky error channel, log callback,
                         #   the in-predicate depth counter (§5.4)
    client.cpp           # connect / update / disconnect / is_connected
    handles.cpp          # resolve+validate helpers
    game.gen.cpp         # generated
    unit.gen.cpp         # generated
    player.gen.cpp       # generated
    force_region.gen.cpp # generated
    types.gen.cpp        # generated — static type data
    bulk.cpp             # hand-written — map grids, collections, events, snapshots
    commands.cpp         # hand-written — UnitCommand, unit-set broadcasts
    closest.cpp          # hand-written — boundary-side closest queries (§5.4)
  vendor/svnrev.h        # generated by upstream's script at pin-bump time (§10.3)
  tools/abi/
    spec/{game,unit,player,types,...}.yaml   # the source of truth
    emit_header.py  emit_source.py  emit_json.py  emit_def.py
    check_coverage.py                        # libclang audit, off the merge path (§9)
  api.json               # generated — the downstream binding contract
  api.schema.json        # checked-in JSON Schema for api.json
  bwapi_c.def            # generated, checked in — the golden symbol artifact
  tests/
    header_hygiene/      # C99 + C++ compile checks
    layout_dump/         # emits GameData field offsets as JSON (§10.2)
    types_test.cpp       # ~500 static-type assertions, no game needed
    mock_server/         # fake BWAPI server + scenario fixtures (§11)
    fuzz/                # boundary fuzz harness generated from the .def (§11)
  bindings/
    rust/bwapi-sys/      # raw FFI only
    node/                # raw FFI only
  examples/{c-example-bot,rust-example-bot,node-example-bot}/
  CMakeLists.txt
```

### What lives here, and what doesn't

`bindings/` holds the **raw FFI layer only** — `bwapi-sys` in Rust terms: `extern`
declarations, constants, build glue, nothing more. Keeping it here means it is generated
from the same `api.json`, versioned with the ABI, and regression-tested in one CI run
against the mock server.

The **idiomatic, safe, host-language wrapper** for each language lives in its own
repository, released on that ecosystem's cadence: a `bwapi` crate from a Rust repo, an npm
package from a JS repo. Those are where `Unit` newtypes, `Result`, iterators and async
integration belong, and they should not be gated on this repo's release process.

**Rust and JavaScript are the only two bindings in scope** for in-repo `bindings/`. Others
are welcome downstream — `api.json` exists precisely so they need nothing from here.

`*.gen.cpp`, `api.json`, `bwapi_c.def` and `vendor/svnrev.h` are **checked in**, not built
by a codegen step at compile time. Contributors without Python (or Windows Script Host)
still get a buildable tree, diffs are reviewable, and CI verifies that regenerating
produces no change.

---

## 8. Module mode

**A v2 item, out of scope for v1.** R4 supplied the concrete reason it should exist — grouped
commands are unavailable to *any* client-mode bot, in any language — without changing the reason
it waits. See **Appendix A**.

---

## 9. Codegen: spec-driven, with a coverage audit

**Rejected: parse the headers and emit directly (libclang/SWIG).** The headers use MSVC
extensions, `#pragma warning`, and heavy templates; a parser-driven generator is brittle
and, worse, would silently change the public ABI when an upstream header is edited. ABI
stability has to be a deliberate act.

**Rejected: hand-write 600 wrappers.** Guaranteed inconsistency in exactly the details —
buffer semantics, invalid-handle behaviour, error latching — that matter most.

**Recommended: a checked-in YAML spec + a small Python emitter + a libclang coverage
audit.**

```yaml
- cpp: "UnitInterface::getHitPoints"
  c:   "bwapi_unit_get_hit_points"
  self: unit
  returns: int32

- cpp: "UnitInterface::attack(BWAPI::Position, bool)"
  c:   "bwapi_unit_attack_position"
  self: unit
  reentrant: forbidden          # command-queue write (§5.4)
  params: [{name: x, type: int32}, {name: y, type: int32},
           {name: queued, type: bool32}]
  returns: bool32
  body: "return self->attack(BWAPI::Position(x, y), queued != 0);"
  divergence: none              # §15

- cpp: "UnitInterface::registerEvent"
  skip: "host-language closures supersede this"
```

A handful of return kinds covers nearly everything: `int32`, `bool32`, `double`, `type`
(→`int32`), `position` (→ packed `int64`), `handle`, `string_out`, `id_array`, `void`.
Anything unusual carries an inline `body:`.

Emitters produce `bwapi_c.h`, `bwapi_c_types.h`, the `*.gen.cpp` files, `bwapi_c.def`, and
**`api.json`** — the machine-readable description that downstream Rust/TS/Python generators
consume, so binding authors never re-parse C.

Five rules that make the spec trustworthy:

**`cpp:` strings must resolve to exactly one declaration.**
`"UnitInterface::attack(Position, bool)"` is overload resolution by string match; assert
uniqueness in the generator or expect to bind the wrong overload eventually.

**`body:` entries still declare full types, and CI emits a `static_assert` that the
referenced C++ overload exists with the declared signature.** Otherwise an opaque C++ body
is a hole in both the coverage audit and `api.json`.

**Store a normalized declaration hash per spec entry.** Coverage-by-name catches additions
and removals and misses *changes*: a changed return type or a new defaulted parameter still
matches a spec entry by name and silently rebinds. A hash is ten lines on top of a parser
that already exists, and it catches the only failure mode that is silent.

**The `.def` file is the golden symbol artifact.** Generate it, check it in, and have CI
assert `dumpbin /exports` matches. Revision 2 had a separate `bwapi_c.symbols` expressing
the same fact; one file, one truth. Assign no ordinals; binding is by name only.

**`api.json` has a checked-in JSON Schema and a `schema_version` field, and the Rust
generator consumes it in CI.** It is the contract every downstream binding depends on,
which makes it an ABI in its own right — it deserves the same versioning discipline as the
header, and a consumer that would notice if it broke.

### The coverage audit runs off the merge path

`check_coverage.py` parses the BWAPI headers with libclang and reports any public
declaration with neither a spec entry nor an explicit `skip:`. Its real value is a
**one-time completeness audit** — proving the spec covers all ~900 declarations rather than
700 — plus a re-run at pin bumps.

It is **not** a per-PR gate. Parsing `v141_xp` headers and 3,098 lines of `Templates.h`
needs `-fms-extensions -fms-compatibility -fdelayed-template-parsing` and a pinned LLVM;
that is a fragile gate to put in front of every merge, and it is guarding against upstream
motion in a dependency that has not moved meaningfully in years. Run it in the §10.3
checklist and on demand.

---

## 10. Build and packaging

### 10.1 Deriving the link closure

**CMake is the only build system.** No `.vcxproj` in `bwapi.sln`; CMake is what Rust
`build.rs`, `node-gyp` and CI want anyway. BWAPI comes in as a pinned submodule at
`third_party/bwapi`.

`CMake/README.md` documents the supported consumption path — `ADD_SUBDIRECTORY` of
`CMake/BWAPI/` and `CMake/Client/`, then link `BWAPI-Static` and `BWAPIClient`. That path
is wrong for us in both directions: `CMake/Client` alone is too little (the type tables and
non-virtual convenience methods live in `BWAPILIB/Source/*.cpp`, which the CMake tree
compiles only into `BWAPI-Static`), and `BWAPI-Static` is far too much (it drags in
`BW/*`, `Detours.cpp`, `CodePatch.cpp`, Storm, and the whole injected-DLL source set).

**So derive the closure; do not declare it.** Build the candidate set as x86 and let the
linker enumerate what is undefined. Inspection of the includes gives the starting point,
and it is smaller than either existing target:

| | Verdict |
|---|---|
| `BWAPILIB/Source/*.cpp`, `BWAPILIB/UnitCommand.cpp` | **In** — type tables, non-virtual `Game`/`Unit` helpers |
| `BWAPIClient/Source/*.cpp` | **In** — `Client`, client `GameImpl`/`UnitImpl`/… |
| `Shared/*.cpp` | **In** — `Templates.h` and the `*Shared.cpp` implementations |
| `BWAPILIB/Source/Streams.cpp`, `BroodwarOutputDevice.cpp` | **Out** — the only two TUs in the closure that reach Boost. `Streams.cpp` includes `boost/iostreams/{stream,tee}.hpp` directly, and both include `BWAPI/BroodwarOutputDevice.h`, which pulls `boost/circular_buffer.hpp` and `boost/iostreams/concepts.hpp`. Together they implement `bwout`/`bwerr`, which §5.9 already excludes. Dropping both **removes the Boost dependency entirely**. Safe because the public `Streams.h` includes only `<ostream>`, so `BWAPI.h` consumers never pull Boost, and nothing else in the closure references `bwout`/`bwerr` |
| `Util/Source/**` | **Out** — nothing in `BWAPIClient/Source`, `Shared`, or `include/BWAPI/Client` includes anything from `Util/`. `RemoteProcess`, `MemoryFrame`, `SharedMemory` and `Exceptions` are injected-DLL infrastructure |
| `Storm/**` | **Out** — nothing in the client path references a Storm symbol. `storm.cpp` is 223 lines of no-op stubs (`{ return TRUE; }`) that exist so the *injected* DLL links without the real `storm.dll` import library. It is also the only project setting `/Zp` (`1Byte`), so excluding it protects §1.4 |
| generated `include/svnrev.h` | **In** — `BWAPILIB/Source/BWAPI.cpp` includes it, and it pulls `starcraftver.h`, which defines `BUILD_DEBUG` (§10.3) |

**Storm's exclusion is the finding that matters**, because Storm is the game's own 32-bit
component and, had the client path genuinely needed it through an import library, **x64
would have been dead before a single `static_assert` was written**. It does not, so x64
stays open. That is the question revision 2 never asked, and it is answered by grep and
confirmed by the linker — not by layout arithmetic.

**Use explicit source file lists, never globs.** A private build of upstream's sources is
untested by upstream. An explicit list turns a new file in `BWAPILIB/Source/` into a loud
link error instead of a silent change in what gets compiled.

**Two known x64 hazards, both in `clientInfo`, both avoidable.**
`bwapi/include/BWAPI/Interface.h:72` is `return (CT)(int)this->getClientInfo(key);` — a
`void*`→`int` truncation — and `:94` is the inverse. Both are template members, so they are
harmless unless instantiated, and §5.9 excludes `clientInfo` from the ABI. The non-template
sibling `bwapi/BWAPILIB/Source/Unitset.cpp:82` (`setClientInfo((void*)clientInfo, index)`,
from an `int`) is a C4312 warning rather than an error. Phase 0 confirms none of the
templates instantiate; if `Unitset.cpp` proves troublesome it can be excluded and its two
`setClientInfo` overloads dropped, since they are excluded from the ABI anyway.

Two settings to carry over: `ADD_DEFINITIONS(/DNOMINMAX=1)`, and `BWAPI_CUSTOM_COMPILE_FLAGS`
as the documented hook for matching compile flags. Statically link the CRT (`/MT`) —
safe because no allocation crosses the boundary (§4), and permitted under LGPL's System
Libraries exception (§0).

**Two upstream bugs found while deriving this**, both worth reporting and neither a
precondition: `CMake/BWAPI/CMakeLists.txt:258` references `${BWAPI_ROOT}/BWAPILib/UnitCommand.cpp`
while the directory is `BWAPILIB` (case-insensitive on Windows, broken on a case-sensitive
filesystem); and the same file declares its `svnrev.h` custom-command `OUTPUT` as
`${BWAPI_ROOT}/svnrev.h` while `revisionUpdate.vbs` actually writes `include/svnrev.h`.

**If upstream declines the proposed client-only CMake target, nothing changes**: the
private target in `bwapi-c` stands indefinitely, and the §10.3 checklist absorbs the
maintenance of keeping its file list current. Goal 7 turns on that being true, so it is
stated rather than assumed.

### 10.2 The x64 question, answered in two stages

**Stage 1, phase 0: does an x64 build link, and do the layouts agree?**

Replace revision 2's hand-written `static_assert(offsetof(...))` suite — several hundred
transcribed constants, which would be wrong at least once — with a **generated layout
dump**. One translation unit emits every field's name, offset and size as JSON for
`GameData`, `UnitData`, `PlayerData`, `ForceData`, `RegionData`, `BulletData` and the
`BWAPIC::*` structs. Compile it x86 and x64; CI diffs the two dumps against a checked-in
x86 baseline. Same coverage, a tenth of the code, and it doubles as the regression detector
for upstream `GameData` edits.

Also confirm by grep that no project in the closure sets `/Zp` or `#pragma pack` (§1.4).

**Stage 2, the mock-server phase: does an x64 client actually talk to an x86 server?**

Static asserts prove the x64 compiler agrees with itself about a struct it compiled. They
do not prove interop. The real test is an x64 client completing a frame handshake against
an x86 server through the shared mapping and the pipe — which is exactly what the mock
server (§11) provides.

**So the x64 verdict is provisional at phase 0 and final at phase 3** (§12), and the plan
says so rather than declaring victory on the arithmetic.

### 10.3 `svnrev.h` and the pin-bump procedure

**Generate `svnrev.h` with upstream's own `cscript.exe revisionUpdate.vbs` when the pin
moves, and check the result in.** The script computes `2383 + git rev-list HEAD --count`
and writes `include/svnrev.h`, which also `#include`s `starcraftver.h` (the definer of
`BUILD_DEBUG`). A synthesised header would make `bwapi_revision()` return a number that is
not BWAPI's revision — worse than not exporting it at all. Running upstream's script once
per pin bump makes it exactly what a BWAPI-built binary would report, and checking it in
keeps Windows Script Host out of the normal build.

**There is no scheduled drift canary.** A nightly job against a dependency that does not
move is noise. Moving the pin is a deliberate act, so the work attaches to that act:

1. Move the `third_party/bwapi` submodule.
2. Run `cscript.exe revisionUpdate.vbs`; commit the generated `svnrev.h`.
3. Regenerate the x86 and x64 layout dumps; diff against the baselines.
4. Run `check_coverage.py`; resolve every added, removed, or changed declaration — the
   declaration hashes (§9) catch the changed ones.
5. Rebuild; run the type tests, the mock-server suite, and the boundary fuzz.
6. Record the new BWAPI revision and `BWAPI::CLIENT_VERSION` in the release notes.

### 10.4 Distribution

`bwapi-c` ships its own artifacts; it adds nothing to BWAPI's `Release_Binary/` or
installer.

- A per-platform release asset — `bwapi-c-<version>-win32.zip` and, if §10.2 permits,
  `-win64.zip` — containing `.dll`, `.lib`, `.def`, headers, `api.json`, `api.schema.json`,
  `COPYING`, `COPYING.LESSER`, and a link to the source at the exact tagged commit (§0).
- Each release records the **pinned BWAPI revision** and `BWAPI::CLIENT_VERSION` (10003
  today), so a consumer can tell which server versions a given `bwapi_c.dll` speaks to. The
  server already version-checks at connect time (`BWAPIClient/Source/Client.cpp`).
- `bindings/rust/bwapi-sys` published to crates.io from this repo; the safe `bwapi` crate
  and the idiomatic npm package publish from their own repos (§7). All carry the LGPL
  notices.

---

## 11. Testing

Ordered by value per unit of effort.

1. **Header hygiene.** Compile `bwapi_c.h` standalone as C99 (`/TC`), as C++, and twice in
   one TU (include-guard check). Catches C++-isms leaking into the public header — the most
   common failure in hand-maintained C ABIs.
2. **Golden `.def` diff.** CI asserts `dumpbin /exports` matches the checked-in
   `bwapi_c.def` (§9). This is the mechanism that makes the stability promise in §4 real
   rather than aspirational, once 1.0 lands.
3. **Layout dumps.** x86 and x64, diffed against the checked-in x86 baseline (§10.2).
4. **Static-type-data tests, no game required.** ~500 assertions
   (`bwapi_unittype_mineral_price(BWAPI_UNIT_TERRAN_MARINE) == 50`) validating the largest
   generated block with zero infrastructure. `bwapi/BWAPILIBTest` is a good model for the
   style; the tests live in `bwapi-c/tests/`.
5. **Mock server — the high-value one.** Client mode needs only a process that creates
   `Local\bwapi_shared_memory_game_list`, a `Local\bwapi_shared_memory_<pid>` mapping and
   `\\.\pipe\bwapi_pipe_<pid>`, then drives the frame handshake
   (`bwapi/BWAPI/Source/BWAPI/Server.cpp:40,99,194` documents the whole protocol; ~300
   lines reproduces the client-facing half). Add **scenario fixtures** that populate a
   synthetic map, units, players and the `xUnitSearch`/`yUnitSearch` arrays, so the spatial
   queries are covered too. With it, the **entire client-mode ABI is testable on Windows CI
   with no StarCraft installation**, and it is what settles x64 for real (§10.2).
6. **Boundary fuzzing.** The headline safety promise is that a wrapper bug in a foreign
   language must not crash StarCraft mid-tournament, and revision 2 had no test for it.
   Generate a harness from the `.def` that calls every export with negative handles, `cap`
   of `INT_MIN`, `NULL` with nonzero `cap`, and `buf_len` one byte short.
7. **Re-entrancy tests**, if callbacks ever land: a predicate calling a read-only function
   returns correct results; one calling a forbidden function gets
   `BWAPI_ERR_REENTRANT_MUTATION`, no side effect, and a warn-level log line.
8. **Coverage audit**, on demand and at pin bumps — not on the merge path (§9).
9. **End-to-end smoke.** The C, Rust and Node example bots against real StarCraft, as a
   pre-release manual gate.

---

## 12. Roadmap

Revision 2's phase 3 was the entire project under one letter, and it bundled the
highest-risk item with the most mechanical one. Split, with a testable exit criterion for
every phase.

| Phase | Deliverable | Exit criterion |
|---|---|---|
| **0. Bootstrap** | Repo, pinned submodule, derived link closure with explicit file lists, client-only CMake target, `svnrev.h` from upstream's script, layout-dump tooling, LGPL files, `bwapi_c.h` skeleton with the §4 conventions | An empty `bwapi_c.dll` links x86; the x86 layout dump is checked in as the baseline; the x64 verdict is recorded as either "empty x64 DLL links and the dumps match" or a named blocking dependency |
| **1. Generator** | Spec format, `emit_*.py`, `check_coverage.py`, `api.json` + its schema; `Player` (54 decls) fully generated as the proving ground | `Player` round-trips spec → header → `.def` → `api.json` → a compiling Rust `extern` block; header hygiene and golden `.def` diff green in CI |
| **2. Static types** | `bwapi_c_types.h` (848 constants), all `*Type` static data (185 accessors + ~14 bulk tables) | ~500 assertions green with no game, no server, and no StarCraft installed |
| **3. Mock server** | Client-facing half of the shared-memory and pipe protocol, plus scenario fixtures populating a synthetic map, units, players and the `xUnitSearch`/`yUnitSearch` arrays | A scenario is asserted end-to-end through the ABI on CI with no StarCraft; **an x64 client completes a frame handshake against an x86 mock**, or the x86-only decision is final and documented |
| **4. Read surface** | `Game` (~120 after the §5.2 collapse) and `Unit` (~250) getters, `Force`/`Region`, events, bulk map grids, the §5.10 snapshots | Every read entry point is exercised against a mock scenario; boundary fuzz green |
| **5. Write surface** | Commands, unit-set broadcasts, the §5.4 boundary-side closest queries | **A C99 example bot builds against `bwapi_c.h` alone, with no C++ toolchain, and against real StarCraft reads game state and moves units** |
| **6. Consumers** | `bwapi-sys` crate, Node raw FFI, Rust and JS example bots, idiomatic wrappers spun out to their own repos | Both example bots play a game; **`bwapi_abi_version()` returns 1.0 and the append-only promise takes effect** |
| **7. Conditional** | `Type::getType` name lookup, borrowed-pointer bulk access, callback predicates | Undertaken only on measured need; nothing depends on this phase |

Two things this ordering buys. **Phase 3 is where the risk lives, and it now comes before
the bulk of the generated surface**, so a protocol surprise lands while there is still time
to absorb it — and it is the phase that settles x64 for real. And **phase 5's criterion is
the one that matters**: the ABI has to be usable from plain C before it is usable from
anything else. A C bot that works is proof the header is honest; a Rust bot that works
could just mean the Rust wrapper is clever.

---

## 13. Risks and decisions

| Risk | Mitigation |
|---|---|
| **x86-only shuts out Node and default Rust** | Storm's exclusion (§10.1) removes the one dependency that would have killed it outright. Phase 0 gives a provisional answer, phase 3 a real one |
| **A private build of upstream sources drifts from what upstream builds** | Explicit file lists, never globs, so a new source file is a loud link error; the §10.3 checklist re-derives at pin bumps |
| **`unordered_set` pointer hashing makes bots irreproducible** | Sort collections by ID; compute closest-unit at the boundary with a lowest-ID tie-break (§5.4). Both recorded in §15 |
| **~770 entry points across two headers is a lot of surface to keep correct** | Codegen, declaration hashes, golden `.def`, boundary fuzz. The generator is the deliverable; the wrappers are output |
| **Struct layout freezes the ABI prematurely** | Size-prefixed PODs (§4), flag bits rather than boolean fields (§5.10), and 0.x until phase 6 (§4) |
| **Per-call FFI cost makes the ABI look slow in Node/Python** | Snapshots (§5.10), bulk grids cropped to the live map (§5.5), sticky errors that avoid a second crossing per call (§4) |
| **A foreign callback unwinds into BWAPI's stack** | Callbacks are a conditional follow-on; if they land, `catch(...)` at every site, the `reentrant: forbidden` guard, and warn-level logging on rejection |
| **256-byte text truncation surprises users** | Documented on every text function and in §15; worth proposing upstream separately |
| **`api.json` breaks downstream bindings silently** | Checked-in JSON Schema, `schema_version`, and the Rust generator consuming it in CI (§9) |
| **Someone builds bindings on the shared-memory layout instead** | The mock server and `api.json` make the C ABI the path of least resistance. Say plainly in the README that re-implementing `Templates.h` per language is a maintenance trap |

### Decisions log

From the revision-1 review:

| # | Question | Decision |
|---|---|---|
| 1 | Is x64 client mode in scope? | **Yes, if feasible.** §10.1 shows nothing in the closure blocks it; §10.2 sequences the proof across phases 0 and 3 |
| 2 | `bindings/` in-tree or separate repos? | **Both, split by layer.** Raw FFI in-repo, idiomatic wrappers per-language downstream. Rust and JavaScript only |
| 3 | Python dependency in CI? | **Yes.** Generated sources stay checked in, so it is a CI-only dependency |
| 4 | Does module mode justify a phase? | **No — deferred indefinitely.** Appendix A |
| 5 | Remove `swig.i` / `swig_lib/` from BWAPI? | **No.** Separate repository; no standing to propose removals there |

From the research round (R1–R11), 2026-09-05 — the eight forks left open by the rev-4 review
([research-vs-rev4-review.md](research/research-vs-rev4-review.md) §3):

| # | Question | Decision |
|---|---|---|
| 6 | Does §5.8 ship as functions or a lookup table? | **Both, plus constants.** Per-accessor functions, generated constants in the header, and a size-prefixed bulk-table export as an optional §5.10-style fast path |
| 7 | Where is the v1 cut line? | **Derived, not chosen: ~671 functions + 848 constants here, 98 in the BWEM header.** ~30% *above* §1.8's 550–600. Review §3.2 shows the arithmetic |
| 8 | `canXxx`: ship ~30 or all of them? | **All base predicates — 88 exports.** No `*Grouped`, no `checkCommandibility` parameter (§15 entries 17–18) |
| 9 | Does module mode return? | **Yes, as a scoped v2 item**, motivated by the grouped-command gap that no client-mode binding in any language can close. Appendix A |
| 10 | Is BWEM in scope? | **Yes** — second header, same §4 conventions. R11; §15.1; §15.2 |
| 11 | Linux via OpenBW? | **Parked, not closed.** A client never links the engine — it reaches a separate `BWAPILauncher` process over shm and a Unix socket — so the unlicensed-engine problem is a process boundary, not a link edge. **Nothing OpenBW is built, linked or distributed from this tree.** The remaining work is a POSIX port of `Client.cpp`'s 7 Win32 imports, as a §15.2-style patch on a pinned LGPL dependency |
| 12 | Test fixtures | **Synthetic by policy.** Recorded fixtures contributor-local and gitignored; JBWAPI's `.bin` files not vendored (R9 §7) |
| 13 | Rust in `bindings/`? | **Yes, as the proof-of-consumer.** Python and C# are the primary consumers; Rust proves the ABI is bindable |

Consequence of 6–8 taken together: **the generator is unconditional and lands in phase 1** — for
§5.8 because 848 constants cannot be hand-maintained, and for the interface layer because 542
declarations map to ~457 exports through five documented exclusion rules and only a coverage
audit can prove that mapping is complete.

From this revision: the ABI is LGPL-3.0 and dynamically consumed (§0); positions pack into
`int64_t` on return only (§4); errors are sticky and first-wins (§4); every crossing POD is
size-prefixed (§4); closest-unit queries move to the boundary and `getBestUnit` is dropped
(§5.4); snapshots are added (§5.10); the closure is derived and Storm excluded (§10.1); the
drift canary is replaced by a pin-bump checklist (§10.3); 1.0 gates on the consumers phase
(§4, §12).

---

## 14. What a consumer sees

**C**, which is the phase 5 exit criterion and therefore the one that has to be pleasant:
```c
#include <bwapi_c.h>

while (!bwapi_client_connect()) { Sleep(1000); }
for (;;) {
    while (bwapi_game_is_in_game()) {
        bwapi_clear_last_error();

        int32_t n = bwapi_game_snapshot_units(NULL, 0);
        bwapi_unit_snapshot* units = malloc(n * sizeof *units);
        units[0].size = sizeof *units;
        n = bwapi_game_snapshot_units(units, n);

        for (int32_t i = 0; i < n; ++i)
            if (units[i].type == BWAPI_UNIT_TERRAN_SCV && (units[i].flags & BWAPI_UF_IDLE))
                bwapi_unit_gather(units[i].id, nearest_mineral(units[i].x, units[i].y), 0);

        free(units);
        if (bwapi_last_error() != BWAPI_ERR_NONE) report(bwapi_last_error());
        bwapi_client_update();
    }
}
```

**Rust** (`bwapi-sys`, generated from `api.json`):
```rust
extern "C" {
    fn bwapi_client_connect() -> i32;
    fn bwapi_client_update();
    fn bwapi_game_snapshot_units(out: *mut BwapiUnitSnapshot, cap: i32) -> i32;
    fn bwapi_unit_get_position(unit: i32) -> i64;   /* packed, §4 */
    fn bwapi_unit_attack_unit(unit: i32, target: i32, queued: i32) -> i32;
}
```
…wrapped into a safe `bwapi` crate with `Unit(i32)` newtypes, `Result`, and iterators,
published from its own repository.

**JavaScript** (koffi, if x64 lands): the same DLL, with `bwapi_client_update()` on a
worker thread since it blocks on the pipe.

Neither consumer compiles a line of C++. That is the whole point.

---

## 15. Divergence register

The ABI deliberately departs from `BWAPI::Game` semantics in the places below. Goal 6 is
"no fork of the game logic", so the departures need one normative table rather than being
scattered through prose where they rot. **Every spec entry carrying a `body:` either names
a divergence here or asserts `divergence: none`.**

| # | Divergence | Where | Rationale |
|---|---|---|---|
| 1 | Collection output sorted ascending by ID | §4 | `Unitset` hashes pointers; ASLR makes iteration order vary run to run |
| 2 | Closest-unit queries computed at the boundary, tie-broken on lowest ID | §5.4 | Sorting cannot reach a single-unit return; `Templates.h` ties resolve by iteration order |
| 3 | `getBestUnit` not exposed | §5.4 | Meaningless without a `BestFilter` |
| 4 | No format-string functions; plain-text only | §5.1 | Format-string vulnerability; 256-byte truncation retained and documented |
| 5 | Positions returned packed in one `int64_t` | §4 | One register instead of a pointer write; lossless, sentinels preserved |
| 6 | Bullets are a snapshot array, not handles | §6.3 | No `Game::getBullet(id)` exists; bullets are transient |
| 7 | Events polled from a per-frame vector, not dispatched | §5.6 | `getEvents()` is a `std::list`; indexing it directly is O(n²) |
| 8 | Invalid handles return a documented neutral value | §4 | A foreign-language bug must not crash the game |
| 9 | Bulk grids cropped to the live map | §5.5 | 16× less data on a 128×128 map; `out_w`/`out_h` report the extent |
| 10 | Text truncated at 256 bytes | §5.1 | Inherited from `GameImpl::vPrintf`, not introduced — but surfaced here because callers cannot see it |
| 17 | `canXxxGrouped` not exposed | §5.4 | 17 declarations under 11 names. Grouped commands are **not implemented by the BWAPI server at all** (JBWAPI #70), so a client bot cannot use them in any language — exposing the predicates would advertise a capability that does not exist in client mode. Revisit with module mode (Appendix A) |
| 18 | `checkCommandibility` not exposed; every `can_*` behaves as if it were `true` | §5.4 | The flag is a default argument on 88 declarations, not a separate overload. Passing `false` skips the "can this unit accept commands at all" precheck — an optimisation for callers chaining several predicates on one unit, which costs a boolean on every signature and is a footgun across an FFI boundary where the precondition cannot be enforced |

### 15.1 BWEM divergences

`bwapi_c2_bwem.h` (R11) departs from `BWEM::Map` semantics in the places below. Same rule: one
normative table rather than prose.

| # | Divergence | Where | Rationale |
|---|---|---|---|
| 11 | `Base` addressed by a **synthesised** flat `int32_t`, 0..`BaseCount()-1` | R11.3 | BWEM's `Base` has no identity of its own and is stored by value inside `Area::Bases()`. `Base::Location()` is the fourth most-used BWEM call (89 sites), so bases need a first-class handle. The ABI owns the mapping table and rebuilds it on reset |
| 12 | Neutrals addressed by their **BWAPI unit id** — the one shared handle space | R11.3 | `Neutral::Unit()` *is* a BWAPI unit. Sharing the id makes the two headers join with no lookup table on either side, and makes `Map::GetMineral(Unit)` the identity function. Deliberate exception to §4's disjoint-handle-space rule, stated in both headers |
| 13 | BWEM's three-phase init collapsed to one `bwapi_bwem_initialize(int32_t, int32_t)` | R11.3, R11.5 | `Initialize` → `EnableAutomaticPathAnalysis` → `FindBasesForStartingLocations` has an ordering dependency with no use case for the intermediate states. An ABI should make ordering errors impossible rather than diagnosable. Blocks ~450 ms; documented |
| 14 | `on_*_destroyed` hooks are **filtered and idempotent**, never throwing | R11.5 | `MapImpl::OnMineralDestroyed` does `bwem_assert(found)` and therefore throws when handed a unit BWEM does not track — which is what a host forwarding every `onUnitDestroy` would do. Extends §4's neutral-value-and-latch rule to a third-party library's assertions |
| 15 | Mutators, internals, and `Graph`/`GridMap`/`UserData`/`Markable`/`MapDrawer`/`MapPrinter` not exposed | R11.1, R11.2 | 42 mutators/internals and 49 declarations on non-bot-facing classes, with **zero** call sites across 119,000 lines of bot code. Not merely unused — unnecessary, since no host runs the analysis |
| 16 | `Tile`/`MiniTile` exposed as scalar accessors first, bulk grids as an optional fast path | R11.1, R11.3 | Grid access is ~2% of BWEM traffic and no existing port exposes bulk grids. Inverts the emphasis §5.5 would suggest |

### 15.2 Patches carried on pinned dependencies

Not ABI-vs-library divergences but modifications to the pinned sources themselves. Each must be
re-applied at a pin bump (§10.3).

| Dependency | Patch | Reason |
|---|---|---|
| `third_party/bwem` (`N00byEdge/BWEM-community`, MIT/X11) | Add `void Map::ResetInstance() { m_gInstance = nullptr; }` | **Upstream bug found in R11.6.** `MapImpl::Initialize` resets in place with `this->~MapImpl(); new (this) MapImpl();`, and `~Neutral` calls `RemoveFromTiles()` which reaches back into the Map's already-destroyed tile storage. Re-initialisation therefore segfaults on any map with neutrals — i.e. every map — and the same root cause crashes at static destruction after the host releases its `GameData`. Stardust's vendored BWEM already carries exactly this method. `bwapi_bwem_reset()` depends on it. Offer upstream; do not gate on a dormant repository |

---

## Appendix A. Module mode

**A scoped v2 item — not v1, and no longer deferred indefinitely.** The concrete reason arrived
with R4: **grouped commands are not implemented by the BWAPI server**, so no client-mode bot in
any language can issue them (JBWAPI #70). That is a capability gap rather than a binding
limitation, every competing binding has it too, and module mode is the only way to close it.
It stays out of v1 because it carries real costs — x86-forever, no crash isolation, and a much
harder test story — and because client mode covers the non-C++ audience today; v1 documents the
gap in the README. Nothing in §12 depends on it.

**How it works today.** BWAPI loads `bwapi-data/AI/<bot>.dll` and resolves `gameInit` and
`newAIModule` by `GetProcAddress` (`bwapi/BWAPI/Source/GameUpdate.cpp:389`;
`bwapi/ExampleAIModule/Source/Dll.cpp` shows the exports). `newAIModule()` must return a
pointer to an object with an **MSVC C++ vtable** matching `BWAPI::AIModule`. A Rust
`cdylib` *can* fake that, but it is a hand-laid vtable pinned to one compiler's ABI —
exactly the fragility this project exists to remove.

**The way out** would be to make `bwapi_c.dll` itself the loadable AI module: it exports
`newAIModule`/`gameInit`, and on `gameInit` loads a *host* DLL named by config, resolving
pure-C entry points against a vtable of function pointers.

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

One C++ class deriving `BWAPI::AIModule` forwards every override to the table, each guarded
by `catch(...)`. There is precedent for the indirection:
`bwapi/AIModuleLoader/Source/AIModuleLoader.cpp:69` already does load-and-`GetProcAddress`.

**What makes revival cheap:** both modes sit behind the same abstract `BWAPI::Game`
(§1.1), so ~95% of the wrapper source is transport-agnostic and would be reused unchanged.
What would *not* carry over: module mode's ID assignment is lazy
(`Server::getUnitID`, `bwapi/BWAPI/Source/BWAPI/Server.cpp:719`) rather than a
constructor-populated vector, though `extractUnitData()` assigns every alive unit an ID
before AI callbacks run (`bwapi/BWAPI/Source/BWAPI/GameUnits.cpp:228`), so §1.3's handle
model still holds. And module mode is x86-only forever, since it is injected into a 32-bit
process — which is the constraint its deferral lifts from the rest of the plan.
