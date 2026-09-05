# R8. The `swig -c` experiment

Built SWIG 4.5.1 from the release tarball and ran its C target against BWAPI's headers.
Reproducible via [`r8/run-swig-experiment.sh`](r8/run-swig-experiment.sh) (fetches and builds
SWIG itself; the release tarball ships a pre-generated parser, so no bison is needed).

**Headline: §1.6's rejection has to be rewritten, and the honest rewrite is a stronger rejection
than the current one. `swig -c` is real, it parses most of BWAPI, and typemaps express more of
§4 than the plan credits — packed `int64_t` positions, `int32_t` booleans and integer handles all
work. But it is still labelled Experimental in 4.5.1; it hard-fails on `Game.h`; it strips
namespace context from every enum so two type modules cannot coexist in one translation unit; it
silently drops the `Type<>` and `Interface<>` CRTP bases, taking `getID`, `getName` and `c_str`
with them; and it heap-allocates every by-value return with `new` and emits no way to free it.
The parts of §4 it cannot express are the ones that define the product.**

---

## 1. Setup and status

SWIG 4.5.1, built from `swig-4.5.1.tar.gz`, GCC 11. The C target is present and documented
(`Source/Modules/c.cxx`, 115 KB, last touched 2026-08-09; `Doc/Manual/C.html`).

`swig -help` places it here:

```
Experimental Target Language Options
     -c              - Generate C wrappers
     -ocaml          - Generate OCaml wrappers
```

and every single run emits:

```
SWIG:1: Warning 524: Experimental target language. Target language C specified by -c
is an experimental language.
```

So the plan's "module still under development" ground survives verbatim — it is SWIG's own
current word for it, in the latest release.

---

## 2. Does SWIG parse BWAPI's headers?

Mostly yes, and `Game.h` is the exception that matters.

| Header | Result | Exports | Warning 761 | Other |
|---|---|---|---|---|
| `WeaponType.h` | OK | 137 | 8 | 401 ×2 |
| `UnitType.h` | OK | 305 | 17 | 401 ×2 |
| `Unit.h` | OK | 503 | 160 | 401 ×2 |
| **`Game.h`** | **FAILED, exit 1, no output** | — | 376 | **40 hard errors**, 760 ×12, 503 ×1 |

```
Game.h:803: Error: Vararg function printf not supported.
Game.h:827: Error: Vararg function sendText not supported.
Game.h:854: Error: Vararg function sendTextEx not supported.
```

`Game.h` is R2's densest header — 143 entry points, 2,520 `Broodwar->` call sites. SWIG will not
emit anything for it until the varargs are handled.

**With eleven `%ignore` directives it processes** (486 exports, still 360 Warning 761s) — but the
twelve vararg functions are then simply absent, and R2 says what that costs:
`drawTextScreen` (230 sites, all 7 corpora), `drawTextMap` (145), `printf` (65), `sendText` (23).
`drawTextScreen` is the sixth most-used entry point in the entire corpus. Losing it is not a
trade, it is a hole.

Notably, MSVC extensions and `Templates.h` were **not** the obstacle the plan feared — SWIG's
preprocessor handled the headers fine. The obstacles are C++ type richness and varargs.

---

## 3. Namespaced enums: broken exactly as documented

This is the §5.8 question, and the answer is unambiguous. `swig -c` emits:

```c
enum Enum {
  Terran_Marine = 0,
  Terran_Ghost,
  ...
```

**The `BWAPI::UnitTypes::` context is gone.** Not prefixed, not mangled — stripped. So
`WeaponType.h` emits its own `enum Enum` with its own `None`, and:

```
$ gcc -std=c99 -fsyntax-only both.c
weapontype_wrap.h:33:6: error: redeclaration of 'enum Enum'
unittype_wrap.h:33:6: note: originally defined here
weapontype_wrap.h:135:3: error: redeclaration of enumerator 'None'
```

**Two SWIG C modules for two BWAPI type classes cannot be included in the same translation unit.**
BWAPI has nineteen such namespaces (`UnitTypes`, `WeaponTypes`, `Orders`, `TechTypes`,
`UpgradeTypes`, `Errors`, `Races`, …), most of which declare `None`, `Unknown` and `MAX`. R2
measured 279 of these constants in actual use across the bot corpus.

The plan asked whether this "actually takes out `UnitTypes::Enum`, `Orders::Enum` and the rest —
i.e. the ~700 constants of §5.8, the highest-value block in the ABI." **It does.**

---

## 4. `BWAPI::Type` does not survive, and neither does `Interface<T>`

```
UnitType.h:273: Warning 401: Nothing known about base class 'Type< UnitType,UnitTypes::Enum::Unknown >'. Ignored.
Unit.h:54:      Warning 401: Nothing known about base class 'Interface< UnitInterface >'. Ignored.
```

