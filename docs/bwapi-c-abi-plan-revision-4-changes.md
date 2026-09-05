# BWAPI C ABI plan — revision 4 change set

Directives to apply to revision 3. Each is a decision, not an option. Section references are to
revision 3.

Revision 3 absorbed the revision-2 review cleanly and is internally consistent. What it is
missing is the outside world. A survey of the ecosystem turned up prior art that occupies this
project's exact position, two live projects serving the two languages we scoped, and a
64-bit-capable open-source engine — and each of those contradicts a premise the document is
currently resting on. The changes below are almost entirely about premises and scope, not
conventions. **The §4 conventions stand unchanged; they are the part of this plan that was
worth writing.**

---

## A. Add prior art — new §1.9, and a rewrite of §1.6

§1.6 currently treats `swig.i` in the BWAPI tree as the only prior art. It is not even close to
the most important. Add a section covering all of it, because every design decision downstream
either agrees or disagrees with something already built.

| Project | What it is | What it means for us |
|---|---|---|
| **RnDome/bwapi-c** | A C bindings library for BWAPI. 148 commits, v1.0, module *and* client mode, MSVC on Windows and GCC on Linux, OpenBW support, `registerEvent` and `issueCommand` exposed. By v0.3 the author reported nearly all of BWAPI wrapped. Dormant ~8 years; CI on Travis and AppVeyor, both dead | **This project already exists.** It also owns the name, and its downstream `bwapi-sys` occupies the crate name §10.4 assumes we can publish |
| **RnDome/bwapi-sys, bwapi-rs** | Raw FFI and idiomatic Rust over bwapi-c, on crates.io | Exactly the two-layer split §7 proposes, already published and abandoned |
| **Bytekeeper/rsbwapi** | Maintained Rust BWAPI client, last release Oct 2024, with a competitive bot (Styx2) behind it | Rust — one of our two in-scope bindings — is already served, by the approach non-goal 1 rejects |
| **JavaBWAPI/JBWAPI** | Pure Java client reading the memory-mapped file directly, no external DLL, 32- and 64-bit Java, OpenBW on Linux | The JVM is served the same way. Also the empirical answer to §10.2 |
| **OpenBW/BWAPI4J** | Java wrapper via a C++ bridge; minimizes JNI calls by caching per-frame data in one large array copy; 64-bit required with OpenBW | Independent confirmation of §5.10, reached under measurement |
| **OpenBW/bwapi** | BWAPI 4.2.0 fork using OpenBW as backend; CMake, Linux, headless; explicitly incompatible with retail Brood War | Bears on non-goal 3, §10.1, §10.2, and whether §11.5's mock server is needed |

Keep the `swig.i` triage note — its `%ignore` list is still a useful record of what does not
survive a binding — but demote it below the above.

**Then rewrite §3's framing.** The recommended shape currently reads as a greenfield design.
It must instead be stated as a position relative to `bwapi-c`: revive it, rebuild against it as
a reference, or explain why neither. A blank-page design for a problem someone already solved
to v1.0 is not credible, however good the design is.

---

## B. Rewrite non-goal 1 as an argument, not an assertion

Non-goal 1 says per-language protocol reimplementation means porting `Templates.h` (3,098 lines
of game rules) and `CommandTemp.h` into every language, and calls it explicitly rejected.

Two of the most active BWAPI ecosystems did exactly that. rsbwapi and JBWAPI are both alive and
in tournament use; the C-ABI project that took our approach is dormant. That does not make the
non-goal wrong — the porting cost is real and each of those projects carries it — but the
document currently declares the conclusion without engaging the counter-evidence, and the
counter-evidence is precisely the two languages we picked.

**Replace the assertion with a measured claim**: how much of rsbwapi and JBWAPI is a port of
BWAPI's shared game rules, what divergence bugs their trackers show, and what a C ABI over the
real `BWAPI::Game` gets for free that they do not. If that measurement comes back small, the
honest move is to weaken the non-goal, not to keep restating it.

