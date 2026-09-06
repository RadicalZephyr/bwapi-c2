#!/usr/bin/env python3
"""clang's JSON AST dump to first-draft YAML (plan section 9; implementation plan 1.3).

Two modes.

Methods: one YAML entry per public method of a class, with cpp:, a snake_cased c: under the
section-4 naming rule, a guessed self: and a guessed returns:, and params: guessed from the
parameter types. Everything the guess could not decide is left as a "??" for the human, who
copies entries into tools/abi/spec/<file>.yaml and edits. Drafts land in tools/abi/draft/,
which is gitignored: a draft is never the source of truth.

    tools/abi/draft_spec.py <header> <class>            # e.g. BWAPI/Player.h PlayerInterface
    tools/abi/draft_spec.py --header bwem.h BWEM::Area

Enums: the enumerators of one enumeration (or the constexpr Color variables of BWAPI::Colors,
the one family that is not an enum) as the values: list of a constants.yaml family, so the
names and values come from the AST and are never typed.

    tools/abi/draft_spec.py --enum BWAPI::UnitTypes::Enum::Enum [--enum BWAPI::Key ...]
    tools/abi/draft_spec.py --update-constants             # rewrite every family's values in place

The header is resolved against the include roots in clang_flags.py; a bare name like
BWAPI/Player.h or bwem.h is enough.
"""
import argparse
import json
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import abispec  # noqa: E402
import clang_flags  # noqa: E402

DRAFT_DIR = os.path.join(clang_flags.HERE, "draft")
CONSTANTS_YAML = os.path.join(abispec.SPEC_DIR, "constants.yaml")

# ---- the dump ---------------------------------------------------------------------------------------


def resolve_header(name):
    if os.path.isabs(name) and os.path.exists(name):
        return name
    for root in (clang_flags.BWAPI_INCLUDE, clang_flags.BWEM_INCLUDE, os.getcwd()):
        candidate = os.path.join(root, name)
        if os.path.exists(candidate):
            return candidate
    sys.exit(f"draft_spec: header {name!r} not found under the include roots")


def ast_dump(header, filter_name):
    """The top-level JSON objects clang prints for every declaration whose name matches the
    filter. With a filter the output is several concatenated objects, not one document."""
    cmd = [clang_flags.clang_binary(), *clang_flags.base_flags(), "-fsyntax-only",
           "-Xclang", "-ast-dump=json", "-Xclang", f"-ast-dump-filter={filter_name}", header]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        sys.exit(f"draft_spec: clang failed:\n{proc.stderr}")
    raw = proc.stdout
    decoder = json.JSONDecoder()
    objs, i = [], 0
    while i < len(raw):
        while i < len(raw) and raw[i].isspace():
            i += 1
        if i >= len(raw):
            break
        obj, i = decoder.raw_decode(raw, i)
        objs.append(obj)
    return objs


def walk(node):
    yield node
    for child in node.get("inner", []) or []:
        yield from walk(child)


# ---- methods ------------------------------------------------------------------------------------------

# The interfaces and their handle kinds; the type classes are self: none; BWEM's classes are the
# bwem_ kinds. Anything else is left for the human.
SELF_BY_CLASS = {
    "PlayerInterface": "player", "UnitInterface": "unit", "ForceInterface": "force",
    "RegionInterface": "region", "GameInterface": "game", "Game": "game",
    "BWEM::Area": "bwem_area", "BWEM::ChokePoint": "bwem_choke", "BWEM::Base": "bwem_base",
    "BWEM::Neutral": "bwem_neutral", "BWEM::Mineral": "bwem_neutral", "BWEM::Geyser": "bwem_neutral",
    "BWEM::StaticBuilding": "bwem_neutral", "BWEM::Map": "bwem_map",
}
TYPE_CLASSES = ("UnitType", "WeaponType", "TechType", "UpgradeType", "Race", "UnitSizeType", "DamageType",
                "ExplosionType", "BulletType", "Order", "PlayerType", "GameType", "UnitCommandType",
                "Error", "Color")
