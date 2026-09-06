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
FAMILY_FIELDS = {"family", "cpp", "prefix", "strip", "values"}
STRUCT_FIELDS = {"name", "doc", "fields", "flags"}


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
        if set(fld) - {"name", "type", "doc"} or "name" not in fld or "type" not in fld:
            raise SpecError(f"{where}: fields[{i}] must be {{name, type, doc?}}")
        if fld["name"] == "size":
            raise SpecError(f"{where}: size is implied, never listed")
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
            if "cpp" in e:
                if e["cpp"] in seen_cpp:
                    raise SpecError(f"cpp {e['cpp']!r} appears twice ({seen_cpp[e['cpp']]} and {stem})")
                seen_cpp[e["cpp"]] = stem
        names = [c for c, _ in ((f["family"], f) for f in self.families)]
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