SWIG drops both CRTP bases. Grepping the generated headers for the inherited API:

| Method | In `unittype_wrap.h`? |
|---|---|
| `getID` | **absent** |
| `getName` | **absent** |
| `c_str` | **absent** |
| `toString` | **absent** |
| `isValid` | **absent** |
| any `operator` | **absent** |

So a C caller receives a `UnitType*` and has **no way to turn it into an integer or a string**.
It cannot be compared against the (colliding, unqualified) enum constants either. R2 measured
`c_str` at 283 call sites across all seven corpora — the third most-used entry point in the
corpus — and it is simply not there.

The same applies to `Interface<UnitInterface>`: `setClientInfo`, `getClientInfo` and
`registerEvent` are all absent from `unit_wrap.h`. (`getID` and `exists` survive on `Unit` only
because `UnitInterface` redeclares them as virtuals in its own body.)

`%template` could instantiate the bases, but that is a per-type manual step and it would
reintroduce the enum collision at greater scale.

---

## 5. The object model: opaque pointers, heap-allocated, never freed

**Handles are opaque pointers, not integer IDs.** `UnitType*`, `UnitInterface*` — with
`UnitType_new()` heap-allocating.

**Every by-value return of a BWAPI type becomes an incomplete opaque typedef:**

```c
SWIGIMPORT SWIGTYPE_p_BWAPI__Race*                     UnitType_getRace(UnitType*);
SWIGIMPORT SWIGTYPE_p_BWAPI__WeaponType*               UnitType_groundWeapon(UnitType*);
SWIGIMPORT SWIGTYPE_p_TilePosition*                    UnitType_tileSize(UnitType*);
SWIGIMPORT SWIGTYPE_std__mapT_BWAPI__UnitType_int_t*   UnitType_requiredUnits(UnitType*);
SWIGIMPORT SWIGTYPE_p_Position*                        UnitInterface_getPosition(UnitInterface*);
SWIGIMPORT SWIGTYPE_p_BWAPI__Unitset*                  UnitInterface_getLoadedUnits(UnitInterface*);
```

`typedef struct SWIGTYPE_p_Position SWIGTYPE_p_Position;` and nothing more — the struct is never
completed, so a C caller cannot read `.x` or `.y`. `std::string`, `Unitset`, `UnitType::list` and
`UnitFilter` all come out the same way.

**And the implementation leaks.** From the generated `.cxx`:

```cpp
SWIGEXPORTC SwigObj* UnitInterface_getPosition(SwigObj* carg1) {
  Position cppresult;
  ...
  cppresult = ((BWAPI::UnitInterface const *)arg1)->getPosition();
  result = (SwigObj*)new Position(cppresult);     // heap allocation
  return result;
}
```

The only `_delete` function SWIG generates in the whole output is `SWIG_CException_delete`. There
is no `Position_delete`. So every call to `getPosition` — 253 sites in R2's corpus, invoked
per-unit per-frame — `new`s eight bytes that nothing frees. At 200 units and 24 frames per second
that is roughly 4,800 leaked allocations a second, from one accessor. It also allocates across the
DLL boundary with the wrapper's CRT, which is exactly the failure mode §4's caller-buffer
convention is designed to eliminate.

**The generated glue is a `.cxx`.** `swig -c` produces a C *interface* but a C++ *implementation*,
compiled by a C++ compiler. §1.6's original objection — "it emits a per-language C++ layer" — is
half right rather than wrong: the layer is C++, it is just not per-language.

---

## 6. Typemaps: more capable than the plan claims, and still not enough

This is where the plan is unfair to SWIG, and the correction is worth making precisely.

**What typemaps express cleanly — tested, working:**

```swig
%typemap(ctype) BWAPI::Position, Position "long long"
%typemap(out)   BWAPI::Position, Position
  "$result = (long long)(((unsigned long long)(unsigned)$1.y << 32) | (unsigned)$1.x);"
```
produces exactly §4's packed position:
```c
SWIGIMPORT long long UnitInterface_getPosition(UnitInterface* carg1);
```
```cpp
result = (long long)(((unsigned long long)(unsigned)(&cppresult)->y << 32) | (unsigned)(&cppresult)->x);
```