SUBJECT_BY_CLASS = {"GameInterface": "game", "Game": "game"}
HANDLE_BY_TYPE = {"Player": "player", "Unit": "unit", "Force": "force", "Region": "region",
                  "const Area *": "bwem_area", "const ChokePoint *": "bwem_choke", "const Base *": "bwem_base",
                  "const Neutral *": "bwem_neutral", "Mineral *": "bwem_neutral", "Geyser *": "bwem_neutral",
                  "StaticBuilding *": "bwem_neutral"}
ID_ARRAY_TYPES = ("Unitset", "Playerset", "Forceset", "Regionset", "Bulletset",
                  "std::vector<Area>", "std::vector<ChokePoint>", "std::vector<Base>",
                  "std::vector<Mineral *>", "std::vector<Geyser *>", "std::vector<StaticBuilding *>",
                  "std::vector<Neutral *>", "std::vector<const Area *>", "std::vector<const ChokePoint *>")


def subject(qualified_class):
    if qualified_class in SUBJECT_BY_CLASS:
        return SUBJECT_BY_CLASS[qualified_class]
    name = qualified_class.split("::")[-1]
    if name.endswith("Interface"):
        name = name[: -len("Interface")]
    if qualified_class.startswith("BWEM::"):
        return "bwem_" + SELF_BY_CLASS.get(qualified_class, name.lower()).replace("bwem_", "")
    return name.lower()


def guess_return(type_spelling):
    t = abispec.normalize_type(type_spelling)
    if t == "void":
        return "void"
    if t in ("int", "unsigned int", "short", "unsigned short", "char", "unsigned char", "long", "int16_t",
             "altitude_t", "Area::id", "id", "groupId"):
        return "int32"
    if t == "bool":
        return "bool32"
    if t == "double":
        return "double"
    if t == "std::string":
        return "string_out"
    if t in TYPE_CLASSES:
        return f"type:{t}"
    if t == "Position":
        return "position"
    if t == "TilePosition":
        return "tile_position"
    if t == "WalkPosition":
        return "walk_position"
    if t in HANDLE_BY_TYPE:
        return f"handle:{HANDLE_BY_TYPE[t]}"
    if t in ID_ARRAY_TYPES:
        return "id_array"
    return f"?? {t}"


def guess_params(params):
    out = []
    for name, type_spelling in params:
        t = abispec.normalize_type(type_spelling)
        pname = abispec.snake_case(name) if name else "??"
        if t in ("int", "unsigned int", "short", "char", "long"):
            out.append({"name": pname, "type": "int32"})
        elif t == "bool":
            out.append({"name": pname, "type": "bool32"})
        elif t == "double":
            out.append({"name": pname, "type": "double"})
        elif t in TYPE_CLASSES:
            out.append({"name": pname, "type": f"type:{t}"})
        elif t in ("Position", "TilePosition", "WalkPosition"):
            out.append({"name": "x", "type": "int32"})
            out.append({"name": "y", "type": "int32"})
        elif t in HANDLE_BY_TYPE:
            out.append({"name": pname, "type": f"handle:{HANDLE_BY_TYPE[t]}"})
        elif t in ("std::string", "char *"):
            out.append({"name": pname, "type": "string_in"})
        else:
            out.append({"name": pname, "type": f"?? {t}"})
    return out


def find_class(objs, qualified_class):
    """The complete CXXRecordDecl for the class (the dump can also hold forward declarations,
    which have no inner)."""
    short = qualified_class.split("::")[-1]
    best = None
    for obj in objs:
        for node in walk(obj):
            if node.get("kind") == "CXXRecordDecl" and node.get("name") == short and node.get("inner"):
                if node.get("completeDefinition"):
                    best = node
    if best is None:
        sys.exit(f"draft_spec: no complete definition of {qualified_class} in the dump")
    return best


