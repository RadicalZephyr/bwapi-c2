# Revision 4 — critical review

A read of [c-abi-plan.md](../c-abi-plan.md) at revision 4 (commit `70f0a082`) for poor
decisions, faulty reasoning, accidental inversions and weaknesses, followed by the prior art the
plan has never engaged with: **published accounts of plain-C wrappers built over C++ codebases
for exactly this purpose** — a simpler FFI target for other languages. Part 2 checks §4 against
those and reports where they agree, where they disagree, and where they expose a gap.

**Summary.** Nothing in revision 4 is wrong in a way that changes its shape. Fourteen findings
below; three are reasoning errors in my own recent work, four are design gaps the prior art
makes visible, and the rest are places where the document says less than it needs to. The
strongest single finding is **F2**: the error latch as specified cannot distinguish a stale
handle from a bogus one, and stale handles are normal play, so the latch would fire every frame
in any bot that remembers unit ids — which is every bot. The second strongest is **F9**, from
ICU: the `cap == 0` size-query idiom the plan demonstrates in §14 doubles the cost of every
collection call, and ICU's documentation says so in as many words.

---

## Part 1 — Findings on the plan as written

Ordered by consequence. Each names the section, the problem, and the edit.

### F1. §1.8 / §9 — the "reason the generator exists" conflates the spec with the emitter

Revision 4 says the generator's justification "moved from typing to the coverage audit": 542
declarations map to ~457 exports through five rules, and only an audit off a machine-readable
spec can prove the mapping complete. **That justifies a spec and an audit. It does not justify
an emitter.** A team could hand-write every wrapper and still keep a YAML list of what maps to
what, and run the same audit against it.

The emitter is justified by two other things the document already states but no longer
connects to the decision: **uniformity of the §4 conventions across ~770 functions** (the
`noexcept` boundary, buffer semantics, invalid-handle behaviour, error latching — §9's "rejected:
hand-write everything" paragraph), and **§5.8**, where 1,033 entries must be emitted from data.
Those were revision 3's reasons. They were right. What the research changed is that the *count*
argument ("600 is too many to type") was shown to be weak — `bwapi-c` typed 530 — not that the
uniformity argument was.

**Edit.** §1.8's last paragraph and §9's opening should say: *the spec and the audit exist
because of the mapping; the emitter exists because 770 functions must share one boundary
implementation and 1,033 must come from data.* The decision does not change. The stated reason
was overclaimed, by me, in the fork-2 write-up and carried into the rewrite.

### F2. §4 / §6 — the latch fires on normal play unless "invalid" and "dead" are separated

§4: *"Invalid handles. Never dereference. Validate, then return a documented neutral value and
latch it."* §4.1: *"A handle from frame N may resolve to a dead unit at frame N+1 — the existing
C++ semantics, unchanged."*

Those two sentences describe different behaviours for the same input and the document does not
say which wins. Every bot keeps unit ids across frames — squad lists, build queues, scout
assignments — and queries them after the unit has died. If a dead unit's id is "invalid" under
§4, the sticky latch records `BWAPI_ERR_INVALID_HANDLE` on the first such query every frame, the
host's end-of-frame check trips on ordinary play, and the latch is useless as a bug detector
from the first game onward.

§6.1's `resolve()` actually settles it — `id < 0` → `nullptr`, otherwise `getUnit(id)`, which
returns the `UnitImpl` for any in-range id whether or not the unit exists — so a dead-but-in-range
id is *not* rejected and BWAPI's own dead-unit semantics apply. But that is a property of a
six-line helper, not a stated rule, and nothing stops a generated wrapper from checking
`exists()` first.

**Edit.** State the rule in §4 and §6.2: **an ABI error is a handle that could never have been
valid** — negative, out of range, wrong kind, or used before `connect` — **not a handle to a
unit that has since died.** Dead units return what BWAPI returns (the C++ semantics, usually
`0`/`None`), do not latch, and are detected by `bwapi_unit_exists()` or the snapshot's `exists`
bit. Add the upper bound (10,000 for units, 12 for players, 5 for forces) to the helper's
description so the rule is checkable. Same statement for BWEM: a neutral id whose unit has been
destroyed is a §8.2 "gap" case, not an error.

### F3. §5.7 — a usage-based cut survived, in a document that just banned them

*"Expose the ID-array form for the ~10 that matter."* That is a frequency judgement, and §1.8
spends a paragraph explaining why frequency is not a scope instrument. It is also unnecessary:
`GameImpl::issueCommand(const Unitset&, UnitCommand)` is a loop over `u->issueCommand(command)`
— the source says `//FIX FIX FIX naive implementation` — so every `Unitset` broadcast in
client mode is sugar over a loop the host can write, and §5.3 already exports the loop as
`bwapi_game_issue_command(ids, n, cmd)`.

**Edit.** Delete §5.7. Ship `bwapi_game_issue_command` and zero per-verb broadcasts. Add one
sentence to §5.3 saying what the function is — a loop, never a grouped command — because after
§15 #17 a reader will otherwise assume the array form *is* the grouped form (F12).

### F4. §12 phase 1 — "zero unaccounted declarations across the six headers" names the wrong universe

The audit's completeness claim is only as good as the list of headers it walks. §1.8 counts six
interface headers; §5.8's 185 accessors live in fourteen type-class headers; `UnitCommand.h`
(58 declarations), `Unitset.h` (48), `Position.h`, `Event.h`, `Filters.h` and
`Client/*.h` are not in either count, and BWEM's `Map.h`/`Area.h`/`ChokePoint.h`/`Base.h`/
`Neutral.h`/`Tile.h` are audited by §9's prose but not by phase 1's exit criterion.

**Edit.** Make the audit's universe an explicit, checked-in list of headers (with a rule for each
excluded one — `Filters.h`: §5.4; `Unitset.h`: F3; `Client/*.h`: not public), and make the phase
1 criterion "zero unaccounted declarations across the audited set." Otherwise "complete" is
relative to a list nobody wrote down.

