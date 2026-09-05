# R10. Name availability, and the `bwapi-c2` rename

Checked every registry the plan proposes publishing to, read crates.io's actual ownership policy,
and measured the symbol-collision question rather than assuming it.

**Headline: the plan's naming premise is half wrong in our favour. `bwapi-sys` is indeed
unobtainable — crates.io is first-come, first-serve and will not transfer without the current
owner's approval — but `bwapi-c` itself is **free** on crates.io, npm, PyPI and NuGet; only the
GitHub repo name under `RnDome` is taken. `bwapi-c2` is free everywhere. And on the symbol
prefix: keeping `bwapi_` is right, but not for the reason given — there is no overlap with
`RnDome/bwapi-c` to tolerate, because it exports no `bwapi_*` symbol at all.**

---

## 1. Availability matrix

| Name | crates.io | npm | PyPI | NuGet | GitHub (`RadicalZephyr/`) |
|---|---|---|---|---|---|
| `bwapi` | **taken** (RnDome, 3,787 dl, last 2018) | free | **taken** (Brandwatch API SDK) | free | — |
| `bwapi-sys` | **taken** (RnDome, 6,127 dl, last 2018) | — | — | — | — |
| `bwapi_wrapper` | **taken** (Bytekeeper/rsbwapi, active) | — | — | — | — |
| `rsbwapi` | **taken** (Bytekeeper, active) | — | — | — | — |
| **`bwapi-c`** | **free** | free | free | free | **taken** (`RnDome/bwapi-c`) |
| **`bwapi-c2`** | **free** | free | free | free | **free** |
| `bwapi-c2-sys` | free | — | — | — | — |
| `bwapi2` / `bwapic` / `bwapi-ffi` | free | free | — | — | — |

Notes that matter:

- **`bwapi` is a contested acronym, not a StarCraft one.** PyPI's `bwapi` is Brandwatch's API SDK
  (39 releases, 2016–2019, `BrandwatchLtd/api_sdk`). R3 also turned up `benSlaughter/bwapi`
  (Ruby, Brandwatch) and `TheBastionBot/bwapi` (TypeScript, Bastion Web API). Any unqualified
  `bwapi` package name invites confusion with two unrelated products. Always qualify.
- **`RadicalZephyr/bwapi_c` already exists** (pushed 2026-08-28) — this project's current repo.
  The rename is a repo rename, not a new repo.
- Registry normalisation: crates.io rejects a new name differing from an existing one only by
  `-`/`_`; PyPI normalises both to the same project. So `bwapi_c2` and `bwapi-c2` are one name in
  practice — claiming either claims both.

## 2. crates.io ownership policy: no route to `bwapi-sys`

From crates.io's published policies (`rust-lang/crates.io`, `svelte/src/routes/policies`):

- **First-come, first-serve on crate names.**
- **The crates.io team will not transfer ownership of existing crates without the explicit
  approval of the current owner**, for stated security reasons.
- Owner-initiated deletion requires <72 h since publish, *or* a single owner **and** <1000
  downloads per month of existing publication **and** no dependents.
- The squatting clause targets names reserved "without having any genuine functionality" — it is
  aimed at empty placeholders, not abandoned-but-working crates.

`bwapi-sys` v0.1.2 has genuine functionality and 6,127 downloads. It is abandoned, not squatted.
**There is no policy route to it**, and the only practical route — asking RnDome — is outreach,
which this research plan forbids and which R1 shows would need four contributors who have been
gone eight years.

**So §10.4's `bindings/rust/bwapi-sys` is unpublishable as written, and so is "the safe `bwapi`
crate".** Both names are gone. This is the concrete naming problem the plan needs to fix, and
`bwapi-c2` fixes it: `bwapi-c2-sys` and `bwapi-c2` are both free.

---

## 3. The symbol prefix: I agree, and the reason is stronger than the one given

You asked for pushback if I thought keeping `bwapi_` was wrong. **I don't — keep it.** But the
stated justification should be replaced, because it rests on a collision that does not exist.

