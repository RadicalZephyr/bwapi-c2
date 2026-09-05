# Plan: `bwapi-c2`, a C ABI for BWAPI and BWEM

## Purpose

BWAPI is consumable only from C++. Everything a bot touches is a C++ class with virtual
dispatch, `std::string`/`std::unordered_set` return values, `std::function` predicates,
template types, and printf-style varargs. None of that crosses an FFI boundary.

Three things have been tried about that, and the record is worth stating before proposing a
fourth. **Two languages reimplemented the client protocol** and are thriving on it — JBWAPI for
the JVM and rsbwapi for Rust are both in tournament use in 2026. **One C ABI already existed**
— `RnDome/bwapi-c` reached v1.0 in 2018, wrapped 530 entry points, and stopped. And **twenty
other attempts died**: six in C#, three in Python, two in Lua, one each in Go, Zig, Nim and
Haskell, plus TorchCraft, archived when its sponsor left. C# has been tried six times and has
no living binding today.

So the question this plan has to answer is why a fourth attempt goes differently, and the
answer has three parts, each of them measured in the companion research
([docs/research/](research/)):

- **It is for the languages with no maintained option** — Python and C# first, then everyone
  `api.json` reaches. Rust and the JVM are served; this is not a third Rust binding.
- **It is built over the real `BWAPI::Game`**, so that the 5,661 lines of shared game rules and
  the ~4,100 lines of static type data that JBWAPI and rsbwapi each ported by hand — 72% and 86%
  of their respective codebases, with fourteen recorded divergence bugs between them — are never
  ported again. That is the trap, and it is where the cost lives; the protocol itself is under
  5% of either project.
- **It ships the two things `bwapi-c` did not: the static type database, and a license.** The
  one bot built on `bwapi-c` hand-wrote 650 lines of enums because the library had none, and
  vendors unlicensed headers because the library has no license. Those are the two reasons a
  C ABI stopped being usable, and both are cheap to fix.

And it wraps **BWEM** too, through a second header, because a bot without map analysis starts
behind every C++ bot on day one and every host ecosystem would otherwise wrap or reimplement it
separately — the same duplicated-effort trap, one library over.

This document plans **`bwapi-c2`**: a stable, flat C ABI (`bwapi_c2.dll` + `bwapi_c2.h` +
`bwapi_c2_bwem.h`) over the existing C++ implementations, so any language with a C FFI can
drive BWAPI and BWEM without writing a line of C++.

This is a design/roadmap document. No production code is included.

> **Revision 4.** Revision 3 was internally consistent and missing the outside world. A survey
> found prior art occupying this project's exact position, live projects serving two of the
> languages it scoped, and an open-source engine contradicting a premise it rested on. Eleven
> research experiments (R1–R11) then settled the premises empirically, and eight forks the
> research left open were decided. **The §4 conventions survived all of it unchanged, and held
> unmodified against a second library with a different object model.** Everything around them
> moved:
>
> - The project is renamed **`bwapi-c2`** (§3); the symbol prefix stays `bwapi_`.
> - Prior art is a section, not a footnote (§1.6), and §3 is stated as a position relative to
>   `bwapi-c`, whose fork is foreclosed by a missing license.
> - Non-goal 1 is a measured argument (§2), and the audience is measured (§1.9).
> - **x64 is settled** by computation and two production existence proofs; the two-stage proof
>   is gone (§10.2). 32-bit Linux is the one incompatible target and is a non-goal.
> - **The mock server is gone.** Synthetic `GameData` fixtures drive BWAPI's real rule engine
>   and BWEM's full analysis with no server, no StarCraft and no Blizzard data (§11).
> - **The surface was measured and the target went up, not down**: ~671 functions plus 848
>   constants, and 98 more for BWEM (§1.8). The generator is unconditional and its
>   justification changed (§9).
> - SWIG's C target was run and rejected on five reproducible grounds (§9); clang's AST dump
>   drafts the spec.
> - **BWEM is in scope** (§8), with six divergences (§15.1) and one carried patch (§15.2).
> - Licensing is restated on the correct premise: the DLL embeds the Library (§0).
> - The roadmap is five phases (§12). Module mode is a scoped v2 item (Appendix A). Linux via
>   OpenBW is parked with the reason it can stay open (Appendix B).

---

## 0. Licensing

`bwapi-c2` is **LGPL-3.0-only**, and the reason is more specific than "inherited". §10.1's
closure compiles BWAPI's own source — `BWAPILIB/Source/*.cpp`, `Shared/*.cpp`,
`BWAPIClient/Source/*.cpp` — into `bwapi_c2.dll`. **The DLL contains the Library's object code**,
so it is conveyed under LGPL-3.0 §4 as a Combined Work and owes Corresponding Source for the
BWAPI portion. Our own wrapper source is an Application under LGPL §0; shipping both under
LGPL-3.0 collapses the question. BWEM's MIT/X11 license is an attribution obligation inside the
combined work, not a second package license. Verified in R9 and R11.7.

Four consequences, three of which constrain the design and one of which constrains test data.

**1. `bwapi_c2.dll` must be consumed dynamically, never statically linked into a consumer.**
LGPL §4(d) gives a bot author two options: 4(d)(0), ship object files so the user can relink
against a modified Library; or 4(d)(1), a shared-library mechanism that picks up a modified
interface-compatible copy at run time. Loading `bwapi_c2.dll` is 4(d)(1). A static `bwapi_c2.a`
would force every closed-source bot into 4(d)(0), so a static variant must not be offered,
however often it is asked for.

**A distributed bot still owes §4(a)–(c)** — prominent notice that the Library is used, copies
of both the GPL and the LGPL alongside it, and the Library's copyright notices if the bot
displays any. A tournament zip containing a bot and `bwapi_c2.dll` is a distribution. The README
must not say "the license does not propagate to bots that call it"; it must say **"your bot's
own code stays yours; your *distribution* carries these files and this notice"**, and the
release asset ships a `NOTICE` snippet a bot author copies verbatim.

**2. The static CRT (`/MT`, §10.1) is fine — for two independent reasons that must both hold.**
GPL-3.0 §1's System Libraries definition means we need not ship the MSVC runtime's *source* in
Corresponding Source. It is not a permission to statically link; that permission comes from
Microsoft's Visual Studio distributable-code terms, a separate contract the LGPL does not touch.
Verify the latter against the VS edition that builds the release; Community and Professional
terms differ. The no-allocation-crosses-the-boundary argument in §4 stands on its own merits.

**3. Every release asset ships the file table below**, and the crates.io, PyPI and NuGet
packages carry the same, declaring `LGPL-3.0-only` in their metadata.

| File | Clause | Note |
|---|---|---|
| `COPYING` (GPL-3.0) | LGPL §4(b) | Not in BWAPI's tree; fetch from gnu.org |
| `COPYING.LESSER` (LGPL-3.0) | LGPL §4(b) | Copy of BWAPI's `LICENSE` |
| `LICENSE.BWEM` (MIT/X11) | attribution | Verbatim copy retaining the Igor Dimitrijevic notice |
| `NOTICE` | LGPL §4(a) | Names BWAPI (LGPL) and BWEM (MIT/X11); records our `ResetInstance` modification (§15.2) |
| source pointer | GPL §6(d) | URL plus the pinned commits of **both** dependencies and our own tag. This line is doing legal work, not boilerplate — it is the Corresponding Source offer |

The X11 no-advertising clause applies: describe BWEM's function in the README; do not use the
copyright holder's name promotionally.

**4. Test data is synthetic by policy.** `GameData` carries no Blizzard static tables, so the
only provenance exposure in a recorded buffer is *map terrain* — and JBWAPI's fifteen fixtures
are community ladder maps with no stated license. Fixtures in this repository are hand-built
(§11); recorded buffers are contributor-local and gitignored; JBWAPI's `.bin` files are not
vendored.

**Two unlicensed dependencies exist in the ecosystem and neither is ours.** `RnDome/bwapi-c` has
no license (§1.6), which forecloses forking it. The OpenBW *engine* has no license — only the
BWAPI fork over it is LGPL — which is why Appendix B builds, links and distributes nothing from
OpenBW.

---

## 1. Ground truth: what the codebase actually looks like

These facts drive every decision below. Where revision 3 inferred, revision 4 measured; the
experiment is cited where it matters.

### 1.1 One interface, one wrapper

`bwapi-c2` targets **client mode**: the bot is a separate process that finds a server via
`Local\bwapi_shared_memory_game_list`, opens `\\.\pipe\bwapi_pipe_<pid>` and maps
`Local\bwapi_shared_memory_<pid>` onto `BWAPI::GameData`
(`bwapi/BWAPIClient/Source/Client.cpp`; server side at
`bwapi/BWAPI/Source/BWAPI/Server.cpp:40,99,194`).

Module mode — the injected 32-bit DLL — is a scoped v2 item; see **Appendix A**. The one fact
worth keeping in the live document is that *both* modes implement the same abstract
`BWAPI::Game` (`bwapi/include/BWAPI/Game.h`), with `bwapi/Shared/` (≈4,700 lines, of which
`Templates.h` is 3,098) compiled into both. That is what makes a v2 cheap, and what lets this
wrapper be written against one interface rather than a transport.

### 1.2 The type system is already almost FFI-shaped

`BWAPI::Type<T, UnknownId>` (`bwapi/include/BWAPI/Type.h`) has exactly **one** data member —
`int tid` — no virtuals, plus `constexpr operator int()`. So all of `UnitType`, `Race`,
`TechType`, `UpgradeType`, `WeaponType`, `Order`, `UnitCommandType`, `UnitSizeType`,
`GameType`, `PlayerType`, `BulletType`, `DamageType`, `ExplosionType`, `Error`, `Color` are
**`int32_t` at the ABI level**. No conversion code, no wrapper objects, no lifetime questions.

`Position` / `TilePosition` / `WalkPosition` are `Point<int, Scale>`
(`bwapi/include/BWAPI/Position.h`) — two `int`s, distinguished only by a compile-time scale. §4
packs them into a single `int64_t` on return; the scale becomes a naming convention
(`_position` = pixels, `_tile` = 32px, `_walk` = 8px).

Sentinels: `Positions::Invalid/None/Unknown/Origin` (`Position.h:399-408`) and their tile/walk
equivalents, exported in both packed and unpacked form (§4).

### 1.3 Object identity maps cleanly onto integer IDs

`Unit`, `Player`, `Force`, `Region`, `Bullet` are typedefs for pointers to interface classes.
But every one exposes `getID()`, and `Game` already has ID-keyed lookups: `GameImpl::getUnit(id)`
is a direct index into a 10,000-entry vector (`bwapi/BWAPIClient/Source/GameImpl.cpp:382`,
populated in the constructor at `:29`) — **O(1)**; `getPlayer` (12), `getForce` (5),
`getRegion` likewise.

**Consequence: use `int32_t` IDs as ABI handles, not raw pointers.** They are stable across
frames, serializable, cheap to validate, cannot dangle, are already what the shared-memory
protocol speaks, and let the host language store them in plain arrays. Exposing
`UnitInterface*` would be faster by a pointer chase and vastly more dangerous — and `bwapi-c`
is the field report: raw `reinterpret_cast` pointers, zero validation in 3,320 lines, and two
abandoned attempts (`safe_cast`, `checkPointer`) to add it afterwards (R1 §3).

One exception: **bullets have no `Game::getBullet(id)`**. Client-side they live in a fixed
100-entry `bulletVector`; `BulletInterface::getID()` returns the game's bullet ID, not the
index. Bullets are transient and few, so expose them as a **POD snapshot array** rather than
handles (§6.3).

### 1.4 `GameData` is pointer-free, and its layout is settled

`bwapi/include/BWAPI/Client/GameData.h` and its members contain **only** `int`, `unsigned`,
`bool`, `double`, `char[]`, `unsigned short`, plain enums, and one `BWAPI::UnitCommandType`
(the 4-byte `Type` above). No pointers, no `size_t`, no `long`.

Revision 3 argued from this that the layout *should* be identical at 32 and 64 bits. **R5
computed it.** Across `i386` and `x86_64`, MSVC and MinGW, the layout is byte-identical and
`sizeof(GameData) == 33,017,048` — the constant JBWAPI hardcodes (`ClientData.java:70`) and
gobwapi asserts in a passing test. Two production clients — JBWAPI and rsbwapi, both 64-bit
processes against the 32-bit server — are the interop proof revision 3 planned to run at phase 3.

The one target that disagrees is **32-bit Linux**: System V aligns `double` to 4, so
`BulletData` is 76 bytes rather than 80 and `sizeof(GameData)` comes out 404 bytes short — a
silent misread, not a crash. It is an explicit non-goal (§2) and is recorded so the hazard is
not rediscovered. Linux x86-64 agrees with Windows.

Packing: the only project setting `StructMemberAlignment` is **Storm** (`1Byte`), and there are
twenty `#pragma pack` directives in the tree — nineteen under `BWAPI/Source/BW/` (the injected
DLL's mirrors of StarCraft's own memory) and one in `Thread.cpp`. **None is in the client
closure**, and the closure contains no bitness conditional of any kind (R6 §6). §10.1 excludes
Storm and `BW/` for independent reasons.

### 1.5 Build reality

- Every C++ project in `bwapi/bwapi.sln` (24 projects) is **`Win32` only**, MSVC toolset
  `v141_xp`, `stdcpp17`.
- CMake exists but builds **static libs** only (`CMake/BWAPI/`, `CMake/Client/`); the BWAPI DLL
  target is commented out as non-functional. `CMake/Client` is simultaneously too little (no
  `BWAPILIB`) and too much (it compiles Storm and seven `Util` TUs that nothing needs).
- **The client closure builds and runs on Linux with GCC** (R6, R7) — 44 translation units,
  seven Win32 imports all in `Client.cpp`, nothing from Storm, Util or Boost. Boost has been
  `#if 0`'d out of the client path since April 2017.
- **Two MSVC-permissive-mode constructs block every non-MSVC compiler**, both inside the
  closure: `include/BWAPI/Client/CommandTemp.h:34` (two-phase lookup; `-fdelayed-template-parsing`
  on clang, no GCC equivalent) and `BWAPIClient/Source/Convenience.h:33` (`va_list&`; a
  one-character fix). §10.1 carries the workarounds; both are upstream bugs worth reporting.
- **`Interface.h:72` truncates a pointer to `int`** in `getClientInfo<CT>()`. It is a template
  member that **does not instantiate anywhere in the closure** (R6 §7), and §5.9 excludes
  `clientInfo` from the ABI. A documentation note, not a build constraint.

