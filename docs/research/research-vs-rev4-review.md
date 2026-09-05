# Research (R1–R10) vs. the revision-4 change set

The rev-4 change set was written from a survey, before the experiments. R1–R10 have now run.
This is a directive-by-directive reconciliation and, at the end, the fork decisions and the v1
cut line they produce.

**Summary: rev-4 holds up well. Every directive survives in substance; six need their premise or
their trigger condition corrected, because the research came back with numbers rev-4 guessed at
and, in four places, guessed wrong. Nothing in R1–R10 argues for abandoning the project — R3's
audience is thin but real and currently active. Eight genuine forks were opened; all eight are
now decided (§3), and §3.2 derives the v1 cut line that follows: ~770 exported functions and
~850 constants across two headers, with the generator unconditional in phase 1.**

---

## 0. Decisions recorded this round

| Decision | Effect |
|---|---|
| Project renamed **`bwapi-c2`**; exported symbol prefix stays **`bwapi_`** | R10 §4 table applies. `RadicalZephyr/bwapi_c` → `bwapi-c2` |
| Internal C++ namespace stays **`BWAPI::CApi`** | Never exported; renaming buys nothing. R10 §4 note resolved |
| Primary consumers are **Python and C#** | Confirms rev-4 §B's consequence and R3/R4's recommendation. Rust demoted (see fork 8) |
| **Do not cut the surface to R2's minimal set.** Target what the surveyed bots collectively need | Changes rev-4 §H's trigger. See §2H below |
| **Draw-call usage data is not authoritative** — draws are dev/debug tooling, and §5.2 already reduces them properly | Correct, and it invalidates one of my own R2 recommendations. See §2H |
| **32-bit Linux is an explicit non-goal** | Closes R5's one incompatible target (`BulletData` 76 vs 80, `sizeof(GameData)` 404 bytes short). Record it in §2 non-goals; the layout hazard then cannot be hit |
| **BWEM must be wrapped** — second header, same §4 conventions | New research project R11 ([r11-bwem-research-plan.md](r11-bwem-research-plan.md)). Supersedes fork 5 below |
| **All eight forks decided** (2026-09-05) | §3 records each; §3.2 derives the cut line. Summarised in the plan's §13 decisions log |
| §5.8 ships as **functions + generated constants + a bulk-table fast path** | Fork 1. Rev-4 §H's table-instead-of-functions suggestion rejected |
| `canXxx`: **all base predicates**, no `*Grouped`, no `checkCommandibility` parameter | Fork 3 → 88 exports. §15 entries 17–18 |
| **Module mode becomes a scoped v2 item** | Fork 4. Appendix A stops saying "deferred indefinitely"; the grouped-command gap is its motivation |
| **Linux/OpenBW parked, not closed** — nothing OpenBW is ever linked or shipped from this tree | Fork 6. A client never links the engine; the exposure is a process boundary, not a link edge |
| **Synthetic fixtures by policy**; recorded ones contributor-local and gitignored | Fork 7 |
| **Rust stays as the proof-of-concept consumer** | Fork 8. Python and C# remain primary |

**On the draw point specifically, you are right and I was wrong.** R2 §4 recommended "ship 8 draw
functions in Map and Screen space; drop the generic `CoordinateType`-taking forms" on the grounds
that no bot passes a runtime coordinate space. But §5.2 already collapses ~90 declarations to **8
functions by taking `ctype` as a parameter** — which reaches the same count *and* keeps all three
spaces, at no extra cost. My recommendation would have traded away Mouse space and the runtime
`ctype` for nothing. §5.2 stands as written; R2 §4's cut-line advice for draws should be struck.
The usage data remains useful as evidence that the collapse is safe, not as a scope argument.

---

## 1. Scorecard