**And follow the consequence.** If Rust and the JVM are well served, the ABI's real audience is
the languages with no option at all — Python, C#, Zig, JavaScript. Say so in the Purpose
section, and reconsider whether Rust belongs in §7's in-repo `bindings/` at all. Building a
second Rust binding alongside a maintained one needs a reason.

---

## C. Rewrite the SWIG rejection — §1.6 and §9

§9 rejects SWIG because it "generates a per-language C++ layer, which is the thing we're trying
to stop needing." **For SWIG's C module that premise is false.** SWIG 4.3 treats C as a target
language and aims to emit a pure ISO C interface to a C or C++ library. The rejection has to be
rewritten whether or not the conclusion changes.

Rewrite it on these four grounds instead, after running the §R8 experiment:

1. The C module is documented as still under development.
2. Its documented C++ shortcomings include **enums with class or namespace context being
   broken** — which takes out `UnitTypes::Enum`, `Orders::Enum`, and the rest of §5.8's ~700
   constants, the highest-value block in the ABI — plus unsupported varargs, stripped
   qualifiers, and no global variables.
3. Class instances cross as opaque `SwigObj*` pointers. That is the pointer model §1.3 rejects,
   and not as a preference: integer handles are what make a foreign-language bug non-fatal and
   what let a host store units in a plain array.
4. None of the §4 conventions is expressible in typemaps at reasonable cost — sorted collection
   output, caller-provided buffers with true-count returns, packed positions, the sticky error
   latch, size-prefixed structs, the §5.10 snapshots, boundary-side closest-unit queries. Those
   are the 15% the design budget went to, and they are the whole point.

**Also record the survey result**, so nobody re-runs it: CppSharp targets C# and C++/CLI;
cppbind targets Swift, Kotlin, and Python from annotated C++; AutoWIG targets high-level
languages via libclang and Mako; SharpGenTools targets C# via CastXML. **No mature tool emits a
flat C ABI from C++ except SWIG's C module.** CastXML, pygccxml, and c2ffi are introspection
front-ends with no emission.

**Add the one tooling win that survives:** use libclang or CastXML to generate the *first draft*
of the YAML spec instead of hand-typing 900 entries. It is the same parser `check_coverage.py`
already needs, run in the other direction.

---

## D. Delete the two-stage x64 proof — rewrite §10.2

§10.2 sequences a provisional verdict at phase 0 and a final one at phase 3, on the grounds that
static asserts prove self-consistency but not interop.

**Interop is already proven in production.** JBWAPI runs a 64-bit JVM against the 32-bit server,
mapping the same `Local\bwapi_shared_memory_<pid>` region and reading `GameData` out of it, and
has done so for years in tournament use. That is the cross-bitness experiment §10.2 wants to run
at phase 3, already run, by software people rely on.

- **Cut the two-stage framing.** State the existence proof, cite it, and move on.
- **Keep the generated layout dump, demoted.** It stops being a phase gate and becomes a cheap
  regression check against upstream `GameData` edits — which was always its better
  justification.
- **Keep the `/Zp` and `#pragma pack` grep.** One command, and it is the only remaining way the
  claim could be wrong.
- **The real x64 gate is the link closure, not the layout** (§10.1 already says this). Leave
  that where it is.

Update §13's risk row and §12's phase 0 and phase 3 exit criteria to match.

---

## E. Re-scope the mock server — §11.5, §12 phase 3

The mock server is the largest single item in the roadmap. §11.5 sizes the client-facing half of
the protocol at ~300 lines; with the scenario fixtures the plan also requires — synthetic map,
units, players, and correctly maintained `xUnitSearch`/`yUnitSearch` arrays, without which the
spatial queries it exists to test return wrong answers — realistic sizing is 800–1500 lines.

Meanwhile OpenBW is a real headless BWAPI that builds with CMake and runs on Linux, and both
JBWAPI and BWAPI4J use it.

**Make the mock server conditional on the OpenBW investigation, and state the open questions in
the document rather than assuming either answer:**

- Does *client mode* work against OpenBW? bwapi-c's README says it does not, or did not.
- OpenBW needs the retail MPQ files, which cannot be committed or downloaded in public CI.
  That is a hard constraint on the "entire client-mode ABI testable on CI with no StarCraft
  installation" claim, and it applies to OpenBW as much as to a mock.
