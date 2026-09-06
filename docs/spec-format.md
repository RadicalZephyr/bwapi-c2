# The spec format

> **Status: provisional.** This is the schema of the YAML files under `tools/abi/spec/`, written
> down before the first one exists (implementation plan 1.2). The emitters, the coverage audit
> and `api.json` (`docs/api-json.md`) are all functions of it; a field that is not here does not
> exist. Plan §9 is the rationale for having a spec at all; this document only says what one
> entry is.

The spec is the source of truth for the ABI. Everything else — the three headers, the
`*.gen.cpp` files, `bwapi_c2.def`, `api.json` and the reference pages — is emitted from it and
checked in, and CI fails when regenerating produces a diff. A change to the ABI is a change to a
spec file; the generated diff is how it is reviewed.

There are three kinds of spec file, all YAML, all under `tools/abi/spec/`:

| File | Holds | Since |
|---|---|---|
| `<interface>.yaml` — `client`, `player`, `game`, `unit`, `force_region`, `types`, `bwem`, … | Function entries, one per export or per skipped declaration | phase 1 |
| `constants.yaml` | The constant families: which enums export, under which prefix, with their values | phase 1 |
| `structs.yaml` | The PODs that cross the boundary: fields, types, flag bits, and the bulk tables (§3) | phase 1 |

Files are read in sorted name order and concatenated. The file a function lives in decides
which header section it lands in and which `src/<file>.gen.cpp` defines it, and nothing else:
`emit_header.py` groups the declarations by spec file, in the order its section list gives
(`client`, `game`, `player`, `unit`, `force_region` into `bwapi_c2.h`; `types` and `bulk` into
`bwapi_c2_types.h`; `bwem*` into `bwapi_c2_bwem.h`), the `.def` sorts by name, and `api.json`
lists in spec order with the file as each function's `section`.

## 1. A function entry

Every entry is a mapping in a top-level list. The plan's example, with every field it uses:

```yaml
- cpp: "PlayerInterface::minerals"
  c:   "bwapi_player_minerals"
  self: player
  returns: int32
  doc: "The player's current mineral count."

- cpp: "UnitInterface::attack(Position, bool)"
  c:   "bwapi_unit_attack_position"
  self: unit
  reentrant: forbidden          # command-queue write (§5.4)
  params: [{name: x, type: int32}, {name: y, type: int32}, {name: queued, type: bool32}]
  returns: bool32
  body: "return self->attack(BWAPI::Position(x, y), queued != 0);"
  doc: "Orders the unit to attack-move to the pixel position (x, y). ..."

- cpp: "UnitInterface::canAttackGrouped"
  skip: "grouped commands are client-mode-impossible; §15 #17"
```

### 1.1 The fields

| Field | Required | Values |
|---|---|---|
| `cpp` | for every wrapper over BWAPI or BWEM; absent only on the ABI's own surface (§1.5) | The C++ declaration this entry accounts for, as `Class::method`, with the parameter types in parentheses when the name is overloaded (§1.2). Must resolve to exactly one declaration |
| `c` | unless `skip` | The exported name, `bwapi_<subject>_<verb>[_<disambiguator>]`, snake_case (plan §4) |
| `self` | unless `skip` | `unit`, `player`, `force`, `region`, `bwem_area`, `bwem_choke`, `bwem_base`, `bwem_neutral`: the function takes that handle as its first parameter, `<kind>_id`. `game`: no handle, but the game must be connected. `bwem_map`: no handle, but BWEM must be initialised. `none`: no handle and no game — the static type accessors and the ABI's own surface |
| `params` | no; default empty | A list of `{name, type}` in order (§1.3). Never the handle, `buf`/`buf_len` or `out`/`cap`; those are implied by `self` and `returns` |
| `returns` | unless `skip` | One of the return kinds of §1.4 |
| `body` | no | The C++ body of the wrapper, when the generated call is not the right one (§1.6). Mutually exclusive with `source` |
| `source` | no | The path of the hand-written file that defines this export, for the pieces §9 names as hand-written: `bulk.cpp`, `commands.cpp`, `closest.cpp`, `bwem_lifecycle.cpp`. The emitter declares it, lists it and describes it, and emits the `static_assert` for its `cpp`, but writes no definition |
| `skip` | instead of `c` | A string naming the rule that excludes the `cpp` declaration: a plan section, a §15 row, or a §1.8 rule. An empty string is an error; "not needed" is not a rule |
| `reentrant` | no; default `allowed` | `allowed` or `forbidden` (plan §5.4). `forbidden` for every shared-memory write, command-queue write and lifecycle call. Carried in `api.json` and the reference from phase 1; the guard that enforces it is emitted only if predicate callbacks ever land |
| `legit_none` | no; default `false` | `true` when a `handle:` return of `BWAPI_NONE` is a legitimate answer and not a rejected handle (plan §6.2: `bwapi_game_self` in a replay, `bwapi_unit_get_target` when idle, …). Only meaningful with a `handle:` return; an error elsewhere |
| `doc` | unless `skip` | Reference-shaped prose (§1.7) |
| `guides` | no | A list of site paths under `how-to/` or `tutorials/` that show the function in use |
| `since` | before 1.0 no; after 1.0 yes | The ABI version that introduced the export, `"1.0"`, `"1.1"`, … |
| `divergences` | no | A list of plan §15 row numbers the function departs from upstream on. Rendered as links on the reference page |