**Measured, not assumed.** `RnDome/bwapi-c` exports **zero** symbols beginning `bwapi_`. Its
prefixes are `Unit_`, `Game_`, `Player_`, `Region_`, `Force_`, `Bullet_`, `Client_`, `Iterator_`,
`BwString_`, `Event_` and `BWAPIC_` (R1's inventory; re-checked against its headers). And BWAPI
itself exports **no unmangled symbols at all** — `nm -D` on the `libBWAPILIB.so` built in R7 finds
zero non-`_Z` defined symbols.

So the position is not "we tolerate an overlap because the libraries are incompatible." It is
**"there is no overlap."** That is a better argument and it should be the one written down,
because the weaker version invites someone to re-litigate it later on the grounds that
incompatibility is not actually a licence to collide.

Three further reasons to keep `bwapi_`, in descending weight:

1. **A version number does not belong in an ABI symbol prefix.** `bwapi_c2_unit_get_hit_points`
   encodes "this is the second attempt at a project called bwapi-c" into ~600 symbols, permanently.
   That is a fact about project history, not about the interface. It also reads as a *library*
   version, so it becomes actively misleading the day we ship our own 2.0.
2. **§4 already specifies `bwapi_`** (`bwapi_unit_get_hit_points`, `bwapi_unit_attack_position`,
   `bwapi_unittype_max_hit_points`). Changing the prefix churns every signature in the spec, the
   generator, `api.json`, and every binding, for no functional gain.
3. **Windows has no process-level symbol namespace to collide in.** Exports are per-module and
   bound through the import table, so even a hypothetical co-loaded `bwapi_c.dll` and
   `bwapi_c2.dll` would not interfere.

### The one caveat worth naming, and its mitigation

The only principled objection is **upstream namespace reservation**: if `bwapi/bwapi` ever added
its own C API, `bwapi_` is the prefix it would want, and we would have taken it. R3 measured how
likely that is — heinermann, closing the 2022 request: *"No plans in core BWAPI to support
scripting languages."* Low, but not zero, and it is the argument a reviewer will raise.

It is cheap to be robust against it:

- **Build with hidden visibility and an explicit export list.** `-fvisibility=hidden` plus the
  `.def` file §10.1 already mandates (and, on ELF, a version script). Then the exported set is
  exactly what we choose, and a future rename is a build-file change rather than a source change.
  This also closes the real technical risk, which is not Windows but **ELF symbol interposition**:
  two `RTLD_GLOBAL` objects exporting the same name can bind a call to the wrong one.
- **Do not export anything `bwapi_`-prefixed that is about *us* rather than about BWAPI.**
  Library-identity entry points — version, build info, the error latch's own accessors — are the
  ones most likely to clash semantically with a future upstream C API. Either keep them
  `bwapi_c2_`-prefixed (a small, deliberate exception) or name them unambiguously
  (`bwapi_abi_version`, not `bwapi_version`). I lean to the latter: one prefix, chosen names.

---

## 4. The rename, applied

Replacing §3's table:

| Thing | Was | **Now** |
|---|---|---|
| Project / repository | `bwapi-c` | **`bwapi-c2`** (rename `RadicalZephyr/bwapi_c`) |
| CMake target | `BWAPI_C` | **`BWAPI_C2`** |
| Shared library | `bwapi_c.dll` / `.so` | **`bwapi_c2.dll`**, `libbwapi_c2.so` |
| Import lib / module def | `bwapi_c.lib`, `bwapi_c.def` | **`bwapi_c2.lib`**, **`bwapi_c2.def`** |
| Public headers | `bwapi_c.h`, `bwapi_c_types.h` | **`bwapi_c2.h`**, **`bwapi_c2_types.h`** |
| Header include guards | — | **`BWAPI_C2_H`**, `BWAPI_C2_TYPES_H` |
| **Exported symbol prefix** | `bwapi_` | **`bwapi_` — unchanged** |
| Internal C++ namespace | `BWAPI::CApi` | **`BWAPI::CApi2`**, or keep `BWAPI::CApi` (internal, never exported — see below) |
| Spec files | `api.json`, `api.schema.json` | unchanged (repo-scoped, no ambiguity) |
| Release asset | `bwapi-c-<ver>-win32.zip` | **`bwapi-c2-<ver>-win32.zip`** |
| Rust FFI crate | `bwapi-sys` ❌ taken | **`bwapi-c2-sys`** |
| Rust safe crate | `bwapi` ❌ taken | **`bwapi-c2`** |
| npm package | (unstated) | **`bwapi-c2`** |
| PyPI package | (unstated) | **`bwapi-c2`** |
| NuGet package | (unstated) | **`BwapiC2`** (R3: C# is the strongest unserved target) |
| SPDX license field | (unstated) | **`LGPL-3.0-only`** (R9 §5) |

One judgement call flagged rather than decided: the **internal C++ namespace** is never exported
(hidden visibility, §3 above) and exists only to keep the wrapper's own C++ tidy. Renaming it to
`BWAPI::CApi2` buys nothing and costs a diff. I would leave it `BWAPI::CApi` and note in §3 that
it is deliberately not renamed because it is not part of any interface. Your call.

§3's existing warning still stands and should be kept verbatim: **do not name anything `BWAPIC`**
— `namespace BWAPIC` already exists in `bwapi/include/BWAPI/Client/*.h` for the shared-memory
PODs, which R5 and R7 both had to work with directly.

---

## 5. Note for §10.4

Replace the third bullet. It currently reads that `bindings/rust/bwapi-sys` publishes to
crates.io and "the safe `bwapi` crate" publishes from its own repo. **Both crate names are owned
by RnDome and unobtainable (§2).** The replacement:

> `bindings/rust/bwapi-c2-sys` publishes to crates.io from this repo; the safe `bwapi-c2` crate
> and the `bwapi-c2` npm package publish from their own repos (§7). All carry the LGPL notices
> and declare `license = "LGPL-3.0-only"` (§0, R9).

And add a line recording *why*, so nobody retries it: crates.io is first-come, first-serve and
will not transfer ownership without the current owner's approval; `bwapi-sys` and `bwapi` are
held by an owner inactive since 2018 and are not reclaimable.

---

## 6. Naming decision, stated once

> The project is **`bwapi-c2`**. Every artifact that carries a project identity — repository,
> CMake target, library file, headers, packages, release assets — uses `bwapi-c2` or
> `bwapi_c2`. The **exported symbol prefix stays `bwapi_`**, because measurement shows nothing
> else in the ecosystem uses it: `RnDome/bwapi-c` exports `Unit_*`/`Game_*`/`BWAPIC_*` and no
> `bwapi_*` symbol, and BWAPI itself exports no unmangled symbols at all. The prefix names the
> API being wrapped, not the project wrapping it, and a project-generation number has no place in
> a permanent ABI surface. The library is built with hidden visibility and an explicit export
> list, so the exported set is a build-time decision that can be revisited without touching a
> line of source.