- OpenBW's BWAPI fork no longer works with retail Brood War, so supporting both is two build
  configurations and two pinned dependencies.

**If OpenBW works, the mock shrinks to a thin fake for error paths and the boundary fuzz
harness** — invalid handles, disconnects, truncated buffers — which is a fraction of the work
and still covers the headline safety promise. If it does not, the mock stays, sized honestly.

---

## F. Revise the cross-platform non-goal — §2 non-goal 3

"Not cross-platform. BWAPI hooks a 32-bit Windows binary. Windows-only is inherent." The second
sentence is true of retail BWAPI and false of the ecosystem. OpenBW runs BWAPI on Linux, and
`bwapi-c` itself ships a `libBWAPIC.so`.

Restate it as a scope decision rather than a law of nature: Windows and retail Brood War are the
v1 target because that is where tournaments run; Linux via OpenBW is a possible second target
whose cost is a second pinned dependency and a second build configuration; it is not ruled out
by physics.

---

## G. Cut the ceremony

The following exist to guard against upstream motion in a dependency that has not moved
meaningfully in years. Several of them are mine, and they were over-priced. **Demote all of them
from machinery to prose in the README or `NOTES.md`:**

| Cut | Currently | Becomes |
|---|---|---|
| Per-entry normalized declaration hashes | §9, a spec field and CI check | A note that the coverage audit should diff signatures at pin bumps |
| `api.schema.json` with `schema_version` and a compatibility policy | §7, §9, §13 | Ship `api.json`; document its fields; skip the schema until a second consumer exists |
| The divergence register as a maintained artifact with a per-spec-entry `divergence:` field | §9, §15 | Keep §15 as a table in the README. Drop the required field on every spec entry |
| The six-step pin-bump checklist | §10.3 | Four lines of README prose. Keep the `cscript.exe revisionUpdate.vbs` step — that one is load-bearing, because a synthesised `svnrev.h` makes `bwapi_revision()` lie |
| Boundary fuzzing as a roadmap item | §11.6, §12 phase 4 | Keep the harness — it tests the headline safety promise — but it is an afternoon generated from the `.def`, not a phase deliverable |

**Keep the golden `.def` diff and header hygiene.** Those are about our own stability, not
upstream's, and they are nearly free.

---

## H. Re-scope the surface, and make the generator conditional — §1.8, §9, §12

§1.8 derives ~900 declarations collapsing to 550–600 entry points. §9 then argues that
hand-writing 600 wrappers is a mistake, therefore a generator, therefore a spec DSL, emitters,
`api.json`, and a coverage audit. **That chain starts from a completeness target nobody
requested.** Goal 5 says "complete enough to write a real bot, not a demo subset" — the ~130
`canXxx` predicates and ~90 draw calls are not what makes that true, and §12's own phase 5 exit
criterion needs perhaps 60 functions.

**Replace the assertion with a measurement**: rank entry points by call-site frequency across a
corpus of open-source bots, and set the v1 cut line where coverage plateaus.

**Then make the generator conditional on that number.** If the 95% line lands under ~250
functions, hand-write them under the §4 conventions in a week and let the tail arrive on demand;
the spec DSL, emitters, and coverage audit become a phase-7 item rather than phase 1. If it
lands near 600, the generator stays as designed. Either way, say in the document *why* the
generator exists, in terms of a number that was measured rather than counted from headers.

**Consequence for §5.8:** ~150 static type-data functions may be better as a single generated
lookup table the host reads once than as 150 exported symbols. Check that against the frequency
data before generating them.

---

## I. Rewrite the roadmap — §12

The eight-phase roadmap with formal exit criteria is a plan for a team, sized for one developer,
for a library whose realistic audience is a handful of bot authors. Collapse it in line with §E
and §H:

- **Phase 0** stays: repo, pinned submodule, derived link closure with explicit file lists,
  client-only CMake target, `svnrev.h` from upstream's script, LGPL files, `bwapi_c.h` skeleton
  with the §4 conventions. Drop the x64 two-stage language per §D.