| | Directive | Research verdict |
|---|---|---|
| **A** | Prior-art section | ✅ do it — **but three facts in the table are wrong** (§2A) |
| **B** | Non-goal 1 as measured argument | ✅ delivered by R4; consequence confirmed. One gap (§2B) |
| **C** | Rewrite SWIG rejection | ✅ delivered by R8; **ground 4 is overstated** (§2C) |
| **D** | Delete two-stage x64 proof | ✅ confirmed and strengthened; **one new hazard** (§2D) |
| **E** | Mock server conditional on OpenBW | ⚠️ **false dichotomy** — neither branch is right (§2E) |
| **F** | Restate cross-platform non-goal | ✅ restatement correct; **three new costs** (§2F) |
| **G** | Cut the ceremony | ✅ unopposed; two cheap checks want adding back (§2G) |
| **H** | Re-scope surface, conditional generator | ⚠️ **trigger keys on the wrong number** (§2H) |
| **I** | Collapse the roadmap | ✅ unaffected except via E and H — **plus R11**: BWEM is not a phase, it is 14 more TUs in the same CMake target and 98 more generated wrappers (§3.1) |
| **J** | Fix the names | ✅ delivered by R10; **premise half wrong** (§2J) |
| **K** | Cite §5.10 corroboration | ⚠️ **corroboration is narrower than claimed** (§2K) |
| **L** | What stands unchanged | ✅ §4 stands entirely. §0 and §10.1 need corrections (§2L) |
| **M** | Purpose framing | ✅ R3 supplies the answer (§2M) |

---

## 2. Where the research changes a directive

### A — prior-art table: three corrections

- *"By v0.3 the author reported nearly all of BWAPI wrapped."* R1 measured it: **530 entry points,
  ~99% of the six dynamic interfaces and 0% of the ~1,030 constants and type accessors.** The
  table should say so, because "nearly all of BWAPI" is the sentence that makes a fork look
  attractive and it is not true.
- *"It also owns the name, and its downstream `bwapi-sys` occupies the crate name."* R10: **`bwapi-c`
  is free on crates.io, npm, PyPI and NuGet.** Only the GitHub repo name is taken. `bwapi-sys` is
  correctly reported.
- **The decisive fact is absent: `RnDome/bwapi-c` has no license** (R1 §8) — no file, no header,
  nothing in 148 commits, `license: null`. That forecloses the fork option regardless of code
  quality, and it belongs in the table, not a footnote.

Rows worth adding, all post-dating the survey: **`BradEwing/gobwapi`** (Go client, Feb 2026),
**`ceverettkoop/oscar_c`** (Zig bot built *on bwapi-c*), **six dead C#/.NET attempts**, and
**`basil-ladder/bwapi@linux-client-support`** (a working POSIX client mode, ~344 lines, unmerged).

### B — non-goal 1: measured, and one gap

R4 delivered the measurement. The trap is real but **mislocated**: the protocol is 926 lines in
JBWAPI (4%) and 212 in rsbwapi (0.6%); the game-rules port plus static type data is 72% and 86%.
Fourteen recorded divergence bugs in JBWAPI, three still open. JBWAPI #88 states the permanent
cost in one sentence — a fix in BWAPI will not fix it for JBWAPI.

