# R11.8 — BWTA2: settle it and close it

**Headline: closed. BWEM superseded BWTA2, the ecosystem has already migrated, BWTA2 has no
maintained canonical repository, and it drags in CGAL and Boost — dependencies R6 spent effort
proving the closure does not have. JBWAPI's `bwta` package, the one thing that looked like
continuing demand, turns out to be a 409-line shim implemented on top of BWEM. Explicit non-goal.**

---

## 1. The ecosystem has migrated, and the split is by vintage

| Bot | Last push | BWTA files | BWEM files |
|---|---|---|---|
| **Stardust** (SSCAIT #1) | 2026-08 | **0** | 59 |
| **McRave** | 2026-05 | **0** | 69 |
| RazeAndPlunder | 2017-04 | 6 | 21 |
| Steamhammer | 2022-12 | 1 | 0 |
| UAlbertaBot | 2022-03 | 16 | 0 |
| OpprimoBot | 2015-11 | 12 | 0 |

**The two currently-developed bots use BWEM exclusively and reference BWTA zero times.** BWTA
usage is concentrated in bots whose last commit is 2015–2022, and RazeAndPlunder shows the
transition mid-flight (both, with BWEM dominant).

---

## 2. There is no maintained BWTA2

Searching GitHub for `bwta2`:

| Repo | Stars | Last push | Note |
|---|---|---|---|
| `s7jones/bwta2` | 4 | **2018-12-09** | LGPL-3.0 |
| `adakitesystems/BWTA2` | 2 | 2018-04-24 | LGPL-3.0 |
| `Tyrion3000/bwta2` | 0 | 2026-02-28 | LGPL-3.0; created and pushed the same day, no parent — a re-upload, not maintenance |

**No canonical home, no active development, nothing since 2018 that is not a re-upload.** Compare
BWEM-community: 17 stars, nine contributors, and the thing every active C++ bot builds against.

---

## 3. JBWAPI's `bwta` package is a BWEM shim, not a BWTA port

This was the one signal that looked like live demand, and it points the other way. From
`src/main/java/bwta/README.md`, in full:

> fake stripped down BWTA with methods pointing to their respective BWEM values (except Polygons)

and from `BWTA.java`:

```java
public class BWTA {
    private static BWEM bwem;
    static Map<Area, Region>          regionMap;
    static Map<ChokePoint, Chokepoint> chokeMap;
    static Map<Base, BaseLocation>     baseMap;
```

409 lines translating BWEM's `Area` / `ChokePoint` / `Base` into BWTA's `Region` / `Chokepoint` /
`BaseLocation`, so that bots written against the old API keep compiling. **Even the JVM ecosystem
treats BWTA as a legacy name to be satisfied by BWEM underneath.**

That also explains JBWAPI issue #86 (R11.2), which reports a BWEM problem in BWTA vocabulary
(`BWTA.getStartLocations()`, `BWTA.analyze()`): the shim forwards, so a BWEM bug surfaces with a
BWTA label.

**If BWTA-shaped API compatibility ever matters to us, the answer is the same one JBWAPI reached:
a thin shim over `bwapi_c2_bwem.h`, in the host language, not in the ABI.**

---

## 4. The dependency argument is decisive on its own

BWTA2's README lists its build requirements:

- **Boost 1.56**
- **CGAL 4.4** — used to build the Segment Delaunay Graph

R6 spent real effort establishing that the client closure pulls **no Boost** (and that the plan's
stated reason for that was stale). R11.4 established that BWEM adds **ten BWAPI symbols and
nothing else**. Wrapping BWTA2 would reintroduce Boost and add CGAL — a large templated
computational-geometry library — to a project whose entire value proposition is being a small,
dependency-light C ABI that a Python or C# developer can consume without a C++ toolchain.

BWTA2 is LGPL-3.0, so licensing would not have blocked it. The dependencies would.

---

## 5. Answers to the questions R11.8 asked

**Is BWTA2 still used by any active bot?** No. Stardust and McRave — the two currently-developed
C++ bots in the corpus — reference it zero times. Usage is confined to bots last touched
2015–2022.

**Is it maintained?** No. Nothing since 2018 except a 2026 re-upload with no history.

**Does anything it provides have no BWEM equivalent?** Only **polygons** — JBWAPI's shim covers
everything else and flags exactly that exception ("except Polygons"). BWEM exposes region geometry
as chokepoint `Geometry()` and area bounding boxes rather than as region outlines. Nobody in the
corpus asked for polygons, and R11.1 found zero usage of anything shaped like them.

---

## 6. Recommendation

**Add an explicit non-goal:**

> **BWTA/BWTA2 is not wrapped.** BWEM supersedes it, every actively-developed bot has migrated,
> BWTA2 has had no maintained repository since 2018, and it requires Boost and CGAL — dependencies
> this project exists to avoid. A host that needs BWTA-shaped names can write the same shim
> JBWAPI did, in its own language, over `bwapi_c2_bwem.h`.

That closes the question rather than leaving a second map library open, which is what R11.8 was
scoped to do.
