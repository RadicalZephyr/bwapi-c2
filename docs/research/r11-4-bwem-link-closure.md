# R11.4 — Building and linking BWEM against the closure

Reproducible via [`r11/link-bwem.sh`](r11/link-bwem.sh).

**Headline: BWEM needs exactly ten BWAPI symbols, and all ten are already in R6's 44-TU client
closure. Zero Storm, zero Util, zero Boost, zero injected-DLL side. All 14 BWEM translation units
compile with the same flags R6 already needs — no new workarounds — and the whole thing links and
runs. This is the cleanest possible answer: BWEM is a 327 KB addition to a library we are already
building, and the one-DLL question answers itself.**

---

## 1. It compiles, unchanged

All 14 of `BWEM/src/*.cpp` compile against the R6 closure's include set with **the same flag set
R6 already established** — `-std=c++14 -fdelayed-template-parsing`, the Win32 shim, the patched
`Convenience.h`:

```
BWEM compiled OK=14 FAIL=0
```

**BWEM introduces no new MSVC-isms.** R6 found two constructs in BWAPI that block non-MSVC
compilers (`CommandTemp.h:34`'s two-phase lookup, `Convenience.h:33`'s `va_list&`); BWEM adds
none. `-fdelayed-template-parsing` was already required for BWAPI and covers BWEM incidentally.

Archive size: **327 KB at `-O2`** (3.2 MB at `-O0`).

---

## 2. The dependency is ten symbols

```
BWAPI::Game::drawBoxMap(Point<int,1>, Point<int,1>, Color, bool)
BWAPI::Game::drawLineMap(Point<int,1>, Point<int,1>, Color)
BWAPI::Game::drawTriangleMap(Point<int,1>, Point<int,1>, Point<int,1>, Color, bool)
BWAPI::Game::getGroundHeight(Point<int,32>) const
BWAPI::Game::isBuildable(Point<int,32>, bool) const
BWAPI::Game::isWalkable(Point<int,8>) const
BWAPI::UnitType::isBuilding() const
BWAPI::UnitType::isCritter() const
BWAPI::UnitType::isMineralField() const
BWAPI::UnitType::tileSize() const
```

Outside `BWAPI::` and `BWEM::`, and excluding the C/C++ runtime, BWEM references **`rand`,
`bcmp`, and one `std::basic_ofstream` VTT** — libc and libstdc++, nothing else.

**Storm / Util / Boost reference count: 0.**

All ten resolve inside the R6 closure, and it is worth noting *where*, because the split is
informative:

| Symbol | Defined in |
|---|---|
| `Game::drawBoxMap`, `drawLineMap`, `drawTriangleMap` | `BWAPILIB/Source/Game.cpp` — non-virtual convenience helpers |
| `Game::getGroundHeight`, `isBuildable`, `isWalkable` | `BWAPIClient/Source/GameImpl.cpp` — the client-mode virtuals |
| `UnitType::isBuilding`, `isCritter`, `isMineralField`, `tileSize` | `BWAPILIB/Source/UnitType.cpp` — static type data |

So BWEM draws on all three parts of the closure R6 identified, and on nothing outside it. **The
finding that mattered — whether BWEM would drag in `Util/`, Storm or the injected-DLL side — comes
back clean.**

Three of the ten are *drawing* calls, used by `MapDrawer`/`bwapiExt`'s debug overlay. Since R11.1
found `MapDrawer` has zero bot usage and R11.3 excluded it from the export set, those three could
be compiled out — but there is no reason to bother: they are already in the closure.

---

## 3. It links and runs

```
$ ./link-bwem.sh <bwapi> <BWEM>
   libbwem.a 327152 bytes
   linked 555192 bytes
BWEM linked. Map singleton at 0x5c1416908e70
  Initialized() = 0
```

A program that touches `BWEM::Map::Instance()` links against `libbwem.a` + R6's `libclosure.a` +
nine Win32 stubs, and runs. The singleton constructs; `Initialized()` correctly reports false
before `Initialize`.

---

## 4. Answers to the questions R11.4 asked

**What BWAPI surface does BWEM use?** Ten symbols, all inside the R6 client closure. `bwapiExt.h`
— the 153-line coupling point — turns out to reference only `Game`'s three map-space draw calls
plus the geometry helpers it defines itself.

**Does it need the same non-MSVC workarounds, or more?** The same, and no more.

**Does it build x64?** Yes — everything above is x86-64. **The 32-bit build was not attempted**:
this machine has no 32-bit libstdc++, the same gap R6 §10 recorded. Since 32-bit *Linux* is now an
explicit non-goal and R5 settled that Win32 and Win64 layouts are byte-identical, the untested
case is Win32, which needs an MSVC toolchain rather than more research.

**One DLL or two?** **One.** Nothing argues for two:

- The dependency is ten symbols into a library we already build. Splitting would mean exporting
  those ten across a second boundary or duplicating the closure.
- 327 KB optimized is not a payload worth making optional.
- The analysis cost is a *runtime* concern (R11.5), and a runtime concern is solved by not
  calling `bwapi_bwem_initialize()`, not by shipping a second file.
- R10's naming already assumes one artifact: `bwapi_c2.dll` with two headers and one `.def`.

**Vendor or submodule?** **Submodule, pinned** — consistent with how BWAPI itself comes in
(§10.1), and `BWEM-community`'s last commit is 2021-06-01 so the pin is stable. MIT permits
vendoring, so this is a maintenance preference rather than a constraint: a submodule keeps the
diff honest and makes the pin-bump procedure identical for both dependencies. R11.7 confirms the
licensing side.

---

## 5. Feeds forward

| To | Finding |
|---|---|
| **R11.5** | The link is a non-issue, so the remaining risk is entirely lifecycle and analysis cost |
| **R11.6** | BWEM composes with the closure in-process, so R7's synthetic-`GameData` harness can drive it directly — the three grid inputs BWEM reads are `isWalkable`, `isBuildable` and `getGroundHeight`, **all three of which `GameData` already carries** and R7's harness already populates |
| **§10.1** | The closure's file list is unchanged. BWEM adds a second source set beside it, not a change to it |
| **§12 roadmap** | BWEM is not a phase. It is 14 more TUs in the same CMake target and 98 more generated wrappers |
| **decision framework** | Direction **A** (one DLL, full bot-facing surface). B and C both required a link-closure surprise, and there is none |