### 1.6 Prior art

Every design decision below either agrees or disagrees with something already built. In order
of relevance:

| Project | What it is | What it means for us |
|---|---|---|
| **`RnDome/bwapi-c`** (2017–2019, v1.0) | Hand-written C bindings: **530 entry points**, ~99% of the six dynamic interfaces and **0% of the ~1,030 constants and type accessors**. Module and client mode, MSVC and GCC, OpenBW support. No tests. Raw unvalidated pointers, heap iterator objects (~3 crossings per element), `BwString` with a mandatory release, `unordered_set` order passed straight through, no error channel, exceptions unwinding through `extern "C"` (issue #52). **No license** — no file, no header, `license: null`, in 148 commits | **This project already existed and stopped.** The fork is foreclosed on licensing alone; R1 §10 shows the §4 conventions would have rewritten every one of 530 signatures anyway. Read-only reference for coverage and overload naming; zero code reuse |
| **`ceverettkoop/oscar_c`** (Zig, 2023–2026) | The one bot built on `bwapi-c`. Hand-wrote 650 lines of type enums because the library had none; vendors the unlicensed headers into an MIT project | The field report on what a C ABI without §5.8 costs, and on what a missing license does downstream |
| **`JavaBWAPI/JBWAPI`** (Java, alive) | Pure client reading the mapped file in place via `Unsafe`; 32- and 64-bit; 28,697 lines of which 11,210 port the game rules and 5,297 the type data (72%). Fourteen recorded divergence bugs, three open; `canBuildHere` wrong since 2023. **Its issue #88 states the permanent cost: a fix in BWAPI will not fix it for JBWAPI** | The JVM is served, by the approach non-goal 1 measures. Also the x64 existence proof (§1.4), and the source of the fixture design §11 adopts (`GameBuilder.java` is 48 lines) |
| **`Bytekeeper/rsbwapi`** (Rust, alive) | Pure client; 37,017 lines, of which 25,367 are a hand-transcribed type database (86%); one author; no latency compensation, no `getBuildLocation`; commit log includes "fixed WeaponTypes being totally off" | Rust is served — by one person, with the gaps a C ABI fills for free. Rust is the proof-of-concept consumer here, not a product (§7) |
| **`OpenBW/BWAPI4J`** (Java, dead 2019) | JNI bridge; batches per-frame data into one array copy | Corroborates §5.10 — for designs with an FFI boundary specifically (see §5.10) |
| **`BradEwing/gobwapi`** (Go, Feb 2026) | New pure client; hardcodes `sizeof(GameData)` with a passing test | The audience is still arriving (§1.9). Third x64 data point |
| **`OpenBW/bwapi`** + `basil-ladder/bwapi@linux-client-support` | BWAPI 4.2 fork over the OpenBW engine; incompatible with retail; `CLIENT_VERSION` 10002; no client transport on OpenBW's repos, a ~344-line POSIX one on the basil-ladder branch, unmerged since 2020 | Appendix B |
| **`OpenBW/openbw`** (the engine) | Builds on GCC 11; needs three retail MPQs to run; **no license file, no README statement, no headers** | Not built, linked or distributed from this tree (§0, Appendix B) |
| `bwapi/include/swig.i` | An unfinished SWIG-Java attempt in the BWAPI tree | Its `%ignore` list is a useful triage record of what does not survive a binding. Not worth resurrecting; §9 rejects SWIG on measured grounds |

Three BWEM ports — JBWAPI's `bwem` (5,338 lines), rsbwapi's (2,496), gobwapi's (2,004) — all
**re-run the analysis in the host language**, because there is nothing to read results out of.
§8 is what changes that.

### 1.7 Exceptions: none in BWAPI's closure, by design in BWEM's

`grep -rn "throw "` across `bwapi/BWAPILIB`, `bwapi/Shared`, `bwapi/BWAPIClient` and
`bwapi/include` returns **nothing**, and the closure contains zero `try`, `catch` or `noexcept`.
`Util/Exceptions.h` types are used only by the injected DLL's config/XML paths.

**BWEM throws from assertion macros compiled into release builds** — `bwem_assert_throw` →
`BWEM::Exception : std::runtime_error` — and JBWAPI's #34 and #51 are those assertions escaping
into bots. So the boundary is `noexcept` with a `catch (const std::exception&)` on every export
in both headers, latching the error and returning the neutral value; the generator emits it as
one template. For BWAPI that is insurance; for BWEM it is the headline safety promise applied to
a library that needs it (§8).

### 1.8 API surface size — measured

The six interface headers (`Game.h`, `Unit.h`, `Player.h`, `Region.h`, `Force.h`, `Bullet.h`)
declare **542 member functions** in this tree, comments and templates stripped: 105 `canXxx`,
49 `draw*`, 388 other. The type classes add 185 accessors and 848 named constants (R1). Every
block is decided by a rule rather than by a count:

| Block | Declared | v1 exports | Rule |
|---|---|---|---|
| Interface methods (ex-`can`, ex-`draw`) | 388 | **361** | Ship the declared surface, less two mechanical merges: 13 filter-taking overloads (§5.4) and 14 `int x, int y` overloads that §4's position rule makes identical to their `Position` sibling |
| `canXxx` | 105 | **88** | All base predicates; no `*Grouped` (17); `checkCommandibility` suppressed (§5.11) |
| `draw*` | 49 | **8** | §5.2's `ctype` collapse; all three coordinate spaces kept |
| Type-data accessors | 185 | **185** | §5.8, shipped whole |
| Bulk type tables | — | **~14** | §5.8's optional fast path |
| Lifecycle, errors, version | — | **~15** | §4 |
| **`bwapi_c2.h`** | | **~671 functions + 848 constants** | |
| **`bwapi_c2_bwem.h`** | 279 | **98** | §8 |

Revision 3 guessed "550–600"; the measurement came out ~30% higher. What the measurement
changed is not the count but the *reason* §9 exists: **542 declarations map to ~457 interface
exports through five documented exclusion rules, and the rules are the thing that can be
wrong.** `bwapi-c` hand-wrote 530 wrappers and they work, so "too many to type" was never the
argument. Only a coverage audit run off a machine-readable spec can show the mapping is
complete. §5.8's 848 constants and 185 accessors are the part nobody should type at all — five
projects have transcribed that database by hand, and at least two got it wrong.

Usage frequency was measured too (R2: a competitive bot uses ~200 distinct entry points; 95% of
call sites at 195) and it turned out to be the wrong instrument for scope. **Usage tells you what
is safe to merge, not what is safe to omit** — draws are development tooling that tournament bots
undercount, `canXxx` predicates exist to answer questions a bot does not yet know it will ask,
and three of R2's four cut recommendations were withdrawn on that principle. The one that
survived is "ship §5.8 whole."

### 1.9 Who this is for — measured, not assumed

The request channel is empty: two Python asks in eight years, both closed the same day.
`BWAPIC.h` has zero consumers in code search. The Rust crates' download counts are crawler
traffic. **One bot in 291 SSCAIT registrations is neither C++ nor JVM.**

The *building* channel is active: a Go client started in February 2026, a Zig bot has run on
`bwapi-c` since 2023, a vendored-rsbwapi Rust bot appeared in December 2025, and two C# clients
were started in 2023 and abandoned within days — because the only path available was
reimplementing the protocol. pybrood, the longest-lived Python attempt, died with six unanswered
issues, every one a coverage complaint, and the last of them is the §5.8 gap exactly: *"is there
any way to create a UnitType?"* The Nim attempt's own README names the cause: `operator->`,
mangling, the `AIModule` vtable, and a wrapper generator that is itself a project.

**So the audience is thin, real, currently active, and unreachable through issue trackers.**
C# is the strongest unserved signal and Python the second; they are the primary consumers.
Nobody here asked for a C ABI, and everyone here built the thing a C ABI would have replaced.

---

## 2. Goals and non-goals

### Goals

1. **Two flat `extern "C"` headers**, valid C99 *and* C++, no C++ types, no macros required at
   the call site: `bwapi_c2.h` for BWAPI and `bwapi_c2_bwem.h` for BWEM, sharing
   `bwapi_c2_types.h` and every §4 convention.
2. **Stable ABI**: append-only, versioned, symbol list under test — from 1.0 onward (§4).
3. **Zero C++ toolchain required for consumers** — a `.dll` + `.h` (+ `.lib`) is the whole
   delivery.
4. **Cheap for binding generators**: `api.json` alongside the headers, so `ctypes`, P/Invoke,
   `bindgen` and JNA-style wrappers are generated, not hand-typed.
5. **Complete enough to write a real bot** in v1 — the declared surface under §1.8's rules,
   not a usage-derived subset. The phase 3 exit criterion (§12) is a working bot in plain C.
6. **No fork of the game logic or the map analysis.** Everything routes through `BWAPI::Game`
   and `BWEM::Map`. Where the ABI's semantics deliberately differ, §15 records it.
7. **Purely additive.** The ABI lives in its own repository and *depends on* BWAPI and BWEM.
   Landing it requires no change to either tree. Patches it must carry are recorded (§15.2)
   and offered upstream on their own merits.
8. **LGPL-clean for closed-source bots** (§0): dynamic consumption only.

### Non-goals

1. **Not a re-implementation of the shared-memory protocol per language.** Two maintained
   projects did exactly that, so this has to be argued rather than asserted. Measured (R4), the
   trap is real but mislocated. The protocol itself is cheap — 926 lines in JBWAPI, 212 in
   rsbwapi, under 5% of either. What is expensive is everything the protocol does not carry:
   BWAPI's shared game rules (`Templates.h`, `UnitShared.cpp`, `CommandTemp.h` — 5,661 lines)
   and its static type database. JBWAPI spent 16,507 lines on those, 72% of its `bwapi` package;
   rsbwapi 31,510, 86% of its total. Both got parts wrong; both skipped parts; and the cost does
   not end at the port, because a fix upstream fixes nothing downstream. A C ABI over the real
   `BWAPI::Game` buys out of that 72–86%. **It does not buy out of the server's own limits**:
   grouped commands are unavailable to any client bot in any language, and server-side bugs
   reach us unchanged (Appendix A). Latency compensation is exposed and controllable (§4.1), not
   reproduced.
2. **Not an idiomatic high-level API.** That belongs in per-language wrappers. The C ABI is the
   thin, boring, mechanical layer.
3. **Not cross-platform in v1, and not because of physics.** Windows and retail Brood War are
   the v1 target because that is where tournaments run. The closure itself builds and runs on
   Linux (R6, R7). Linux via OpenBW is **parked, not closed** — Appendix B records what remains
   and why it can stay open at no licensing cost.
4. **Not 32-bit Linux, ever.** The one target where `GameData`'s layout differs (§1.4).
5. **Not BWTA2.** Superseded by BWEM; the ecosystem has migrated (the two currently-developed
   SSCAIT bots reference BWTA zero times); no maintained repository since 2018; needs Boost
   *and* CGAL. JBWAPI's `bwta` package is a 409-line shim over BWEM (R11.8).
6. **Not exposing `Interface<T>::registerEvent`, `get/setClientInfo`, or the `Broodwar << …`
   stream.** Host languages have closures and hash maps.
7. **Not exposing `std::function` filters as first-class composable objects**, and not an
   enum-based filter DSL either. See §5.4.
8. **Not shipping, building or linking anything from OpenBW** (§0, Appendix B).

---

## 3. Recommended shape, stated against the prior art

> **A separate `bwapi-c2` repository producing `bwapi_c2.dll`, consuming BWAPI and BWEM as
> pinned submodules, wrapping `BWAPI::Game` and `BWEM::Map` behind `int32_t` handles, generated
> from a checked-in API spec, client mode only, LGPL-3.0 and dynamically consumed.**

A blank-page design for a problem someone already solved to v1.0 is not credible, so the shape
is stated as a position relative to `RnDome/bwapi-c`. **Three options were on the table**:
revive it, rebuild against it as a reference, or explain why neither.

**Revival is not available.** `bwapi-c` has no license (§1.6), and relicensing needs four
contributors who have not touched it in seven years. That is decisive on its own. It would have
been marginal anyway: under §4, a fork keeps the *names* of 530 functions and rewrites the
implementation of every one — handles, collections, strings, positions, errors, exception
safety, struct versioning — and still has to write the ~1,030 entry points of §5.8 that
`bwapi-c` never had, plus every test, since it has none.

**So: rebuild, with `bwapi-c` as a read-only reference and zero code reuse.** Its 530-name
inventory is a free, checked list of which methods are wrappable and which overloads need
type-suffixed names — a convention this plan adopts. Its tracker is a list of the failure modes
§4 exists to prevent.

Five decisions worth defending:

**Its own repository, BWAPI and BWEM as dependencies.** Nothing about the ABI requires editing
either tree, so nothing about it should. It pins known-good revisions of dependencies that have
not moved meaningfully in years, and releases on its own cadence. §7 covers layout, §10.1 the
mechanics, §10.3 the pin-bump.

**Client mode only.** No injection, no C++ vtable interop, crashes stay isolated, and the
closure runs on both bitnesses. Module mode is a scoped v2 item (Appendix A) with one concrete
motivation the research found.

**ID handles, not pointers.** §1.3 — and `bwapi-c`'s history is the argument.

**Generated, not hand-written — for the reason in §1.8**, which is the coverage audit, not the
typing.

**One DLL, two headers.** BWEM needs exactly ten BWAPI symbols, all already in the closure, and
adds 327 KB (R11.4). A second DLL would buy a second thing to version and nothing else.

**Naming.** The project is **`bwapi-c2`**. Every artifact carrying a project identity uses
`bwapi-c2` or `bwapi_c2`. The **exported symbol prefix stays `bwapi_`**: nothing else in the
ecosystem uses it (`bwapi-c` exports `Unit_*`/`Game_*`/`BWAPIC_*`; BWAPI exports nothing
unmangled), the prefix names the API being wrapped rather than the project wrapping it, and a
project-generation number has no place in a permanent ABI surface. The library builds with
hidden visibility and an explicit export list, so the exported set is a build-time decision
(R10).

| Thing | Name |
|---|---|
| Repository | `bwapi-c2` |
| CMake target | `BWAPI_C2` |
| Shared library | `bwapi_c2.dll` (`libbwapi_c2.so` if Appendix B lands) |
| Import lib / module def | `bwapi_c2.lib`, `bwapi_c2.def` |
| Public headers | `bwapi_c2.h`, `bwapi_c2_bwem.h`, `bwapi_c2_types.h` |
| Include guards | `BWAPI_C2_H`, `BWAPI_C2_BWEM_H`, `BWAPI_C2_TYPES_H` |
| Exported symbol prefix | `bwapi_` (BWAPI), `bwapi_bwem_` (BWEM) |
| Internal C++ namespace | `BWAPI::CApi` — never exported, deliberately not renamed |
| Packages | crates.io `bwapi-c2-sys` / `bwapi-c2`; PyPI `bwapi-c2`; NuGet `BwapiC2` |
| Release asset | `bwapi-c2-<ver>-win32.zip`, `-win64.zip` |
| SPDX | `LGPL-3.0-only` |

⚠️ Do **not** name anything `BWAPIC` — `namespace BWAPIC` already exists in
`bwapi/include/BWAPI/Client/*.h` for the shared-memory PODs. And `bwapi-sys`, `bwapi`,
`bwapi_wrapper` and `rsbwapi` on crates.io are held by owners inactive since 2018; crates.io is
first-come-first-serve and will not transfer them. Do not retry.

---

## 4. ABI conventions

These are the rules the generator enforces, and they are the part of this plan that was worth
writing. They were designed against BWAPI and then mapped, unmodified, onto BWEM (R11.3); R8
incidentally showed three of them are expressible even in SWIG's typemap language. Locking them
down first is most of the work.

**Linkage and calling convention.** Everything `extern "C"`. Explicit
`#define BWAPI_C2_CALL __cdecl` on every exported function — never rely on the project
default, since consumers (notably JNA-style loaders) guess. Export via a `.def` file so names
are undecorated even in x86 builds, and **assign no ordinals**: binding is by name only, so a
reordered `.def` can never silently rebind a consumer.

**Every export is a `noexcept` boundary** with `catch (const std::exception&)` and `catch (...)`
that latch the error channel and return the neutral value. BWAPI's closure never throws (§1.7);
BWEM does; `bwapi-c` had neither and its #52 is the result. One generator template, not
hundreds of hand-written `try` blocks.

**Types.** Only `int32_t`, `int64_t`, `uint32_t`, `uint8_t`, `int16_t` (bulk grids only),
`double`, `char`, `void*`, function pointers, and PODs declared in `bwapi_c2_types.h`. Never
C++ `bool`. No enums in signatures — `int32_t` with `#define`d constants, so an unknown future
value cannot be UB in a strict-enum language. No structs by value in or out; pass `const T*`
in, `T*` out.

**Strings are plain `const char*` — no `...` and no `va_list` in any exported signature.**
Formatting is the host language's job and happens before the call. `...` is not portably
callable through `ctypes`, JNA, koffi, P/Invoke or cgo; `va_list` is an ABI-specific type that
no FFI layer models; and a foreign string in a format position is a format-string vulnerability
(§5.1). `bwapi-c` exports twelve variadic functions plus `va_list` forms and is the
counter-example. In a sample of ~660 bot call sites every format string was a literal, and the
commonest idiom was already `sendText("%s", text.c_str())`.

### Booleans: scalar vs. bulk

Two different rules, and each rests on its own reasoning.

- **Scalar** boolean parameters and return values are `int32_t`, 0 or 1.
  - *For returns*: a narrow integer return leaves the upper bits of the register unspecified,
    and FFI layers disagree about whether the callee zero-extends — `ctypes`, JNA, koffi,
    P/Invoke and hand-written `extern` blocks all model an `int`-width return most reliably.
  - *For parameters*: under `__cdecl` on x86, arguments occupy word-aligned stack slots, so a
    narrower type saves nothing at all. Uniformity is therefore free, and it means the header
    has exactly one integer width for every scalar in the ABI, since all `Type` ids are already
    `int32_t`.
- **Bulk** boolean grids (§5.5) and flag words (§5.10) do not follow that rule. Grids are
  `uint8_t` per element; snapshot booleans are bits in a `uint32_t`. Four bytes per tile would
  quadruple a walkability copy for nothing, and the destination is a typed array in the host
  language, not a call argument, so none of the above applies.

### Positions: one packed `int64_t` on return

A `Position` return value is a **single `int64_t`**: x in the low 32 bits, y in the high 32,
two's complement.

```c
typedef int64_t bwapi_position;   /* also used for tile and walk positions */
#define BWAPI_POS_X(p)      ((int32_t)(uint32_t)((uint64_t)(p) & 0xFFFFFFFFu))
#define BWAPI_POS_Y(p)      ((int32_t)(uint32_t)((uint64_t)(p) >> 32))
#define BWAPI_POS_MAKE(x,y) ((bwapi_position)(((uint64_t)(uint32_t)(y) << 32) \
                                              | (uint32_t)(x)))
```

Out-params would cost an extra argument, a pointer write, and a host-side allocation on every
call to something bots invoke constantly. A packed `int64_t` is one register, no pointer, and
trivially decoded everywhere. It also has **no ABI ambiguity** — an 8-byte POD returned by value
is precisely where MSVC and i386 System V disagree, and `bwapi-c`'s README papers over that with
a `-mabi=ms` instruction living outside the header. The macros are a convenience; the encoding
is documented, so a host language decodes it however it likes.

**Position *parameters* stay as separate `int32_t x, y`.** Packing exists to solve returning two
values without a pointer; parameters have no such problem, and unpacked arguments are cheaper to
build and easier to read in every host language. Do not pack for symmetry. **Positions inside
structs likewise stay as separate `int32_t x, y` fields.** A **position array** (BWEM's
chokepoint geometry, §8) is an array of packed `int64_t` — the return rule applied elementwise.

Packing is lossless, so the sentinels survive unchanged: emit `Positions::Invalid`, `None`,
`Unknown`, `Origin` and their tile and walk equivalents in **both** unpacked
(`BWAPI_POSITION_NONE_X` / `_Y`) and packed (`BWAPI_POSITION_NONE`) forms. Do not invent a new
invalid bit pattern. **The neutral return for a position-returning function given a bad handle
is packed `Positions::None`** — one rule, matching the invalid-handle policy below.

**Naming.** `bwapi_<subject>_<verb>[_<disambiguator>]`, snake_case:
`bwapi_unit_get_hit_points`, `bwapi_unit_attack_position`, `bwapi_unit_attack_unit`,
`bwapi_unittype_max_hit_points`. Overloads get an explicit type-suffixed name — the convention
`bwapi-c` used, whose inventory is the checklist — no mangling, no defaulted arguments (C has
none, so every parameter is explicit and the per-language wrapper re-adds defaults).

**Strings out.** snprintf convention:
```c
int32_t bwapi_game_map_name(char* buf, int32_t buf_len);
```
Writes at most `buf_len` bytes including NUL; returns the length the string *would* need
(excluding NUL), so callers can size a second call. `buf` may be `NULL` when `buf_len == 0`,
purely to query length. Never returns an interior pointer — `mapName()` returns `std::string`
**by value** (`Game.h`), so any borrowed pointer would dangle immediately. (`bwapi-c`'s
`Event.text` is a bare `void*` into BWAPI's own `std::string`.)

**Strings in.** `const char*`, UTF-8-ish (StarCraft is codepage-bound; document as
"pass-through, no transcoding").

**Collections out.** Caller-provided buffer + true-count return:
```c
int32_t bwapi_game_get_all_units(int32_t* out_ids, int32_t cap);
```
Fills up to `cap`, returns the *total* available. `cap == 0` with `out_ids == NULL` is the size
query. No allocation crosses the boundary, so there is no free function and no allocator
mismatch — the classic MSVC-CRT-vs-host-CRT bug is designed out. The alternative is measured:
`bwapi-c`'s heap iterator costs ~3 crossings plus a virtual dispatch per element, so 600
crossings to enumerate 200 units.

**When `cap < total`, the returned elements are the first `cap` in ID order.** So a retry with a
larger buffer is coherent with the truncated result, and a caller can page. Leaving this
unspecified would make truncation a silent random sample.

**Order.** `Unitset` is a `SetContainer` over `std::unordered_set` hashing **pointers**
(`Unitset.h`: `SetContainer<BWAPI::Unit, std::hash<void*>>`). ASLR moves those pointers, so
iteration order varies *run to run* — this is genuine nondeterminism, not merely an unspecified
order. A bot that iterates and takes the first match would be irreproducible through no fault of
its own. **The ABI sorts collection output ascending by ID** and documents it (§15). Sorting
alone does not fix the closest/best queries, which return one unit — see §5.4.

**Struct evolution: every POD that crosses the boundary begins with `int32_t size;`.** The
append-only policy covers function signatures and says nothing about struct layout, but
`bwapi_event`, `bwapi_bullet`, `bwapi_unit_command` and the §5.10 snapshots are layout
contracts compiled into every consumer — and `ctypes` and P/Invoke consumers hardcode those
layouts in script and never recompile.

- The caller sets `size` on input structs. The callee validates it, reads only the prefix it
  understands, and ignores the rest.
- The callee sets `size` on output structs, writes the fields it knows, zero-fills the remainder
  up to the caller's `size`, and never writes past it.
- **For array-out functions the caller sets `size` on element zero, and that value is the
  uniform stride for the whole array.** No separate `elem_size` parameter — one mechanism, no
  redundancy.

Applied uniformly, including to `bwapi_unit_command`. The per-call cost is setting one field on
a struct most bots never touch (§5.3's ~40 convenience functions are the real command path), and
uniformity is worth more than four bytes.

**Invalid handles.** Never dereference. Validate, then return a documented neutral value (`0` /
`-1` / packed `Positions::None` / empty) and latch it in the ABI error channel. Rationale: a
wrapper bug in a foreign language must not crash StarCraft mid-tournament.

**Handle spaces are disjoint** — a unit id is never valid where a player id is expected, and
BWEM's area, chokepoint and base ids are disjoint from each other and from BWAPI's — **with one
deliberate exception**: a BWEM neutral *is* a BWAPI unit and is addressed by its unit id (§8,
§15 #12). Stated in both headers.

### Two error channels, with different lifetimes

```c
int32_t bwapi_last_error(void);                            /* does not clear */
int32_t bwapi_last_error_message(char* buf, int32_t len);  /* snprintf convention */
void    bwapi_clear_last_error(void);
```

**`bwapi_last_error()` is sticky and latches the *first* error. It is never cleared
implicitly.** A failing call sets the code only if the current code is `BWAPI_ERR_NONE`.
Successful calls do not touch it. Reading does not clear it.

Two reasons, and the first is the one that matters. **Clear-on-success would force a second FFI
crossing after every call** — a bot reading 3,000 values a frame would double its crossing count
just to find out whether any of them was a lie, which in Python is the difference between
comfortable and unusable. Sticky lets the host clear once at the top of the frame and check once
at the bottom (§4.1). Second, **the first error is the causal one**; a last-write-wins channel
reports whichever downstream call happened to fail next.

`bwapi_game_get_last_error()` remains an unchanged pass-through of `Game::getLastError()`, with
BWAPI's own per-call semantics. **The header states plainly that the two channels have different
lifetimes** — that is the thing a binding author will otherwise assume wrong. A caught
`BWEM::Exception` routes its `what()` into `bwapi_last_error_message()` rather than being
discarded.

**Process-wide singleton.** `BroodwarPtr` is a global, and so is `BWEM::Map::Instance()`; there
is no context handle and there will not be one. `bwapi_client_connect()` when already connected
latches `BWAPI_ERR_ALREADY_CONNECTED` and returns 0.

**Diagnostics.** `Client::connect()` writes directly to `std::cout`/`std::cerr`
(`bwapi/BWAPIClient/Source/Client.cpp`), which is useless-to-harmful for an embedded consumer.
Add `bwapi_set_log_callback(void(*)(int32_t level, const char* msg, void* user), void* user)`
and route wrapper diagnostics there. Every callback in the ABI carries a `void* user` — the
omission is the reason `bwapi-c`'s filter callbacks could not carry a closure. Leave the
underlying C++ prints alone in v1, but document them.

**Threading.** BWAPI is single-threaded and frame-synchronous. All calls happen on the thread
that calls `bwapi_client_update()` — see §4.1, which is normative.

**Versioning and the stability timeline.**
```c
uint32_t bwapi_abi_version(void);      /* semver of this ABI */
int32_t  bwapi_client_version(void);   /* BWAPI::CLIENT_VERSION, 10003 today */
int32_t  bwapi_revision(void);         /* SVN_REV from upstream's generator, §10.3 */
int32_t  bwapi_is_debug(void);
```

**The ABI is `0.x` and explicitly unstable until the consumers phase completes.** Real bindings
always shake out ergonomics, and promising append-only stability while also planning to revise
on consumer feedback cannot both hold. **Append-only begins when `bwapi_abi_version()` returns
1.0**, and reaching 1.0 is the exit criterion of phase 4 (§12). After that: new functions get
new names; existing signatures never change meaning; a removal is a major bump.

---

## 4.1 The frame loop

The ABI's only stateful protocol is connect → update → poll → repeat. It is normative: without
it written down, every binding author reconstructs it independently and at least one gets the
reconnect path wrong.

```c
while (!bwapi_client_connect()) { sleep(1000); }
for (;;) {
    while (!bwapi_game_is_in_game()) {
        bwapi_client_update();
        if (!bwapi_client_is_connected()) goto reconnect;
    }
    /* match start: optionally bwapi_bwem_initialize(1, 1) — blocks ~450 ms (§8) */
    while (bwapi_game_is_in_game()) {
        bwapi_clear_last_error();
        /* poll events, take snapshots, issue commands */
        bwapi_client_update();                    /* blocks on the pipe */
        if (!bwapi_client_is_connected()) break;
    }
    bwapi_bwem_reset();                           /* before the mapping goes away */
}
```

Alongside it, state:

- `bwapi_client_update()` **blocks** on `ReadFile` against the pipe. A host with an event loop
  runs it on a worker thread or stalls.
- Every other call must happen on that same thread.
- Event indices (§5.6) and snapshot contents (§5.10) are valid until the next `update()`.
- A handle from frame *N* may resolve to a dead unit at frame *N+1* — the existing C++
  semantics, unchanged.
- Clear the ABI error at frame start; check it before `update()`.
- **BWEM has nothing per-frame.** Its whole footprint on the loop is one match-start call and
  an explicit teardown. `bwapi_client_disconnect()` performs the teardown itself if the host
  forgot — the first thing in either header with a required shutdown order, and the reason is
  concrete: BWEM's singleton must be destroyed before the `GameData` mapping it points into
  disappears, and before static destruction (§8, §15.2).

**Latency compensation belongs here, not in a footnote.**

```c
void    bwapi_game_set_lat_com(int32_t enabled);
int32_t bwapi_game_is_lat_com_enabled(void);
```

Latcom changes what getters return *within* a frame: `CommandTemp::execute()` writes predicted
state straight into the local `UnitData` when a command is issued. That is a first-order fact
about what the read surface means, and it is the mechanism behind §5.4's argument that issuing a
command mutates the data an enclosing query is reading. It is also 1,032 lines JBWAPI spent
eleven months porting and rsbwapi does not have at all — here it is two functions.

---

## 5. What is mechanical vs. what needs design

Roughly **85% of the surface is mechanical** — a getter returning `int`, `double`, `bool`, a
`Type` (→ `int32_t`), a `Position` (→ packed `int64_t`), or a handle. That includes the entire
`canXxx` family, all of the static type data, every `Player` accessor, every `Unit` state query,
every command method, and nearly all of BWEM. These want a generator, not a human.

The other 15% is the interesting part, and §9 names the hand-written pieces explicitly so the
generator's coverage claim stays honest.

### 5.1 Varargs

`Game::printf`, `sendText`, `sendTextEx`, `drawText*` are printf-style, and the implementation
`vsnprintf`s into a **256-byte** buffer (`bwapi/BWAPIClient/Source/GameImpl.cpp:622-635`).

Expose **non-format** functions only, per §4's plain-`const char*` rule:
```c
void bwapi_game_printf(const char* text);
void bwapi_game_send_text(const char* text);
void bwapi_game_send_text_ex(int32_t to_allies, const char* text);
void bwapi_game_draw_text(int32_t ctype, int32_t x, int32_t y, const char* text);
```
implemented as `Broodwar->printf("%s", text)`. Passing a foreign string straight into a format
position is a **format-string vulnerability**, and a unit name containing `%s` would be enough
to trigger it. Document the 256-byte truncation (§15).

### 5.2 The `drawX{Map,Mouse,Screen}` triples

`Game.h` has 49 draw declarations, but only 8 primitives (`text`, `box`, `triangle`, `circle`,
`ellipse`, `dot`, `line`) × 3 coordinate spaces × `int`/`Position` overloads. The virtual ones
already take `CoordinateType::Enum`. Emit **one function per primitive** with an explicit
`ctype` parameter: 49 → 8, **all three coordinate spaces kept.** Usage data showing that no
tournament bot passes a runtime `ctype` is evidence the collapse is safe, not a reason to drop
Mouse space — draws are development tooling and tournament bots undercount them.

### 5.3 `UnitCommand`

`BWAPI::UnitCommand` holds raw `Unit` pointers. `BWAPIC::UnitCommand`
(`bwapi/include/BWAPI/Client/UnitCommand.h`) is already the ID-based mirror used over the wire.
Mirror *that*, with the §4 size prefix:
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
Also expose the ~40 convenience methods directly (`bwapi_unit_attack_unit`, `bwapi_unit_train`,
`bwapi_unit_build`, …) — that is what bots actually call, and going through a struct for
`move()` is a needless ergonomic tax on every wrapper author.

### 5.4 Filters, predicates, and re-entrancy

`UnitFilter` is `UnaryFilter<Unit>` over `std::function<bool(Unit)>`; `Filter::IsWorker` et al.
are composable objects with operator overloads. **None of that crosses C.**

1. **No filters in the exposed queries.** Every query has an unfiltered form — the 13
   filter-taking overloads simply do not cross — and the host language filters the returned ID
   array, or better, the §5.10 snapshot it already has.
2. **Keep the spatial index.** `getUnitsInRectangle` uses `Templates::iterateUnitFinder` over
   the shared-memory `xUnitSearch`/`yUnitSearch` arrays. That optimisation must not be lost —
   expose the rectangle and radius queries natively and let filtering happen after. (JBWAPI
   ignores the index and scans every unit; rsbwapi substituted an R-tree. Both are divergences
   a C ABI does not have to make.)
3. **Closest-unit queries are computed at the ABI boundary** over the sorted candidate set,
   tie-breaking on lowest ID. See "Determinism" below.
4. **Callback predicates are a conditional follow-on**, not a planned phase.

An enum-based mini-filter DSL (`BWAPI_FILTER_IS_WORKER`) is tempting and is **declined**: it
re-encodes upstream semantics in a switch statement that silently rots. R2 proposed shipping the
~26 filters bots use as constants; this paragraph is why not.

#### Determinism, and why sorting is not enough

Sorting collection output (§4) fixes the set-returning queries. It cannot reach the
**closest/best** queries, which return one unit and break ties by iteration order *inside*
`Templates::iterateUnitFinder` — so the exact calls a bot uses to pick a target would stay
nondeterministic while everything around them was fixed. That is the worst of both worlds.

**Implement the closest queries at the ABI boundary.** Keep the native rectangle and radius
queries so the `unitFinder` spatial index is preserved, then compute the minimum ABI-side over
the sorted candidate set, tie-breaking on lowest ID. Distance comparison is not game rules, so
this does not violate goal 6 — and there is no cleverness being discarded:
`Game::getClosestUnit` is literally `getClosestUnitInRectangle` over a bounding box
(`bwapi/BWAPILIB/Source/Game.cpp:717-725`), with no expanding-ring search. One extra pass over a
small candidate list buys reproducibility, which in a competitive-AI framework is worth more.
Recorded in §15.

**`getBestUnit` is not exposed at all.** It is meaningless without a `BestFilter`, and the
callback mechanism that would supply one is a conditional follow-on. This is a removal, not a
deferral.

#### What a predicate may call, if callbacks ever land

The rule is derived, not asserted. All three filtered queries — `GameImpl::getUnitsInRectangle`,
`getClosestUnitInRectangle`, `getBestUnit` (`bwapi/BWAPIClient/Source/GameImpl.cpp:475-543`) —
hold only **function-local** state, and `Templates::iterateUnitFinder`
(`bwapi/Shared/Templates.h:69-152`) keeps its scratch in a **local** `std::unordered_map` while
iterating shared-memory arrays the server rewrites only during `update()`. No global or static
scratch, no `GameImpl` member container mutated. So re-entering the ABI is not inherently
unsafe; three specific categories are.

The spec flag is **`reentrant: forbidden`**, not `mutates:` — because what is being gated is not
"mutates game state". Drawing does not mutate game state; it appends to `data->shapes[]`.
`bwapi_client_disconnect` mutates nothing; it frees `BroodwarPtr`. The three forbidden
categories are:

| Category | Members | Why |
|---|---|---|
| **Shared-memory writes** | drawing, `printf`, `send_text`, `enable_flag`, `set_local_speed` | Append to `data->shapes[]` / `commands[]` / `unitCommands[]`, bounded only by `assert` — no runtime check in release |
| **Command-queue writes** | every unit command | `UnitImpl::issueCommand` runs `Command{cmd}.execute()` (`BWAPIClient/Source/UnitImpl.cpp:59`), and `CommandTemp::execute()` writes into `unit->self->order`, `->target`, `->isConstructing` … (`include/BWAPI/Client/CommandTemp.h:196-214`) — the very `UnitData` the enclosing query is filtering on |
| **Lifecycle** | `connect`, `update`, `disconnect`, `bwem_initialize`, `bwem_reset` | `Client::disconnect()` deletes `BroodwarPtr` while the in-flight query still holds `this`. Use-after-free |

Everything else — every read-only accessor, static type data, scalar tile queries, all of
BWEM's reads — is **allowed**. Nested queries are allowed but discouraged (memory-safe, each
builds fresh local state, but O(n) inside O(n)).

Two refinements: **save and restore `lastError` around every predicate invocation** — most of
BWAPI's read-only surface calls `setLastError`, every `canXxx` does, so a predicate clobbering
the enclosing query's error is the normal case; and **fire the log callback at warn level on
every rejected re-entrant call**, in addition to latching the ABI error.

The guard itself is a thread-local depth counter; mutating wrappers check it and fail with
`BWAPI_ERR_REENTRANT_MUTATION` and no side effect. **Carry the `reentrant` flag in the spec from
phase 1 regardless of whether callbacks land** — it is free metadata, it feeds `api.json` and the
generated docs, and retrofitting a per-entry flag across 770 entries later is not free. Emit the
guard only if callbacks land.

### 5.5 Bulk map data

`isWalkable` is a `bool[1024][1024]`; `isBuildable`/`isVisible`/`isExplored`/`hasCreep` are
`bool[256][256]`; `getGroundHeight` is `int[256][256]` (`GameData.h`). Per-cell FFI calls mean
up to **1,048,576 calls per frame** for a walkability sweep. That would make the ABI look slow
when the underlying library is not.

Give every one of these both forms:
```c
int32_t bwapi_game_is_walkable(int32_t wx, int32_t wy);                 /* scalar */
int32_t bwapi_game_copy_walkability(uint8_t* out, int32_t cap,
                                    int32_t* out_w, int32_t* out_h);    /* bulk */
```

- **Row-major, index `y * width + x`.**
- **Cropped to the live map**, not the full backing array: `mapWidth()*4 × mapHeight()*4` for
  walk tiles, `mapWidth() × mapHeight()` for tile grids. On a 128×128 map that is 16× less data
  than copying the whole 1024×1024. `out_w`/`out_h` report the cropped extent.
- Boolean grids are `uint8_t` per element (§4); `getGroundHeight` stays `int32_t` per element,
  matching its source array.

A borrowed-pointer variant is possible (the data lives in mapped shared memory), but a **copy**
is the right default: a borrowed pointer's validity window across `update()` is a subtle
contract to get wrong, and it would hand a foreign language a writable view of the server's own
memory. Revisit only with a measured need.

### 5.6 Events

`Broodwar->getEvents()` returns a **`std::list<Event>`**, and `BWAPI::Event` holds a heap
`std::string*`. Indexing a `std::list` directly would be O(n²) over a frame's events, so **the
event list is snapshotted into a vector during `bwapi_client_update()`, and indices are stable
until the next one.**

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

No `text_len` field — `bwapi_game_event_text()` already returns the needed length under the §4
string convention. Polling rather than callbacks: it is the natural client-mode shape, avoids
re-entrancy entirely, and every host language can build its own callback dispatch on top.

The same event pump is where the ABI drives BWEM's three destruction hooks internally (§8), so
a host that never calls them still gets a correct map.

### 5.7 Unit-set broadcasts

`Unitset` has 48 broadcast methods (`unitset.attack(pos)` → all units). Rather than reproducing
them, expose the ID-array form for the ~10 that matter:
`bwapi_units_attack_position(const int32_t* ids, int32_t n, int32_t x, int32_t y, int32_t queued)`.
The rest are host-language `for` loops.

### 5.8 Static type data — the product

`UnitType::maxHitPoints()` and friends (185 accessors across `UnitType`, `WeaponType`,
`TechType`, `UpgradeType`, `Race`, `UnitSizeType` and the rest) are **pure functions of an
`int`** — no game instance needed. A host language builds its complete static type table at
startup with zero game connection, and they are testable without StarCraft.

They are also, measured, the most-used block in the API by density — 80 accessors at 1,671 call
sites, 20% of all BWAPI traffic — and the block whose absence has cost the most: rsbwapi's
`unit_type.rs` is 20,341 lines, gobwapi's tables 4,261, SparCraft's and BOSS's 1,119 each, the
Zig bot's 650. Five hand transcriptions, at least two wrong. `bwapi-c` shipped none of it and
its own example bot is reduced to `case 7: // SCV`. **This block is the product.**

The few that return containers need out-buffers: `requiredUnits()` → `std::map<UnitType,int>`
→ parallel `(type[], count[])` arrays; `whatBuilds()` → `std::pair` → two out-params;
`abilities()`/`upgrades()`/`buildsWhat()` → ID arrays.

**Shipped three ways.** The 185 accessors ship as **functions**, because that is what the 1,671
call sites are written against and it is the only form that carries upstream's semantics rather
than a snapshot of them. The **848 constants** ship generated into `bwapi_c2_types.h` — every
`UnitTypes::Enum`, `Orders::Enum`, `TechTypes::Enum`, `UpgradeTypes::Enum`, `WeaponTypes::Enum`,
`Races::Enum`, `Errors::Enum`, `EventType::Enum`, `Flag::Enum`, `CoordinateType::Enum`,
`Text::{Enum,Size::Enum}`, `MouseButton`, `Key`, `Latency::Enum`, `Colors`, and the position
sentinels in both forms; names come from the enum identifiers, so they cannot drift. And **one
size-prefixed bulk table per type class** ships as an optional §5.10-style fast path, so a host
that would rather pay one crossing at startup than 185 can have it. Declined: shipping *only*
the table — that is what rsbwapi does because it has no ABI to ask, and it forces every host to
re-derive accessor semantics the ABI already knows.

### 5.9 Explicitly not exposed

| Not exposed | Why |
|---|---|
| `Interface<T>::registerEvent` | Host closures do this better |
| `get/setClientInfo` | Host hash maps do this better. Also the one place in the closure with a pointer↔`int` cast (§1.5) |
| `GameWrapper` / `Broodwar << …` | C++ streams. `Streams.cpp` is excluded from the build (§10.1) |
| `getBestUnit` | Meaningless without a `BestFilter` (§5.4). A removal, not a deferral |
| `canXxxGrouped`, `checkCommandibility` | §5.11, §15 #17–18 |
| Filter-taking query overloads | §5.4 |
| `Type::getType(string)` name lookup | Conditional follow-on |
| `TournamentModule` | Module-mode-only (Appendix A) |
| BWEM mutators, internals, `Graph`, `GridMap`, `UserData`, `Markable`, `MapDrawer`, `MapPrinter`, `BreadthFirstSearch` | §8, §15 #15 |

### 5.10 Per-frame snapshots

§5.5 refuses per-cell FFI for map data — and then per-unit getters push the call count back up,
for exactly the languages that pay most per crossing. Apply the same fix where it matters most.

```c
int32_t bwapi_game_snapshot_units(bwapi_unit_snapshot* out, int32_t cap);
int32_t bwapi_game_snapshot_players(bwapi_player_snapshot* out, int32_t cap);
```

Same convention as every other collection (§4): fills up to `cap`, returns the total, sorted
ascending by ID, **existing units only**, `cap == 0` with `NULL` is the size query, and element
zero's `size` is the uniform stride. `UnitData` is already pointer-free, so this is a
field-select copy loop, not new logic. (`last_command_frame` is the one field that comes from
the interface rather than `UnitData` — it is a client-side `UnitImpl` member.)

**Booleans in the snapshot are bits in a `uint32_t flags`, not fields**: `exists`,
`is_completed`, `is_constructing`, `is_idle`, `is_moving`, `is_attacking`, `is_cloaked`,
`is_burrowed`, `is_stuck`, `is_under_attack`, `is_morphing`, `is_selected`, `is_powered`,
`is_visible_to_self`. This is not the §4 scalar-bool rule — that governs parameters and returns
— and a bit is also how a future boolean gets added without disturbing the layout, which pairs
with the size prefix.

v1 unit fields: `size`, `id`, `player_id`, `type`, `x`, `y`, `hit_points`, `shields`, `energy`,
`resources`, `resource_group`, `order`, `order_target_id`, `secondary_order`, `target_id`,
`build_type`, `remaining_build_time`, `remaining_train_time`, `training_queue_count`,
`addon_id`, `transport_id`, `carrier_id`, `hatchery_id`, `ground_weapon_cooldown`,
`air_weapon_cooldown`, `spell_cooldown`, `last_command_frame`, `flags`, then
`double angle, velocity_x, velocity_y`.

Player snapshot stays scalar-only: `id`, `race`, `type`, `color`, `start_x`, `start_y`,
`minerals`, `gas`, `gathered_minerals`, `gathered_gas`, `supply_used[3]`, `supply_total[3]`,
`flags`. **Upgrade and tech levels stay as scalar calls** — 63 upgrades × 12 players copied every
frame, to serve values that change a handful of times per game, is the wrong trade.

The snapshots are **additive**. Per-unit getters remain, because the `canXxx` family and the
derived queries have no snapshot form.

**What the precedent does and does not say.** BWAPI4J batches per-frame data into a single array
copy for exactly this reason, and reached it under measurement — but BWAPI4J is an in-process
JNI bridge with an FFI boundary. JBWAPI and rsbwapi are pure clients that read the mapped region
*in place* and copy nothing; JBWAPI's headline performance claim is that this beats marshalling
by a large factor. So §5.10 is **compensation for a per-call boundary this design deliberately
reintroduces in exchange for BWAPI's real implementation** — a trade, not a free lunch, and the
precedent is for boundary-having designs specifically. Stated that way it survives a reader who
knows both projects.

The consequence for §5.4 is the important one: once the host has the frame's units as a typed
array, "filter inside the query" stops being interesting except for genuinely early-exiting
spatial searches — and §5.4 computes those at the boundary. Between the two, the remaining case
for running foreign code inside a BWAPI query is empty.

### 5.11 The `canXxx` family

`Unit.h` and `Game.h` declare 105 `canXxx` functions under 68 unique names: 88 declarations
under 57 names are base predicates, 17 under 11 names are `*Grouped`. **All 88 base
declarations ship**, one export per distinct signature — `canAttack()`,
`canAttack(Position)` and `canAttack(Unit)` are three functions with three meanings.

Two exclusions, both in §15. **`*Grouped` is not exposed**: grouped commands are not
implemented by the BWAPI server at all, so a client bot cannot use them in any language, and
exporting the predicates would advertise a capability that does not exist in client mode
(Appendix A). **`checkCommandibility` is not exposed**: it is a default argument, not an
overload, so suppressing it costs no declarations; every `can_*` behaves as `true`. Passing
`false` skips the "can this unit accept commands at all" precheck — an optimisation for chaining
several predicates on one unit, which would cost a boolean on every signature and is a footgun
across a boundary where the precondition cannot be enforced.

Declined: collapsing the no-target and target-taking forms into one function with a
`BWAPI_NONE` target, the way §5.2 collapses draws. §4 already gives an invalid handle a neutral
return, so `-1` would be ambiguous between "no target supplied" and "bad handle", and the two
queries mean different things.

Usage was 29 of 68 names called anywhere in 125,000 lines of bot code. That is not a reason to
ship 29: a predicate exists precisely so a bot can ask a question it does not yet know it will
ask, and this family is where `bwapi-c`'s 530-name inventory earns its keep as a naming
checklist.

---

## 6. Handle model in detail

```c
typedef int32_t bwapi_unit;    /* Game::getUnit(id)   — O(1) */
typedef int32_t bwapi_player;  /* Game::getPlayer(id) — 0..11 */
typedef int32_t bwapi_force;   /* Game::getForce(id)  — 0..4 */
typedef int32_t bwapi_region;  /* Game::getRegion(id) */
#define BWAPI_NONE (-1)
```

Distinct typedefs (even though all are `int32_t`) so generated wrappers can newtype them and
catch a unit ID passed where a player ID belongs. BWEM adds `bwapi_bwem_area`,
`bwapi_bwem_choke` and `bwapi_bwem_base` (§8); neutrals are `bwapi_unit`.

### 6.1 Resolution

One internal helper per kind:
```cpp
inline BWAPI::Unit resolve(bwapi_unit id) {
  if (id < 0 || !BWAPI::BroodwarPtr) return nullptr;
  return BWAPI::BroodwarPtr->getUnit(id);
}
```
Every generated wrapper begins with a resolve-and-guard. Cost is a bounds check plus a vector
index — negligible next to the virtual call that follows.

### 6.2 Validity, and legitimate `BWAPI_NONE`

`bwapi_unit_exists(id)` maps to `UnitInterface::exists()`. Handles need no explicit release:
they are indices into game-owned storage, nothing is retained, nothing leaks.

**The header enumerates the functions that legitimately return `BWAPI_NONE`** — among them
`bwapi_game_self` and `bwapi_game_enemy` in a replay or an observer slot,
`bwapi_unit_get_target` when idle, `bwapi_unit_get_addon` on a building without one,
`bwapi_unit_get_transport` when not loaded, `bwapi_unit_get_build_unit` when not building.
Without that list, `BWAPI_NONE` is indistinguishable from a rejected handle unless the caller
reads the error channel after every single call — which is precisely the second crossing §4
designed away.

**Zero is not "none" anywhere in BWAPI.** Every unit-index field in `UnitData` — `transport`,
`target`, `orderTarget`, `buildUnit`, `addon`, `carrier`, `hatchery`, `rallyUnit`,
`lastAttackerPlayer` — uses `-1`, and a zero is a *valid* index into unit 0. R7's fixture found
this the hard way (a `calloc`'d SCV believed it was loaded inside unit 0 and refused to move);
§11's fixture builder encodes it once.

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

The C ABI lives in its own repository, `bwapi-c2`, and consumes BWAPI and BWEM as pinned
submodules. It is a purely additive layer: nothing here requires a change to either tree, so
nothing here belongs in them.

```
bwapi-c2/                           # separate repository, LGPL-3.0-only
  COPYING  COPYING.LESSER  LICENSE.BWEM  NOTICE      # §0
  third_party/bwapi/                # git submodule, pinned revision
  third_party/bwem/                 # git submodule, pinned; do NOT recurse its submodules
  patches/bwem-reset-instance.patch # §15.2 — re-applied at pin bump
  include/
    bwapi_c2.h             # generated — BWAPI
    bwapi_c2_bwem.h        # generated — BWEM
    bwapi_c2_types.h       # generated — 848 constants + PODs
  src/
    abi.cpp                # version, sticky error channel, log callback, noexcept boundary,
                           #   the in-predicate depth counter (§5.4)
    client.cpp             # connect / update / disconnect (with BWEM teardown) / is_connected
    handles.cpp            # resolve+validate helpers; the BWEM base-id table
    game.gen.cpp  unit.gen.cpp  player.gen.cpp  force_region.gen.cpp   # generated
    types.gen.cpp          # generated — static type data + bulk tables
    bwem.gen.cpp           # generated — BWEM reads
    bulk.cpp               # hand-written — map grids, collections, events, snapshots
    commands.cpp           # hand-written — UnitCommand, unit-set broadcasts
    closest.cpp            # hand-written — boundary-side closest queries (§5.4)
    bwem_lifecycle.cpp     # hand-written — initialize/reset, the three filtered hooks (§8)
  vendor/svnrev.h          # generated by upstream's script at pin-bump time (§10.3)
  tools/abi/
    draft_spec.py          # clang AST dump → first-draft YAML (§9)
    spec/{game,unit,player,types,bwem,...}.yaml   # the source of truth
    emit_header.py  emit_source.py  emit_json.py  emit_def.py
    check_coverage.py      # libclang audit, off the merge path (§9)
  api.json                 # generated — the downstream binding contract
  bwapi_c2.def             # generated, checked in — the golden symbol artifact
  tests/
    header_hygiene/        # C99 + C++ compile checks
    layout_dump/           # emits GameData field offsets as JSON; x86 and x64 (§10.2)
    derive_closure/        # builds the closure and links upstream's ExampleAIClient (§10.1)
    fixture/               # the synthetic-GameData builder shared by every suite (§11)
    types_test.cpp         # ~500 static-type assertions, no game needed
    read_write/            # BWAPI read path, rules, command emission on fixtures
    bwem/                  # BWEM full analysis on synthetic terrain
    errors/                # malformed fixtures: bad handles, truncation, latch
    transport_fake/        # ~150 lines: game table, mapping, pipe handshake — no semantics
    fuzz/                  # boundary fuzz harness generated from the .def
  bindings/
    python/                # raw ctypes layer, generated from api.json
    csharp/                # raw P/Invoke layer, generated from api.json
    rust/bwapi-c2-sys/     # raw FFI — the proof-of-concept consumer
  examples/{c-example-bot,python-example-bot,csharp-example-bot}/
  CMakeLists.txt
```

### What lives here, and what doesn't

`bindings/` holds the **raw FFI layer only**: `extern` declarations, constants, build glue,
nothing more. Keeping it here means it is generated from the same `api.json`, versioned with the
ABI, and regression-tested in one CI run against the fixtures. **Two raw bindings consuming
`api.json` in CI are the test of `api.json`'s stability** — that is why there is no separate
JSON Schema.

The **idiomatic, safe, host-language wrapper** for each language lives in its own repository,
released on that ecosystem's cadence: a `bwapi-c2` package on PyPI, `BwapiC2` on NuGet, a
`bwapi-c2` crate. Those are where `Unit` newtypes, exceptions, iterators and async integration
belong, and they should not be gated on this repo's release process.

**Python and C# are the primary consumers; Rust is the proof-of-concept.** Python and C# are
the two languages with the strongest measured unserved demand (§1.9). Rust is served by rsbwapi
and stays here as the cheapest proof that the ABI is bindable — with Styx2's usage as a
baseline — and because a Rust binding over this ABI has enough genuine advantages over rsbwapi
(latency compensation, `getBuildLocation`, the real rule engine) that building one downstream is
reasonable; it is just not this repository's product. No JavaScript attempt has ever existed;
`api.json` is there when one does.

`*.gen.cpp`, `api.json`, `bwapi_c2.def` and `vendor/svnrev.h` are **checked in**, not built by a
codegen step at compile time. Contributors without Python (or Windows Script Host) still get a
buildable tree, diffs are reviewable, and CI verifies that regenerating produces no change.

---

## 8. The BWEM header

`bwapi_c2_bwem.h` wraps `N00byEdge/BWEM-community` — the maintained fork, MIT/X11, 5,222 lines,
last commit 2021 — under every §4 convention, unchanged. R11.3's headline is the design result:
**no new convention was required.** §4 was designed against one library and holds against a
second with a different object model. Six divergences are recorded in §15.1, and every one is an
application of a §4 rule rather than an exception to it.

**Why in scope.** Both maintained competitors ship a BWEM port — JBWAPI 5,338 lines, rsbwapi
2,496 — because bots need map analysis; gobwapi started one within a month of existing. All three
**re-run the analysis in the host language**, and JBWAPI's tracker shows the divergence trap in
its BWEM-specific form: a hand-maintained checklist of individual maps that break. A C ABI over
`BWAPI::Game` exposes none of BWEM because BWAPI does not contain it, so without this header
every host ecosystem wraps or reimplements it separately.

**What it costs.** Ten BWAPI symbols — three `drawXxxMap` helpers from `BWAPILIB`, three
client-mode virtuals (`isWalkable`, `isBuildable`, `getGroundHeight`), and four `UnitType`
accessors — all already in the closure, one from each of its three parts. Fourteen
translation units compiling with the flags R6 already needs, introducing no new MSVC-isms.
327 KB. BWEM takes the game as an explicit `BWAPI::Game*` and reads no globals, so it composes
with a closure that already owns a `GameImpl`. Not a phase, not a DLL: 14 TUs in the same CMake
target and 98 more generated wrappers.

### 8.1 The surface

Of 279 declarations in BWEM's headers, **98 export**: 85 scalar read accessors, 7 lifecycle entry
points, and 6 bulk grids. 42 mutators and internals and 49 declarations on non-bot-facing classes
(`Graph`, `MapImpl`, `MapPrinter`, `MapDrawer`, `TempAreaInfo`, `GridMap`, `Markable`,
`UserData`, `BreadthFirstSearch`) have **zero** call sites across 119,000 lines of bot code and
are not merely unused but unnecessary, since no host runs the analysis. The eleven names all
three ports agree on are all covered. Full signatures: [research/r11/bwapi_c2_bwem.h.sketch](research/r11/bwapi_c2_bwem.h.sketch).

| BWEM shape | ABI shape |
|---|---|
| `Area::id` (`int16_t`, 1-based; 0 = not walkable) | `int32_t` handle, BWEM's own numbering; `BWAPI_BWEM_AREA_NONE` is 0 |
| `ChokePoint::Index()` | `int32_t` handle, BWEM's own numbering |
| `Base` — **no identity at all**, stored by value in `Area::Bases()` | **Synthesised** flat `int32_t`, 0..`BaseCount()-1`, table owned by the ABI (§15 #11) |
| `Neutral*`/`Mineral*`/`Geyser*`/`StaticBuilding*` | **The BWAPI unit id** — the join between the headers (§15 #12) |
| `Neutral::IsMineral()`/`IsGeyser()`/`IsStaticBuilding()` | One `bwapi_bwem_neutral_kind()` discriminator |
| `Map::GetPath(a, b, int*)` → `const CPPath&` | Caller buffer of choke ids + true count, **plus `int32_t* out_length`** — BWEM's out-param survives |
| `ChokePoint::GetAreas()` → `pair` | Two `int32_t*` out-params |
| `ChokePoint::Geometry()` → `deque<WalkPosition>` | Caller buffer of **packed `int64_t`** |
| `ChokePoint::Pos(node)` | `int32_t node` + three `#define`s |
| `Tile`/`MiniTile` reads | Scalar `(tx,ty)`/`(wx,wy)` accessors first — the shape every port shipped; grid access is ~2% of BWEM traffic (§15 #16) |
| `Map::Tiles()`/`MiniTiles()` | **Optional** §5.5 bulk grids, six of them; `int16_t` for area ids and altitudes |
| `altitude_t` (`int16_t`) | `int32_t` in signatures, `int16_t` in grids |

### 8.2 Lifecycle

```c
int32_t bwapi_bwem_initialize(int32_t enable_path_analysis,
                              int32_t find_bases_for_start_locations);
int32_t bwapi_bwem_initialized(void);
void    bwapi_bwem_reset(void);
void    bwapi_bwem_on_mineral_destroyed(int32_t unit_id);
void    bwapi_bwem_on_static_building_destroyed(int32_t unit_id);
void    bwapi_bwem_on_blocking_neutral_destroyed(int32_t unit_id);
```

**One call replaces three.** `Initialize` → `EnableAutomaticPathAnalysis` →
`FindBasesForStartingLocations` has an ordering dependency and no use case for the intermediate
states; an ABI should make ordering errors impossible rather than diagnosable (§15 #13). It
blocks for **~450 ms** on a 128×128 map, all of it at match start; BWEM has nothing per-frame.

**Ids are stable from `initialize` to the next `initialize`.** `OnMineralDestroyed` erases a
mineral from lists and never renumbers areas, chokepoints or bases; a blocking neutral's
destruction changes a `GroupId`, not an `Id`. A neutral's id outlives its unit briefly — BWEM
keeps the `Neutral` until the hook runs — and `bwapi_bwem_neutral_exists()` covers the gap.
Nothing carries across a match boundary.

**The three hooks are filtered and idempotent, and the ABI drives them itself.**
`MapImpl::OnMineralDestroyed` does `bwem_assert(found)` and therefore *throws* when handed a unit
BWEM does not track — which is exactly what a host forwarding every `onUnitDestroy` does on the
first dead marine. So the event pump (§5.6) dispatches `UnitDestroy` to BWEM with the right filter
once, correctly, and the three explicit entry points stay in the header for hosts that want
control, made safe to call with any unit id (§15 #14). These are the first genuinely
non-mechanical wrappers in either header, and §9 names them as hand-written.

**Teardown is explicit, and `bwapi_client_disconnect()` performs it.** BWEM's singleton points
into `GameData` and must die before the mapping does and before static destruction. That is the
first required shutdown order in the ABI, and the reason is a crash R11.6 found rather than a
preference:

**`bwapi_bwem_reset()` depends on a patch we carry.** `MapImpl::Initialize` resets in place with
`this->~MapImpl(); new (this) MapImpl();`, and `~Neutral` calls `RemoveFromTiles()`, which
reaches back into the Map's already-destroyed tile storage. Re-initialisation therefore
segfaults on any map with neutrals — every map — and the same root cause crashes at static
destruction. The fix is `void Map::ResetInstance() { m_gInstance = nullptr; }`, which Stardust's
vendored BWEM already carries. It is recorded in §15.2, re-applied at every pin bump (§10.3),
and offered upstream without gating on a repository dormant since 2021. JBWAPI's #51 —
`IllegalStateException` on consecutive games — is plausibly this bug inherited through the port.

### 8.3 Exceptions

BWEM throws from assertions compiled into release builds (§1.7). Every `bwapi_bwem_*` export is
the §4 `noexcept` boundary, latching `BWAPI_ERR_BWEM` and routing `Exception::what()` into
`bwapi_last_error_message()`. JBWAPI's #34 ("BWEM has no areas", an intermittent crash in 1 in
10–20 games) is what this turns into a latched error and a neutral return.

---

## 9. Codegen: spec-driven, drafted by clang, audited by libclang

**Why a generator, restated.** Not because 770 wrappers are too many to type — `bwapi-c` typed
530 and they work. Because §1.8's mapping from 542 declarations to ~457 interface exports goes
through five exclusion rules, §5.8's 1,033 entries must come from a machine-readable source or
every consumer transcribes them again, and only a coverage audit run off a spec can show the
first is complete and the second is exact. The generator is the deliverable; the wrappers are
output.

**Rejected: parse the headers and emit directly.** The headers use MSVC extensions, `#pragma
warning`, and heavy templates; a parser-driven generator is brittle and, worse, would silently
change the public ABI when an upstream header is edited. ABI stability has to be a deliberate act.

**Rejected: SWIG's C target — on measured grounds, not the ones revision 3 gave.** `swig -c`
does emit an ISO C interface rather than a per-language C++ layer, so revision 3's objection was
wrong on its facts. Tested against BWAPI at SWIG 4.5.1 (R8), it is rejected on five reproducible
grounds:

1. **Still experimental.** SWIG's own `-help` lists `-c` under "Experimental Target Language
   Options" and emits Warning 524 on every invocation.
2. **`Game.h` does not compile.** Twelve vararg functions are hard errors; suppressing them
   deletes `drawTextScreen`, `drawTextMap`, `printf` and `sendText` — 463 call sites in R2's
   corpus.
3. **Namespaced enums lose their namespace**, so no two BWAPI type modules can be included in
   one translation unit (`redeclaration of enum Enum`). This alone forecloses §5.8.
4. **The `Type<>` and `Interface<T>` CRTP bases are dropped**, taking `getID`, `getName` and
   `c_str` with them. A C caller cannot convert a `UnitType*` to an integer.
5. **Every by-value return is heap-allocated with `new` and never freed**, across the DLL
   boundary — the exact failure §4's caller-buffer rule exists to prevent.

Typemaps *do* express packed positions, `int32_t` booleans and integer handles correctly, and
that is worth conceding. They cannot express caller-provided buffers, sorted output,
size-prefixed structs or the sticky latch, because those change a function's *arity* and a
typemap only transforms values. Reaching them requires `%extend` — hand-written C++ per function
— at which point SWIG contributes name mangling and an experimental dependency, and nothing else.

Survey result, so nobody re-runs it: CppSharp targets C#; cppbind Swift/Kotlin/Python; AutoWIG
high-level languages; SharpGenTools C#. **No mature tool emits a flat C ABI from C++ except
SWIG's C module.** CastXML, pygccxml and c2ffi are introspection front-ends with no emission.

**Rejected: hand-write everything.** Guaranteed inconsistency in exactly the details — buffer
semantics, invalid-handle behaviour, error latching, the `noexcept` boundary — that matter most.

**Recommended: a checked-in YAML spec, drafted by clang, emitted by Python, audited by libclang.**

**The first draft is not hand-typed.** `clang++ -Xclang -ast-dump=json -Xclang
-ast-dump-filter=<class> -fsyntax-only` recovers every method with full qualified types — all 81
`UnitType` methods from a 20-line walker (R8 §9) — with no new dependency: it is the same clang
that R5's layout dump and R6's build already require. `draft_spec.py` turns that into YAML; a
human then applies the §1.8 rules and adds the `body:`, `skip:` and `reentrant:` fields.

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

- cpp: "UnitInterface::canAttackGrouped"
  skip: "grouped commands are client-mode-impossible; §15 #17"

- cpp: "BWEM::ChokePoint::Center"
  c:   "bwapi_bwem_choke_center"
  self: bwem_choke
  returns: position             # packed WalkPosition
```

A handful of return kinds covers nearly everything: `int32`, `bool32`, `double`, `type`
(→`int32`), `position` (→ packed `int64`), `handle`, `string_out`, `id_array`, `void`. Anything
unusual carries an inline `body:`. **Every `skip:` names the rule that justifies it** — that is
how the coverage audit proves the §1.8 mapping.

Emitters produce `bwapi_c2.h`, `bwapi_c2_bwem.h`, `bwapi_c2_types.h`, the `*.gen.cpp` files,
`bwapi_c2.def`, and **`api.json`** — the machine-readable description that the Python, C# and
Rust raw layers consume, so binding authors never re-parse C.

**Hand-written, and named as such so the coverage claim stays honest:** the bulk grids,
collections, events and snapshots (`bulk.cpp`); `UnitCommand` and broadcasts (`commands.cpp`);
the boundary-side closest queries (`closest.cpp`); and BWEM's lifecycle and three filtered hooks
(`bwem_lifecycle.cpp`). Each has a spec entry with a `body:` so it appears in `api.json` and the
`.def` like everything else.

Three rules that make the spec trustworthy:

**`cpp:` strings must resolve to exactly one declaration.** `"UnitInterface::attack(Position,
bool)"` is overload resolution by string match; assert uniqueness in the generator or expect to
bind the wrong overload eventually.

**`body:` entries still declare full types, and CI emits a `static_assert` that the referenced
C++ overload exists with the declared signature.** Otherwise an opaque C++ body is a hole in both
the coverage audit and `api.json`.

**The `.def` file is the golden symbol artifact.** Generate it, check it in, and have CI assert
`dumpbin /exports` matches. Assign no ordinals; binding is by name only.

What revision 3 had and this revision does not: per-entry declaration hashes, a JSON Schema
with a `schema_version`, and a required `divergence:` field on every entry. All three guarded
against motion in a dependency that has not moved meaningfully in years, and were over-priced.
The coverage audit diffs signatures at pin bumps; two generated bindings consuming `api.json`
in CI are its compatibility test; §15 is a table, not a per-entry field.

### The coverage audit runs off the merge path

`check_coverage.py` parses the BWAPI and BWEM headers with libclang and reports any public
declaration with neither a spec entry nor a rule-bearing `skip:`, and any spec entry whose
declared signature no longer matches. Its real value is a **one-time completeness audit** —
proving the spec accounts for all 542 + 279 declarations — plus a re-run at pin bumps.

It is **not** a per-PR gate. Parsing `v141_xp` headers and 3,098 lines of `Templates.h` needs
`-fms-extensions -fms-compatibility -fdelayed-template-parsing` and a pinned LLVM; that is a
fragile gate to put in front of every merge. Run it in the §10.3 checklist and on demand.

---

## 10. Build and packaging

### 10.1 The link closure — derived, and now built

**CMake is the only build system.** BWAPI comes in as a pinned submodule at `third_party/bwapi`,
BWEM at `third_party/bwem` (**without recursing its submodules** — they include googletest and an
unlicensed OpenBW, all test-only).

`CMake/README.md` documents the supported consumption path — `CMake/BWAPI/` and `CMake/Client/`,
then link `BWAPI-Static` and `BWAPIClient`. That path is wrong in both directions: `CMake/Client`
is too little (no `BWAPILIB`, so no type tables or non-virtual helpers) *and* too much (it
compiles Storm and seven `Util` TUs that nothing needs), and `BWAPI-Static` drags in `BW/*`,
`Detours.cpp`, `CodePatch.cpp`, Storm, and the whole injected-DLL source set.

**So the closure was derived by building it** (R6, `derive-closure.sh`, which becomes a CI
job): 44 translation units, and upstream's own `ExampleAIClient` links against the result and
runs to `Client::connect()`.

| | Verdict |
|---|---|
| `BWAPILIB/Source/*.cpp`, `BWAPILIB/UnitCommand.cpp` | **In** — type tables, non-virtual `Game`/`Unit` helpers |
| `BWAPIClient/Source/*.cpp` | **In** — `Client`, client `GameImpl`/`UnitImpl`/… |
| `Shared/*.cpp` | **In** — `Templates.h` and the `*Shared.cpp` implementations |
| `BWEM/src/*.cpp` (14 TUs) | **In** — needs ten BWAPI symbols, all already above (R11.4) |
| `BWAPILIB/Source/Streams.cpp` | **Out** — the sole definition of `bwout`/`bwerr`/`out`/`err`, which §5.9 excludes and nothing in the closure references. **Not** because of Boost: Boost has been `#if 0`'d out of the client path since commit `109ff28a` (April 2017). Note that a C++ consumer of the archive would find those four symbols missing; ours is a C shim, so it does not matter |
| `BWAPILIB/Source/BroodwarOutputDevice.cpp` | **Out** — entirely inside `#if 0`; compiles to an empty object |
| `Util/Source/**` | **Out** — nothing in the client path includes it. `RemoteProcess`, `MemoryFrame`, `SharedMemory` and `Exceptions` are injected-DLL infrastructure |
| `Storm/**` | **Out** — `storm.cpp` is 223 lines of no-op stubs so the *injected* DLL links without the real `storm.dll`. Also the only project setting `/Zp` (`1Byte`), so excluding it protects §1.4 |
| generated `include/svnrev.h` | **In** — `BWAPI.cpp` includes it, and it pulls `starcraftver.h`, which defines `BUILD_DEBUG` (§10.3) |

**Storm's exclusion is the finding that mattered**, because Storm is the game's own 32-bit
component and, had the client path genuinely needed it through an import library, x64 would have
been dead before a single layout was computed. It does not. Seven Win32 imports remain, all in
`Client.cpp` — the transport, and nothing else (Appendix B).

**The include-directory list is load-bearing and revision 3 omitted it.** `Shared/*.cpp` include
their Impl headers unqualified (`#include "UnitImpl.h"`) and resolve them against
`include/BWAPI/Client/`; without that directory six TUs fail outright.

```
-I<bwapi>/include
-I<bwapi>/include/BWAPI/Client     # the one that is easy to miss
-I<bwapi>/Shared
-I<bwapi>/BWAPIClient/Source
-I<generated svnrev.h dir>
-I<bwem>/include  (or wherever the pin puts BWEM's headers)
```

**Two MSVC-isms need workarounds on any other compiler**, both upstream bugs worth reporting:
`CommandTemp.h:34`'s two-phase lookup (`-fdelayed-template-parsing`; clang only) and
`Convenience.h:33`'s `va_list&` (change to `va_list ap`; carried as a patch on the pin alongside
BWEM's, §15.2). Neither affects the symbol closure.

**Use explicit source file lists, never globs.** A private build of upstream's sources is
untested by upstream. An explicit list turns a new file in `BWAPILIB/Source/` into a loud link
error instead of a silent change in what gets compiled.

Two settings to carry over: `ADD_DEFINITIONS(/DNOMINMAX=1)`, and `BWAPI_CUSTOM_COMPILE_FLAGS` as
the documented hook. Statically link the CRT (`/MT`) — safe because no allocation crosses the
boundary (§4), and permitted for the two independent reasons in §0.

**Two upstream CMake bugs, both confirmed, neither a precondition:**
`CMake/BWAPI/CMakeLists.txt:258` references `BWAPILib/UnitCommand.cpp` while the directory is
`BWAPILIB` (broken on case-sensitive filesystems); and the same file declares its `svnrev.h`
custom-command `OUTPUT` as `${BWAPI_ROOT}/svnrev.h` while `revisionUpdate.vbs` writes
`include/svnrev.h`.

**If upstream declines a client-only CMake target, nothing changes**: the private target in
`bwapi-c2` stands indefinitely, and the §10.3 checklist absorbs keeping its file list current.
Goal 7 turns on that being true, so it is stated rather than assumed.

### 10.2 x64: settled

Revision 3 sequenced a provisional x64 verdict at phase 0 and a final one at phase 3, on the
grounds that static asserts prove self-consistency but not interop. **Both halves are now
answered, and neither needed a phase.**

- **Self-consistency is computed.** R5's generated layout dump, run across `i386` and `x86_64`
  for MSVC, MinGW and Linux: all four Windows targets are byte-identical at
  `sizeof(GameData) == 33,017,048`, with no bitness conditional anywhere in the closure. Linux
  x86-64 agrees; Linux i386 does not (§1.4) and is a non-goal.
- **Interop is proven in production.** JBWAPI hardcodes `SIZE = 33017048` and runs 64-bit JVMs
  against the 32-bit server in tournaments; rsbwapi's README tells users their x64 executable
  "should run fine in all current tournaments/ladders"; gobwapi asserts the same constant in a
  passing test. Both competitors *generated* their layout rather than hand-writing it — JBWAPI
  from `clang -fdump-record-layouts`, rsbwapi from bindgen — which is exactly the tooling below.

What remains is a cheap regression check, not a gate. **The layout dump** (`tests/layout_dump/`,
from R5's `run-layout-dump.sh`) compiles one TU x86 and x64, emits every field's name, offset and
size as JSON, and CI diffs both against the checked-in baseline — the detector for an upstream
`GameData` edit at pin bump. And **one grep** confirms no project in the closure sets `/Zp` or
`#pragma pack` (§1.4). Both run in seconds.

**The real x64 gate was always the link closure, not the layout** (§10.1), and the closure links.
`bwapi-c2-<ver>-win64.zip` ships from phase 0.

### 10.3 `svnrev.h`, patches, and the pin-bump procedure

**Generate `svnrev.h` with upstream's own `cscript.exe revisionUpdate.vbs` when the pin moves,
and check the result in.** The script computes `2383 + git rev-list HEAD --count` and writes
`include/svnrev.h`, which also `#include`s `starcraftver.h` (the definer of `BUILD_DEBUG`). A
synthesised header would make `bwapi_revision()` return a number that is not BWAPI's revision —
worse than not exporting it at all.

**There is no scheduled drift canary.** A nightly job against dependencies that do not move is
noise. Moving a pin is a deliberate act, so the work attaches to that act, and **there are now two
pins and two carried patches** (§15.2):

1. Move the submodule (`third_party/bwapi` or `third_party/bwem`).
2. Re-apply the carried patches; if one no longer applies, that is the first finding.
3. For BWAPI: run `cscript.exe revisionUpdate.vbs`; commit the generated `svnrev.h`.
4. Run `derive_closure` and the layout dumps; diff against the baselines.
5. Run `check_coverage.py`; resolve every added, removed or changed declaration.
6. Rebuild; run every suite in §11. Record the new revision and `CLIENT_VERSION` in the notes.

### 10.4 Distribution

`bwapi-c2` ships its own artifacts; it adds nothing to BWAPI's `Release_Binary/` or installer.

- Per-platform release assets — `bwapi-c2-<ver>-win32.zip` and `-win64.zip` — containing `.dll`,
  `.lib`, `.def`, the three headers, `api.json`, the §0 file table, and the Corresponding Source
  pointer to the exact tagged commit and both pinned dependency commits.
- Each release records the **pinned BWAPI revision** and `BWAPI::CLIENT_VERSION` (10003 today),
  so a consumer can tell which server versions a given `bwapi_c2.dll` speaks to. The client
  refuses to connect on mismatch (`Client.cpp:120`).
- `bindings/rust/bwapi-c2-sys` publishes to crates.io from this repo; the safe `bwapi-c2` crate,
  the PyPI `bwapi-c2` package and the NuGet `BwapiC2` package publish from their own repos (§7).
  All declare `LGPL-3.0-only` and carry the notices.

---

## 11. Testing

**There is no mock server, and no live game in CI.** Revision 3 budgeted a mock BWAPI server at
~300 lines; realistic sizing with scenario fixtures was 800–1,500, and R7 showed it would have
been *wrong* in at least three places a hand-written mock cannot know about. The substrate that
replaces it was demonstrated in working code before this revision was written.

**The substrate is a synthetic `GameData`.** A ~60-line harness `calloc`s a `GameData`, fills in
the fields a scenario needs, constructs the real client `GameImpl` over it, and drives the real
API: reads, BWAPI's actual `canMove`/`canCommand` rule engine returning real `Errors::` codes,
and command emission landing `{Move, unitIndex 0, x 1500, y 2500}` in `data->unitCommands`. No
server, no shared memory, no pipe, no MPQs, no StarCraft (R7 §7). The same substrate drives
**BWEM's full analysis** on thirty lines of hand-drawn terrain — two areas, one chokepoint, two
bases with four minerals each, a working `GetPath` across the wall (R11.6). JBWAPI reached the
same design independently: its `GameBuilder` is 48 lines. OpenBW was evaluated as an alternative
and is unusable in public CI — it needs Blizzard's MPQs, and the two projects that tried disabled
their workflows in 2022 (Appendix B).

**Fixtures are synthetic by policy** (§0). Recorded buffers are contributor-local and gitignored.
The cost of that policy is exactly the three things R7 and R11.6 found a synthetic fixture must
get right, so **one shared fixture builder encodes them once** rather than every suite
rediscovering them:

1. `UnitImpl`'s constructor reads the global `BWAPI::BWAPIClient.data`, not the `GameData*`
   handed to `GameImpl` — set it first or segfault.
2. Zero is a valid unit index; BWAPI's "none" is `-1` in every index field (§6.2).
3. `isPowered` and `isInterruptible` both gate `canMove`, and both default false.
4. Neutrals arrive via the `UnitDiscover` **event stream**, not `data->units`, and
   `PlayerImpl::isNeutral()` reads a `PlayerData` flag rather than the player type — so a
   fixture that wants BWEM bases must synthesise events too.

Ordered by value per unit of effort:

| Layer | Substrate | Cost |
|---|---|---|
| **Header hygiene** — `bwapi_c2.h` and `bwapi_c2_bwem.h` compiled standalone as C99 (`/TC`), as C++, and twice in one TU | none | ~0 |
| **Golden `.def` diff** — `dumpbin /exports` matches the checked-in file (§9) | none | ~0 |
| **Layout dumps** and the **derived closure**, as CI jobs (§10.1, §10.2) | none | seconds |
| **Static type data** — ~500 assertions (`bwapi_unittype_mineral_price(BWAPI_UNIT_TERRAN_MARINE) == 50`) | none | ~0 |
| **Read path, rules, command emission** — every read and write entry point exercised | synthetic `GameData` | small; the harness exists |
| **BWEM** — full analysis, ids, paths, hooks, reset, teardown order | synthetic terrain | small; the fixture exists |
| **Error paths** — bad handles, truncation, the sticky latch, BWEM assertions caught | deliberately malformed `GameData` | small |
| **Boundary fuzz** — a harness generated from the `.def` calling every export with negative handles, `cap` of `INT_MIN`, `NULL` with nonzero `cap`, `buf_len` one byte short. The headline safety promise, tested | synthetic | an afternoon |
| **Transport** — game table, mapping, two-byte handshake | a ~150-line fake of the transport only, no game semantics | small |
| **Coverage audit** — on demand and at pin bumps, not on the merge path (§9) | libclang | — |
| **End-to-end** — the C, Python and C# example bots against real StarCraft | manual, gated, pre-release | occasional |

**The whole suite is Linux-native**, free of Windows, Wine and Blizzard's files, because the
closure builds and runs on Linux (R6, R7). That is worth more to this project than OpenBW was
ever going to be.

---

## 12. Roadmap

Five phases. Revision 3's eight were sized for a team and guarded against a dependency that does
not move; two of them (the mock server, the two-stage x64 proof) have been answered by research
rather than by work.

| Phase | Deliverable | Exit criterion |
|---|---|---|
| **0. Bootstrap** | Repo; both pinned submodules with carried patches; the derived closure with explicit file lists and include dirs; the client-only CMake target; `svnrev.h` from upstream's script; §0 license files; header skeletons with the §4 conventions; the layout-dump and derived-closure CI jobs; the shared fixture builder | An empty `bwapi_c2.dll` links **x86 and x64**; both layout dumps match the baseline at 33,017,048; R7's harness and R11.6's BWEM fixture run green inside the repo |
| **1. Generator and static types** | `draft_spec.py`, the emitters, `check_coverage.py`, `api.json`, the golden `.def`; `bwapi_c2_types.h` with 848 constants, 185 accessors and the bulk tables; `Player` fully generated as the interface proving ground | ~500 type assertions green with no game; `Player` round-trips spec → header → `.def` → `api.json` → a compiling Python `ctypes` and C# P/Invoke layer; the coverage audit reports **zero unaccounted declarations** across the six headers — every one has an entry or a rule-bearing `skip:` |
| **2. Read surface** | `Game`, `Unit`, `Force`, `Region` getters; the 88 `can_*`; events; bulk grids; snapshots; boundary-side closest queries | Every read entry point exercised against a synthetic fixture; boundary fuzz green |
| **3. Write surface and BWEM** | Commands and broadcasts; `bwapi_c2_bwem.h` — 98 functions, the three hand-written hooks, reset and teardown | **A C99 example bot builds against the headers alone, with no C++ toolchain, and against real StarCraft reads game state, moves units, and finds its natural expansion through BWEM** |
| **4. Consumers → 1.0** | Python and C# raw layers from `api.json`; the Rust proof-of-concept; Python and C# example bots; idiomatic wrappers spun out to their own repos | Both example bots play a game; **`bwapi_abi_version()` returns 1.0 and the append-only promise takes effect** |

**Phase 3's criterion is the one that matters.** The ABI has to be usable from plain C before it
is usable from anything else. A C bot that works is proof the headers are honest; a Python bot
that works could just mean the Python wrapper is clever.

Deferred to measured need, not scheduled: `Type::getType` name lookup, borrowed-pointer bulk
access, callback predicates. Deferred to v2: module mode (Appendix A). Parked: Linux
(Appendix B).

---

## 13. Risks and decisions

| Risk | Mitigation |
|---|---|
| **The audience is thin** (§1.9) | Real, active, and building the wrong thing for lack of this one. The plan is sized for one developer and five phases, and §5.8 alone is worth shipping: five projects have paid for its absence |
| **~770 entry points across two headers is a lot of surface to keep correct** | The generator (§9), the coverage audit, the golden `.def`, boundary fuzz. The wrappers are output |
| **A private build of upstream sources drifts from what upstream builds** | Explicit file lists, never globs; `derive_closure` in CI; the §10.3 checklist at pin bumps |
| **We carry patches on two dormant dependencies** (§15.2) | Recorded, re-applied at every bump, offered upstream. If a patch stops applying, that is the pin bump's first finding, not a surprise |
| **`unordered_set` pointer hashing makes bots irreproducible** | Sort collections by ID; compute closest-unit at the boundary with a lowest-ID tie-break (§5.4). Both in §15 |
| **BWEM throws from release-build assertions** | Every export is a `noexcept` boundary; the destruction hooks are filtered and driven internally (§8) |
| **Struct layout freezes the ABI prematurely** | Size-prefixed PODs (§4), flag bits rather than boolean fields (§5.10), and 0.x until phase 4 |
| **Per-call FFI cost makes the ABI look slow in Python** | Snapshots (§5.10), bulk grids cropped to the live map (§5.5), the bulk type tables (§5.8), sticky errors that avoid a second crossing (§4) |
| **Grouped commands are impossible in client mode** | Not a binding limitation — no client bot in any language has them. Documented in the README; the motivation for module mode in v2 (Appendix A) |
| **A foreign callback unwinds into BWAPI's stack** | Callbacks are a conditional follow-on; if they land, `catch(...)` at every site, the `reentrant: forbidden` guard, and warn-level logging on rejection |
| **256-byte text truncation surprises users** | Documented on every text function and in §15; worth proposing upstream separately |
| **Someone builds bindings on the shared-memory layout instead** | Two already did, and §2's non-goal 1 records what it cost them. `api.json` and the fixtures make the C ABI the path of least resistance |
| **Pressure to ship a Linux build links unlicensed code** | Appendix B: the process boundary means it never has to. Nothing OpenBW is built, linked or distributed from this tree |

### Decisions log

From the revision-1 review:

| # | Question | Decision |
|---|---|---|
| 1 | Is x64 client mode in scope? | **Yes** — and now settled rather than sequenced (§10.2) |
| 2 | `bindings/` in-tree or separate repos? | **Both, split by layer.** Raw FFI in-repo, idiomatic wrappers downstream |
| 3 | Python dependency in CI? | **Yes.** Generated sources stay checked in, so it is a CI-only dependency |
| 4 | Does module mode justify a phase? | **No** — v2 (Appendix A) |
| 5 | Remove `swig.i` / `swig_lib/` from BWAPI? | **No.** Separate repository; no standing to propose removals there |

From revisions 2 and 3: the ABI is LGPL-3.0 and dynamically consumed (§0); positions pack into
`int64_t` on return only (§4); errors are sticky and first-wins (§4); every crossing POD is
size-prefixed (§4); closest-unit queries move to the boundary and `getBestUnit` is dropped
(§5.4); snapshots are added (§5.10); the closure is derived and Storm excluded (§10.1); the drift
canary is replaced by a pin-bump checklist (§10.3); 1.0 gates on the consumers phase (§4, §12).

From the research round (R1–R11) and the fork decisions of 2026-09-05
([research/research-vs-rev4-review.md](research/research-vs-rev4-review.md)):

| # | Question | Decision |
|---|---|---|
| 6 | Fork `bwapi-c`? | **Foreclosed** — no license. Read-only reference, zero code reuse (§3) |
| 7 | Project name | **`bwapi-c2`**; symbol prefix stays `bwapi_`; namespace stays `BWAPI::CApi` (§3) |
| 8 | Primary consumers | **Python and C#.** Rust is the proof-of-concept; no JavaScript binding in-repo (§7) |
| 9 | Surface | **The declared surface under §1.8's rules, not a usage-derived subset.** Draw usage is not authoritative |
| 10 | §5.8 shape | **Functions + generated constants + optional bulk tables** (§5.8) |
| 11 | `canXxx` | **All 88 base declarations**; no `*Grouped`; `checkCommandibility` suppressed (§5.11) |
| 12 | The generator | **Unconditional, phase 1**, justified by the coverage audit rather than the typing (§9) |
| 13 | Module mode | **Scoped v2 item**, motivated by the grouped-command gap (Appendix A) |
| 14 | BWEM | **In scope**, second header, same conventions (§8). BWTA2 is a non-goal |
| 15 | Linux / OpenBW | **Parked, not closed.** The process boundary means nothing unlicensed is ever linked (Appendix B). 32-bit Linux is a non-goal |
| 16 | Test fixtures | **Synthetic by policy**; recorded buffers contributor-local; no mock server (§11) |
| 17 | SWIG | **Rejected on five measured grounds**; clang drafts the spec (§9) |

---

## 14. What a consumer sees

**C**, which is the phase 3 exit criterion and therefore the one that has to be pleasant:
```c
#include <bwapi_c2.h>
#include <bwapi_c2_bwem.h>

while (!bwapi_client_connect()) { Sleep(1000); }
for (;;) {
    while (!bwapi_game_is_in_game()) bwapi_client_update();
    bwapi_bwem_initialize(1, 1);
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
    bwapi_bwem_reset();
}
```

**Python** (`ctypes`, generated from `api.json`):
```python
lib = ctypes.CDLL("bwapi_c2.dll")
lib.bwapi_unit_get_position.restype = ctypes.c_int64      # packed, §4
pos = lib.bwapi_unit_get_position(unit_id)
x, y = pos & 0xFFFFFFFF, pos >> 32
```
…wrapped into a `bwapi_c2` package with a `Unit` class, exceptions raised from the sticky latch
once per frame, and `numpy` views over the bulk grids and snapshots.

**C#** (P/Invoke, generated from `api.json`):
```csharp
[DllImport("bwapi_c2", CallingConvention = CallingConvention.Cdecl)]
static extern long bwapi_unit_get_position(int unit);
[DllImport("bwapi_c2", CallingConvention = CallingConvention.Cdecl)]
static extern int bwapi_game_snapshot_units([Out] UnitSnapshot[] buf, int cap);
```
…wrapped into `BwapiC2` with `Span<T>` over the snapshot buffer and a `Unit` struct newtype.

**Rust** (`bwapi-c2-sys`, the proof-of-concept): an `extern "C"` block from `api.json`, and a
downstream `bwapi-c2` crate if anyone wants latency compensation and `getBuildLocation` in Rust.

None of them compiles a line of C++. That is the whole point.

---

## 15. Divergence register

The ABI deliberately departs from `BWAPI::Game` and `BWEM::Map` semantics in the places below.
Goal 6 is "no fork of the game logic", so the departures need one normative table rather than
being scattered through prose where they rot.

| # | Divergence | Where | Rationale |
|---|---|---|---|
| 1 | Collection output sorted ascending by ID | §4 | `Unitset` hashes pointers; ASLR makes iteration order vary run to run |
| 2 | Closest-unit queries computed at the boundary, tie-broken on lowest ID | §5.4 | Sorting cannot reach a single-unit return; `Templates.h` ties resolve by iteration order |
| 3 | `getBestUnit` not exposed | §5.4 | Meaningless without a `BestFilter` |
| 4 | No format-string functions; plain `const char*` only, no `...` or `va_list` anywhere | §4, §5.1 | Not FFI-callable; format-string vulnerability; 256-byte truncation retained and documented |
| 5 | Positions returned packed in one `int64_t` | §4 | One register instead of a pointer write; lossless, sentinels preserved; no 8-byte-POD ABI ambiguity |
| 6 | Bullets are a snapshot array, not handles | §6.3 | No `Game::getBullet(id)` exists; bullets are transient |
| 7 | Events polled from a per-frame vector, not dispatched | §5.6 | `getEvents()` is a `std::list`; indexing it directly is O(n²) |
| 8 | Invalid handles return a documented neutral value | §4 | A foreign-language bug must not crash the game |
| 9 | Bulk grids cropped to the live map | §5.5 | 16× less data on a 128×128 map; `out_w`/`out_h` report the extent |
| 10 | Text truncated at 256 bytes | §5.1 | Inherited from `GameImpl::vPrintf`, not introduced — but surfaced here because callers cannot see it |
| 17 | `canXxxGrouped` not exposed | §5.11 | 17 declarations under 11 names. Grouped commands are **not implemented by the BWAPI server** (JBWAPI #70), so a client bot cannot use them in any language; exposing the predicates would advertise a capability that does not exist in client mode. Revisit with module mode |
| 18 | `checkCommandibility` not exposed; every `can_*` behaves as if it were `true` | §5.11 | A default argument on 88 declarations, not an overload. Passing `false` skips the commandability precheck — an optimisation for chaining predicates, costing a boolean on every signature and a footgun across a boundary where the precondition cannot be enforced |
| 19 | Filter-taking query overloads not exposed; every query has an unfiltered form | §5.4 | `std::function` does not cross C; the host filters the ID array or the snapshot. 13 declarations |
| 20 | `int x, int y` overloads merged into their `Position` sibling | §4 | §4 unpacks position parameters, so the two declarations have one C signature. 14 declarations. A de-duplication, recorded so the coverage audit expects it |

### 15.1 BWEM divergences

`bwapi_c2_bwem.h` departs from `BWEM::Map` semantics in the places below. Every one is an
application of a §4 rule, not an exception to it.

| # | Divergence | Where | Rationale |
|---|---|---|---|
| 11 | `Base` addressed by a **synthesised** flat `int32_t`, 0..`BaseCount()-1` | §8.1 | BWEM's `Base` has no identity of its own and is stored by value inside `Area::Bases()`. `Base::Location()` is the fourth most-used BWEM call (89 sites), so bases need a first-class handle. The ABI owns the mapping table and rebuilds it on reset |
| 12 | Neutrals addressed by their **BWAPI unit id** — the one shared handle space | §8.1 | `Neutral::Unit()` *is* a BWAPI unit. Sharing the id makes the two headers join with no lookup table on either side, and makes `Map::GetMineral(Unit)` the identity function. Deliberate exception to §4's disjoint-handle-space rule, stated in both headers |
| 13 | BWEM's three-phase init collapsed to one `bwapi_bwem_initialize(int32_t, int32_t)` | §8.2 | `Initialize` → `EnableAutomaticPathAnalysis` → `FindBasesForStartingLocations` has an ordering dependency with no use case for the intermediate states. An ABI should make ordering errors impossible rather than diagnosable. Blocks ~450 ms; documented |
| 14 | `on_*_destroyed` hooks are **filtered and idempotent**, never throwing, and driven internally by the event pump | §8.2 | `MapImpl::OnMineralDestroyed` does `bwem_assert(found)` and therefore throws when handed a unit BWEM does not track — which is what a host forwarding every `onUnitDestroy` would do. Extends §4's neutral-value-and-latch rule to a third-party library's assertions |
| 15 | Mutators, internals, and `Graph`/`GridMap`/`UserData`/`Markable`/`MapDrawer`/`MapPrinter`/`BreadthFirstSearch` not exposed | §8.1 | 42 mutators/internals and 49 declarations on non-bot-facing classes, with **zero** call sites across 119,000 lines of bot code. Not merely unused — unnecessary, since no host runs the analysis |
| 16 | `Tile`/`MiniTile` exposed as scalar accessors first, bulk grids as an optional fast path | §8.1 | Grid access is ~2% of BWEM traffic and no existing port exposes bulk grids. Inverts the emphasis §5.5 would suggest |

### 15.2 Patches carried on pinned dependencies

Not ABI-vs-library divergences but modifications to the pinned sources themselves. Each is a
file under `patches/`, re-applied at every pin bump (§10.3), and offered upstream without gating
on a response.

| Dependency | Patch | Reason |
|---|---|---|
| `third_party/bwem` (`N00byEdge/BWEM-community`, MIT/X11) | Add `void Map::ResetInstance() { m_gInstance = nullptr; }` | **Upstream bug found in R11.6.** `MapImpl::Initialize` resets in place with `this->~MapImpl(); new (this) MapImpl();`, and `~Neutral` calls `RemoveFromTiles()` which reaches back into the Map's already-destroyed tile storage. Re-initialisation segfaults on any map with neutrals — every map — and the same root cause crashes at static destruction after the host releases its `GameData`. Stardust's vendored BWEM already carries exactly this method. `bwapi_bwem_reset()` depends on it |
| `third_party/bwapi` (LGPL-3.0) | `BWAPIClient/Source/Convenience.h:33`: `va_list &ap` → `va_list ap` | MSVC's `va_list` is `char*`, so a reference binds; glibc's is an array type and cannot. One character; blocks `GameImpl.cpp` on every non-MSVC compiler (R6 §5). Needed only for the Linux-native test suite (§11), which is reason enough |

---

## Appendix A. Module mode — a scoped v2 item

**Not v1, and no longer deferred indefinitely.** The concrete reason arrived with R4: **grouped
commands are not implemented by the BWAPI server**, so no client-mode bot in any language can
issue them — JBWAPI #70, from its own maintainers: *there's no way to fix this on JBWAPI's end.*
That is a capability gap rather than a binding limitation, every competing binding has it too,
and module mode is the only way to close it. It stays out of v1 because it carries real costs —
x86-forever, no crash isolation, and a much harder test story — and because client mode covers
the non-C++ audience today. v1 documents the gap in the README and does not export the
`*Grouped` predicates (§15 #17).

**How it works today.** BWAPI loads `bwapi-data/AI/<bot>.dll` and resolves `gameInit` and
`newAIModule` by `GetProcAddress` (`bwapi/BWAPI/Source/GameUpdate.cpp:389`;
`bwapi/ExampleAIModule/Source/Dll.cpp` shows the exports). `newAIModule()` must return a pointer
to an object with an **MSVC C++ vtable** matching `BWAPI::AIModule`. A foreign `cdylib` *can*
fake that, but it is a hand-laid vtable pinned to one compiler's ABI — exactly the fragility this
project exists to remove.

**The way out** is to make `bwapi_c2.dll` itself the loadable AI module: it exports
`newAIModule`/`gameInit`, and on `gameInit` loads a *host* DLL named by config, resolving pure-C
entry points against a vtable of function pointers. `bwapi-c` shipped exactly this pattern and it
is clean C; what it lacked — and this adds — is a `void* user` on every slot and a `destroy` slot
(its `destroyAIModuleWrapper` was never called; open since 2018).

```c
typedef struct bwapi_bot_vtable {
  void (BWAPI_C2_CALL *on_start)(void* bot);
  void (BWAPI_C2_CALL *on_end)(void* bot, int32_t is_winner);
  void (BWAPI_C2_CALL *on_frame)(void* bot);
  void (BWAPI_C2_CALL *on_unit_create)(void* bot, bwapi_unit u);
  /* … one per AIModule virtual … */
  void (BWAPI_C2_CALL *destroy)(void* bot);
} bwapi_bot_vtable;

/* the host DLL would export exactly this: */
void* BWAPI_C2_CALL bwapi_bot_create(const bwapi_bot_vtable** out_vtable);
```

One C++ class deriving `BWAPI::AIModule` forwards every override to the table, each guarded by
`catch(...)`. `bwapi/AIModuleLoader/Source/AIModuleLoader.cpp:69` already does
load-and-`GetProcAddress`.

**What makes it cheap:** both modes sit behind the same abstract `BWAPI::Game` (§1.1), so ~95%
of the wrapper source is transport-agnostic. What would not carry over: module mode's ID
assignment is lazy (`Server::getUnitID`, `Server.cpp:719`) rather than a constructor-populated
vector, though `extractUnitData()` assigns every alive unit an ID before AI callbacks run
(`GameUnits.cpp:228`), so §1.3's handle model still holds. And module mode is x86-only forever,
since it is injected into a 32-bit process — which is the constraint its deferral lifts from the
rest of the plan.

---

## Appendix B. Linux via OpenBW — parked, and why it can stay open

Non-goal 3 says v1 is Windows. This appendix records what a Linux target would actually take,
because the research found it is both smaller and safer than it looked, and the question should
not be re-litigated from scratch.

**The licensing problem is a process boundary, not a link edge.** The OpenBW *engine* has no
license (R9 §6). But a client never links the engine: it reaches a separate `BWAPILauncher`
process over shared memory and a Unix socket. So the unlicensed code is a program the *user*
supplies and runs, which `bwapi-c2` neither builds, links nor distributes. The BWAPI *fork* over
OpenBW (`OpenBW/bwapi`) is LGPL-3.0, legitimately inherited from BWAPI, with a `LICENSE`
byte-identical to upstream's; it is the engine underneath it that has none. And even that fork
is not needed on our side: what a Linux `libbwapi_c2.so` needs from BWAPI is the *client
transport*, and that is upstream code.

**What remains is ordinary engineering, all of it measured:**

1. **A POSIX client transport.** Seven Win32 imports, all in `Client.cpp` (R6 §2):
   `CreateFileMappingA` → `shm_open`+`mmap`; `CreateNamedPipe` → `AF_UNIX` socket; the two-byte
   handshake unchanged. Already written line-for-line on
   `basil-ladder/bwapi@linux-client-support` — ~344 lines, 8 files, unmerged since 2020, needs
   one `#include <string>` on GCC 11. It becomes a §15.2-style patch on the pinned upstream
   BWAPI: LGPL on LGPL.
2. **The version gate.** OpenBW's fork is `CLIENT_VERSION` 10002; upstream is 10003; `Client.cpp:120`
   refuses to connect on mismatch. Relax the check, make the constant configurable, or accept
   that OpenBW users run a fork. One decision.
3. **No public CI.** OpenBW needs three retail MPQs; the one public download source is gone;
   JBWAPI wrote both end-to-end workflows and disabled them in April 2022 (R7 §3). A Linux
   target is exercised the way the Windows one is — synthetic fixtures in CI, live runs manual.
4. **x86-64 only.** Linux x86-64 agrees with Windows on `GameData`; i386 does not (§1.4).

**Standing rules while parked:** nothing from `OpenBW/openbw` is vendored, built, linked or
distributed from this tree; the closure stays free of platform conditionals outside `Client.cpp`;
and the test suite stays Linux-native (§11), so the day someone wants this, everything but the
transport is already running on Linux.
