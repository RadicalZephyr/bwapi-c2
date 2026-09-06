#!/usr/bin/env python3
"""The spec format as code (docs/spec-format.md): the naming rules and the cpp-string
normalisation that draft_spec.py, the emitters and check_coverage.py must all agree on, and the
loader that reads tools/abi/spec/*.yaml and refuses anything the format does not define.

Nothing here runs clang; nothing here writes a file.
"""
import glob
import os
import re

import yaml

HERE = os.path.dirname(os.path.abspath(__file__))
SPEC_DIR = os.path.join(HERE, "spec")

# ---- names --------------------------------------------------------------------------------------

_CAMEL_BOUNDARY = re.compile(r"(?<=[a-z0-9])(?=[A-Z])|(?<=[A-Z])(?=[A-Z][a-z])")


def snake_case(name):
    """getHitPoints -> get_hit_points; isUnitAvailable -> is_unit_available; Terran_SCV -> terran_scv."""
    return _CAMEL_BOUNDARY.sub("_", name).replace("__", "_").lower()


def constant_name(prefix, enumerator, strip=""):
    """spec-format.md section 2: the enumerator with an underscore at every lower-to-upper
    boundary, uppercased, under the family's prefix. Terran_Marine -> BWAPI_UNIT_TERRAN_MARINE;
    AllUnits -> BWAPI_UNIT_ALL_UNITS; Unknown_0x0E -> BWAPI_GAMETYPE_UNKNOWN_0X0E."""
    name = enumerator
    if strip and name.startswith(strip):
        name = name[len(strip):]
    name = re.sub(r"(?<=[a-z])(?=[A-Z])", "_", name)
    return f"{prefix}_{name.upper()}"


def exportable_enumerator(name):
    """Enumerators beginning with two underscores are upstream placeholders and reserved
    identifiers in C; they are not exported (spec-format.md section 2)."""
    return not name.startswith("__")


# ---- the cpp string -------------------------------------------------------------------------------

_QUALIFIERS = re.compile(r"\b(const|volatile)\b|&|\bBWAPI::|\bBWEM::")


def normalize_type(spelling):
    """spec-format.md section 1.2: as declared, with BWAPI::/BWEM:: qualifiers, const and
    reference qualifiers dropped, whitespace collapsed. 'const BWAPI::Player' -> 'Player';
    'const UnitFilter &' -> 'UnitFilter'; 'std::pair<UnitType, int>' unchanged."""
    s = _QUALIFIERS.sub("", spelling)
    s = re.sub(r"\s+", " ", s).strip()
    s = s.replace(" <", "<").replace("< ", "<").replace(" >", ">").replace(" ,", ",")
    s = re.sub(r",\s*", ", ", s)
    return s


def cpp_key(qualified_class, method, param_types, overloaded):
    """The cpp: string for one declaration: Class::method, with the normalised parameter types in
    parentheses only when the name is overloaded within its class."""
    if not overloaded:
        return f"{qualified_class}::{method}"
    return f"{qualified_class}::{method}({', '.join(normalize_type(t) for t in param_types)})"


def split_cpp(cpp):
    """'UnitInterface::attack(Position, bool)' -> ('UnitInterface', 'attack', ['Position', 'bool'])
    and 'PlayerInterface::minerals' -> ('PlayerInterface', 'minerals', None)."""
    m = re.fullmatch(r"([\w:]+)::(~?\w+|operator\S+)(?:\((.*)\))?", cpp.strip())
    if not m:
        raise ValueError(f"malformed cpp string: {cpp!r}")
    cls, method, params = m.group(1), m.group(2), m.group(3)
    if params is None:
        return cls, method, None
    params = params.strip()
    return cls, method, [normalize_type(p) for p in re.split(r",\s*(?![^<]*>)", params)] if params else []


# ---- the vocabulary --------------------------------------------------------------------------------

HANDLE_KINDS = ("unit", "player", "force", "region", "bwem_area", "bwem_choke", "bwem_base", "bwem_neutral")
SELF_KINDS = HANDLE_KINDS + ("game", "bwem_map", "none")

SCALAR_PARAM_TYPES = ("int32", "bool32", "double", "string_in", "void_ptr",
                      "int32_out", "double_out", "position_out",
                      "int32_array_out", "int16_array_out", "uint8_array_out", "position_array_out")
SCALAR_RETURN_KINDS = ("int32", "bool32", "double", "position", "tile_position", "walk_position",
                       "string_out", "id_array", "position_array", "void")