An entry has either `c` or `skip`, never both and never neither. Unknown fields are an error,
so a typo cannot silently drop a flag. One declaration may back more than one export
(`UnitType::requiredUnits` is an accessor and a table), but not an export and a skip.

### 1.2 The `cpp` string

`Class::method`, unqualified by namespace: `PlayerInterface::minerals`, `UnitType::maxHitPoints`,
`BWEM::ChokePoint::Center` (BWEM keeps its namespace because `Map`, `Area` and `Base` are short
enough to collide with nothing today and something tomorrow). When the name is overloaded the
parameter types follow in parentheses, spelled the way `draft_spec.py` spells them: as declared,
with `BWAPI::` and `BWEM::` qualifiers, `const` and reference qualifiers dropped, and one space
after each comma. `UnitInterface::attack(Position, bool)`;
`GameInterface::getUnitsInRectangle(int, int, int, int, UnitFilter)`. The method's own `const`
is never written. The string must match exactly one declaration in the audited headers, and both
`emit_source.py` (through the `static_assert` it writes for every `body:` and `source:` entry)
and `check_coverage.py` fail when it matches none or more than one.

Default arguments do not exist at the ABI (C has none), so an overload with defaults is one
declaration with every parameter explicit; the per-language wrapper re-adds the defaults.

### 1.3 Parameter types

| `type` | C spelling | Notes |
|---|---|---|
| `int32` | `int32_t` | |
| `bool32` | `int32_t` | 0 or 1; the wrapper passes `!= 0` |
| `double` | `double` | |
| `type:<Class>` | `int32_t` | A type id: `type:UnitType`, `type:Race`, `type:UpgradeType`, … The wrapper constructs `BWAPI::<Class>(id)` |
| `handle:<kind>` | `bwapi_<kind>_id` | A second handle beside `self`, resolved and range-checked the same way. A parameter named `other` is `other_id` in the C signature and the resolved pointer `other` in a body, the same pairing as `player_id` and `self` (`bwapi_player_is_ally(player_id, other_id)`) |
| `string_in` | `const char*` | Passed through, no transcoding (plan §4) |
| `int32_out`, `double_out`, `position_out` | `int32_t*`, `double*`, `bwapi_position*` | One value written. NULL is allowed and skips the write |
| `int32_array_out`, `int16_array_out`, `uint8_array_out`, `position_array_out` | `int32_t*`, `int16_t*`, `uint8_t*`, `bwapi_position*` | An array the body fills; the `cap` that sizes it is a separate `int32` parameter the entry names |
| `struct_in:<name>`, `struct_out:<name>`, `struct_array_out:<name>` | `const bwapi_<name>*`, `bwapi_<name>*` | Size-prefixed PODs from `structs.yaml` (plan §4) |
| `callback:<typedef>` | the typedef | `bwapi_log_callback`, `bwapi_error_callback` |
| `void_ptr` | `void*` | The `user` beside a callback, and nothing else |

A position parameter is two `int32` parameters, `x` and `y` (plan §4: packing is for returns).
Every out-pointer type is for `body:` and `source:` entries; a generated call never needs one.