| §4 convention | Typemap? |
|---|---|
| **Packed `int64_t` positions on return** | ✅ works, generated correctly |
| **`int32_t` booleans, never C++ `bool`** | ✅ works |
| **Integer handles instead of pointers** | ✅ works — `UnitInterface_getTarget(int) -> int`, including the `self` parameter |
| Explicit calling convention, `.def` exports | ✅ trivially, via `%insert` / build flags |
| **Caller-provided buffer + true-count return** | ❌ **impossible** |
| Sorted collection output | ❌ (follows from the above) |
| Size-prefixed structs | ❌ (follows from the above) |
| Sticky first-error latch | ❌ needs a wrapper around every call, not a type mapping |
| `void* user` on callbacks | ❌ the C++ signature has no such parameter to map |

The two typemaps above cut `Unit.h`'s Warning 761 count from 160 to 114 — real progress.

**The wall is arity.** `%typemap(out)` transforms a return *value*; it cannot add parameters to a
function whose C++ signature does not have them. `bwapi_unit_get_loaded_units(int32_t* out_ids,
int32_t cap) -> int32_t` requires inventing two parameters that `UnitInterface::getLoadedUnits()`
does not take. Attempting it yields only the count:

```c
SWIGIMPORT int UnitInterface_getLoadedUnits(int carg1);   /* where do out_ids and cap go? */
```

The escape hatch is `%extend`, which means hand-writing a replacement method in C++ for every
collection-returning function, every string-returning function, and every function needing the
error latch. At that point SWIG is doing name mangling and nothing else, and the plan's
generator would be doing the same work with full control and no experimental dependency.

---

## 7. Answers to the questions as asked

**Does SWIG parse BWAPI's headers at all, given MSVC extensions and `Templates.h`?** Yes —
those were not the problem. `UnitType.h`, `WeaponType.h` and `Unit.h` all parse. `Game.h` fails
hard on varargs and needs eleven `%ignore`s, which delete four of R2's top-ten entry points.

**Do namespaced enums actually break?** Yes. Namespace context is stripped entirely; two type
modules produce `redeclaration of 'enum Enum'` and `redeclaration of enumerator 'None'` in one
translation unit. All ~848 constants of §5.8 are affected.

**What shape does `BWAPI::Type` come out as? Does `constexpr operator int()` survive?** It comes
out as an opaque pointer, and no — the `Type<>` base is dropped with Warning 401, so `operator
int`, `getID`, `getName`, `c_str`, `toString` and `isValid` are all absent. `%template` could
instantiate it, per type, by hand.

**`std::string`, `Unitset`, `std::function`, varargs?** Opaque `SWIGTYPE_*` pointers for the first
three (heap-allocated, unfreeable); a hard compile error for the fourth. The vararg result is a
design input beyond SWIG — see §10, which settles §4 on exposing plain `const char*` only.

**Can typemaps express any of the §4 conventions?** Three of them cleanly — packed positions,
`int32_t` booleans, integer handles. Not caller-provided buffers, sorted output, size-prefixed
structs, or the sticky error latch, because those change function arity and typemaps cannot.

---

## 8. The rewritten rejection

§1.6 and §9 should say this instead:

> SWIG's C target (`swig -c`) does emit an ISO C interface rather than a per-language C++ layer,
> so the original objection was wrong on its facts. Tested against BWAPI at SWIG 4.5.1, it is
> rejected on five specific grounds, all reproducible:
>
> 1. **Still experimental.** SWIG's own `-help` lists `-c` under "Experimental Target Language
>    Options" and emits Warning 524 on every invocation.
> 2. **`Game.h` does not compile.** Twelve vararg functions are hard errors; suppressing them
>    deletes `drawTextScreen`, `drawTextMap`, `printf` and `sendText` — 463 call sites in R2's
>    corpus.
> 3. **Namespaced enums lose their namespace**, so no two BWAPI type modules can be included in
>    one translation unit. This alone forecloses §5.8, which R2 identified as the product.
> 4. **The `Type<>` and `Interface<T>` CRTP bases are dropped**, taking `getID`, `getName`,
>    `c_str` and `registerEvent` with them. A C caller cannot convert a `UnitType*` to an integer.
> 5. **Every by-value return is heap-allocated with `new` and never freed**, across the DLL
>    boundary — the exact failure §4's caller-buffer rule exists to prevent.
>
> Typemaps do express packed positions, `int32_t` booleans and integer handles correctly, and
> that is worth knowing. They cannot express caller-provided buffers, sorted output,
> size-prefixed structs or the sticky error latch, because those change a function's arity and a
> typemap only transforms values. Reaching them requires `%extend` — hand-written C++ per
> function — at which point SWIG contributes name mangling and an experimental dependency, and
> nothing else.

---

## 9. The fallback finding: clang gives us the spec draft

The plan asked whether libclang or CastXML could generate the first draft of the YAML spec
instead of hand-typing entries. **Confirmed, with no new dependency** — it is the same clang that
R5's layout dump and R6's build already require:

```bash
clang++ -std=c++14 -I<bwapi>/include \
        -Xclang -ast-dump=json -Xclang -ast-dump-filter=UnitType \
        -fsyntax-only ast.cpp > ut.json
```

3.2 MB of JSON, from which a 20-line Python walker recovers **all 81 `UnitType` methods with full
qualified types**:

```
getRace                :: BWAPI::Race () const
whatBuilds             :: const std::pair<UnitType, int> () const
requiredUnits          :: const std::map<UnitType, int> &() const
mineralPrice           :: int () const
...
```

That is the realistic tooling win, and it is better than SWIG for our purpose because it produces
*data we own* rather than *code we did not write*: the same AST feeds the spec draft, the
`check_coverage.py` audit in the other direction, and the R5 layout regression check. Unfiltered
dumps are large (188 MB for `UnitType.h`, because libstdc++ comes along), so use
`-ast-dump-filter` or a libclang cursor walk rather than dumping the whole TU.

`castxml` is not installed here and is not needed; clang's built-in JSON dump is sufficient.

---

## 10. What this does to the plan

- **Rewrite §1.6 and §9's SWIG rejection** with §8 above. The current wording is wrong on the premise and would not survive review by anyone who has run the tool.
- **Keep the generator ours.** The conclusion is unchanged; the reasoning is now evidence rather than assertion, and it is stronger for conceding what typemaps genuinely do.
- **Adopt clang's AST dump for the spec draft**, per §9. It removes the "hand-type 900 entries" objection to the generator without adding a dependency, and it is the same tool three other experiments already rely on.
- **Record the vararg finding as a design input, not just a SWIG problem, and settle it in §4.**
  BWAPI's `printf` / `sendText` / `sendTextEx` / `drawText{,Map,Mouse,Screen}` family is awkward
  across any FFI boundary, not only SWIG's. **§4 should state explicitly that the ABI exposes
  only plain `const char*`: no `...` and no `va_list` in any exported signature. Formatting is
  the host language's job and must be done before the call.**

  ```c
  /* the only shape the ABI exports */
  void bwapi_game_printf         (const char* text);
  void bwapi_game_send_text      (const char* text);
  void bwapi_game_send_text_ex   (int32_t to_allies, const char* text);
  void bwapi_game_draw_text_screen(int32_t x, int32_t y, const char* text);
  void bwapi_game_draw_text_map   (int32_t x, int32_t y, const char* text);
  ```

  Rationale, in order of weight:

  1. **`...` is not portably callable through FFI.** `ctypes`, JNA, koffi, P/Invoke and Go's cgo
     either cannot call variadic functions at all or do so only with per-platform special cases,
     because the variadic calling convention differs from the fixed one on x86-64 SysV, AArch64
     and Win64. An ABI whose stated purpose is to be bound from other languages must not export
     one.
  2. **`va_list` is worse, not a fallback.** It is an opaque, ABI-specific type — `char*` on MSVC
     x86, an array type on glibc (R6 §5 tripped over exactly this) — with no representation in
     any host language's FFI. Exporting `bwapi_game_vprintf(const char*, va_list)` would be
     unusable by every consumer this project exists to serve.
  3. **It is a format-string injection hazard.** Passing caller data as the format argument is
     the classic `printf(user_input)` bug. A single `const char*` text parameter removes the
     class entirely.
  4. **Nothing is lost.** Every host language already has better formatting than C's —
     `format!`, f-strings, `String.format`, template literals. Spot-checking the R2 corpus (~660
     text call sites across the five bots), every format argument I sampled is a **string
     literal** with the values passed as trailing arguments — and the commonest idiom is already
     the degenerate one: `Broodwar->sendText("%s", text.c_str())`, i.e. "I have a string, print
     it." Under a `const char*`-only ABI that becomes `bwapi_game_send_text(text)`. This is a
     sample rather than an exhaustive audit, but I found no counter-example.

  **This is a deliberate divergence from bwapi-c, which got it wrong.** R1 records twelve
  variadic exports in its `Game.h` (`Game_printf(Game*, const char*, ...)` and siblings), plus
  parallel `va_list` forms — and R3's Zig bot consequently calls
  `Game_sendText(Broodwar, "My name is %s", module.*.name)` straight through the boundary, which
  works only because Zig happens to be C-ABI-native. It is precisely what a Python or C# consumer
  could not do.

  **The maintained bindings all already do it our way**, which is the strongest evidence that
  this is the right shape: gobwapi exports `func (g *Game) Printf(text string)` and
  `SendText(text string)`; JBWAPI takes a pre-formatted `String` (its `Text...` parameter is a
  Java-side colour list, resolved before the call, not a C vararg). Neither passes a format
  string across the boundary.