ENTRY_FIELDS = {"cpp", "c", "self", "params", "returns", "body", "source", "skip", "reentrant",
                "legit_none", "doc", "guides", "since", "divergences"}
FAMILY_FIELDS = {"family", "cpp", "prefix", "strip", "doc", "values"}
STRUCT_FIELDS = {"name", "doc", "fields", "flags", "table"}
FIELD_TYPES = ("int32", "bool32", "double", "int16", "uint8", "uint32")


class SpecError(Exception):
    pass


def _check_param_type(t, where):
    if t in SCALAR_PARAM_TYPES:
        return
    for prefix in ("type:", "handle:", "struct_in:", "struct_out:", "struct_array_out:", "callback:"):
        if t.startswith(prefix) and len(t) > len(prefix):
            if prefix == "handle:" and t[len(prefix):] not in HANDLE_KINDS:
                raise SpecError(f"{where}: unknown handle kind in {t!r}")
            return
    raise SpecError(f"{where}: unknown parameter type {t!r}")


def _check_return_kind(t, where):
    if t in SCALAR_RETURN_KINDS:
        return
    for prefix in ("type:", "handle:", "struct_array:"):
        if t.startswith(prefix) and len(t) > len(prefix):
            if prefix == "handle:" and t[len(prefix):] not in HANDLE_KINDS:
                raise SpecError(f"{where}: unknown handle kind in {t!r}")
            return
    raise SpecError(f"{where}: unknown return kind {t!r}")


def validate_entry(e, where):
    unknown = set(e) - ENTRY_FIELDS
    if unknown:
        raise SpecError(f"{where}: unknown field(s) {sorted(unknown)}")
    if ("skip" in e) == ("c" in e):
        raise SpecError(f"{where}: an entry has either c: or skip:, never both and never neither")
    if "skip" in e:
        if not isinstance(e["skip"], str) or not e["skip"].strip():
            raise SpecError(f"{where}: skip: must name the rule that excludes the declaration")
        if "cpp" not in e:
            raise SpecError(f"{where}: skip: needs the cpp: it skips")
        extra = set(e) - {"cpp", "skip"}
        if extra:
            raise SpecError(f"{where}: a skipped entry carries only cpp: and skip:, not {sorted(extra)}")
        return
    for required in ("c", "self", "returns", "doc"):
        if required not in e:
            raise SpecError(f"{where}: missing {required}:")
    if not re.fullmatch(r"bwapi_[a-z0-9_]+", e["c"]):
        raise SpecError(f"{where}: c: {e['c']!r} is not a bwapi_ snake_case name")
    if e["self"] not in SELF_KINDS:
        raise SpecError(f"{where}: self: {e['self']!r} is not one of {SELF_KINDS}")
    _check_return_kind(e["returns"], where)
    for i, p in enumerate(e.get("params") or []):
        if set(p) != {"name", "type"}:
            raise SpecError(f"{where}: params[{i}] must be {{name, type}}")
        if not re.fullmatch(r"[a-z][a-z0-9_]*", p["name"]):
            raise SpecError(f"{where}: params[{i}] name {p['name']!r} is not snake_case")
        _check_param_type(p["type"], f"{where} params[{i}]")
    if "body" in e and "source" in e:
        raise SpecError(f"{where}: body: and source: are mutually exclusive")
    if "cpp" not in e and "body" not in e and "source" not in e:
        raise SpecError(f"{where}: an entry without cpp: must carry body: or source:")
    if e.get("reentrant", "allowed") not in ("allowed", "forbidden"):
        raise SpecError(f"{where}: reentrant: is allowed or forbidden")
    if "legit_none" in e:
        if not isinstance(e["legit_none"], bool):
            raise SpecError(f"{where}: legit_none: is a bool")
        if e["legit_none"] and not e["returns"].startswith("handle:"):
            raise SpecError(f"{where}: legit_none: is only meaningful with a handle: return")
    if not isinstance(e["doc"], str) or not e["doc"].strip():
        raise SpecError(f"{where}: doc: is required prose")
    for field in ("guides", "divergences"):
        if field in e and not isinstance(e[field], list):
            raise SpecError(f"{where}: {field}: is a list")
    if "since" in e and not isinstance(e["since"], str):
        raise SpecError(f"{where}: since: is a string like \"1.0\"")