### 1.4 Return kinds and their neutral values

The neutral value is what a function returns when it does not run: a wrong thread, no
connection, an invalid handle, a bad buffer, or an exception inside. It is stated once here, per
kind, and the emitter writes it into every wrapper and every reference page.

| `returns` | C type | Neutral value | Conversion from the C++ value |
|---|---|---|---|
| `int32` | `int32_t` | `0` | `static_cast<int32_t>` |
| `bool32` | `int32_t` | `0` | `? 1 : 0` |
| `double` | `double` | `0.0` | none |
| `type:<Class>` | `int32_t` | the class's `Unknown` id: `BWAPI::<Class>(-1).getID()`, which `Type<>`'s constructor clamps to its `UnknownId` argument (`UnitTypes::Unknown` = 234, `Races::Unknown` = 8, `Color(-1)` = 255, …) | `.getID()` |
| `position`, `tile_position`, `walk_position` | `bwapi_position` | the packed `None` of the kind's own scale: `BWAPI_POSITION_NONE` (32000/32032), `BWAPI_TILEPOSITION_NONE` (1000/1001), `BWAPI_WALKPOSITION_NONE` (4000/4004) | `BWAPI_POS_MAKE(p.x, p.y)` |
| `handle:<kind>` | `bwapi_<kind>_id` | `BWAPI_NONE` | the kind's id: `getID()`, BWEM `Id()` / `Index()`, the base table, `Unit()->getID()` |
| `string_out` | `int32_t` | an empty string (one NUL, when `buf_len > 0`) and `0` | `write_string(buf, buf_len, std::string)` |
| `id_array` | `int32_t` | nothing written, `0` | fills `out` with the ids of a set of interfaces sorted ascending, up to `cap`; returns the total |
| `position_array` | `int32_t` | nothing written, `0` | fills `out` with packed positions **in upstream's order** (a chokepoint's geometry is a polyline; sorting it would destroy it), up to `cap`; returns the total |
| `struct_array:<name>` | `int32_t` | nothing written, `0` | `body:` or `source:` only; the caller's `size` on element zero is the stride |
| `void` | `void` | nothing | none |

The three position kinds share one C type and one rule for the neutral value: the packed
`None` of the function's own scale (plan §4). They differ in which sentinel that is and in what
the documentation says the scale is, and the kind is the whole reason there are three: a
tile-scale caller compares against `BWAPI_TILEPOSITION_NONE` after a latch, which is the
sentinel it would compare against anyway. An earlier draft made `BWAPI_POSITION_NONE` the
neutral for all three scales in the name of one rule; that was one rule with a foot-gun in it,
since a tile-scale consumer would never think to test for a pixel-scale sentinel, and the
emitter knows the kind, so the scale-correct sentinel costs nothing.

`id_array` is the `int32_t` member of the array family; `position_array` and `struct_array` are
the other two. All three expand the same way (§1.5) and follow the same rule for a short buffer:
the first `cap` elements in output order, and the total as the return, so a retry with a larger
buffer is coherent with the truncated result (plan §4).

### 1.5 How the signature is built

The emitter never reads a signature from the spec; it builds one, in this order:

1. the handle, when `self` is a handle kind: `bwapi_<kind>_id <kind>_id`;
2. every entry of `params`, in order;
3. for `string_out`: `char* buf, int32_t buf_len`; for `id_array`, `position_array` and
   `struct_array`: `<elem>* out, int32_t cap`.

So a spec entry never spells `buf`, `buf_len`, `out` or `cap`, and no two functions can disagree
about their order or their names. A function with no parameters at all is declared `(void)`.

`bwapi_player_get_name` is therefore
`int32_t bwapi_player_get_name(bwapi_player_id player_id, char* buf, int32_t buf_len)` from
three fields: `self: player`, no `params`, `returns: string_out`.

**The ABI's own surface** — the version functions, the error channel, the two callback setters
and the client lifecycle — is specced too, in `client.yaml`, so that the header is wholly
generated and `api.json` is complete. Those entries have `self: none` (or `game` for the
lifecycle calls that need a connection), a `body:` or a `source:`, and no `cpp` where there is
nothing to account for: `bwapi_last_error` wraps no C++ declaration. `bwapi_client_connect`
does name `BWAPI::Client::connect`, because the audit should know the client is covered.

