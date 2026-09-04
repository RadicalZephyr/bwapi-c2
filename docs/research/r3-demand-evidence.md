# R3. Demand evidence from public artifacts

No outreach. Everything below is from crates.io, PyPI, the SSCAIT ladder, GitHub code and issue
search, and the repositories themselves, gathered 2026-09-04.

**Headline, and it is not the one the plan anticipated. Nobody asks for BWAPI bindings — the
request channel is empty to three decimal places. But people keep *building* them, right now,
in languages with no option: a Go client started February 2026, a Zig bot built on `bwapi-c`
itself, two independent .NET clients in 2023. Every one of them pays the same toll, and one of
them already paid it using the artifact R1 examined. The unserved audience is not two people and
it is not hypothetical; it is a steady trickle of one-person projects, each re-deriving the same
several thousand lines.**

---

## 1. Download counts: the Rust crates have no users

| Crate | All-time | Last 90 days | Versions | Reverse deps |
|---|---|---|---|---|
| `rsbwapi` | 14,923 | **45** | 12 | **0** |
| `bwapi_wrapper` | 13,929 | 104 | 9 | 0 |
| `bwapi-sys` (RnDome) | 6,127 | **23** | 3 | 0 |
| `bwapi` (RnDome) | 3,787 | 17 | 2 | 0 |

Raw totals flatter these. The per-version breakdown does not:

```
rsbwapi   0.0.1 (2020) 1209    0.2.4 (2021) 1282    0.3.3 (2024) 1716
          0.2.0 (2020) 1207    0.3.0 (2022) 1443    0.3.4 (2024) 1311
          0.2.3 (2021) 1202    0.3.2 (2023) 1561    3.6.3 (2025)   75
```

**Every version has roughly the same count regardless of age.** Real adoption produces a
strongly skewed distribution — the current release dominates, old ones flatline. A uniform
~1,200–1,700 across five years is the signature of index crawlers and mirrors, not users, and
the current release (3.6.3, Nov 2025) has 75 downloads total. Daily series confirm it:
rsbwapi averaged 5.0/day over the last 90 days, with many zero days; `bwapi-sys` — whose
upstream has been dead for eight years — averaged 2.9/day, which is what a crate with *no* users
looks like.

**Zero reverse dependencies on crates.io for all four.** And the one real Rust bot, Styx2, takes
rsbwapi as a **git dependency**, not from crates.io:

```toml
rsbwapi = { git = "https://github.com/Bytekeeper/rsbwapi.git", features = ["metrics"] }
```

So the download figures do not undercount a hidden population. There is no population.

---

## 2. The SSCAIT ladder: 291 bots, and effectively one is neither C++ nor Java

Scraped all 291 bot profiles from `sscaitournament.com`. SSCAIT records a `Bot type` per entry.

| Bot type | Meaning | All-time | Currently enabled |
|---|---|---|---|
| `AI_MODULE` | C++ DLL, module mode | 144 (49%) | 69 (61%) |
| `JAVA_MIRROR` | BWMirror (JNI over C++ BWAPI) | 86 (30%) | 25 (22%) |
| `JAVA_JNI` | JNIBWAPI / BWAPI4J | 30 (10%) | 5 (4%) |
| `EXE` | standalone client-mode executable | 31 (11%) | 14 (12%) |
| | | **291** | **113** |