def validate_family(f, where):
    unknown = set(f) - FAMILY_FIELDS
    if unknown:
        raise SpecError(f"{where}: unknown field(s) {sorted(unknown)}")
    for required in ("family", "cpp", "prefix", "values"):
        if required not in f:
            raise SpecError(f"{where}: missing {required}:")
    if not re.fullmatch(r"[a-z][a-z0-9_]*", f["family"]):
        raise SpecError(f"{where}: family: {f['family']!r} is not snake_case")
    if not re.fullmatch(r"BWAPI_[A-Z0-9_]+", f["prefix"]):
        raise SpecError(f"{where}: prefix: {f['prefix']!r} is not BWAPI_UPPER")
    seen = set()
    for i, v in enumerate(f["values"]):
        if set(v) != {"name", "value"} or not isinstance(v["value"], int):
            raise SpecError(f"{where}: values[{i}] must be {{name, value: int}}")
        if v["name"] in seen:
            raise SpecError(f"{where}: enumerator {v['name']!r} listed twice")
        seen.add(v["name"])


def validate_struct(s, where):
    unknown = set(s) - STRUCT_FIELDS
    if unknown:
        raise SpecError(f"{where}: unknown field(s) {sorted(unknown)}")
    for required in ("name", "doc", "fields"):
        if required not in s:
            raise SpecError(f"{where}: missing {required}:")
    for i, fld in enumerate(s["fields"]):
        if set(fld) - {"name", "type", "doc", "from"} or "name" not in fld or "type" not in fld:
            raise SpecError(f"{where}: fields[{i}] must be {{name, type, doc?, from?}}")
        if fld["name"] == "size":
            raise SpecError(f"{where}: size is implied, never listed")
        base = re.sub(r"\[\d+\]$", "", fld["type"])
        if base not in FIELD_TYPES and not base.startswith("type:"):
            raise SpecError(f"{where}: fields[{i}] type {fld['type']!r} is not a struct field type")
        if "from" in fld and "table" not in s:
            raise SpecError(f"{where}: fields[{i}] from: is only meaningful in a table: struct")
    if "table" in s:
        t = s["table"]
        if set(t) != {"class", "c", "doc"}:
            raise SpecError(f"{where}: table: must be {{class, c, doc}}")
        if t["class"] not in TYPE_CLASS_PREFIX:
            raise SpecError(f"{where}: table.class {t['class']!r} is not a type class")
        if not s["fields"] or s["fields"][0]["name"] != "id":
            raise SpecError(f"{where}: a table row's first field is id")
    for i, flag in enumerate(s.get("flags") or []):
        if set(flag) - {"name", "bit", "doc"} or "name" not in flag or "bit" not in flag:
            raise SpecError(f"{where}: flags[{i}] must be {{name, bit, doc?}}")


# ---- loading ----------------------------------------------------------------------------------------


class Spec:
    """Everything under spec/: the function entries by file, the constant families and the
    structs, validated. `entries` is a list of (file_stem, entry) in sorted file order."""

    def __init__(self, spec_dir=SPEC_DIR):
        self.spec_dir = spec_dir
        self.entries = []
        self.families = []
        self.structs = []
        for path in sorted(glob.glob(os.path.join(spec_dir, "*.yaml"))):
            stem = os.path.splitext(os.path.basename(path))[0]
            with open(path, encoding="utf-8") as f:
                data = yaml.safe_load(f) or []
            if not isinstance(data, list):
                raise SpecError(f"{path}: a spec file is a top-level list")
            for i, item in enumerate(data):
                where = f"{os.path.basename(path)}[{i}]"
                if not isinstance(item, dict):
                    raise SpecError(f"{where}: not a mapping")
                if stem == "constants":
                    validate_family(item, where)
                    self.families.append(item)
                elif stem == "structs":
                    validate_struct(item, where)
                    self.structs.append(item)
                    if "table" in item:
                        self.entries.append(("bulk", table_entry(item)))
                else:
                    validate_entry(item, where)
                    self.entries.append((stem, item))
        self._check_unique()

    def _check_unique(self):
        seen_c, seen_cpp = {}, {}
        for stem, e in self.entries:
            if "c" in e:
                if e["c"] in seen_c:
                    raise SpecError(f"{e['c']} is defined in both {seen_c[e['c']]} and {stem}")
                seen_c[e["c"]] = stem
            # One declaration may back more than one export (UnitType::requiredUnits is an
            # accessor and a table), but a declaration skipped in one place cannot be exported in
            # another.
            if "cpp" in e:
                kind = "skip" if "skip" in e else "export"
                prior = seen_cpp.setdefault(e["cpp"], kind)
                if prior != kind:
                    raise SpecError(f"cpp {e['cpp']!r} is both skipped and exported")
        names = [f["family"] for f in self.families]
        if len(names) != len(set(names)):
            raise SpecError("a constant family is listed twice")
        macros = {}
        for f in self.families:
            for v in f["values"]:
                if not exportable_enumerator(v["name"]):
                    continue
                macro = constant_name(f["prefix"], v["name"], f.get("strip", ""))
                if macro in macros:
                    raise SpecError(f"{macro} is produced by both {macros[macro]} and {f['cpp']}::{v['name']}")
                macros[macro] = f"{f['cpp']}::{v['name']}"

    def exports(self):
        """The (file_stem, entry) pairs that export something, in spec order."""
        return [(stem, e) for stem, e in self.entries if "c" in e]

    def skips(self):
        return [(stem, e) for stem, e in self.entries if "skip" in e]