### 1.6 What a `body:` may assume

A `body:` is the inside of the wrapper's lambda, after the prologue and before the conversion,
and it is fully typed C++ with these names in scope:

- `self`: the resolved `BWAPI::Unit` / `Player` / `Force` / `Region`, or the BWEM
  `const Area*` / `const ChokePoint*` / `const Base*` / `const Neutral*`, for a handle kind;
  `BWAPI::Broodwar` (a `Game*`) for `game`; `BWEM::Map::Instance()` as `map` for `bwem_map`;
  nothing for `none`;
- every `params` entry by name, as its C type; a `handle:` parameter named `x` is the C
  parameter `x_id` and, in the body, the resolved pointer `x` (a failed resolution has already
  returned the neutral value);
- `buf` and `buf_len`, or `out` and `cap`, for the kinds that have them, already checked for
  the NULL-with-nonzero and negative cases;
- everything `abi_internal.h` declares: `latch`, `log`, `write_string`, `write_ids`,
  `write_positions`, `pack`, the resolvers.

The body returns the **C++ value** of the return kind — a `BWAPI::Race`, a `Position`, a
`std::string`, a `Unitset` or other range of interfaces for `id_array` — and the emitter applies
the conversion of §1.4. A body for a `void` function returns nothing. A body that must fail
calls `latch(...)` and returns the neutral C++ value itself.

A `body:` never re-implements the prologue, never `try`s and never touches the error channel
except to latch. Everything a body could get wrong about the boundary is generated around it.

For every `body:` and `source:` entry the emitter also writes a `static_assert` that the `cpp`
declaration exists (plan §9): a `body:` is opaque to the coverage audit, and this is what keeps
it from being a hole. The assertion proves existence and no more. For an unoverloaded name it
takes the member's address, which fails to compile when the name is gone; for an overloaded one
it forms a call with the parameter types the spec spells, which fails when no overload accepts
them. Neither checks the return type, because the spec never states a C++ signature to check
against. What catches a changed return type at a pin bump is the body itself: it is fully typed
C++ that converts the call's result to the return kind, so a `UnitType` that became a
`std::pair` fails the build of `*.gen.cpp` in the body, not in the assertion.

### 1.7 `doc` is reference-shaped by rule

`doc` is what the header shows on hover (its first sentence) and what the reference page renders
in full (plan §16.1). It states, in this order and in prose: what the function does; what each
parameter means where the name does not say; what the return is; anything function-specific
about when it latches an error. It does not say how to use the function, when to prefer another,
or what a bot should do with the answer; a "you should" in a `doc:` is moved to a guide under
`guides:` at review.

The parts of a reference page that are the same for every function of a kind — the neutral
value, the `INVALID_HANDLE` latch for a handle kind, the `NOT_CONNECTED` latch for everything
but `none`, `BWEM_NOT_INITIALIZED` for the BWEM kinds, `BAD_BUFFER` for the buffer kinds,
`WRONG_THREAD` for everything — are **not** in `doc`. `emit_docs.py` derives them from the
kinds, once, and a doc that repeats them is a doc with two places to be wrong.

## 2. Constant families

`constants.yaml` is a list of families. A family names a C++ enumeration (or, for
`BWAPI::Colors`, a namespace of `constexpr Color` variables, the one family that is not an
enum), the prefix its constants carry, and the values, which `draft_spec.py --enum` writes from
the AST and which are never typed by hand:

```yaml
- family: unit_type
  cpp: "BWAPI::UnitTypes::Enum::Enum"
  prefix: BWAPI_UNIT
  values:
    - {name: Terran_Marine, value: 0}
    - {name: Terran_Ghost, value: 1}
    # ...
- family: key
  cpp: "BWAPI::Key"
  prefix: BWAPI_KEY
  strip: "K_"
  values: [...]
```

