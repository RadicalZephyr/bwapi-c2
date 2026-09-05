# R6. Deriving the link closure by building it

Built the candidate source set as a static archive and linked upstream's own
`ExampleAIClient` against it. Reproducible via
[`r6/derive-closure.sh`](r6/derive-closure.sh).

**§12 and §13 were added later on request**: an audit of the `awest813/OpenSnowstorm---Brood-War`
fork, and an assessment of what adding client mode to OpenBW's BWAPI fork would take. §13 changes
the answer §8 gives, so read both.

**Headline: §10.1's closure is correct — 44 translation units, zero Storm, zero Util, zero
Boost, and the resulting archive links and runs upstream's client example. But three things in
§10.1 need fixing. The Boost rationale is stale: Boost was already `#if 0`'d out of this tree in
April 2017, so "dropping both removes the Boost dependency entirely" describes work upstream
already did. The file list is missing a load-bearing include path without which the closure does
not compile at all. And two MSVC-permissive-mode constructs in the closure break every
non-MSVC compiler — which matters because §10.1 makes CMake the only build system.**

---

## 1. The experiment

44 translation units — `BWAPILIB/UnitCommand.cpp` + `BWAPILIB/Source/*.cpp` (31) +
`Shared/*.cpp` (6) + `BWAPIClient/Source/*.cpp` (7) — compiled with clang++ on Linux x86-64,
archived, and linked against `ExampleAIClient/Source/ExampleAIClient.cpp`.

```
== archive: 6,739,732 bytes, 44 objects
== undefined symbols outside the C++/C runtime ==
CloseHandle  CreateFileA  MapViewOfFile  OpenFileMappingA  ReadFile  SetCommTimeouts  WriteFile
== Storm / Util / Boost references ==  count: 0
== link upstream ExampleAIClient against the closure ==
  linked OK: 1,668,152 bytes
Connecting...
Game table mapping not found.
```

That last line is `Client.cpp:39` — the bot ran, entered `Client::connect()`, and reported the
same message bwapi-c's README shows from a real Wine session. The closure is not merely
link-complete; it executes the connect path.

Undefined-symbol breakdown of the archive (429 total):

| Category | Count |
|---|---|
| C++ runtime / libstdc++ | 64 |
| libc (`memcmp`, `vsnprintf`, `strncpy`, `nanosleep`, `abs`, …) | 10 |
| **Win32 imports** | **7** |
| BWAPI-internal, resolved within the archive | 346 |
| **Storm / Util / Boost** | **0** |

**All seven Win32 imports come from exactly one translation unit, `BWAPIClient/Source/Client.cpp`.**
Nothing else in the closure touches the platform.

---

## 2. Verdicts on §10.1's table

| §10.1 says | Verdict |
|---|---|
| `BWAPILIB/Source/*.cpp` + `BWAPILIB/UnitCommand.cpp` — **In** | ✅ Confirmed. All 31 compile; nothing else defines the type tables |
| `BWAPIClient/Source/*.cpp` — **In** | ✅ Confirmed |
| `Shared/*.cpp` — **In** | ✅ Confirmed — **but see §4, the include path is not optional** |
| `Streams.cpp`, `BroodwarOutputDevice.cpp` — **Out** | ✅ Safe to drop (verified by relinking without them) — **but the stated reason is wrong; see §3** |
| `Util/Source/**` — **Out** | ✅ Confirmed twice: zero `Util::` symbols in the archive, and OpenBW compiles only `Util/Source/Util/Sha1.cpp`, into the *server* |
| `Storm/**` — **Out** | ✅ Confirmed twice: zero Storm symbols, and OpenBW keeps `Storm/` in the tree while referencing it from **no CMakeLists at all** |
| generated `include/svnrev.h` — **In** | ✅ Confirmed; `BWAPILIB/Source/BWAPI.cpp` is its only consumer |

**Storm's exclusion — the finding §10.1 says matters most — holds under the linker.** x64 stays
open, and R5 independently settled the layout half.

---

## 3. Correction: Boost was already removed upstream

