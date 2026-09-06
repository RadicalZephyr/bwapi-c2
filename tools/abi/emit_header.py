#!/usr/bin/env python3
"""The header emitter (plan section 9; implementation plan 1.4 and 1.5): tools/abi/spec/ and
tools/abi/templates/*.h.in to include/bwapi_c2.h and include/bwapi_c2_types.h.

The templates carry the prose and the hand-written macros; this fills in the ABI's own
constants, the generated constant families, and one declaration per export with the first
sentence of its doc, grouped by spec file. bwapi_c2_bwem.h is emitted from spec/bwem*.yaml
from phase 3 on; until then the phase-0 skeleton stands and this script leaves it alone.

    tools/abi/emit_header.py            # writes both headers
    tools/abi/emit_header.py --check    # exits 1 if either would change
"""
import os
import re
import sys
import textwrap

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import abispec  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
INCLUDE = os.path.join(REPO, "include")
TEMPLATES = os.path.join(HERE, "templates")

# Which header a spec file's declarations land in, and the order and title of the sections.
# A spec file not listed here goes to bwapi_c2.h after these, alphabetically, under its stem.
SECTIONS = [
    ("client", "bwapi_c2.h", "the ABI itself and the client: versions, the error channel, callbacks, lifecycle"),
    ("game", "bwapi_c2.h", "Game"),
    ("player", "bwapi_c2.h", "Player"),
    ("unit", "bwapi_c2.h", "Unit"),
    ("force_region", "bwapi_c2.h", "Force and Region"),
    ("types", "bwapi_c2_types.h", "static type data"),
    ("bulk", "bwapi_c2_types.h", "bulk type tables"),
]


def header_of(stem):
    for s, header, _ in SECTIONS:
        if s == stem:
            return header
    if stem.startswith("bwem"):
        return "bwapi_c2_bwem.h"
    return "bwapi_c2.h"


def title_of(stem):
    for s, _, title in SECTIONS:
        if s == stem:
            return title
    return stem


def section_order(stems):
    known = [s for s, _, _ in SECTIONS if s in stems]
    return known + sorted(s for s in stems if s not in known)


def comment(text, width=97):
    """A C comment, wrapped, that never contains a comment terminator."""
    text = text.replace("*/", "* /")
    lines = textwrap.wrap(" ".join(text.split()), width=width - 3)
    if len(lines) == 1:
        return f"/* {lines[0]} */"
    return "/* " + "\n * ".join(lines) + " */"


def declaration(entry):
    params = ", ".join(f"{t} {n}" for t, n in abispec.signature(entry)) or "void"
    ret = abispec.c_return_type(entry["returns"])
    return f"BWAPI_C2_API {ret} BWAPI_C2_CALL {entry['c']}({params}) BWAPI_C2_NOEXCEPT;"


def declarations_for(spec, header):
    by_stem = {}
    for stem, e in spec.exports():
        if header_of(stem) == header:
            by_stem.setdefault(stem, []).append(e)
    out = []
    if not by_stem:
        return "/* (nothing yet: the spec files for this header arrive with implementation plan 1.5) */"
    for stem in section_order(by_stem):
        title = title_of(stem)
        out.append(f"/* ---- {title} " + "-" * max(1, 92 - len(title)) + " */")
        out.append("")
        for e in by_stem[stem]:
            out.append(comment(abispec.first_sentence(e["doc"])))
            out.append(declaration(e))
        out.append("")
    return "\n".join(out).rstrip("\n")


def defines(rows):
    """Aligned #define lines. rows: (macro, value, comment). Numeric values are right-aligned in
    a column; macro-expression values are not, because there is no digit place to line up."""
    width = max(len(m) for m, _, _ in rows)
    numeric = all(isinstance(v, int) for _, v, _ in rows)
    vwidth = max(len(str(v)) for _, v, _ in rows) if numeric else 0
    lines = []
    for macro, value, note in rows:
        line = f"#define {macro:<{width}} {str(value):>{vwidth}}"
        if note:
            line += f" /* {note} */"
        lines.append(line.rstrip())
    return "\n".join(lines)


def abi_constant_block(family):
    rows = [(f"{family['prefix']}_{name}", value, note) for name, value, note in family["values"]]
    return defines(rows)