### F5. §8.2 — `bwapi_bwem_initialize` must reset first, by its own principle

§15 #13 collapses three calls into one because *"an ABI should make ordering errors impossible
rather than diagnosable."* The obvious ordering error is then left possible: a host that calls
`bwapi_bwem_initialize()` at the second match's start without having called `bwapi_bwem_reset()`
hits the R11.6 in-place-reset crash the patch exists to avoid — because the patch adds
`ResetInstance()`, it does not make `Initialize()` safe to call twice.

**Edit.** `bwapi_bwem_initialize()` calls `reset()` itself if already initialised, and the
document says so. `bwapi_bwem_reset()` stays exported for hosts that want to free the ~30 MB
between matches, but nothing depends on them calling it.

### F6. §0 — the lawyer flag from R9 was dropped

R9 §4 flagged two things for counsel: whether static-CRT redistribution is permitted under the
specific VS edition, and **whether LGPL §4's reverse-engineering permission is in tension with
the VS distributable-code terms' reverse-engineering clause.** Revision 4 kept the first and
silently dropped the second. It is the standard LGPL-on-Windows question and it has a standard
answer, but the plan should carry the question rather than imply it was resolved.

**Edit.** One sentence in §0 consequence 2.

### F7. §5.8 — the bulk table cannot carry the container-valued accessors

"One size-prefixed bulk table per type class" is right for the ~170 scalar accessors. It cannot
represent `requiredUnits()` (a map), `abilities()`/`upgrades()`/`buildsWhat()` (sets) or
`whatBuilds()` (a pair) without variable-length encoding, and the document does not say those
stay function-only.

**Edit.** State that the table carries scalar fields only, that container-valued accessors are
function-only, and — since `requiredUnits` is on the hot path for build-order code — consider a
second flat table `(type, required_type, count)` sorted by type, which is the same shape as the
snapshot arrays.

### F8. §7 / §9 — dropping `schema_version` went one step too far

Rev-4 §G cut the JSON Schema and the plan's argument — two in-repo generators consuming
`api.json` in CI are its compatibility test — is sound *for those two consumers*. It is not a
test for a third-party consumer (the Go or Zig binding `api.json` is explicitly there for), and
the plan dropped the **version field** along with the schema. Godot's `extension_api.json` is the
closest precedent for a dumped API description consumed by many generators, and its tracker has
an open "hardening of the specification" issue precisely because consumers multiplied.