§10.1 says `Streams.cpp` "includes `boost/iostreams/{stream,tee}.hpp` directly, and both include
`BWAPI/BroodwarOutputDevice.h`, which pulls `boost/circular_buffer.hpp`… Dropping both **removes
the Boost dependency entirely**."

In this tree that is not true, because there is no Boost dependency to remove:

- `include/BWAPI/BroodwarOutputDevice.h` — **the entire file body is inside `#if 0`** (lines 3–89), boost includes included.
- `BWAPILIB/Source/Streams.cpp` — the two boost includes are inside `#if 0` (lines 5–15), as is the boost-backed implementation (lines 20–35). What remains live is four lines aliasing `std::cout`/`std::cerr`.
- `BWAPILIB/Source/BroodwarOutputDevice.cpp` — **the whole file is inside `#if 0`.** It compiles to a 952-byte object with zero symbols: an empty translation unit.

Disabled in commit `109ff28a` (2017-04-27, *"CMakeLists to build client mode libraries + fix for
VS2017"*) — the same commit that added `CMake/Client`. The only file in the whole tree that still
reaches Boost live is `bwapi/BWAPI/Source/BWAPI/Console.cpp`, which is injected-DLL code far
outside the closure.

**Dropping the two files is still the right call, but for a different reason and with a caveat
§10.1 does not state.** `Streams.cpp` is the *sole* definition of `BWAPI::bwout`, `bwerr`, `out`
and `err`, which `include/BWAPI/Streams.h` declares `extern` and `BWAPI.h` therefore exposes.
Relinking without it succeeds only because nothing in the closure or in `ExampleAIClient`
references them. If our static archive is ever consumed by C++ rather than by the C ABI, those
four public symbols will be missing. Since the consumer is a C shim, keeping them out is fine —
but say so, rather than justifying the exclusion with a Boost dependency that has not existed
since 2017. `BroodwarOutputDevice.cpp` can be dropped on the honest grounds that it is empty.

---

## 4. Missing from §10.1: the client include path is load-bearing

`Shared/*.cpp` do not include their Impl headers by qualified path. `UnitShared.cpp:3` is
`#include "UnitImpl.h"`, `GameShared.cpp:2` is `#include "GameImpl.h"`, and so on for
`PlayerImpl.h`, `RegionImpl.h`, `BulletImpl.h`. With only `-Iinclude` those six TUs fail outright:

```
Shared/UnitShared.cpp:3:10: fatal error: UnitImpl.h: No such file or directory
```

They resolve against **`include/BWAPI/Client/`**, which is what makes `Shared/` dual-purpose: the
same six files compile against either the client Impl classes or the injected DLL's, chosen by
include path alone. Upstream's `CMake/Client/CMakeLists.txt` adds `${BWAPI_INCL_DIR}/BWAPI/Client`
for exactly this reason.

**§10.1 specifies file lists but not include directories, and the closure does not build without
this one.** The required set is:

```
-I<bwapi>/include
-I<bwapi>/include/BWAPI/Client     # <-- the one that is easy to miss
-I<bwapi>/Shared
-I<bwapi>/BWAPIClient/Source
-I<generated svnrev.h dir>
```

Also worth correcting: §10.1 calls `CMake/Client` "too little." It is, for `BWAPILIB` — but it is
simultaneously *too much*, because `CMake/Client/CMakeLists.txt` compiles `Storm/storm.cpp` and
seven `Util/Source/Util/*.cpp` files into `BWAPIClient`, none of which the linker wants.

---

## 5. New finding: two MSVC-isms block every non-MSVC compiler

Both are inside the closure and neither is mentioned in the plan. They matter because §10.1 makes
CMake the only build system and R7 puts an OpenBW/Linux build on the table.

**`include/BWAPI/Client/CommandTemp.h:34`**
```cpp
buf[frames - 1].push_back(std::forward<Command>(command)); // Forward rvalue ref
```
`Command` here is a template parameter, but under two-phase lookup GCC and clang bind it to
`BWAPIC::Command` instead and reject the call. MSVC's permissive mode accepts it. Workaround:
`-fdelayed-template-parsing` (clang only — GCC has no equivalent). This takes out
`BWAPIClient/Source/UnitImpl.cpp`.

**`BWAPIClient/Source/Convenience.h:33`**
```cpp
template <size_t N> inline void VSNPrintf(char (&dst)[N], const char *fmt, va_list &ap)
```
MSVC's `va_list` is `char*`, so a reference binds. glibc's is `__va_list_tag[1]` — an array type —
so the reference cannot bind to the decayed argument. This takes out
`BWAPIClient/Source/GameImpl.cpp` at three call sites. Fixing it is a one-character change
(`va_list ap`), which is what the reproduction script does.

Both are upstream bugs worth reporting alongside the two CMake ones. Neither affects the symbol
closure — they are front-end issues — but a CMake build on anything but MSVC needs them patched
or worked around, and that belongs in the plan's build section rather than being discovered at
phase 0.

---

## 6. `#pragma pack` and `/Zp`: closure is clean, but the count is higher than stated

| Search | Result |
|---|---|
| `StructMemberAlignment` in any `.vcxproj` | **1** — `Storm/Storm.vcxproj:36`, `1Byte`. Exactly as §10.1 says |
| `/Zp` as an explicit compiler flag | **0** anywhere in the tree |
| `#pragma pack` | **20 occurrences** — 19 under `BWAPI/Source/BW/` (the injected DLL's mirrors of StarCraft's own memory layout) plus `BWAPI/Source/Thread.cpp` |
| `#pragma pack` **inside the closure** | **0** |
| Bitness conditionals (`_WIN64`, `__x86_64`, `_M_X64`, `_M_IX86`, `__i386`) in the closure | **0** |

§10.1 says Storm "is also the only project setting `/Zp`", which is right about the project
setting but leaves the impression that Storm is the only thing that packs. Twenty `#pragma pack`
directives exist; all of them are in the injected-DLL's `BW/` layer, which is outside the closure
for the same reason Storm is. The conclusion is unchanged and better supported: **nothing in the
client path alters structure packing.**

---

## 7. `clientInfo`: answered, and §10.1's contingency is unnecessary

§10.1 asks whether the two `clientInfo` templates instantiate. Reading the built objects:

```
W  void BWAPI::Interface<BWAPI::UnitInterface>::setClientInfo<void*>(void* const&, int)
T  BWAPI::Unitset::setClientInfo(void*, int) const
T  BWAPI::Unitset::setClientInfo(int, int) const
```

**Exactly one template instantiates: `setClientInfo<void*>`, from `Unitset.cpp:77` — the safe
direction.** The truncating one, `getClientInfo<CT>()` at `Interface.h:72`
(`return (CT)(int)this->getClientInfo(key);`, R5 §4), **does not instantiate anywhere in the
closure**; nothing in `BWAPILIB`, `BWAPIClient` or `Shared` calls it.

`Unitset.cpp:82` is `this->setClientInfo((void*)clientInfo, index)` from an `int` — a *widening*
`int`→`void*` conversion on x64, lossless, MSVC C4312-class warning at worst.

**So `Unitset.cpp` does not need excluding and its two `setClientInfo` overloads can stay.**
§10.1's contingency ("if `Unitset.cpp` proves troublesome it can be excluded") can be deleted.
The truncation hazard is reachable only if a *bot* calls `getClientInfo<T>()`, which is a
documentation note for the ABI, not a build constraint.

---

## 8. OpenBW's CMake — read first, as §10.1 step 4 suggested

Worth doing, and it corroborates the closure from an independent direction.

- **`Storm/` is present in OpenBW's tree and referenced by zero CMakeLists.** They deleted it from the build outright.
- **`Util/` appears only as an include directory**, plus one file — `Util/Source/Util/Sha1.cpp` — compiled into the *server* target, never the client.
- `BWAPILIB`'s file list is essentially identical to ours, including the two `#if 0`'d files (harmless).
- `Shared/*.cpp` are compiled into the **server** (`BWAPIObj`), not the client.
- `svnrev.h` is **checked in** at `bwapi/svnrev.h` rather than generated — which is what §10.3 proposes doing.

**And the finding that belongs to R7: OpenBW has no client mode.** Its `BWAPIClient` target
compiles a single 477-byte `Client.cpp` and links the whole in-process server, and the body is:

```cpp
bool Client::connect()
{
  throw std::runtime_error("Client not supported :(");
  return true;
}
```

bwapi-c's 2017 README said client mode "is not supported by OpenBW." Still true at OpenBW's last
commit (2020-06-11) — **but only on OpenBW's own branches.** A working POSIX client mode already
exists on a third-party fork, and §13 measures it. Read §13 before concluding anything about
OpenBW as a CI substrate.

---

## 9. The two claimed upstream CMake bugs: both confirmed

1. **`CMake/BWAPI/CMakeLists.txt:110`** — `SET(BWAPI_BWAPILIB_DIR ${BWAPI_ROOT}/BWAPILib/Source)`. The directory is `BWAPILIB`. Case-insensitive on Windows, broken on a case-sensitive filesystem.
2. **`CMake/BWAPI/CMakeLists.txt:303`** — `ADD_CUSTOM_COMMAND(OUTPUT ${BWAPI_ROOT}/svnrev.h …)` while `revisionUpdate.vbs:14` writes `include/svnrev.h`.

§10.3's description of the script also checks out: `revisionUpdate.vbs:11` is
`revNumber = 2383 + CInt(gitResult.StdOut.ReadAll())` over `git rev-list HEAD --count`, and it
writes `#include "starcraftver.h"`.

---

## 10. What I could not do

**The x86-versus-x64 build comparison (step 2) did not run.** This machine has no 32-bit
libstdc++ (`/usr/include/c++/*/i686-linux-gnu/bits/c++config.h` does not exist), so `-m32`
codegen is unavailable. Stated plainly rather than worked around, because the honest position is:

- The **layout** half of the x86/x64 question — the part that could have killed x64 — is settled
  by R5, which computed the full struct matrix across six targets from the headers and found the
  four Windows targets byte-identical.
- The **symbol** half is what step 2 would have added, and the closure contains **zero bitness
  conditionals** (§6), so there is no source-level reason for the symbol set to differ. That is
  an argument, not a measurement, and it should be checked once on a machine with multilib or an
  MSVC toolchain — as a one-off, not as a phase gate.

---

## 11. Recommended edits to §10.1

1. **Add the include-directory list.** The closure does not compile without `include/BWAPI/Client`, and the file list alone does not imply it.
2. **Rewrite the `Streams.cpp` / `BroodwarOutputDevice.cpp` row.** Boost has been `#if 0`'d since April 2017. Drop `BroodwarOutputDevice.cpp` because it is an empty TU; drop `Streams.cpp` because the ABI does not use `bwout`/`bwerr` and §5.9 excludes them — and note that doing so leaves those four public symbols undefined for any C++ consumer of the archive.
3. **Add the two MSVC-isms** (`CommandTemp.h:34`, `Convenience.h:33`) to the build section as known non-MSVC blockers, with the one-line fixes.
4. **Delete the `Unitset.cpp` contingency.** Measured: only the safe `setClientInfo<void*>` instantiates.
5. **Note that `CMake/Client` is too much as well as too little** — it compiles Storm and seven Util TUs into `BWAPIClient`.
6. **Soften the `/Zp` sentence.** Twenty `#pragma pack` directives exist; all are in `BWAPI/Source/BW/`, none in the closure. The conclusion survives and is better supported.
7. **Revisit the OpenBW client-mode assumption before R7.** §13 shows a working POSIX client mode already exists on `basil-ladder/bwapi@linux-client-support` (~344 lines, unmerged, five years idle). "OpenBW has no client mode" is true of OpenBW's repos and false of the ecosystem.
8. **Keep `derive-closure.sh` as a CI job.** It builds the closure and links upstream's own example in seconds, and it would catch a pin bump that adds a Storm, Util or Boost reference — the same regression-check role R5 recommends for the layout dump.


---

## 12. The `awest813/OpenSnowstorm---Brood-War` fork

Checked on request. **It is a fork of `OpenBW/openbw` — the engine — not of `OpenBW/bwapi`, and
it is not about BWAPI.** It is a single-player campaign client. Almost nothing in it bears on
this project, with one exception noted at the end.

| | |
|---|---|
| Parent | `OpenBW/openbw` (the engine) |
| Created / last push | 2026-03-02 / 2026-06-05 |
| Stars / forks | 6 / 1 |
| License | LGPL-3.0 — **asserted, not inherited**: added by the fork on 2026-03-01 (`e2e90a6`); upstream `OpenBW/openbw` has no license at all (R9 §6) |
| Position | 93 ahead, 6 behind `OpenBW/openbw@master` |
| Merge base | `a59617d` (2025-01-07) |

### What the 93 commits are

Read as a sequence, they are a phased "modernize the engine and build a playable campaign client"
programme run over three months, **largely by AI coding agents** — the commit authorship is 58
`awest813`, 14 `Cursor Agent`, 11 `copilot-swe-agent[bot]`, 4 `Claude`, 6 `nerdretro`, with
recurring `Initial plan` commits and branch names like `codex/…`, `cursor/…`, `copilot/…`,
`claude/…`.

Themes, in order:

1. **Engine decomposition** — splitting the `bwgame.h` monolith into `pathfinding.h`, `triggers.h`, `creep.h`, `bwgame_combat.h`, `bwgame_state.h`, `bwgame_tables.h`, `ai.h`, `serialization.h`. `bwgame.h` alone is +2,809 / −5,900.
2. **Brood War compatibility** — replay player colours, `is_broodwar` on `tech_type_t`, missing BW actions (Patrol 29, building Land 36), replay `starting_gas` round-trip.
3. **Sync/desync work** — `sync_protocol.h`, structured desync diagnostics, an action-history ring buffer, replay hash record/verify fixtures.
4. **Performance** — `perf_metrics.h`, pre-allocating the `unit_finder_x/y` vectors, table consolidation.
5. **The bulk: a single-player campaign client** in `ui/gfxtest.cpp` (+3,970) and `ui/ui.h` (+5,045 / −2,002) — command panels, fog-of-war controls, quicksave/quickload, multi-slot saves, trigger-engine expansion, briefing/objectives/debrief UI, GRP portrait rendering, campaign chaining, a WebAssembly build.

### Scale, honestly measured

The headline `244 files changed, 89,534 insertions` is misleading. **69,010 of those insertions
are vendored SDL2 2.30.2 headers and committed build logs.** The tree carries `SDL2.zip`,
`pwsh.exe`, and 37 committed `.log`/`.txt` build transcripts (`rebuild.txt`, `rebuild2.txt`,
`rebuild3.txt`, `out.txt`, `output.txt`, `err.txt`, `headless_final.txt`, …). Excluding vendored
and generated files, the real change is **~19,754 insertions / ~8,452 deletions** across roughly
55 source, script and doc files.

### Relevance to this project: one item

**Nothing here touches the BWAPI client protocol, `GameData`, `Server.cpp`, or client mode.**
The engine-side work is below the layer a C ABI cares about.

The exception is **`mini-openbwapi`**, and it is worth recording because R7 asks about it:

- `mini-openbwapi` is a 2,657-line BWAPI-API-compatible **in-process** shim over the OpenBW engine (`openbwapi.h` 1,138 + `openbwapi.cpp` 1,474 + an 8-line `BWAPI.h` aliasing `namespace BWAPI = OpenBWAPI`).
- **Upstream `OpenBW/openbw` deleted it on 2026-07-16** (commit `8265ec4`, "Remove mini-openbwapi."). It is gone from OpenBW master.
- This fork predates the deletion and **still carries and maintains it**: a `/bigobj` fix for MSVC, campaign scenario chaining in `openbwapi.cpp` (+22), an `OPENSNOWSTORM_BUILD_BWAPI` CMake option, and two CI jobs — one of which builds `mini-openbwapi` headless with no SDL2.
- Its `Client` class is a **façade, not a client**: `connect()` sets a bool, `update()` calls `g.update()` in-process, and `mini-openbwapi/BWAPI/Client.h` is a **zero-byte file** so that `#include <BWAPI/Client.h>` compiles. No shared memory, no socket, no second process.

So `mini-openbwapi` answers R7's "is there a smaller target than full BWAPI" with *yes, 2,657
lines* — but it is source-compatibility only, it exposes ~263 functions rather than BWAPI's full
surface, it has now been removed upstream, and **the only maintained copy is inside an
AI-agent-driven campaign-client fork with six stars.** That is not a dependency to build on. It
is worth knowing it exists as a reference for how small an OpenBW-backed BWAPI façade can be.

---

## 13. Adding client mode to the OpenBW BWAPI fork — already done, and measured

The assessment turned out to be an archaeology problem rather than an estimation problem.
**Someone already wrote it.** JBWAPI's `ClientConnectionPosix.java` says so in a header comment:

> To be used with OpenBWs BWAPI **including server support**. At time of writing, only the
> following fork includes this: `https://github.com/basil-ladder/openbw`

and JBWAPI ships a `build_with_openbw.md` documenting the build:

```bash
git clone https://github.com/basil-ladder/openbw
git clone -b linux-client-support https://github.com/basil-ladder/bwapi
```

### What it cost: 5 commits, ~344 lines, 8 files, 16 days

`basil-ladder/bwapi` branch `linux-client-support`, all by Bytekeeper (author of rsbwapi and
Styx), October 2020:

| Commit | Date | Files | +/− |
|---|---|---|---|
| `b4a3fa12` Linux Server using shm and local sockets | 2020-10-01 | `Server.cpp`, `Server.h`, `BWAPILauncher/Main.cpp`, `.gitignore` | +185 / −11 |
| `ce9b2d41` Set keepAliveTime on server | 2020-10-01 | `Server.cpp` | +3 / −1 |
| `283df1cf` Allow server to update game table status | 2020-10-02 | `BWAPILauncher/Main.cpp` | +2 |
| `e648efce` Updated readme, added some basic client code | 2020-10-02 | `BWAPIClient/Client.cpp`, `include/BWAPI/Client/Client.h`, `Server.cpp`, `Command.h`, `README.md` | +150 / −8 |
| `8ac3673d` shm requires rt library | 2020-10-17 | `BWAPILIB/CMakeLists.txt` | +4 |
| **Total** | | **8 files** | **≈ +344 / −20** |

### Why it is that small

R6's own build (§1) already showed the answer. OpenBW's fork did not remove the client
*protocol* — it removed the client *transport*. Its `Server::Server()` reads:

```cpp
Server::Server()
{
  if ( serverEnabled ) { }        // <-- game table + shm + named pipe, deleted
  if ( !data ) { data = new GameData; localOnly = true; }
  initializeSharedMemory();
  if ( serverEnabled ) { }        // <-- deleted
}
```

`checkForConnections()` and `callOnFrame()` are empty bodies. Everything else —
`updateSharedMemory()`, `processCommands()`, `clearAll()`, the entire `GameData` population — is
**intact and running every frame**, because OpenBW uses `GameData` as its own internal state
representation. `data` simply points at `new GameData` instead of a named mapping. Diffed against
upstream, OpenBW's `Server.cpp` is 655 lines to upstream's 852, and 37 transport call sites drop
to 3.

So the work is a straight Win32→POSIX port of code that already exists upstream, and Bytekeeper's
diff is exactly that:

| Upstream (Win32) | `linux-client-support` (POSIX) |
|---|---|
| `CreateFileMappingA("Local\bwapi_shared_memory_game_list")` | `shm_open("/bwapi_shared_memory_game_list", O_RDWR\|O_CREAT)` + `ftruncate` + `mmap` |
| `CreateFileMappingA("Local\bwapi_shared_memory_<pid>")` | `shm_open("/bwapi_shared_memory_<pid>")` + `ftruncate(sizeof(GameData))` + `mmap` |
| `CreateNamedPipe("\\.\pipe\bwapi_pipe_<pid>")` | `socket(AF_UNIX, SOCK_STREAM)` bound to `/tmp/bwapi_socket_<pid>`, `listen` |
| `ConnectNamedPipe` | `select` with a 5 s timeout, then `accept` |
| `WriteFile`/`ReadFile` 2-byte handshake | `write`/`read` of the same two bytes (server writes `2`, spins until it reads `1`) |
| PSID/PACL/PSECURITY_DESCRIPTOR ACL setup | not needed |

The game-table slot-allocation logic is copied across essentially line for line. The whole thing
is `#ifndef _WIN32`-guarded, so the Windows path is untouched — **and therefore still gutted**:
this branch gives OpenBW a *POSIX* client mode only.

`BWAPILauncher/Main.cpp` gains five lines that spin `server.update()` until a client connects
before starting the game, which is what makes a client bot able to attach.

### Current state: unmerged, and slightly bit-rotted

- **Not upstreamed.** `b4a3fa12` is in no `OpenBW/bwapi` branch. `OpenBW/bwapi@develop-openbw` (last commit 2020-06-11) still has `Client::connect()` → `throw std::runtime_error("Client not supported :(")`.
- The branch tip is `c1b9c4e7` (2021-05-25), and `basil-ladder/bwapi` was last pushed 2021-12-05. Five years idle.
- **It requires the paired engine fork.** Building against upstream `OpenBW/openbw` fails immediately: `BWData.cpp:869` calls `ui->play_sound(...)`, which only `basil-ladder/openbw` provides. Two forks pinned together, exactly as JBWAPI's doc instructs.

### I tried to build it

Configured and built `linux-client-support` against `basil-ladder/openbw` on GCC 11:

- **`Server.cpp` — the client-mode server itself — compiles cleanly**, after adding one line: `#include <string>` to `Server.h`. The branch added a `std::string shareName;` member in 2020 without the include and got away with it on the libstdc++ of the day; GCC 11 rejects it. One line.
- `BWAPILIB`, `ExampleAIModule` and `TestAIModule` build.
- The **headless** build (`-DOPENBW_ENABLE_UI=0`) then fails in the *engine fork*, not the client code: `BWData.cpp` calls `ui_wrapper::screen_pos_x` / `screen_pos_y` and a four-argument `ui_wrapper` constructor that the `#ifndef OPENBW_ENABLE_UI` stub does not declare. The headless stub is out of sync with its callers — the same class of bug as `play_sound`.
- The **UI** build (`-DOPENBW_ENABLE_UI=1`, which is what JBWAPI's doc uses) needs `SDL2_mixer`, which is not installed here, so I could not complete it. Not attempted further: installing system packages is the user's call.

**So the client-mode code is sound and still compiles; what is broken is the paired engine fork's
headless path, plus one missing include.** Both are small, well-understood fixes.

### What it would actually take to have this

Ordered by cost, assuming the goal is a working POSIX client-mode OpenBW:

1. **Add `#include <string>` to `Server.h`.** One line. (Verified: `Server.cpp` then compiles on GCC 11.)
2. **Fix or bypass the headless `ui_wrapper` stub** in `basil-ladder/openbw` — three missing members (`play_sound`, `screen_pos_x`, `screen_pos_y`) and one constructor overload. Alternatively build with `OPENBW_ENABLE_UI=1` and add `libsdl2-mixer-dev`, which is what BASIL and JBWAPI actually do. Either is a day, not a project.
3. **Rebase onto current OpenBW.** The branch is five years behind, and both forks have to move together. This is the real cost and it is unbounded until someone tries it.
4. **Windows.** Not addressed by this branch at all — the `_WIN32` server path in the OpenBW fork remains empty. If OpenBW-on-Windows matters, that is a second port.
5. **Upstreaming.** Nobody has proposed it in five years; `OpenBW/bwapi` has had no commits since 2021-05-27.

**Bottom line for R7: "OpenBW has no client mode" is true of OpenBW's own repositories and false
of the ecosystem.** A working POSIX implementation exists, is ~344 lines, was written by the
rsbwapi author, is documented by JBWAPI, and is the substrate BASIL's Linux tooling was built
against. Adopting it means pinning two five-year-old forks and owning small fixes to both — which
is a materially different proposition from writing the transport, and R7 should price it as
maintenance rather than as development.