- **Fold the generator phase into "conditional"** unless §H's measurement demands it.
- **Fold the mock-server phase into the test infrastructure of whichever phase needs it**, per
  §E, rather than standing as a phase whose exit criterion is a protocol handshake.
- **Keep the read-surface and write-surface split**, and keep phase 5's exit criterion exactly as
  written — a C99 bot that builds against `bwapi_c.h` alone, with no C++ toolchain, and against
  real StarCraft reads state and moves units. **That is the best sentence in the document.** It
  is the one criterion that proves the header is honest rather than proving a wrapper is clever.
- **Keep 1.0 gated on the consumers phase.** The stability timeline is right.

Target four or five phases, not eight.

---

## J. Fix the names — §3, §10.4

- `bwapi-c` is taken as a repository name by RnDome, and their published crates reference it by
  that name. Either fork it and keep the name, or pick another.
- **`bwapi-sys` is taken on crates.io**, published roughly eight years ago. §10.4 assumes we can
  publish it and we cannot without a transfer request. Pick a fallback now.
- `bwapi`, `bwapi_wrapper`, and `rsbwapi` are also taken. The obvious Rust names are gone.
- Check npm before §7's layout hardens around a package name.

---

## K. Cite the corroboration — §5.10

BWAPI4J states that it minimizes JNI calls by caching the data likely to be queried every frame
on the Java side using a single large array copy. That is §5.10's snapshot design, arrived at
independently by a project that measured the cost. Cite it. An argument from first principles
plus an existence proof is much stronger than either alone, and it costs one sentence.

Same for §5.5's bulk grids: JBWAPI's headline performance claim is that reading the memory-mapped
file directly beats the marshalling-heavy alternative by a large factor. The per-crossing cost
model this plan is built around is the one the ecosystem already validated.

---

## L. What stands unchanged

So that none of this reads as a retreat. The following survived the survey intact and should not
be reopened:

- **§0 licensing** and its three consequences — dynamic consumption only, `/MT` under the System
  Libraries exception, license files in every release asset. Verify the license text itself
  (§R9), but the reasoning holds.
- **All of §4.** Integer handles, `__cdecl` and a `.def` with no ordinals, `int32_t` scalar
  booleans with the return-register and stack-slot reasoning, packed `int64_t` position returns
  with unpacked parameters and lossless sentinels, snprintf string convention, caller-provided
  buffers with true-count returns and first-`cap`-in-ID-order truncation, ID-sorted collections,
  size-prefixed PODs with element-zero stride, neutral values on invalid handles, the sticky
  latch-first-error channel with its two-crossings argument, the process-wide singleton
  statement, and the 0.x-until-consumers stability timeline.
- **§4.1's frame loop**, including latency compensation exposed rather than footnoted.
- **§5.4's determinism work** — boundary-side closest queries with lowest-ID tie-breaks,
  `getBestUnit` dropped, callbacks demoted to conditional, the `reentrant` flag kept as free
  spec metadata.
- **§5.10 snapshots**, now with corroboration.
- **§6's handle model**, including the enumerated list of functions that legitimately return
  `BWAPI_NONE`.
- **§10.1's derived link closure**, with Storm's exclusion as the finding that keeps x64 alive —
  though check OpenBW's CMake first, since they have already solved "what does a non-injected
  BWAPI need."

---

## M. The framing to add to the Purpose section

Revision 3 opens by stating that non-C++ languages must either reimplement the protocol or
maintain a per-language C++ shim. After the survey, that sentence needs a caveat and a sharper
claim: two languages already chose reimplementation and are thriving, one C ABI already existed
and stopped, and the question this project has to answer is why the third attempt goes
differently.

The honest answer is probably some version of: the ABI is worth building for the languages that
have no maintained option, it is worth building over the real `BWAPI::Game` so the game rules in
`Templates.h` are never ported again, and it is worth building small. If the research in the
companion document cannot support that sentence, the plan should shrink or stop rather than get
more thorough.