**Edit.** Keep a top-level integer `"api_json_version"` in `api.json` and a `docs/api-json.md`
describing every field. Bring the schema back when a third-party consumer appears — rev-4's
rule, applied honestly.

### F9. §4 / §14 — the `cap == 0` size query runs every collection call twice, and the example teaches it

§14's C example does `n = snapshot_units(NULL, 0)`, `malloc`, then `snapshot_units(units, n)`.
For the snapshot that is two full field-select copies over every unit; for `get_all_units` it is
two builds and two sorts. ICU, which has used the identical convention for twenty-five years,
documents the cost bluntly: *"pure preflighting is inefficient since the operation executes
twice,"* and recommends a pre-sized buffer with a retry on overflow.

The convention is right — it is what ICU, TensorFlow's `TF_Buffer`, and every snprintf-shaped
API do — but the plan should not demonstrate the slow idiom as the canonical one, in the section
whose purpose is to show the ABI is pleasant.

**Edit.** In §4, add the retry idiom as the recommended one: allocate to last frame's count plus
slack, call once, grow only when the return exceeds `cap`. Rewrite §14's example to use it. Add
cheap counters where BWAPI already has the number for free (`bwapi_game_unit_count()` from
`data->initialUnitCount`/`unitCount`, event count, bullet count) so the common cases need no
preflight at all.

### F10. §4 — thread affinity is prose, and a wrong-thread call is exactly the bug the latch exists for

*"All calls happen on the thread that calls `bwapi_client_update()`."* A Python host with an
`asyncio` loop, or a C# host with a UI thread, will get this wrong at least once, and the failure
mode is a data race inside `GameImpl` rather than a latched error. The check is one thread-id
compare per call, which is cheaper than the resolve-and-guard every wrapper already does.

**Edit.** Record the thread id at `connect`; latch `BWAPI_ERR_WRONG_THREAD` and return the
neutral value on any call from another thread. Make it the first example in the boundary-fuzz
list, since it is the one a fuzz harness will not find on its own.

### F11. §11 — "synthetic by policy" gives up the argument R7 made, and the plan does not say what replaces it

R7's case against a mock server was that a hand-built game state is *plausibly wrong* in ways
its author cannot know (three were found), and a recorded buffer is correct by construction.
Fork 7 then chose synthetic fixtures for provenance reasons — correctly — but the plan now
carries R7's finding as a list of four gotchas encoded in a fixture builder, which is exactly the
"our understanding of BWAPI's invariants" that R7 said a mock gets wrong. Recorded buffers are
"contributor-local and gitignored" and have no stated job.

**Edit.** Give them one: a **differential test**, run locally by whoever has a StarCraft
install, that loads a recorded frame-0 buffer, runs the same read-path assertions the synthetic
suite runs, and reports any assertion that passes on one substrate and fails on the other. That
is the check that the fixture builder's invariants are BWAPI's invariants, and it costs one
script. Say in §11 that CI coverage is synthetic and correctness of the synthetic substrate is
established locally and periodically, not assumed.

### F12. §5.3 / §15 #17 — `bwapi_game_issue_command(ids, n, cmd)` looks like the grouped command the register just removed

After reading that `*Grouped` predicates are not exported because grouped commands do not exist
in client mode, a reader meets an array-taking command function and reasonably assumes it is
the grouped form. It is a loop (F3).

**Edit.** One sentence in §5.3 and a cross-reference in §15 #17.

### F13. §1.9 — the C# argument is inferential and stated as causal

"Six C# attempts, no survivor" is evidence of demand. "Both 2023 attempts chose the hardest path
because nothing else was available" is an inference about why they died — and R3 §5 shows the
2023 attempts died in 15 days and 1 day, which is consistent with "lost interest" as much as
with "the path was too hard." The purpose section leans on it.

**Edit.** Soften to what the record supports: repeated attempts, none surviving, none with an
easier path available. The conclusion — C# is the strongest unserved target — stands on the
count alone.

### F14. §4 — `bwapi_abi_version()` is the one scalar that is not `int32_t`, and its packing is unspecified

"One integer width for every scalar in the ABI" and then `uint32_t bwapi_abi_version(void)
/* semver */`. How three semver components pack into 32 bits is not stated, and a host cannot
compare versions without knowing.

