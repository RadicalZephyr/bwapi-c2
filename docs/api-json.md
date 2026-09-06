# `api.json`

> **Status: provisional.** `api.json` is the machine-readable description of the ABI (plan §7,
> §9, §16.1): the file the Python, C# and Rust raw layers are generated from, the file the
> reference pages are rendered from, and the file a third-party binding author downloads from
> `/api.json` on the site. It is emitted by `tools/abi/emit_json.py` from the spec
> (`docs/spec-format.md`) and checked in. Every field is documented here; a field that is not
> here does not exist, and a consumer that reads one that is not here is reading a bug.

There is no JSON Schema yet, deliberately (plan §7): two raw layers consuming this file in CI are
its stability test for those two consumers, and the schema comes back the day a third-party
consumer appears. Until then the top-level `api_json_version` is the compatibility handle: it
is bumped when a field changes meaning or goes away, and never when one is added.

## Top level

| Field | Type | Meaning |
|---|---|---|
| `api_json_version` | integer | The format version of this file. `1` today. Additive changes do not bump it |
| `abi_version` | string | `bwapi_abi_version()` of the library this file describes, `"major.minor.patch"`, from `project(VERSION)` in `CMakeLists.txt` |
| `functions` | array | Every exported function, in spec order (§2) |
| `constants` | array | Every constant family (§3) |
| `structs` | array | Every POD that crosses the boundary (§4): the bulk table rows from phase 1, the events, bullets and snapshots from phase 2 |

## 2. A function

| Field | Type | Meaning |
|---|---|---|
| `c` | string | The exported name. Binding is by name; there are no ordinals |
| `section` | string | The spec file the function came from (`client`, `player`, `types`, …). The reference page lives under `/reference/<section>/<c>/`, and a raw layer may use it to group |
| `signature` | string | The C declaration without the linkage macros, for humans and for a generator that wants to show it. Never parse it: `params` and `returns` are the structured form |
| `cpp` | string or null | The C++ declaration the function wraps (`docs/spec-format.md` §1.2), or null for the ABI's own surface |
| `self` | string | The handle kind of the first parameter (`unit`, `player`, `force`, `region`, `bwem_area`, `bwem_choke`, `bwem_base`, `bwem_neutral`), or `game` (no handle; needs a connection), `bwem_map` (no handle; needs BWEM initialised) or `none` (needs nothing) |
| `params` | array | Every C parameter in order, the handle and the implied buffer pair included (§2.1) |
| `returns` | object | The return kind and its C type and neutral value (§2.2) |
| `reentrant` | string | `allowed` or `forbidden` (plan §5.4): whether the function may be called from inside a callback |
| `legit_none` | boolean | For a `handle:` return: `BWAPI_NONE` is a legitimate answer, not only a rejected handle (plan §6.2) |
| `generated` | boolean | `true` when the wrapper is emitted from `cpp` alone; `false` when the spec supplied a `body:` or the definition is hand-written |
| `doc` | string | The reference text: what the function does, its parameters, its return, its function-specific latches. Plain prose, one paragraph. The kind-derived facts (the neutral value, the standard latches) are not repeated here |
| `guides` | array of strings | Optional. Site paths of guides that show the function in use |
| `since` | string | Optional before 1.0, present after. The ABI version that introduced the function |
| `divergences` | array of integers | Optional. Plan §15 rows the function departs from upstream on |

### 2.1 A parameter

| Field | Type | Meaning |
|---|---|---|
| `name` | string | The C parameter name. A handle parameter named `x` in the spec is `x_id` here |
| `type` | string | The spec's parameter type (`docs/spec-format.md` §1.3): `int32`, `bool32`, `double`, `type:<Class>`, `handle:<kind>`, `string_in`, `void_ptr`, `callback:<typedef>`, the `*_out` pointer kinds, the `struct_*:<name>` kinds; plus two that only the implied buffer pair uses: `string_buf` for `char* buf` and `array_out` for the `out` of an array-returning function |
| `c_type` | string | The C spelling: `int32_t`, `double`, `const char*`, `bwapi_player_id`, `int32_t*`, `bwapi_position*`, `bwapi_log_callback`, `void*`, … A generator maps this and nothing else |

A `bool32` parameter takes 0 or 1. A `type:<Class>` parameter takes an id from the constant
family `returns.constants` names for that class (`type:UnitType` takes `BWAPI_UNIT_*`). A
`handle:<kind>` parameter takes an id of that kind; a value that could never have been valid
returns the neutral value and latches `BWAPI_ERR_INVALID_HANDLE`.