`EXE` is the only category that can hold a non-C++, non-JVM bot, and reading all 31
descriptions, almost all of them are C++ anyway — `Zerg_Hell` ("C++ client bot written in 8
hours"), `Raze_and_Plunder` ("modern C++ bot"), the four `tscmoo*` entries, `SAIDA`,
`CherryPi_2018_AIIDE_MOD` (Facebook's TorchCraft bot, C++ core).

The exception is **`StyxZ2`** — Bytekeeper's Rust bot on rsbwapi, ELO 2437, currently enabled.

**One bot out of 291 registrations across fourteen years is written in a language other than
C++, Java, or a JVM language.** JVM languages are well represented on top of the Java bindings
(PurpleWave in Scala, Styx in Kotlin, Atlantis in Java), which is the point: where a good
binding exists, bots follow it.

*(BASIL was unreachable throughout this research — `basil-ladder.net` returned HTTP 522 on every
attempt, and archive.org was itself offline, so I could not date the outage or recover its bot
list. BASIL draws its roster from SSCAIT opt-ins anyway, so the SSCAIT figures above are the
primary registry either way.)*

---

## 3. The request channel is empty

Searched `bwapi/bwapi`, `Bytekeeper/rsbwapi`, `JavaBWAPI/JBWAPI`, `OpenBW/bwapi`,
`OpenBW/BWAPI4J`, `RnDome/bwapi-c`, and all of GitHub.

**Total binding requests found anywhere: two.** Both on `bwapi/bwapi`, both for Python, both
closed within a day:

- **#832** (2019-05-17, `elcolie`) — *"Do you have any play to support Python?"* Closed same day
  by `dgant` pointing at TorchCraft and pybrood. Reply: "Thank you."
- **#882** (2022-01-12, `Anonymous9999999`) — *"Any possible plans to support higher-level api
  written in a script language, e.g. python?"* `dgant` links pybrood, PyBW, TorchCraft;
  `heinermann` closes it: **"No plans in core BWAPI to support scripting languages."**

Searches for Rust, JavaScript, or "C API" binding requests, across all repositories, return
**zero** issues. `RnDome/bwapi-c`'s tracker contains no request for anything from an outside
user in eight years (R1 §9).

**The correct reading is not "there is no demand." It is that this community does not file
feature requests — it forks and builds.** Which is measurable, and is what the rest of this
document measures.

---

## 4. Code search: `BWAPIC.h` has zero consumers; `rsbwapi` has four

| Query | Hits | Real consumers |
|---|---|---|
| `BWAPIC.h` | **0** | none |
| `libBWAPIC` | 4 | bwapi-c's own README/CMake, one Zig bot, one third-party research note |
| `bwapi-sys` | 31 | **none** — all crates.io index mirrors, RnDome's own repos, and `genact` fake-terminal wordlists |
| `rsbwapi` | 103 | 4: `Bytekeeper/Styx2`, `gnmerritt/barcode`, `alexmickelson/broodwarWineBot`, `BradEwing/gobwapi` |

Not one file on GitHub `#include`s bwapi-c's header. `bwapi-sys`'s only non-mirror appearances
are inside the RnDome org. rsbwapi, by contrast, has four genuine downstream users — none of
whom appear in its crates.io numbers, because three of the four vendor or git-pin it.

---

## 5. What people build instead: twenty attempts, five approaches, one survivor per language

This is the substance of R3. Every non-C++ BWAPI binding I could find:

### Pure-client reimplementations (read the shared memory, re-derive everything)

| Project | Lang | Created | Last push | Status |
|---|---|---|---|---|
| `JavaBWAPI/JBWAPI` | Java | 2018-09 | **2026-02** | **Alive.** The Java standard |
| `Bytekeeper/rsbwapi` | Rust | 2019-03 | **2026-02** | **Alive.** One bot on it |
| `BradEwing/gobwapi` | **Go** | **2026-02** | 2026-03 | **New.** 14,141 lines |
| `acoto87/bwapi.net` | C# | 2023-01 | 2023-01 | Dead after 15 days |
| `kant2002/NBWAPI` | C# | 2023-07 | 2023-07 | Dead after 1 day |

### Wrapping the real C++ BWAPI (JNI / pybind11 / C++-CLI)

| Project | Lang | Created | Last push | Status |
|---|---|---|---|---|
| `vjurenka/BWMirror` | Java | 2015-02 | 2017-11 | Dead; still 86 SSCAIT bots |
| `OpenBW/BWAPI4J` | Java | 2017-03 | 2019-07 | Dead |
| `JNIBWAPI/JNIBWAPI` | Java | 2015-02 | 2015-02 | Dead |
| `neumond/pybrood` | Python | 2016-09 | 2018-06 | Dead; 6 open unanswered issues |
| `crass/PyBW` | Python | 2011-11 | **2011-11** | Dead after 2 days |
| `Lamarth/BWAPI-CLI` | .NET | 2015-12 | 2017-04 | Dead |
| `satikcz/BWAPI-CLI` | .NET | 2018-08 | 2018-08 | Dead after 2 days |
| `suegy/bwapi-mono-bridge2` | C# | 2015-03 | 2021-01 | Dead |
| `kant2002/bwapidotnet` | C# | 2022-03 | 2022-03 | Dead after 0 days |
| `kazenshi/bwapicore` | .NET | 2019-06 | 2019-06 | Dead after 0 days |
| `squeek502/BWAPI-Lua` | Lua | 2017-02 | 2017-06 | Dead |
| `dgant/BWAPI-Lua` | Lua | 2009 (uploaded 2021) | 2021-04 | Dead, self-described "ancient" |

### Over a C ABI

| Project | Lang | Created | Last push | Status |
|---|---|---|---|---|
| `RnDome/bwapi-c` | C | 2017-05 | 2019-02 | Dead (R1) |
| **`ceverettkoop/oscar_c`** | **Zig** | **2023-12** | **2026-02** | **Active hobby project, built on `bwapi-c`** |

### Other

| Project | Approach | Created | Status |
|---|---|---|---|
| `TorchCraft/TorchCraft` | Socket protocol → Lua/Python | 2016-12 | **Archived** 2021 (1,398 stars) |
| `mapinguari/SC_HS_Proxy` | Haskell over a proxy | 2013-06 | Dead 2014 |
| `ratiotile/bwapi-nim-test` | Nim, direct C++ binding | 2017-06 | Abandoned incomplete |

**Six independent .NET attempts. Three Python. Two Lua. One each of Go, Zig, Nim, Haskell.**
Not one of the non-JVM ones consolidated. C# has been attempted six times and has no living
binding today.

---

## 6. The Zig bot is the finding

`ceverettkoop/oscar_c` — "Starcraft Brood War bot written in Zig", MIT, created 2023-12-30, last
commit 2026-02-22. Its README:

> BWAPI 4.2 broodwar DLL bot written in Zig using the BWAPI-C wrapper.
> `zig build -Dwindows=true` will cross compile a 1.16.1 compatible DLL bot…
> for other platforms (using openbw), you will probably have to build openbw and bwapi-c from
> source

It vendors `include/{Game,Unit,Types,…}.h` **byte-identical to RnDome/bwapi-c**, plus prebuilt
`BWAPIC.dll` and `BWAPI.dll`. `src/bwapi_module.zig` is headed *"human written/readable functions
at top, followed by auto generated C to zig mapping"* — i.e. `zig translate-c` over bwapi-c's
headers, which is exactly the workflow a C ABI is supposed to enable, working as intended in a
language nobody planned for.

And then, precisely as R1 and R2 predicted, they had to write the missing half themselves:
**`src/bwenums.zig`, 650 lines**, hand-transcribing `UnitType`, `Race`, `Errors`, `TechTypes`,
`Orders`, `UnitCommandType`, `UpgradeTypes`, and a bespoke `WhatBuilds` table — with the comment
*"a few are missing bc not expected to implement."* bwapi-c ships zero constants, so the Zig
developer transcribed the eight namespaces they needed and skipped the rest.

Three qualifications, stated plainly: it is a 3,399-line hobby project, 0 stars, last commit
message "wip", and it has never appeared on a ladder. It is a data point about *what a C ABI
enables*, not about audience size. And it vendors an unlicensed library's headers into an MIT
repository — R1's licensing problem, already in the wild.

---

## 7. Everyone rebuilds the same table

R2 found rsbwapi carrying 25,367 lines of transcribed static type data. R3 turns that into a
pattern, because the new bindings do it too:

| Project | Static type table | Lines |
|---|---|---|
| `rsbwapi` (Rust) | `unit_type.rs` + weapon/upgrade/tech | **25,367** |
| `gobwapi` (Go) | `pkg/bwapi/unittype_data_table.go` | **4,261** |
| `UAlbertaBot/SparCraft` (C++) | vendored `UnitType.cpp` | 1,119 |
| `UAlbertaBot/BOSS` (C++) | vendored `UnitType.cpp` | 1,119 |
| `oscar_c` (Zig) | `bwenums.zig` | 650 (partial) |

Five transcriptions, four languages, none aware of the others. This is the recurring cost the
ecosystem pays, and it is the clearest thing a C ABI could actually stop.

---

## 8. Why the Python attempts died

`neumond/pybrood` (pybind11 over C++ BWAPI, last release 2017-06-16, dead 2018-06) has six open
issues, **none of them ever answered**, and they are all coverage complaints:

- #2 (2017-04) *64-bit builds on pip*
- #4, #5 (2017-09) *"Type not registered yet" on import*; *Why isn't Position/TilePosition/WalkPosition wrapped?*
- #6 (2018-05) *getLastCommand wrapped?*
- #7, #8 (2018-09) *adding other functions besides onFrame()?*; *Is there any way to create a UnitType with pybrood.UnitTypes?*

The last is the same gap R1 found in bwapi-c and §5 found in oscar_c: **users cannot construct or
query type constants.** `crass/PyBW` was abandoned two days after creation in 2011. TorchCraft —
1,398 stars, Facebook AI Research, the most successful non-C++ access path BWAPI ever had — was
**archived in 2021** when FAIR moved on.

`ratiotile/bwapi-nim-test` is the most articulate autopsy, and it is a firsthand statement of the
problem a C ABI exists to solve:

> Some C++ code is necessary to implement the AIModule interface…
> It would be more efficient to automatically generate wrappers. There are 2 approaches:
> using/enhancing the existing c2nim, or building my own wrapper generator. … Started working on
> this, but it's incomplete.
> **## Problems / ## How to wrap `operator->()`?** … I couldn't get it to work as two separate
> wrapper methods. I needed to trick the compiler…

Someone tried to bind C++ BWAPI directly from Nim, hit C++ ABI and name-mangling walls, started
writing a generator, and stopped. Same author filed three of pybrood's six unanswered issues.

---

## 9. Answers to the questions as asked

**How many bots exist outside C++ and Java, and what do they use?** On SSCAIT, one — `StyxZ2`,
Rust, on rsbwapi — out of 291 registrations. Off-ladder, a handful of hobby projects: a Zig bot
on bwapi-c, two Rust bots on rsbwapi, and a Go client with no bot yet.

**Is there a visible unserved language, or is the unserved set hypothetical?** Visible, and
plural. **C# is the strongest signal — six independent attempts, zero survivors**, and both 2023
attempts were pure-client reimplementations abandoned within days. **Python is the second** —
three attempts, all dead, and the two requests that were ever filed were for Python. Go and Zig
each have exactly one active project apiece, both started in the last three years.

**Has anyone tried and abandoned a Python/JS binding, and why?** Three Python attempts. pybrood
died with six unanswered coverage complaints; PyBW died in two days; TorchCraft was archived when
its corporate sponsor left. No JavaScript attempt exists at all. The Nim attempt names the cause
directly: binding C++ from a non-C++ language means fighting `operator->`, mangling, and the
AIModule vtable, and the generator you need to make it tractable is itself a project.

---

## 10. What this does to scope

The plan asked for the honest paragraph, including the possibility that the honest answer is
"two people." Here it is.

**The audience is small, and it is real, and it is currently active.** In the three years to
2026: a Go client (Feb 2026), a Zig bot on bwapi-c (2023–2026), a vendored-rsbwapi Rust bot (Dec
2025), two C# clients (2023). None of these people filed an issue asking anyone for anything;
they are not reachable through the channel the plan hoped to measure. Ladder representation is
one bot out of 291, and that number will not move much — SSCAIT is a C++ and Java community and
will remain one.

Three things follow for scope, and they mostly agree with where R1 and R2 landed.

**Rust should probably come out of scope.** rsbwapi is alive, maintained through Feb 2026, has a
competitive bot behind it, and has four downstream users. It is the counter-example to non-goal
1, not the audience for a C ABI. Serving it would be duplicating the one language that is
already served outside C++ and Java. (R4 owns the final call.)

**C# is the strongest unserved target and is not in the plan's scope list.** Six attempts and no
survivor is a louder signal than anything Python or Zig produced, and the 2023 pair chose the
hardest possible path — reimplementing the client protocol in C# — because nothing else was
available. Whether that changes the in-scope binding set is a real question the plan should
answer rather than assume.

**Ship the static type data or do not ship.** Five independent transcriptions across four
languages, and the one existing C ABI is unusable without a companion table its own users hand-
write. R2 already concluded §5.8 is the product; R3 shows what its absence costs in the field.

And the direction-framework consequence: **R3 does not support direction C.** "No audience beyond
the served languages" is contradicted by a Go client started six months ago, a Zig bot that
already runs on the very artifact this research examined, and six abandoned C# attempts. The
audience is thin, but it exists, it is unserved, and it is currently paying several thousand
lines a head for the privilege. What R3 does support is *scoping to that audience honestly* —
a small, complete, well-licensed C ABI with the type database in it, not a 600-function
completeness project defended by eight phases of process.