**Edit.** `void bwapi_abi_version(int32_t* major, int32_t* minor, int32_t* patch)` plus
`int32_t bwapi_abi_version_string(char*, int32_t)` under the string convention. Uniform, and
self-describing.

---

## Part 2 — Prior art: C ABIs over C++ codebases, built as FFI targets

The plan's prior art (§1.6) is all BWAPI-specific. This is the general field: projects that put
a flat C layer over a C++ implementation specifically so that other languages could bind to it,
and wrote down how. Each is checked against §4. The pattern has a name — Stefanus Du Toit's
**"hourglass interface"** (CppCon 2014): *C++ on top of C89 on top of C++*, chosen because "C
ABIs on many platforms have been stable for decades, practically every language supports binding
to C code through foreign function interfaces, and including nearly any C89 header has a
negligible effect on compile time." Everything below is an instance of it.

### 2.1 The case studies

| Project | Shape | What it settled, and how it bears on §4 |
|---|---|---|
| **SkiaSharp / Skia C API** (Google → Mono) | Hand-written `sk_*` C layer in a **fork** of Skia; P/Invoke declarations **generated from the C headers**; hand-written C# wrappers on top. Three pointer categories — raw (borrowed), owned (`new`/`delete` pairs), ref-counted (`sk_sp`) — with a `HandleDictionary` to keep one managed wrapper per native handle | **The closest analogue to our C# target, and the cautionary one.** Google removed the C API from upstream Skia for lack of interest; the binding maintainers now carry it. Its maintainer describes stability as "additive between milestones, but really at the whim of the underlying C++ API." Three lessons: (a) *the C layer is maintained by whoever needs it* — here, us, which is why the repo is separate and pinned; (b) a C API that mirrors the C++ object model inherits its lifetime complexity — three ownership regimes and a handle dictionary — which is precisely what **integer handles into game-owned storage** remove; (c) a hand-written C layer over a moving C++ target is what "at the whim of" means, and our dependency does not move |
| **TensorFlow C API** | `TF_`-prefixed, opaque structs allocated and freed through the API, `TF_Status*` out-param for errors, `TF_Buffer` with an optional `data_deallocator`. Documented rule: **"New language support should be built on top of the C API"** and the API "leans towards simplicity and uniformity instead of convenience since most usage will be by language-specific wrappers" | The design-principle statement is the one the plan should adopt verbatim, and it cuts against one decision: §5.3's ~40 convenience commands are *convenience*. They are defensible — they are generated, and the phase-3 C bot is a first-class consumer — but the plan should say they exist for the C consumer and that wrappers are expected to build on `issue_command`, rather than "that's what bots actually call," since bots call the wrapper |
| **ONNX Runtime** | One exported entry point, `OrtGetApiBase()->GetApi(ORT_API_VERSION)`, returning a **versioned struct of function pointers** to which functions are only ever appended; `OrtStatus*` returns, null on success; `ORT_API_VERSION` at 30 | The alternative to a `.def` with 770 names: one symbol plus a table. Considered and **declined** for a reason the plan should record — `ctypes` and P/Invoke bind named exports natively and bind function-pointer tables by hand, so a table trades our two primary consumers' ergonomics for a versioning mechanism append-only symbols already provide. Godot's `get_proc_address` is the same trade for the same reason (an engine loading *many* extensions) |
| **Godot GDExtension** | `gdextension_interface.h` (function pointers the engine hands the extension) plus **`extension_api.json`, dumped by the engine and consumed by godot-cpp, godot-rust and others** to generate bindings | The strongest precedent for `api.json` as *the* binding contract, at ecosystem scale. Also the strongest precedent for F8: once consumers multiplied, a "hardening of the specification" issue appeared. Keep the version field |
| **cimgui** (Dear ImGui) | A **Lua generator** parses ImGui's C++ headers and emits `cimgui.h`/`.cpp` plus **`definitions.json` and `structs_and_enums.json`**; overload names are algorithmic with an override table; consumed by Go, C#, Rust, Zig, Nim, Odin, Julia and more | The closest *structural* precedent to §9: generated C over C++, with a JSON sidecar that a dozen bindings consume. Two differences worth stating: cimgui parses headers and emits directly — the approach §9 rejects because an upstream header edit silently changes the ABI — which is acceptable for cimgui because it *tracks* ImGui releases by design and unacceptable for us because we *pin*; and its overload naming is algorithmic-with-exceptions where ours is explicit per entry, which is the right choice at 770 and the wrong one at cimgui's thousands |
| **HarfBuzz** | C API over a C++ implementation from day one; opaque handles with `reference`/`destroy` refcounting; constructors **never return NULL** — allocation failure returns an inert "empty object" singleton that is "safe (although typically useless) to pass around"; `make_immutable`; reserved members in non-opaque structs because "exposing a struct in the public API makes it impossible to expand the struct in the future" | Two direct endorsements of §4: the inert-object-on-failure rule is the neutral-value rule under another name, and the reserved-members concern is what the `int32_t size` prefix solves more flexibly. HarfBuzz is also the counterexample to SkiaSharp: a C-over-C++ API that has been stable for fifteen years, because the C API *is* the product and the C++ is an implementation detail |
| **ICU4C** | **Only the C API is ABI-stable across releases**: "the design of C++ language and runtime environments present extreme technical difficulties." `UErrorCode*` in-out parameter that every function checks on entry and propagates; per-version symbol renaming (`ucnv_open_3_8`) for side-by-side loading; the **preflighting** convention — return the full required length even when the buffer is too small, `NULL`/0 for pure preflighting, with an explicit warning that it "executes twice" | The C-only-stability statement is the one-sentence justification for this whole project, from the most widely deployed C-over-C++ library in existence. The preflighting warning is F9. The `UErrorCode` in-out is the design §4's sticky latch improves on for FFI: ICU pays a parameter on every call; we pay one read per frame |
| **LibreOfficeKit** | C ABI over C++ LibreOffice for embedders; every class is a **struct of function pointers beginning with `size_t nSize`**, new members appended only, `LIBREOFFICEKIT_HAS_MEMBER(strct, member, nSize)` for capability checks; explicit note that using the library's own free avoids "crashes from mismatched C runtime libraries, particularly on Windows" | The `nSize` prefix is §4's size-prefixed POD, in production for a decade; their `HAS_MEMBER` macro is worth copying for consumers that want to feature-test a snapshot field. The CRT-mismatch note is the bug §4 designs out by having no allocation cross at all |
| **LLVM-C / libclang** | C API over LLVM and Clang; **"best effort" stability**, explicitly "limited by the abstractness of the interface and the stability of the C++ API that it wraps"; release branches never break it; `CXString` with `clang_disposeString` for returned strings; small structs (`CXCursor`) passed by value | The honest stability statement, worth borrowing for §4's 0.x paragraph: our stability is bounded by BWAPI's, and BWAPI's is high because it is frozen. `CXString`+dispose is the pattern §4 rejects in favour of caller buffers, and LLVM's is fine because libclang consumers are tools, not per-frame bots |
| **Z3** | C API is the base for Python, .NET, Java, OCaml, Julia, Rust, Go, Smalltalk bindings; **per-context last-error code** (`Z3_get_error_code`) plus an **optional error-handler callback** (`Z3_set_error_handler`) invoked at the failing call — added because bindings wanted to raise an exception immediately rather than poll | The error model closest to ours, one iteration further along. Z3's is last-write and per-context; ours is first-write and process-wide, which is right for a singleton. But the **callback** is a real gap in §4: a Python wrapper wants to raise at the failing call in a debug build, and today its only options are polling the latch or parsing the warn-level log. See 2.3 |
| **FoundationDB** | C API "primarily intended for use in implementing higher level APIs"; **`fdb_select_api_version()`** at runtime so a program built against an old header keeps old semantics on a new library; `fdb_error_t` integer with `fdb_get_error()` for the message | Behaviour versioning, which append-only does not give. Not needed while the dependency is frozen; worth one sentence in §4 saying so, because it is the first thing a reader from that world will ask |
| **Apache Arrow C Data Interface** | A C ABI so projects can share Arrow data "without necessarily using Arrow libraries or reinventing the wheel"; **frozen structs with no size field** — "should not change in any way, including adding new members"; producer-owned release callback | The opposite evolution strategy to the size prefix: freeze and never touch. Right for a wire format shared between independent runtimes; wrong for us, where the same team owns both sides and expects to add snapshot fields |
| **TVM-FFI** (2025) | An open ABI for ML systems: a 16-byte type-erased `TVMFFIAny`, **one calling convention for every function** (`args[]`, `num_args`, `result*`, `int` return), object headers with `type_index` and a deleter, errors as objects in thread-local storage | The other end of the design space — dynamic dispatch through one signature so *frameworks* can interoperate. Static per-function exports are right for a fixed 770-function surface consumed through FFI generators; a single-signature ABI would make every `ctypes`/P/Invoke call a marshalling exercise. Cite so nobody proposes it |
| **OpenCV** (anti-example) | The 1.x C API was kept beside the 2.x C++ API, then removed in 4.0 and finally in 5.0. Stated reason: "all the new stuff has the C++ API, and it is not backported to the C. So, C API becomes obsolete and causes pain in the neck, since it should be maintained" | What kills a C-over-C++ layer is being hand-maintained *separately* from the thing it wraps, on a target that moves. Ours is generated from a spec that the coverage audit ties to the headers, over a target that does not move. The OpenCV failure mode is the one §9 and §10.3 are built to prevent, and the plan should say so |