| Field | Values |
|---|---|
| `family` | The snake_case name the constants are grouped under in `api.json` and the reference (`unit_type`, `order`, `event_type`, `text_size`, `color`, …) |
| `cpp` | The enumeration, fully qualified, as `BWAPI::UnitTypes::Enum::Enum` (BWAPI spells its type enumerations `namespace Enum { enum Enum {...} }`), `BWAPI::EventType::Enum`, `BWAPI::Key`; or the namespace, for `BWAPI::Colors` |
| `prefix` | The macro prefix |
| `strip` | Optional: a prefix removed from every enumerator name before the transformation (`K_` for `Key`, `M_` for `MouseButton`) |
| `doc` | Optional: what the family is, one paragraph. It is the family's `doc` in `api.json` (`docs/api-json.md` §3) and the body of its reference table page |
| `values` | `{name, value}` pairs in declaration order, from the AST. `draft_spec.py --enum <cpp>` prints them; `--update-constants` rewrites this list in place, which is the pin-bump step |

**The C name** is `<prefix>_<NAME>`, where `NAME` is the enumerator with an underscore inserted
at every lower-to-upper boundary and the whole uppercased: `Terran_Marine` → `TERRAN_MARINE`,
`AllUnits` → `ALL_UNITS`, `Zerg_Lurker_Egg` → `ZERG_LURKER_EGG`, `Unknown_0x0E` →
`UNKNOWN_0X0E`. Enumerators beginning with two underscores (`Key`'s `__UNDEFINED_7`,
`__RESERVED_A`) are not exported: they are placeholders upstream, and reserved identifiers in C.
`MAX`/`Max` sentinels are exported like anything else.

Every generated constant is also a `static_assert` in `constants.gen.cpp` against the C++ value
(`static_assert(BWAPI_UNIT_TERRAN_MARINE == BWAPI::UnitTypes::Enum::Terran_Marine)`), so a
value in the spec that drifts from upstream fails the build, not a test. The position sentinels
are not a family: they are written by hand into `bwapi_c2_types.h`'s template and asserted the
same way.

## 3. Structs

`structs.yaml` holds every POD that crosses the boundary: from phase 1 the bulk type table rows
of plan §5.8 and the flat `requiredUnits` row; from phase 2 `bwapi_event`, `bwapi_bullet` and
the snapshots. A struct is `{name, doc, fields: [{name, type, doc}], flags: [{name, bit, doc}]}`,
where `type` is one of `int32`, `bool32`, `double`, `int16`, `uint8`, `uint32`, `type:<Class>`
(an `int32_t` holding a type id), any of those with an `int32[3]`-style fixed-array suffix;
`size` is always the first field and never listed; and `flags` names the bits of a
`uint32_t flags` field when the struct has one. The C type is `bwapi_<name>`.

A struct that is one row of a bulk table also carries a `table: {class, c, doc}` block, and
each field after `id` a `from:` naming the accessor it mirrors:

```yaml
- name: race_row
  doc: "One row of the Race table: every scalar accessor of the class for one id."
  table:
    class: Race
    c: bwapi_race_table
    doc: "Fills one bwapi_race_row per Race id, 0 to Unknown inclusive, up to cap; returns the total."
  fields:
    - {name: id, type: int32, doc: "the type id, which is also the row's index"}
    - {name: get_worker, type: "type:UnitType", from: "Race::getWorker"}
```

The `table:` block declares a function the loader adds to the `bulk` section as a `body:` entry
(`bwapi_race_table(bwapi_race_row* out, int32_t cap)`, `self: none`, `returns:
struct_array:race_row`): one row per id of the class, `0` to `Unknown` inclusive, each field
filled by the accessor its `from:` names and converted by the field's type, through the stride
rule of plan §4. `from:` is meaningful only under a `table:`, and a table row's first field is
always `id`. The one table a `from:` cannot express, the flat `requiredUnits` table, is a
hand-written `source:` entry in `bulk.yaml` over a plain struct.

## 4. What is deliberately not in the format

- **No per-entry declaration hash, no schema version, no per-entry `divergence:` requirement**
  (plan §9, end): the audit diffs signatures at pin bumps, `api_json_version` is the compatibility
  handle, and `divergences` is optional metadata for the reference.
- **No `mutates:` flag.** `reentrant` is the flag, because what §5.4 gates is not "mutates game
  state" (drawing does not; `disconnect` frees a pointer).
- **No formatting of any kind in `doc`.** Plain prose; the templates lay out.
- **No signature text.** Built, never spelled (§1.5).