def position_sentinels():
    fam = next(f for f in abispec.ABI_CONSTANTS if f["family"] == "position_sentinel")
    rows = [(f"BWAPI_{name}", value, "") for name, value, _ in fam["values"]]
    packed = [(f"BWAPI_{scale}_{which}", f"BWAPI_POS_MAKE(BWAPI_{scale}_{which}_X, BWAPI_{scale}_{which}_Y)", "")
              for scale in ("POSITION", "WALKPOSITION", "TILEPOSITION")
              for which in ("INVALID", "NONE", "UNKNOWN", "ORIGIN")]
    return "\n".join([defines(rows), "", defines(packed)])


def constant_families(spec):
    if not spec.families:
        return "/* (no constant families yet: spec/constants.yaml arrives in implementation plan 1.5) */"
    blocks = []
    for fam in spec.families:
        rows = [(abispec.constant_name(fam["prefix"], v["name"], fam.get("strip", "")), v["value"], "")
                for v in fam["values"] if abispec.exportable_enumerator(v["name"])]
        blocks.append(f"/* {fam['family']}: {fam['cpp']} */")
        blocks.append(defines(rows))
        blocks.append("")
    return "\n".join(blocks).rstrip("\n")


def struct_block(spec):
    if not spec.structs:
        return "/* (no structs yet: spec/structs.yaml arrives with the first POD that crosses the boundary) */"
    blocks = []
    for st in spec.structs:
        rows = [("int32_t", "size", "", "the struct-evolution prefix: the bytes the writer of this struct filled")]
        for f in st["fields"]:
            ctype, suffix = abispec.c_field_type(f["type"])
            note = f.get("doc") or (f.get("from", "") and f"{f['from']}()")
            rows.append((ctype, f["name"], suffix, note))
        width = max(len(f"{c} {n}{sfx};") for c, n, sfx, _ in rows)
        blocks.append(comment(abispec.first_sentence(st["doc"]) + (f" Filled by {st['table']['c']}()." if "table" in st else "")))
        blocks.append(f"typedef struct bwapi_{st['name']} {{")
        for ctype, name, suffix, note in rows:
            decl = f"{ctype} {name}{suffix};"
            blocks.append(f"  {decl:<{width}}" + (f" /* {note} */" if note else ""))
        blocks.append(f"}} bwapi_{st['name']};")
        if "flags" in st and st["flags"]:
            for flag in st["flags"]:
                blocks.append(f"#define BWAPI_{st['name'].upper()}_{flag['name'].upper()} (1u << {flag['bit']})")
        blocks.append("")
    return "\n".join(blocks).rstrip("\n")


def render(spec, header):
    with open(os.path.join(TEMPLATES, header + ".in"), encoding="utf-8") as f:
        text = f.read()
    text = text.replace("@DECLARATIONS@", declarations_for(spec, header))
    if header == "bwapi_c2_types.h":
        errors = next(f for f in abispec.ABI_CONSTANTS if f["family"] == "error")
        levels = next(f for f in abispec.ABI_CONSTANTS if f["family"] == "log_level")
        text = text.replace("@ERROR_CODES@", abi_constant_block(errors))
        text = text.replace("@LOG_LEVELS@", abi_constant_block(levels))
        text = text.replace("@POSITION_SENTINELS@", position_sentinels())
        text = text.replace("@CONSTANTS@", constant_families(spec))
        text = text.replace("@STRUCTS@", struct_block(spec))
    leftover = re.findall(r"@[A-Z_]+@", text)
    if leftover:
        raise abispec.SpecError(f"{header}.in: unfilled placeholder(s) {leftover}")
    return text


def outputs(spec):
    return {os.path.join(INCLUDE, h): render(spec, h) for h in ("bwapi_c2_types.h", "bwapi_c2.h")}


def main():
    spec = abispec.Spec()
    changed = []
    for path, text in outputs(spec).items():
        old = open(path, encoding="utf-8").read() if os.path.exists(path) else None
        if old != text:
            changed.append(path)
            if "--check" not in sys.argv:
                with open(path, "w", encoding="utf-8") as f:
                    f.write(text)
    if "--check" in sys.argv:
        for p in changed:
            print(f"emit_header: {os.path.relpath(p, REPO)} is out of date", file=sys.stderr)
        sys.exit(1 if changed else 0)
    for p in changed:
        print(f"wrote {os.path.relpath(p, REPO)}")


if __name__ == "__main__":
    main()