# ---- C spellings, signatures and neutral values (spec-format.md sections 1.3 to 1.5) --------------

C_PARAM_TYPES = {
    "int32": "int32_t", "bool32": "int32_t", "double": "double", "string_in": "const char*",
    "void_ptr": "void*", "int32_out": "int32_t*", "double_out": "double*", "position_out": "bwapi_position*",
    "int32_array_out": "int32_t*", "int16_array_out": "int16_t*", "uint8_array_out": "uint8_t*",
    "position_array_out": "bwapi_position*",
}


def c_field_type(t):
    """A struct field's C spelling; a trailing [N] is a fixed array."""
    base, _, count = t.partition("[")
    scalar = {"int32": "int32_t", "bool32": "int32_t", "double": "double", "int16": "int16_t",
              "uint8": "uint8_t", "uint32": "uint32_t"}.get(base, "int32_t" if base.startswith("type:") else None)
    if scalar is None:
        raise SpecError(f"unknown field type {t!r}")
    return scalar, ("[" + count) if count else ""


def field_conversion(kind, expr):
    """The C++ value of an accessor to a table field of the field's type (section 1.4's rules)."""
    base = kind.partition(":")[0]
    if base in ("int32", "int16", "uint8", "uint32"):
        return f"static_cast<{c_field_type(kind)[0]}>({expr})"
    if base == "bool32":
        return f"({expr}) ? 1 : 0"
    if base == "double":
        return f"static_cast<double>({expr})"
    if base == "type":
        return f"({expr}).getID()"
    raise SpecError(f"no conversion for field type {kind}")


def table_entry(struct):
    """The function entry a table: struct declares (spec-format.md section 3): one row per id of
    the class, 0 to Unknown inclusive, each field filled from the accessor its from: names,
    through the stride rule of section 4 (the caller's size on element zero)."""
    t = struct["table"]
    cls = t["class"]
    lines = [f"const int32_t total = BWAPI::{cls}(-1).getID() + 1;",
             f"return write_rows(out, cap, total, [](bwapi_{struct['name']}& row, int32_t id) {{",
             f"  const BWAPI::{cls} t(id);", "  row.id = id;"]
    for f in struct["fields"][1:]:
        if "from" not in f:
            raise SpecError(f"struct {struct['name']}: table field {f['name']} needs from:")
        _, method, _ = split_cpp(f["from"])
        lines.append(f"  row.{f['name']} = {field_conversion(f['type'], f't.{method}()')};")
    lines.append("});")
    return {"c": t["c"], "self": "none", "returns": f"struct_array:{struct['name']}", "doc": t["doc"],
            "body": "\n".join(lines)}


def c_param_type(t):
    if t in C_PARAM_TYPES:
        return C_PARAM_TYPES[t]
    kind, _, arg = t.partition(":")
    return {
        "type": "int32_t",
        "handle": f"bwapi_{arg}_id",
        "struct_in": f"const bwapi_{arg}*",
        "struct_out": f"bwapi_{arg}*",
        "struct_array_out": f"bwapi_{arg}*",
        "callback": arg,
    }[kind]


def c_param_name(param):
    """A handle: parameter named x is x_id at the C boundary; the resolved pointer is x in a
    body (spec-format.md section 1.3)."""
    return param["name"] + "_id" if param["type"].startswith("handle:") else param["name"]