### 2.2 The return

| Field | Type | Meaning |
|---|---|---|
| `kind` | string | The spec's return kind (`docs/spec-format.md` §1.4): `int32`, `bool32`, `double`, `type:<Class>`, `position`, `tile_position`, `walk_position`, `handle:<kind>`, `string_out`, `id_array`, `position_array`, `struct_array:<name>`, `void` |
| `c_type` | string | The C spelling of the return: `int32_t`, `double`, `bwapi_position`, `bwapi_force_id`, `void` |
| `neutral` | string | The value returned when the function does not run (wrong thread, not connected, invalid handle, bad buffer, exception), as prose: `"0"`, `"BWAPI_NONE"`, `"BWAPI_POSITION_NONE"`, `"an empty string, and 0"`, … |
| `type_class` | string | For `type:` kinds: the C++ type class (`Race`, `UnitType`, …) |
| `constants` | string | For `type:` kinds: the prefix of the constant family the ids belong to (`BWAPI_RACE`) |
| `family` | string | For `type:` kinds: the `family` in `constants` the ids are listed under (`race`) |
| `handle_kind` | string | For `handle:` kinds: the kind (`force`, `unit`, …) |
| `scale` | string | For the position kinds: `pixel`, `tile` or `walk`. All three are one packed `int64_t` with the same neutral value |

For `string_out` the C function takes `char* buf, int32_t buf_len` after the spec's parameters
and returns the length the string needs, snprintf-style. For `id_array`, `position_array` and
`struct_array:` it takes `<elem>* out, int32_t cap` and returns the total available, having
written the first `min(cap, total)` elements. Both pairs appear in `params`.

## 3. A constant family

| Field | Type | Meaning |
|---|---|---|
| `family` | string | The family's name: `unit_type`, `order`, `error`, `log_level`, `position_sentinel`, … |
| `prefix` | string | The macro prefix every value's name carries |
| `cpp` | string or null | The C++ enumeration the values come from, or null for the ABI's own families (`error`, `log_level`, `position_sentinel`) |
| `doc` | string | What the family is |
| `values` | array | `{name, cpp_name, value, doc}`: the C macro name, the C++ enumerator it came from (null for the ABI's own), the integer value, and a per-value note where there is one. In declaration order |

The three ABI-own families are not from any enum. `error` is the `BWAPI_ERR_*` codes of the
sticky channel; `log_level` the `BWAPI_LOG_*` argument of the log callback; `position_sentinel`
the unpacked `_X`/`_Y` halves of BWAPI's `Invalid`, `None`, `Unknown` and `Origin` for the
pixel, walk and tile scales. The packed forms are not listed: a consumer forms them with the
documented packing (x in the low 32 bits, y in the high 32, two's complement), which is what
`BWAPI_POS_MAKE` does.

## 4. A struct

One entry per struct in `structs.yaml` (`docs/spec-format.md` §3), in spec order.

| Field | Type | Meaning |
|---|---|---|
| `name` | string | The spec name (`unittype_row`, `required_unit`, …) |
| `c_type` | string | `bwapi_<name>`, the typedef a parameter's `c_type` points at |
| `doc` | string | What the struct is, one paragraph |
| `fields` | array | Every field in declaration order, **`size` first**: `{name, type, c_type, doc}`, plus `from` on a table row's fields. `type` is the spec type (`int32`, `bool32`, `double`, `int16`, `uint8`, `uint32`, `type:<Class>`, with an `[N]` suffix for a fixed array); `c_type` is its C spelling with the array suffix attached; `from` is the C++ accessor the field mirrors (`UnitType::maxHitPoints`) |
| `flags` | array | The bits of a `uint32_t flags` field, `{name, bit, doc}`, when the struct has one; empty otherwise |
| `table` | object | Present on a bulk table row only: `{class, c}`, the type class the rows enumerate and the function that fills them (`bwapi_unittype_table`) |

A generator declares the struct with every field in order, `size` included, at the C types
given; a consumer sets `size` on element zero before an array-out call (plan §4) and reads it
back per row to learn what the DLL filled.

## 5. What a generator should and should not do

Map `c_type` to the host's type, declare the function under `c` with `params` in order, and
expose `constants` as named integers. Do not parse `signature`, do not infer anything from a
name, and do not special-case a section: the file carries everything a raw layer needs, and a
raw layer that reads the C headers as well has two sources of truth.