**The gap:** rev-4 says to state "what a C ABI over the real `BWAPI::Game` gets for free that they
do not." R4 found the honest answer has a second half. **Grouped commands are unavailable to any
client bot in any language** (JBWAPI #70 — the BWAPI *server* does not support them), and
server-side bugs reach us unchanged. §B should say what we get free *and* what we do not, or the
claim will not survive a reader who knows the tracker.

### C — SWIG: ground 4 is overstated

R8 built SWIG 4.5.1 and ran it. Grounds 1–3 are confirmed, ground 2 more strongly than expected
(two type modules cannot coexist in one TU: `redeclaration of enum Enum` / `enumerator None`).

**Ground 4 — "none of the §4 conventions is expressible in typemaps" — is wrong and should be
softened, not deleted.** Tested working: packed `int64_t` positions, `int32_t` booleans, and
**integer handles including the `self` parameter**. What typemaps cannot do is change a function's
*arity*, which is what caller-provided buffers, sorted output and size-prefixed structs require.
Say that instead; it is both true and a stronger argument, because it identifies the structural
limit rather than asserting a blanket one.

Two grounds rev-4 lacks and R8 found: **`Game.h` hard-fails** (12 vararg errors; suppressing them
deletes 463 call sites including `drawTextScreen`), and **every by-value return is `new`'d and
never freed**. The fallback is confirmed: `clang -ast-dump=json` recovers all 81 `UnitType`
methods with full types, no new dependency.

### D — x64: confirmed, and one new hazard

R5 did better than cite JBWAPI: it computed the layout across six targets. **All four Windows
targets are byte-identical, `sizeof(GameData) == 33017048`** — the exact constant JBWAPI hardcodes
and gobwapi asserts in a passing test. No bitness conditionals in either. Delete the two-stage
framing as directed.

**New:** **32-bit Linux is the one incompatible target.** i386 System V aligns `double` to 4, so
`BulletData` is 76 rather than 80 and `sizeof(GameData)` is 33,016,644 — 404 bytes short. Silent
misreads, not a crash. This is a live input to §F, not an abstract caveat.

Also settled: the `clientInfo` pointer→`int` truncation at `Interface.h:72` is real but **does not
instantiate anywhere in the closure** (R6 §7), so §10.1's "exclude `Unitset.cpp` if troublesome"
contingency can be deleted outright.

### E — the mock server: rev-4's conditional has no true branch

Rev-4 frames it as: if OpenBW works, the mock shrinks; if not, the mock stays. **R7 says neither.**

OpenBW *builds* fine (GCC 11, zero errors, `BWAPILauncher` produced) and is *unusable in CI*: it
needs Blizzard's MPQs, **JBWAPI wrote both end-to-end workflows and disabled them in April 2022**,
and the StarCraft URL they downloaded from is now a 404. Nobody in the ecosystem runs BWAPI games
in public CI.

But the mock should not be built either, because R7 demonstrated the alternative in working code:
a **~60-line harness driving a synthetic `GameData`** through R6's 44-TU closure, exercising the
read path, BWAPI's real `canMove`/`canCommand` rule engine, and command emission —
`cmd[0]: type=Move unitIndex=0 x=1500 y=2500` — with no server, no shared memory, no pipe, no
StarCraft. JBWAPI reached the same design independently: `GameBuilder.java` is **48 lines**.

**Rewrite §E as a decision, not a conditional:** synthetic-`GameData` fixtures are the substrate;
a transport-only fake (~150 lines, no game semantics) covers the handshake if it needs covering;
live-server runs are manual and gated. That deletes the largest line item in the roadmap and
replaces it with something already proven to work.

### F — cross-platform: restatement right, three new costs

Keep rev-4's framing. Add what R5–R9 priced:

1. **OpenBW's engine has no license at all** (R9 §6) — no file, no README statement, no headers,
   `license: NONE`. Only the BWAPI *fork* is LGPL-3.0. Same blocker as bwapi-c, and it propagates
   to `basil-ladder/openbw`.
2. **OpenBW has no client mode** on its own repos (`connect()` throws). A working POSIX
   implementation exists on `basil-ladder/bwapi@linux-client-support` — ~344 lines, unmerged,
   five years idle, needs one `#include <string>` to build on GCC 11, and requires pinning *two*
   forks that must move together.
3. **32-bit Linux breaks the layout** (§D above), and **`CLIENT_VERSION` differs** — 10003
   upstream vs 10002 on the OpenBW fork, and `Client.cpp:120` refuses to connect on mismatch.

Net: Linux-via-OpenBW is not ruled out by physics, but it is ruled out by licensing today. Say
that.

### G — ceremony: unopposed, two additions

Nothing in the research defends any of the five cuts. Two things want adding back, both
script-sized and both run in seconds:

- **`r5/run-layout-dump.sh`** — the layout matrix, as the pin-bump regression check rev-4 §D
  already wants.
- **`r6/derive-closure.sh`** — builds the closure and links upstream's `ExampleAIClient`; would
  catch a pin bump that introduces a Storm, Util or Boost reference.

These are the *opposite* of ceremony: they replace prose assertions with a command.

### H — the surface: the trigger keys on the wrong number

R2's measurement: 95% of call sites at **195** entry points; per bot **160–215**; the
≥2-corpora-or-≥10-sites cut at **263** covering 97.7%; the **union across all seven corpora is
347**.

Rev-4's rule — *"if the 95% line lands under ~250 functions… the spec DSL, emitters, and coverage
audit become a phase-7 item"* — would therefore fire, and defer the generator. **That is the wrong
conclusion, for two independent reasons.**

**First, your scope decision.** "Cover what all surveyed bots collectively need" is the union, not
the 95% line. That is **347 entry points**: 270 interface, 70 type-data accessors, 7 other — of
which 8 are draws (which §5.2 collapses regardless) and 30 are `canXxx`.

**Second, and more important: the trigger measures the part that does not need a generator.** The
~270 interface functions are one-liners; R1 showed bwapi-c hand-wrote 530 of them successfully.
What nobody should hand-type is **§5.8: 848 constants and 185 type accessors** — and R2/R3 found
**five independent projects that transcribed that database by hand**: rsbwapi 25,367 lines,
gobwapi 4,261, UAlbertaBot's SparCraft and BOSS 1,119 each, the Zig bot 650 (partial). rsbwapi's
own commit log has *"fixed WeaponTypes being totally off."*

**Rewrite §H's trigger:** the generator's justification is not entry-point count, it is that the
static type database must be emitted from a machine-readable source or every consumer pays for it
again. Under that rule the generator is **unconditional for §5.8** and **optional for the
interface layer**. §9's emitters, `api.json` and coverage audit survive on the narrower claim;
the declaration-hash and divergence-register machinery still goes, per §G.

**And §H's §5.8 consequence needs reversing.** Rev-4 suggests "~150 static type-data functions may
be better as a single generated lookup table the host reads once." R2 measured those accessors at
**1,671 call sites, 20% of all BWAPI traffic**, at a higher density per entry point than the
interface classes. A table is how rsbwapi does it — and rsbwapi ships the table *because it has no
ABI to ask*. We do. This is fork 2 below.

### J — names: premise half wrong, delivered

`bwapi-sys` is correctly reported as unobtainable — R10 confirmed crates.io is first-come,
first-serve and will not transfer without the owner's approval, and the squatting clause does not
reach an abandoned-but-functional crate. But **`bwapi-c` is free on crates.io**, and rev-4 implies
otherwise. Resolved by the `bwapi-c2` rename either way.

### K — the corroboration is narrower than rev-4 claims

BWAPI4J's single-array-copy claim is real and R4 verified it in source
(`private native int[] getAllUnitsData()`). **But BWAPI4J batches because it is in-process JNI —
it has an FFI boundary. JBWAPI and rsbwapi are pure clients and do not copy at all**: they read
the mapped region in place via `Unsafe` offsets and a `Shm<GameData>` deref respectively.

So rev-4 §K's second paragraph actually cuts the other way: JBWAPI's headline performance claim —
that reading the mapped file directly beats marshalling — is an argument for *not* having a
boundary. §5.10 should be stated as **compensation for a per-call boundary we deliberately
reintroduce in exchange for BWAPI's real implementation**, with BWAPI4J as the precedent for
boundary-having designs specifically. That is still a good argument. It is not the universal one
rev-4 implies, and overstating it invites the obvious rebuttal.

### L — what stands: §4 entirely; §0 and §10.1 need edits

**§4 stands, as rev-4 says** — R8 incidentally paid it a compliment by showing three of its
conventions are cleanly expressible even in SWIG's typemap language.

**§0 needs three corrections** (R9): it reasons about `bwapi_c2.dll` as an Application linked
against BWAPI, but R6 showed the DLL **embeds BWAPI's compiled source**, so the binary conveys the
Library and needs a Corresponding Source pointer; the proposed README line ("the license does not
propagate to bots that merely call it") is false of a bot's *distribution*, which still owes LGPL
§4(a)–(c); and the System Libraries carve-out removes a *source-shipping* obligation rather than
granting permission to statically link — Microsoft's terms do that, separately.

**§10.1 needs three corrections** (R6): the file list omits a load-bearing include directory
(`include/BWAPI/Client`, without which `Shared/*.cpp` will not compile); **Boost has been `#if 0`'d
out of this tree since April 2017**, so "dropping both removes the Boost dependency" describes work
already done; and two MSVC-permissive-mode constructs (`CommandTemp.h:34` two-phase lookup,
`Convenience.h:33` `va_list&`) block every non-MSVC compiler.

### M — purpose framing: R3 supplies the answer

Rev-4 asks why the third attempt goes differently. R3's evidence: the request channel is empty
(two Python asks in eight years, both closed same-day), but the *building* channel is active — a
Go client started February 2026, a Zig bot running on bwapi-c since 2023, six dead C# attempts, a
vendored-rsbwapi Rust bot from December 2025. **One bot in 291 SSCAIT registrations is neither C++
nor JVM.**

The differentiators are concrete and both are things bwapi-c lacked: **§5.8 shipped** (the Zig bot
hand-wrote 650 lines of enums because bwapi-c had none) and **a license** (bwapi-c has none, which
is why that Zig repo vendors unlicensed headers into an MIT project).

---

## 3. Forks — all eight decided

Decided 2026-09-05. Each entry keeps the question as posed, then the decision and what follows
from it. **§3.2 derives the v1 cut line the decisions imply**, which is what fork 2 asked for.

**1. Does §5.8 ship as exported functions, a generated lookup table, or both? — RESOLVED: all
three.**
R2: 185 accessors, 848 constants; 80 accessors used at 1,671 sites (20% of all traffic).
Decided as recommended: **per-accessor functions** (§4's shape, and what the 1,671 sites are
actually written against), **constants generated into the header**, and a **size-prefixed bulk
table export as a §5.10-style optional fast path** for hosts that would rather pay one crossing
at startup than 185 per type. Rev-4 §H's "a table *instead of* functions" is rejected: a table is
what rsbwapi ships *because it has no ABI to ask*, and shipping only a table would force every
host to re-derive the accessor semantics it already has. Consequence: §5.8 grows a small export
block (~1 per type class) and §5.10's snapshot machinery is reused rather than duplicated.

**2. Where exactly is the v1 cut line now? — RESOLVED: derived, not chosen.**
Your reading was right — forks 1, 3 and 5 determine it, and §5.4 (already in the plan) determines
the filter block. Worked out in **§3.2**: **~671 exported functions and 848 constants in
`bwapi_c2.h`, plus 98 in `bwapi_c2_bwem.h` — call it ~770 functions across the two headers.**
That is ~30% *above* §1.8's 550–600 target, not below it. The consequence fork 2 existed to
settle: **the generator is phase 1 and unconditional for both blocks**, and its justification for
the interface layer is the coverage audit, not the typing (§3.2).

**3. Do the `canXxx` predicates get the draw treatment or the usage treatment? — RESOLVED: ship
them all, minus two mechanical exclusions.**
Ship every base predicate; exclude the `*Grouped` variants and do not expose the
`checkCommandibility` parameter. Measured against this tree's headers rather than R2's
name inventory: `Unit.h` + `Game.h` declare **105 `canXxx` functions under 68 unique names — 88
declarations / 57 names non-Grouped, 17 declarations / 11 names `*Grouped`**. The
`checkCommandibility` flags are default arguments, not separate overloads, so suppressing them
costs no declarations; every `can_*` behaves as `checkCommandibility = true`. **v1 exports 88
`can_*` functions.** Both exclusions are recorded in §15 (entries 17–18).

*Note on the count.* R2 §3 reported "74 `canXxx` predicates"; that figure counted name/class pairs
across the whole inventory including `UnitType::canProduce`-class accessors, which belong to
§5.8. The Unit/Game family is 68 names. The 29-used / 45-never split is unaffected, and is now
moot for scope.

*Available but not taken:* the no-target and target-taking forms of the same predicate
(`canAttackUnit()` vs `canAttackUnit(Unit)`) could collapse into one function with a
`BWAPI_NONE` target, the way §5.2 collapses draws. **Declined** — §4 already gives an invalid
handle a neutral return, so `-1` would be ambiguous between "no target supplied" and "bad handle",
and the two queries mean genuinely different things. 88 distinct signatures stay distinct.

**4. Does module mode come back off the shelf? — RESOLVED: v2, not v1.**
R4's finding stands: grouped commands are impossible in client mode in any language (JBWAPI #70 —
the BWAPI *server* does not implement them), so this is a capability gap no competitor can close
either. It is a real reason for module mode to exist, but not a reason to build it now.
**Appendix A stops saying "deferred indefinitely" and becomes a scoped v2 item**, with the
grouped-command gap as its stated motivation; the README documents the limitation for v1.
Nothing in §12 changes.

**5. ~~Is BWEM / map analysis an explicit non-goal?~~ — RESOLVED earlier: in scope.**
Wrapped through a second header under the same §4 conventions. Scoped and executed as **R11**
([plan](r11-bwem-research-plan.md)); see [R11.4](r11-4-bwem-link-closure.md) (ten symbols, all
already in the closure) and [R11.8](r11-8-bwta2.md) (BWTA2 closed as a non-goal). Outcomes in
§3.1; divergences in §15.1; the dependency patch in §15.2.

**6. Linux/OpenBW: closed or parked? — RESOLVED: parked, and the exposure is smaller than R9
made it look.**
Your instinct to separate it from the library is right, and the separation already exists in the
architecture: **a client never links the engine.** R6 §11 and R7 §3 together establish that
OpenBW runs as its own process (`BWAPILauncher`) and a client reaches it over shared memory plus
a Unix socket. So a Linux target needs (a) a POSIX port of the client transport — **7 Win32
imports, all in one TU, `Client.cpp`** (R6 §2), with the port already written line-for-line on
`basil-ladder/bwapi@linux-client-support` (~344 lines, R6 §11) — and (b) a server binary the user
supplies. (a) is a patch on a pinned **LGPL** dependency, exactly the §15.2 pattern BWEM already
established. (b) we neither link nor distribute.

**R9's unlicensed-engine blocker therefore applies to *shipping OpenBW*, not to *supporting
Linux*.** What remains are ordinary engineering costs, all measured: the `CLIENT_VERSION`
10002/10003 split (`Client.cpp:120` refuses to connect on mismatch), no public CI (R7: MPQs, and
JBWAPI's own workflows disabled since April 2022), and i386 — now a non-goal, and **Linux x86-64
agrees with Windows at `sizeof(GameData) == 33,017,048`** (R5 §2), so the one layout hazard is
already outside the matrix.

Recorded as: **parked for v1; not closed; no OpenBW artifact is ever built, linked or
distributed from this tree.** Rev-4 §F's "possible second target" wording stands, with the
process boundary as the reason it can stay open at no licensing cost.

**7. Test fixtures: synthetic only, or synthetic plus recorded? — RESOLVED: as recommended.**
**Synthetic by policy.** Recorded fixtures stay contributor-local and gitignored; JBWAPI's
fifteen `.bin` files are not vendored. R9 §7's finding is why this is cheap: `GameData` carries
no Blizzard static tables, so the only provenance exposure is *map terrain* — and R11.6 showed a
hand-built `GameData` drives BWEM's full analysis too, so neither header needs a recorded map.
Pairs with §3.1's shared fixture builder.

**8. Is Rust in `bindings/` at all? — RESOLVED: yes, as the proof-of-concept.**
Rust stays the test consumer that proves the ABI is bindable — the cheapest such proof, and R2
gives a usage baseline from Styx2. It is **not** positioned as a product competing with rsbwapi,
but the door is not closed either: rsbwapi lacks latency compensation, `getBuildLocation`,
`clientInfo` and `registerEvent`, all of which a C ABI provides for free, so a Rust binding on
`bwapi-c2` has enough additional value that building one is reasonable. Python and C# remain the
primary consumers (§0).

---

### 3.1 What R11 closed, and what it added

**Closed:**

| | Outcome |
|---|---|
| Fork 5 — BWEM in or out | **In.** R11.4: ten BWAPI symbols, all already in the closure, zero Storm/Util/Boost, 327 KB. One DLL, submodule-pinned. R11.8 closes BWTA2 as an explicit non-goal (no maintained repo since 2018; needs Boost **and** CGAL) |
| Does §4 survive a second library? | **Yes, unmodified.** R11.3 mapped every BWEM shape onto an existing rule. Six BWEM divergences recorded in §15.1 — all of them *applications* of §4, not exceptions to it |
| Is the synthetic-fixture substrate general? | **Yes.** R11.6 drove BWEM's full analysis from a hand-built `GameData`. R9's map-provenance question does not arise for BWEM either |

**Added, and these are new constraints rather than open questions:**

1. **The wrapper carries a patch on a pinned dependency.** R11.6 found an upstream BWEM crash on
   re-initialisation; §15.2 records the `Map::ResetInstance` patch. §10.3's pin-bump procedure now
   covers **two** dependencies and must re-apply it.
2. **Teardown becomes explicit.** BWEM's singleton must be destroyed before the `GameData` mapping
   goes away and before static destruction. `bwapi_client_disconnect()` grows a BWEM step — the
   first thing in either header with a required shutdown order.
3. **The first non-mechanical wrappers.** The three filtered `on_*_destroyed` hooks (§15.1 #14)
   have real logic and are hand-written, not generated. Worth naming in §9 so the generator's
   coverage claim stays honest.
4. **`§4.1` needs no BWEM section.** R11.5: BWEM has nothing per-frame at all, only a ~450 ms
   match-start call and three event hooks. A one-line note, not a subsection.
5. **A shared fixture builder.** R11.6's two gotchas — neutrals arrive via the `UnitDiscover`
   event stream, and `isNeutral()` reads a `PlayerData` flag rather than the player type — belong
   in one helper used by both headers' tests, solved once.

**Unchanged by R11:** forks 1, 2, 3, 4, 6, 7 and 8, all since decided above.

---

### 3.2 The v1 cut line, derived from the decisions

Fork 2 asked where the line lands. It is now a calculation rather than a choice, so here is the
calculation. Counts are from this tree's headers, comments and templates stripped, taken with
`Game.h`, `Unit.h`, `Player.h`, `Region.h`, `Force.h`, `Bullet.h` — **542 member function
declarations**, of which 105 are `canXxx`, 49 are `draw*`, and 388 are everything else.

| Block | Declared | v1 exports | Rule that decides it |
|---|---|---|---|
| Interface methods (ex-`can`, ex-`draw`) | 388 | **361** | Ship the declared surface, less two mechanical merges: 13 filter-taking overloads (§5.4 — the unfiltered sibling ships instead) and 14 `int x, int y` overloads that §4's position rule makes identical to their `Position` sibling |
| `canXxx` | 105 | **88** | Fork 3: all base predicates, no `*Grouped` (17 declarations), `checkCommandibility` suppressed |
| `draw*` | 49 | **8** | §5.2's `ctype` collapse — all three coordinate spaces kept |
| §5.8 type-data accessors | 185 | **185** | Fork 1: ship whole, as functions |
| §5.8 bulk-table export | — | **~14** | Fork 1: one size-prefixed table per type class, optional fast path |
| Lifecycle, error latch, version, connection | — | **~15** | §4 machinery |
| **`bwapi_c2.h` total** | | **~671 functions** | plus **848 constants** (§5.8, generated) |
| `bwapi_c2_bwem.h` | | **98** | R11.3's sketch, unchanged |
| **Both headers** | | **~769 functions, ~850 constants** | |

Three things follow, and the first is the one that matters.

**The generator is phase 1, unconditional, for both blocks — but for different reasons.** For
§5.8 the reason is the one R2 found: 848 constants and 185 accessors must come from a
machine-readable source or every consumer transcribes them again, as five projects already have.
For the interface layer the reason is new and comes out of this table: **542 declarations map to
~457 exports through five documented exclusion rules**, and nothing but a coverage audit can show
that mapping is complete. R2 concluded ~200 hand-written one-liners need no DSL, and that was
right for ~200 chosen by usage. It is not right for 457 chosen by rule, where the rules are the
thing that can be wrong. Rev-4 §H's "if the 95% line lands under ~250, defer the generator"
trigger never fires and should be deleted rather than re-tuned.

**§1.8's 550–600 was low, not high.** The measurement did not shrink the completeness target; it
raised it by about 30%, and moved the justification from "be complete" to "here are five rules,
and here is the audit that proves we followed them." Worth saying plainly in §1.8, because the
number will otherwise look like scope creep.

**Two of R2's four cut recommendations are now withdrawn**, both for the same reason and both in
the plan's favour — see §4. The `canXxx` cut ("ship 10, defer ~120") is withdrawn by fork 3. The
filter recommendation ("ship ~26 as constants") is withdrawn by **§5.4, which already declines an
enum filter DSL** on the grounds that it re-encodes upstream semantics in a switch statement that
silently rots. That argument was sound when it was written and R2 did not engage with it. The
generalisation from the draw correction holds a third and fourth time: *usage frequency is
evidence about what is safe to merge, not about what is safe to omit.*

---

## 4. One correction to my own work

R2 §4's recommendation — "ship 8 draw functions in Map and Screen space; drop the generic
`CoordinateType`-taking forms" — is **withdrawn**. §5.2's collapse reaches 8 functions while
keeping all three coordinate spaces and the runtime `ctype`, which is strictly better. The R2
usage data stands as evidence that the collapse is safe; it should not have been read as a scope
argument. Your framing — draws are development tooling, so tournament-bot usage undercounts them —
is the right correction and it generalises: **usage frequency is evidence about what is safe to
merge, not about what is safe to omit.**

**Two more R2 recommendations are withdrawn on the same principle**, both after the fork
decisions:

- *"`canXxx`: ship 10, defer ~120"* — withdrawn by fork 3. The family is not dev tooling, but it
  is the block where "never called in a tournament bot" is weakest evidence: a predicate exists
  precisely so a bot can ask a question it does not yet know it will ask.
- *"Filters: ship ~26 as constants"* — withdrawn by **§5.4**, which had already declined an
  enum filter DSL because it re-encodes upstream semantics in a switch that silently rots. R2
  proposed the DSL without engaging that argument. The plan was right; the filter-less queries
  plus host-side filtering stand, and the 13 filter-taking overloads simply do not cross.

So of R2's four cut-line recommendations, **three are withdrawn and the fourth (§5.8: "ship it
all — this is the product") is the one that survives and was strengthened.** That is worth
recording as a methodology result, not just a tally: R2 measured usage well and reasoned from it
badly, in a consistent direction, four times out of four.