def c_return_type(kind):
    base, _, arg = kind.partition(":")
    if base in ("int32", "bool32", "string_out", "id_array", "position_array", "struct_array"):
        return "int32_t"
    if base == "double":
        return "double"
    if base in ("position", "tile_position", "walk_position"):
        return "bwapi_position"
    if base == "type":
        return "int32_t"
    if base == "handle":
        return f"bwapi_{arg}_id"
    if base == "void":
        return "void"
    raise SpecError(f"unknown return kind {kind!r}")


def signature(entry):
    """[(c_type, name)] in the order section 1.5 builds them: the handle, params, then the
    buffer pair the return kind implies."""
    sig = []
    if entry["self"] in HANDLE_KINDS:
        sig.append((f"bwapi_{entry['self']}_id", f"{entry['self']}_id"))
    for p in entry.get("params") or []:
        sig.append((c_param_type(p["type"]), c_param_name(p)))
    r = entry["returns"]
    if r == "string_out":
        sig += [("char*", "buf"), ("int32_t", "buf_len")]
    elif r == "id_array":
        sig += [("int32_t*", "out"), ("int32_t", "cap")]
    elif r == "position_array":
        sig += [("bwapi_position*", "out"), ("int32_t", "cap")]
    elif r.startswith("struct_array:"):
        sig += [(f"bwapi_{r.partition(':')[2]}*", "out"), ("int32_t", "cap")]
    return sig


def c_signature(entry):
    params = ", ".join(f"{t} {n}" for t, n in signature(entry)) or "void"
    return f"{c_return_type(entry['returns'])} {entry['c']}({params})"


def neutral_c(kind):
    """The neutral value as a C expression (section 1.4), for the source and the docs."""
    base, _, arg = kind.partition(":")
    return {
        "int32": "0", "bool32": "0", "double": "0.0", "string_out": "0", "id_array": "0",
        "position_array": "0", "struct_array": "0", "void": "",
        "position": "BWAPI_POSITION_NONE", "tile_position": "BWAPI_POSITION_NONE",
        "walk_position": "BWAPI_POSITION_NONE", "handle": "BWAPI_NONE",
        "type": f"BWAPI::{arg}(-1).getID()",
    }[base]


def neutral_doc(kind):
    """The neutral value as the reference states it."""
    base, _, arg = kind.partition(":")
    if base == "type":
        if arg == "Color":
            return "255, the Unknown id of Color, which has no named Unknown"
        return f"the Unknown id of {arg} (BWAPI_{family_prefix_of(arg)}_UNKNOWN)"
    return {
        "int32": "0", "bool32": "0", "double": "0.0",
        "string_out": "an empty string, and 0",
        "id_array": "nothing written, and 0", "position_array": "nothing written, and 0",
        "struct_array": "nothing written, and 0", "void": "nothing",
        "position": "BWAPI_POSITION_NONE", "tile_position": "BWAPI_POSITION_NONE",
        "walk_position": "BWAPI_POSITION_NONE", "handle": "BWAPI_NONE",
    }[base]


# The constant-family prefix each type class's ids come from, for docs and the raw layers.
TYPE_CLASS_PREFIX = {
    "UnitType": "UNIT", "WeaponType": "WEAPON", "TechType": "TECH", "UpgradeType": "UPGRADE",
    "Race": "RACE", "UnitSizeType": "UNITSIZE", "DamageType": "DAMAGE", "ExplosionType": "EXPLOSION",
    "BulletType": "BULLET", "Order": "ORDER", "PlayerType": "PLAYERTYPE", "GameType": "GAMETYPE",
    "UnitCommandType": "UNITCOMMAND", "Error": "ERROR", "Color": "COLOR",
}


# The constant family each type class's ids are listed under, for the reference's links.
TYPE_CLASS_FAMILY = {
    "UnitType": "unit_type", "WeaponType": "weapon_type", "TechType": "tech_type", "UpgradeType": "upgrade_type",
    "Race": "race", "UnitSizeType": "unit_size_type", "DamageType": "damage_type", "ExplosionType": "explosion_type",
    "BulletType": "bullet_type", "Order": "order", "PlayerType": "player_type", "GameType": "game_type",
    "UnitCommandType": "unit_command_type", "Error": "error_code", "Color": "color",
}


def family_prefix_of(type_class):
    if type_class not in TYPE_CLASS_PREFIX:
        raise SpecError(f"{type_class!r} is not a type class")
    return TYPE_CLASS_PREFIX[type_class]


# ---- the ABI's own constants ----------------------------------------------------------------------------

