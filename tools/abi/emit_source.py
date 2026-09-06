#!/usr/bin/env python3
"""The source emitter (plan section 9; implementation plan 1.4): one src/<stem>.gen.cpp per spec
file, holding one wrapper per export that is not hand-written, plus a compile-time assertion
for every body: and source: entry that its cpp: declaration exists.

The per-function template is the one every wrapper shares (docs/spec-format.md sections 1.4
to 1.6): the noexcept guard, the thread-then-connected prologue, the buffer check, the
resolve-and-guard of every handle, the call (generated from cpp: or the entry's body:), and
the conversion of the C++ value to the return kind. Nothing about the boundary is written by
hand anywhere else.

    tools/abi/emit_source.py            # writes src/*.gen.cpp
    tools/abi/emit_source.py --check    # exits 1 if any would change
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import abispec  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
SRC = os.path.join(REPO, "src")

RESOLVER = {
    "unit": ("BWAPI::Unit", "resolve_unit"), "player": ("BWAPI::Player", "resolve_player"),
    "force": ("BWAPI::Force", "resolve_force"), "region": ("BWAPI::Region", "resolve_region"),
    "bwem_area": ("const BWEM::Area*", "resolve_bwem_area"),
    "bwem_choke": ("const BWEM::ChokePoint*", "resolve_bwem_choke"),
    "bwem_base": ("const BWEM::Base*", "resolve_bwem_base"),
    "bwem_neutral": ("const BWEM::Neutral*", "resolve_bwem_neutral"),
}


def qualify(cls):
    """The spec writes PlayerInterface::minerals and BWEM::Area::Id; the code needs BWAPI:: on
    the former."""
    return cls if cls.startswith("BWEM::") else "BWAPI::" + cls


def cpp_class_and_method(entry):
    cls, method, params = abispec.split_cpp(entry["cpp"])
    return qualify(cls), method, params


def argument(param):
    """How a C parameter is passed to the C++ call."""
    t, name = param["type"], param["name"]
    if t == "bool32":
        return f"{name} != 0"
    if t.startswith("type:"):
        return f"BWAPI::{t.partition(':')[2]}({name})"
    if t.startswith("handle:"):
        return name  # the resolved pointer
    return name


def convert(kind, expr):
    """The C++ value of the return kind to the C return (spec-format.md section 1.4)."""
    base = kind.partition(":")[0]
    if base == "int32":
        return f"return static_cast<int32_t>({expr});"
    if base == "bool32":
        return f"return ({expr}) ? 1 : 0;"
    if base == "double":
        return f"return static_cast<double>({expr});"
    if base == "type":
        return f"return ({expr}).getID();"
    if base in ("position", "tile_position", "walk_position"):
        return f"return pack({expr});"
    if base == "handle":
        return f"return id_of({expr});"
    if base == "string_out":
        return f"return write_string(buf, buf_len, {expr});"
    if base == "id_array":
        return f"return write_ids(out, cap, {expr});"
    if base == "position_array":
        return f"return write_positions(out, cap, {expr});"
    if base == "struct_array":
        return f"return {expr};"
    if base == "void":
        return f"{expr};"
    raise abispec.SpecError(f"no conversion for {kind}")


def prologue(entry, neutral):
    """The lines before the call: thread and connection, buffers, self, the other handles."""
    fn = entry["c"]
    lines = []
    ret_neutral = f"return {neutral};" if neutral else "return;"
    r = entry["returns"]
    if r == "string_out":
        lines.append(f"    if (!check_string_buffer(buf, buf_len)) {ret_neutral}")
        lines.append("    empty_string(buf, buf_len);")
    elif r in ("id_array", "position_array") or r.startswith("struct_array:"):
        lines.append(f"    if (!check_buffer(out, cap)) {ret_neutral}")
    self_kind = entry["self"]
    if self_kind != "none":
        lines.append(f'    if (!game_ready("{fn}")) {ret_neutral}')
    if self_kind in abispec.HANDLE_KINDS:
        ctype, resolver = RESOLVER[self_kind]
        lines.append(f'    {ctype} self = {resolver}({self_kind}_id, "{fn}");')
        lines.append(f"    if (!self) {ret_neutral}")
    elif self_kind == "game":
        lines.append("    BWAPI::Game* self = BWAPI::BroodwarPtr;")
    elif self_kind == "bwem_map":
        lines.append(f'    if (!bwem_ready("{fn}")) {ret_neutral}')
        lines.append("    const BWEM::Map& map = BWEM::Map::Instance();")
    for p in entry.get("params") or []:
        if p["type"].startswith("handle:"):
            kind = p["type"].partition(":")[2]
            ctype, resolver = RESOLVER[kind]
            lines.append(f'    {ctype} {p["name"]} = {resolver}({p["name"]}_id, "{fn}");')
            lines.append(f"    if (!{p['name']}) {ret_neutral}")
    return lines


def generated_call(entry):
    """self->method(args), or Class(id).method(args) for a self: none accessor whose receiver
    is the first type: parameter of the cpp class."""
    cls, method, _ = cpp_class_and_method(entry)
    params = entry.get("params") or []
    if entry["self"] == "none":
        short = cls.split("::")[-1]
        receiver = next((p for p in params if p["type"] == f"type:{short}"), None)
        if receiver is None:
            raise abispec.SpecError(f"{entry['c']}: a self: none entry without a body needs a type:{short} parameter to call {entry['cpp']} on")
        args = [argument(p) for p in params if p is not receiver]
        return f"{argument(receiver)}.{method}({', '.join(args)})"
    if entry["self"] == "bwem_map":
        return f"map.{method}({', '.join(argument(p) for p in params)})"
    return f"self->{method}({', '.join(argument(p) for p in params)})"


def wrapper(entry):
    r = entry["returns"]
    c_ret = abispec.c_return_type(r)
    neutral = abispec.neutral_c(r)
    params = ", ".join(f"{t} {n}" for t, n in abispec.signature(entry)) or "void"
    lines = [f"BWAPI_C2_API {c_ret} BWAPI_C2_CALL {entry['c']}({params}) BWAPI_C2_NOEXCEPT {{"]
    if r == "void":
        lines.append("  guard([&] {")
    else:
        lines.append(f"  return guard<{c_ret}>({neutral}, [&]() -> {c_ret} {{")
    lines += prologue(entry, neutral)
    if "body" in entry:
        body = entry["body"].strip()
        if r == "void":
            lines.append("    [&] {")
            lines += ["      " + l for l in body.splitlines()]
            lines.append("    }();")
        else:
            lines.append("    auto body = [&] {")
            lines += ["      " + l for l in body.splitlines()]
            lines.append("    };")
            lines.append("    " + convert(r, "body()"))
    else:
        lines.append("    " + convert(r, generated_call(entry)))
    lines.append("  });")
    lines.append("}")
    return "\n".join(lines)


def signature_assertion(entry, stem):
    """For a body: or source: entry: the cpp declaration exists, and with the parameter types
    the spec names when the name is overloaded."""
    cls, method, params = cpp_class_and_method(entry)
    where = f"spec/{stem}.yaml {entry['c']}: {entry['cpp']}"
    if params is None:
        return f'static_assert(sizeof(&{cls}::{method}) > 0, "{where} not found");'
    args = ", ".join(f"std::declval<{qualify_type(p)}>()" for p in params)
    expr = f"std::declval<{cls}&>().{method}({args})"
    return (f"static_assert(std::is_same_v<decltype({expr}), decltype({expr})>,\n"
            f'              "{where} not found with that signature");')


def qualify_type(t):
    """A normalised parameter type back to something clang can resolve at namespace scope."""
    if t in ("int", "bool", "double", "char", "unsigned int", "short", "long", "std::string", "char *") or t.startswith("std::"):
        return t
    if t.startswith("BWEM::"):
        return t
    return "BWAPI::" + t


def source_defines(entry):
    """A source: entry must be defined in the file it names; the check is textual and cheap."""
    path = os.path.join(REPO, entry["source"])
    if not os.path.exists(path):
        raise abispec.SpecError(f"{entry['c']}: source {entry['source']} does not exist")
    with open(path, encoding="utf-8") as f:
        if not re.search(rf"\b{re.escape(entry['c'])}\s*\(", f.read()):
            raise abispec.SpecError(f"{entry['c']}: not defined in {entry['source']}")


def render(stem, entries):
    out = [f"// GENERATED by tools/abi/emit_source.py from tools/abi/spec/{stem}.yaml; do not edit.",
           "// The boundary every wrapper shares is abi_internal.h; the conversions are docs/spec-format.md",
           "// section 1.4. A change to the ABI is a change to the spec file.",
           '#include "abi_internal.h"', "", "#include <BWAPI/Client.h>", "#include <bwem.h>", "",
           "#include <type_traits>", "#include <utility>", "", "using namespace bwapi_c2;", ""]
    assertions = []
    wrappers = []
    for e in entries:
        if "source" in e:
            source_defines(e)
        if ("body" in e or "source" in e) and "cpp" in e:
            assertions.append(signature_assertion(e, stem))
        if "source" not in e:
            wrappers.append(wrapper(e))
    if assertions:
        out.append("// Every body: and source: entry names the C++ declaration it stands for; these fail the build")
        out.append("// when a pin bump removes or reshapes one (plan section 9).")
        out += assertions
        out.append("")
    if wrappers:
        out.append('extern "C" {')
        out.append("")
        out.append("\n\n".join(wrappers))
        out.append("")
        out.append('}  // extern "C"')
    else:
        out.append("// Every export in this file is hand-written (source:); nothing to generate.")
    return "\n".join(out) + "\n"


def outputs(spec):
    by_stem = {}
    for stem, e in spec.exports():
        by_stem.setdefault(stem, []).append(e)
    return {os.path.join(SRC, f"{stem}.gen.cpp"): render(stem, entries) for stem, entries in by_stem.items()}


def check_cmake(paths):
    """cmake/ never globs, so every generated file must be named in CMakeLists.txt."""
    with open(os.path.join(REPO, "CMakeLists.txt"), encoding="utf-8") as f:
        text = f.read()
    missing = [os.path.relpath(p, REPO) for p in paths if os.path.relpath(p, REPO) not in text]
    if missing:
        raise abispec.SpecError("add to bwapi_c2_abi in CMakeLists.txt: " + ", ".join(missing))


def main():
    spec = abispec.Spec()
    outs = outputs(spec)
    check_cmake(outs)
    changed = []
    for path, text in outs.items():
        old = open(path, encoding="utf-8").read() if os.path.exists(path) else None
        if old != text:
            changed.append(path)
            if "--check" not in sys.argv:
                with open(path, "w", encoding="utf-8") as f:
                    f.write(text)
    if "--check" in sys.argv:
        for p in changed:
            print(f"emit_source: {os.path.relpath(p, REPO)} is out of date", file=sys.stderr)
        sys.exit(1 if changed else 0)
    for p in changed:
        print(f"wrote {os.path.relpath(p, REPO)}")


if __name__ == "__main__":
    main()