def methods_of(class_node):
    """(name, return type, [(param name, param type)], is_static) for every public method that a
    C caller could want: no constructors, destructors, operators or copy machinery."""
    access = "public" if class_node.get("tagUsed") == "struct" else "private"
    for node in class_node.get("inner", []) or []:
        kind = node.get("kind")
        if kind == "AccessSpecDecl":
            access = node["access"]
            continue
        if kind != "CXXMethodDecl" or access != "public":
            continue
        name = node.get("name", "")
        if node.get("isImplicit") or name.startswith("operator") or name.startswith("~"):
            continue
        qual = node["type"]["qualType"]
        m = re.match(r"^(.*?)\s*\((.*)\)\s*(const)?\s*(noexcept)?$", qual)
        if not m:
            continue
        ret = m.group(1).strip()
        params = [(p.get("name", ""), p["type"]["qualType"]) for p in node.get("inner", []) or []
                  if p.get("kind") == "ParmVarDecl"]
        yield name, ret, params, node.get("storageClass") == "static"


def draft_methods(header, qualified_class):
    objs = ast_dump(header, qualified_class.split("::")[-1])
    cls = find_class(objs, qualified_class)
    found = list(methods_of(cls))
    counts = {}
    for name, *_ in found:
        counts[name] = counts.get(name, 0) + 1
    subj = subject(qualified_class)
    self_kind = SELF_BY_CLASS.get(qualified_class, "none" if qualified_class in TYPE_CLASSES else "??")
    lines = [f"# Draft of {qualified_class} from {os.path.relpath(header, clang_flags.REPO)}: {len(found)} methods.",
             "# Generated by tools/abi/draft_spec.py; never the source of truth. Copy entries into",
             "# tools/abi/spec/ and edit. Every ?? is a decision the human makes.", ""]
    for name, ret, params, is_static in found:
        cpp = abispec.cpp_key(qualified_class, name, [t for _, t in params], counts[name] > 1)
        cname = f"bwapi_{subj}_{abispec.snake_case(name)}"
        lines.append(f"- cpp: {yaml_str(cpp)}")
        lines.append(f"  c:   {yaml_str(cname)}")
        lines.append(f"  self: {self_kind}")
        if self_kind not in ("none", "??") and is_static:
            lines.append("  # static: needs self: game or bwem_map, and a body")
        ps = guess_params(params)
        if ps:
            lines.append("  params: [" + ", ".join(
                f"{{name: {p['name']}, type: {yaml_str(p['type']) if '??' in p['type'] else p['type']}}}" for p in ps) + "]")
        r = guess_return(ret)
        lines.append(f"  returns: {yaml_str(r) if '??' in r else r}")
        lines.append('  doc: "??"')
        lines.append("")
    return "\n".join(lines)


def yaml_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


# ---- enums -----------------------------------------------------------------------------------------------


def enumerators(objs, qualified):
    """(name, value) for BWAPI::X::Enum, BWAPI::Key (a plain enum) or BWAPI::Colors (constexpr
    Color variables; the value is the constructor's integer argument)."""
    parts = qualified.split("::")
    want_enum = parts[-1] != "Colors"
    if want_enum:
        target_kind, target_name = "EnumDecl", parts[-1]
    else:
        target_kind, target_name = "NamespaceDecl", "Colors"
    # Find the node by walking the qualification path. A filtered dump starts at the matching
    # declaration, not at the translation unit, so the leading parts (BWAPI, and any enclosing
    # namespace that is not the filter) are absent and the path may start at any suffix.
    matches = []
    for obj in objs:
        for start in range(len(parts)):
            for path in paths_to(obj, parts[start:], 0):
                matches.append(path[-1])
    matches = [m for m in matches if m.get("kind") == target_kind and m.get("name") == target_name]
    if not matches:
        sys.exit(f"draft_spec: {qualified} not found in the dump")
    values = []
    if want_enum:
        # The dump may hold the enum once per translation unit position; the definition has inner.
        node = next((m for m in matches if m.get("inner")), None)
        if node is None:
            sys.exit(f"draft_spec: {qualified} has no enumerators in the dump")
        counter = 0
        for child in node.get("inner", []) or []:
            if child.get("kind") != "EnumConstantDecl":
                continue
            explicit = None
            for sub in walk(child):
                if sub is child:
                    continue
                if sub.get("kind") == "ConstantExpr" and "value" in sub:
                    explicit = int(sub["value"])
                    break
            if explicit is not None:
                counter = explicit
            values.append((child["name"], counter))
            counter += 1
    else:
        for node in matches:
            for child in node.get("inner", []) or []:
                if child.get("kind") != "VarDecl" or child["type"]["qualType"] not in ("const Color", "const BWAPI::Color"):
                    continue
                literal = None
                for sub in walk(child):
                    if sub.get("kind") == "IntegerLiteral":
                        literal = int(sub["value"])
                        break
                if literal is None:
                    sys.exit(f"draft_spec: {qualified}::{child['name']} has no integer initialiser")
                values.append((child["name"], literal))
    if not values:
        sys.exit(f"draft_spec: {qualified} yielded no values")
    return values