### 2.2 What §4 gets confirmed on

Every §4 convention has at least one production precedent in the table, and the two most
contested ones have the strongest: **integer handles** are what SkiaSharp's three-regime pointer
model is the cost of not having; **caller-provided buffers with a true-count return** are ICU's
convention, in production since the 1990s, with the same `NULL`/0 preflight. The **size prefix**
is LibreOfficeKit's `nSize` and answers HarfBuzz's reserved-members concern. **Neutral values on
bad input** are HarfBuzz's inert singletons. **No allocation crossing the boundary** is the
LibreOfficeKit CRT warning taken to its conclusion. **Exceptions caught at the boundary** is in
every one of them. **C-only ABI stability** is ICU's design document, one sentence, no argument
needed.

Two places the plan is *ahead* of the precedents: the sticky first-error latch (ICU and Z3 both
pay per call; TensorFlow pays a status object per call) and the ID-sorted deterministic output,
which none of them needed because none of them wraps a competitive simulation.

### 2.3 Where the precedents expose a gap

1. **An error callback, not only a log callback (Z3).** `bwapi_set_error_callback(void(*)(int32_t
   code, const char* msg, void* user), void* user)`, invoked at the moment of latching. Off by
   default; a Python wrapper turns it on in debug builds to raise at the failing call; the sticky
   latch is unchanged for release builds. This closes the plan's own worry — "a silently no-op'd
   attack command in a language where nobody checks error codes is a miserable afternoon" — with
   the mechanism the closest precedent added for the same reason.