# Not from any enum: the error codes, the log levels and the position sentinels are the ABI's
# own. Stated here once; emit_header.py writes them into bwapi_c2_types.h and emit_json.py into
# api.json, so a code has one definition and the raw layers see it.
ABI_CONSTANTS = [
    {"family": "error", "prefix": "BWAPI_ERR", "doc": "The ABI's own error codes, latched by the sticky "
     "first-error channel. A code says which section-4 rule fired. None of these values is ever reused.",
     "values": [
         ("NONE", 0, "nothing latched since the last clear"),
         ("ALREADY_CONNECTED", 1, "bwapi_client_connect() while connected"),
         ("WRONG_THREAD", 2, "a call from a thread other than the update thread"),
         ("REENTRANT_MUTATION", 3, "a mutating call from inside a callback"),
         ("BWEM", 4, "a BWEM::Exception, or a map BWEM would crash on"),
         ("INVALID_HANDLE", 5, "an id that could never have been valid"),
         ("NOT_CONNECTED", 6, "a game or unit call before connect, or after disconnect"),
         ("BWEM_NOT_INITIALIZED", 7, "a bwapi_bwem_* query before bwapi_bwem_initialize()"),
         ("BAD_BUFFER", 8, "a NULL buffer with a nonzero cap or buf_len, or a negative one"),
         ("EXCEPTION", 9, "a C++ exception other than BWEM's escaped the wrapped call; what() is the message"),
     ]},
    {"family": "log_level", "prefix": "BWAPI_LOG", "doc": "The level argument of the log callback. "
     "Client::connect()'s own std::cout and std::cerr output is not redirected in v1; these carry the "
     "ABI's diagnostics.",
     "values": [("INFO", 0, ""), ("WARN", 1, "a rejected re-entrant call, among others"), ("ERROR", 2, "")]},
    {"family": "position_sentinel", "prefix": "BWAPI", "doc": "BWAPI's own position sentinels "
     "(BWAPI/Position.h) in unpacked form, for the pixel, walk (scale 8) and tile (scale 32) scales. "
     "The packed forms are BWAPI_POS_MAKE of each pair; packing is lossless, so a C++ bot sees the same "
     "values. BWAPI_POSITION_NONE packed is the neutral return of every position-returning function.",
     "values": [
         ("POSITION_INVALID_X", 32000, ""), ("POSITION_INVALID_Y", 32000, ""),
         ("POSITION_NONE_X", 32000, ""), ("POSITION_NONE_Y", 32032, ""),
         ("POSITION_UNKNOWN_X", 32000, ""), ("POSITION_UNKNOWN_Y", 32064, ""),
         ("POSITION_ORIGIN_X", 0, ""), ("POSITION_ORIGIN_Y", 0, ""),
         ("WALKPOSITION_INVALID_X", 4000, ""), ("WALKPOSITION_INVALID_Y", 4000, ""),
         ("WALKPOSITION_NONE_X", 4000, ""), ("WALKPOSITION_NONE_Y", 4004, ""),
         ("WALKPOSITION_UNKNOWN_X", 4000, ""), ("WALKPOSITION_UNKNOWN_Y", 4008, ""),
         ("WALKPOSITION_ORIGIN_X", 0, ""), ("WALKPOSITION_ORIGIN_Y", 0, ""),
         ("TILEPOSITION_INVALID_X", 1000, ""), ("TILEPOSITION_INVALID_Y", 1000, ""),
         ("TILEPOSITION_NONE_X", 1000, ""), ("TILEPOSITION_NONE_Y", 1001, ""),
         ("TILEPOSITION_UNKNOWN_X", 1000, ""), ("TILEPOSITION_UNKNOWN_Y", 1002, ""),
         ("TILEPOSITION_ORIGIN_X", 0, ""), ("TILEPOSITION_ORIGIN_Y", 0, ""),
     ]},
]


def abi_version():
    """project(bwapi_c2 VERSION x.y.z) from CMakeLists.txt: the one place the version lives."""
    root = os.path.join(HERE, "..", "..", "CMakeLists.txt")
    with open(root, encoding="utf-8") as f:
        m = re.search(r"^project\(bwapi_c2\s+VERSION\s+(\d+\.\d+\.\d+)", f.read(), re.M)
    if not m:
        raise SpecError("project(bwapi_c2 VERSION ...) not found in CMakeLists.txt")
    return m.group(1)


def first_sentence(doc):
    """The header's one line of hover text: the doc up to its first sentence end."""
    text = " ".join(doc.split())
    m = re.search(r"^(.*?[.!?])(\s|$)", text)
    return m.group(1) if m else text