def paths_to(node, parts, depth):
    """Every path of nested declarations whose names spell `parts` from this node down."""
    if node.get("name") != parts[depth]:
        return
    if depth == len(parts) - 1:
        yield [node]
        return
    for child in node.get("inner", []) or []:
        for tail in paths_to(child, parts, depth + 1):
            yield [node] + tail


def enum_values(qualified, header=None):
    header = resolve_header(header or ("BWAPI.h" if qualified.startswith("BWAPI::") else "bwem.h"))
    # BWAPI spells its type enumerations `namespace Enum { enum Enum {...} }`, so the qualified
    # name is UnitTypes::Enum::Enum; the filter is the last part that is not Enum.
    filter_name = [p for p in qualified.split("::") if p != "Enum"][-1]
    objs = ast_dump(header, filter_name)
    return enumerators(objs, qualified)


def format_values(values):
    width = max(len(n) for n, _ in values)
    return "\n".join(f"    - {{name: {n + ',':<{width + 1}} value: {v}}}" for n, v in values)


def update_constants(path):
    """Rewrite every family's values: list in constants.yaml from the AST, keeping the file's
    other text as it is. The values block of each family is replaced wholesale."""
    text = open(path, encoding="utf-8").read()
    families = re.finditer(r"^- family: (\S+)\n(?:(?!^- ).*\n)*?  cpp: \"([^\"]+)\"\n", text, re.M)
    cpps = {m.group(1): m.group(2) for m in families}
    for family, cpp in cpps.items():
        values = enum_values(cpp)
        block = f"  values:\n{format_values(values)}\n"
        pattern = re.compile(rf"(^- family: {re.escape(family)}\n(?:(?!^- ).*\n)*?)  values:\n(?:    .*\n)*", re.M)
        if not pattern.search(text):
            sys.exit(f"draft_spec: family {family} has no values: block to replace")
        text = pattern.sub(lambda m: m.group(1) + block, text, count=1)
        print(f"{family}: {len(values)} values from {cpp}")
    open(path, "w", encoding="utf-8").write(text)


# ---- main ------------------------------------------------------------------------------------------------


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("header", nargs="?", help="header under an include root, e.g. BWAPI/Player.h")
    ap.add_argument("cls", nargs="?", help="class to draft, e.g. PlayerInterface or BWEM::Area")
    ap.add_argument("--header", dest="header_opt", help="header for --enum (default BWAPI.h / bwem.h)")
    ap.add_argument("--enum", action="append", default=[], help="print a family's values: from the AST")
    ap.add_argument("--update-constants", action="store_true", help=f"rewrite values in {os.path.relpath(CONSTANTS_YAML)}")
    ap.add_argument("--out", help="draft output path (default tools/abi/draft/<class>.yaml)")
    args = ap.parse_args()

    if args.update_constants:
        update_constants(CONSTANTS_YAML)
        return
    if args.enum:
        for qualified in args.enum:
            values = enum_values(qualified, args.header_opt)
            print(f"# {qualified}: {len(values)} enumerators")
            print("  values:")
            print(format_values(values))
        return
    if not args.header or not args.cls:
        ap.error("a header and a class, or --enum, or --update-constants")
    header = resolve_header(args.header)
    text = draft_methods(header, args.cls)
    out = args.out or os.path.join(DRAFT_DIR, args.cls.replace("::", "_") + ".yaml")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w", encoding="utf-8") as f:
        f.write(text)
    print(f"wrote {os.path.relpath(out)}")


if __name__ == "__main__":
    main()