2. **A stated stability model (LLVM-C, ICU).** §4 promises append-only from 1.0 and says nothing
   about the bound. Say it: the ABI is as stable as the pinned BWAPI and BWEM, which have not moved
   since 2018 and 2021, and a pin bump is the only event that can change semantics behind an
   unchanged signature — which is why §10.3 exists and why the divergence register is a table
   rather than a promise.
3. **The preflight cost (ICU) — F9.**
4. **The purpose statement (TensorFlow).** "Simplicity and uniformity over convenience, because
   the consumers are wrappers" belongs in §4's opening paragraph. It is what §4 already does, and
   it is the sentence that settles arguments about adding convenience later.
5. **Capability testing (LibreOfficeKit).** A `BWAPI_HAS_FIELD(struct, field, size)` macro in
   `bwapi_c2_types.h` costs one line and is how a consumer compiled against 1.3 reads a 1.2 DLL's
   snapshot safely. The size prefix makes it *possible*; the macro makes it *easy*.

### 2.4 What to add to the plan

A **§1.6.1 "The general pattern"** paragraph — six sentences: Du Toit's name for it, the
ICU stability sentence, SkiaSharp as the near neighbour and its lesson, cimgui and Godot as the
`api.json` precedents, OpenCV as the failure mode, and a pointer to this document for the table.
Then the five items in 2.3 folded into §4, and the fourteen edits in Part 1.

---

## Sources

Prior art, retrieved 2026-09-05:

- Stefanus Du Toit, *Hourglass Interfaces for C++ APIs*, CppCon 2014 — [talk](https://www.youtube.com/watch?v=PVYdHDm0q6Y), [slides and code](https://github.com/CppCon/CppCon2014/tree/master/Presentations/Hourglass%20Interfaces%20for%20C++%20APIs), [isocpp summary](https://isocpp.org/blog/2015/07/cppcon-2014-hourglass-interfaces-for-cpp-apis-stefanus-dutoit), [a worked example](https://github.com/JarnoRalli/hourglass-c-api)
- SkiaSharp — [architecture](https://github.com/mono/SkiaSharp/blob/main/documentation/dev/architecture.md), [memory management](https://github.com/mono/SkiaSharp/blob/main/documentation/dev/memory-management.md), [binding config](https://github.com/mono/SkiaSharp/blob/main/binding/libSkiaSharp.json), [discussion #2585 on C API stability](https://github.com/mono/SkiaSharp/discussions/2585), [issue #1110](https://github.com/mono/SkiaSharp/issues/1110)
- TensorFlow — [C API header](https://github.com/tensorflow/tensorflow/blob/master/tensorflow/c/c_api.h), [language bindings guide](https://github.com/tensorflow/docs/blob/master/site/en/r1/guide/extend/bindings.md), [TF_Buffer](https://github.com/tensorflow/tensorflow/blob/master/tensorflow/c/tf_buffer.h)
- ONNX Runtime — [onnxruntime_c_api.h](https://github.com/microsoft/onnxruntime/blob/main/include/onnxruntime/core/session/onnxruntime_c_api.h)
- Godot — [What is GDExtension](https://github.com/godotengine/godot-docs/blob/master/tutorials/scripting/gdextension/what_is_gdextension.rst), [gdextension_interface.h](https://github.com/godotengine/godot/blob/4.4-stable/core/extension/gdextension_interface.h), [godot-cpp gdextension/](https://github.com/godotengine/godot-cpp/tree/master/gdextension), [issue #113732](https://github.com/godotengine/godot/issues/113732)
- cimgui — [repository](https://github.com/cimgui/cimgui)
- HarfBuzz — [object model](https://harfbuzz.github.io/object-model.html), [object lifecycle](https://harfbuzz.github.io/object-model-lifecycle.html)
- ICU — [design](https://unicode-org.github.io/icu/userguide/icu/design.html), [strings and preflighting](https://unicode-org.github.io/icu/userguide/strings/)
- LibreOfficeKit — [LibreOfficeKit.h](https://github.com/LibreOffice/core/blob/master/include/LibreOfficeKit/LibreOfficeKit.h), [overview](https://docs.libreoffice.org/libreofficekit.html)
- LLVM — [developer policy, C API changes](https://llvm.org/docs/DeveloperPolicy.html); libclang — [tutorial](https://clang.llvm.org/docs/LibClang.html), [CXString.cpp](https://github.com/llvm-mirror/clang/blob/master/tools/libclang/CXString.cpp)
- Z3 — [README](https://raw.githubusercontent.com/Z3Prover/z3/master/README.md), [Z3_set_error_handler](https://docs.rs/z3-sys/latest/z3_sys/fn.Z3_set_error_handler.html), [Z3_get_error_code](https://docs.rs/z3-sys/0.3.0/z3_sys/fn.Z3_get_error_code.html), [test_capi.c](https://github.com/Z3Prover/z3/blob/master/examples/c/test_capi.c)
- FoundationDB — [C API](https://apple.github.io/foundationdb/api-c.html)
- Apache Arrow — [C Data Interface](https://arrow.apache.org/docs/format/CDataInterface.html)
- Apache TVM-FFI — [ABI overview](https://tvm.apache.org/ffi/concepts/abi_overview.html), [stable C ABI](https://tvm.apache.org/ffi/get_started/stable_c_abi.html), [announcement](https://tvm.apache.org/2025/10/21/tvm-ffi)
- OpenCV — [OE-1 Old C API](https://github.com/opencv/opencv/wiki/OE-1.-Old-C-API), [4.0 release notes](https://opencv.org/opencv-4-0/), [forum: dropping the C API](https://answers.opencv.org/question/17546/opencv-will-drop-c-api-support-soon/)

BWAPI source cited in Part 1: `bwapi/BWAPIClient/Source/GameImpl.cpp:724-733`
(`GameImpl::issueCommand(const Unitset&, UnitCommand)` — the naive loop).
