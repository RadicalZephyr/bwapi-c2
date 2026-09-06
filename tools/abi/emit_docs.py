#!/usr/bin/env python3
"""The reference emitter (plan section 16.1; implementation plan 1.4): api.json to Markdown
pages under site/content/reference/, at build time, never checked in.

It reads api.json and not the spec, so the reference can only ever describe what the raw
bindings see. One page per function under <section>/<c>.md with every api.json field in the
front matter and the doc as the body; one _index.md per section; one table page per constant
family and per struct, with the rows in the front matter. The templates
(site/templates/reference-function.html, reference-table.html) do all the layout; nothing here
formats anything.

    tools/abi/emit_docs.py [--api api.json] [--out site/content/reference]
"""
import argparse
import json
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))

SECTION_TITLES = {
    "client": ("The ABI and the client", "Versions, the sticky error channel, the two callbacks, and connect, update, disconnect."),
    "game": ("Game", "The game: map, frame, players and units, drawing, text, the closest queries."),
    "player": ("Player", "Everything about one player: resources, supply, counts, upgrades, research, scores."),
    "unit": ("Unit", "Everything about one unit: state, rules, commands."),
    "force_region": ("Force and Region", "Teams and BWAPI's own map regions."),
    "types": ("Static type data", "Pure functions of a type id, needing no game: UnitType, WeaponType, TechType and the rest."),
    "bulk": ("Bulk type tables", "One size-prefixed table per type class, for a host that would rather pay one crossing than 185."),
    "constants": ("Constants", "Every constant family: the type enumerations, the events, flags and keys, the error codes, the position sentinels."),
    "structs": ("Structs", "Every POD that crosses the boundary, with its fields, offsets and flag bits."),
}
SECTION_ORDER = ["client", "game", "player", "unit", "force_region", "types", "bulk", "bwem", "constants", "structs"]

# What the reference says about when a function latches, derived from its kinds and never
# repeated in a doc (docs/spec-format.md section 1.7).
LATCH_WRONG_THREAD = "BWAPI_ERR_WRONG_THREAD when called from a thread other than the one that connected"
LATCH_NOT_CONNECTED = "BWAPI_ERR_NOT_CONNECTED when not connected"
LATCH_BWEM = "BWAPI_ERR_BWEM_NOT_INITIALIZED before bwapi_bwem_initialize()"
LATCH_HANDLE = "BWAPI_ERR_INVALID_HANDLE for a handle that could never have been valid: negative, out of range, or of the wrong kind"
LATCH_BUFFER = "BWAPI_ERR_BAD_BUFFER for a NULL buffer with a nonzero length, or a negative length"
LATCH_EXCEPTION = "BWAPI_ERR_BWEM or BWAPI_ERR_EXCEPTION when an exception escapes the wrapped call"


def toml_str(s):
    return json.dumps(s, ensure_ascii=False)


def toml_value(v):
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, (int, float)):
        return str(v)
    if isinstance(v, str):
        return toml_str(v)
    if v is None:
        return '""'
    if isinstance(v, list):
        return "[" + ", ".join(toml_value(x) for x in v) + "]"
    raise TypeError(type(v))


def latches(fn):
    out = []
    # The same rule emit_source.py's prologue applies: a handle: parameter is resolved through
    # the game, so an entry that takes one is connection-checked even with self: none.
    takes_handle = any(p["type"].startswith("handle:") for p in fn["params"])
    if fn["self"] != "none" or takes_handle:
        out.append(LATCH_WRONG_THREAD)
        out.append(LATCH_NOT_CONNECTED)
    if fn["self"].startswith("bwem"):
        out.append(LATCH_BWEM)
    if takes_handle:
        out.append(LATCH_HANDLE)
    if fn["returns"]["kind"] in ("string_out", "id_array", "position_array") or fn["returns"]["kind"].startswith("struct_array:"):
        out.append(LATCH_BUFFER)
    out.append(LATCH_EXCEPTION)
    return out


def function_page(fn, weight, families):
    lines = ["+++", f"title = {toml_str(fn['c'])}", f"description = {toml_str(first_sentence(fn['doc']))}",
             'template = "reference-function.html"', f"weight = {weight}", "", "[extra]"]
    for key in ("c", "section", "signature", "self", "reentrant", "legit_none", "generated"):
        lines.append(f"{key} = {toml_value(fn[key])}")
    lines.append(f"cpp = {toml_value(fn['cpp'])}")
    for key in ("since", "guides", "divergences"):
        if key in fn:
            lines.append(f"{key} = {toml_value(fn[key])}")
    lines.append(f"latches = {toml_value(latches(fn))}")
    lines.append("")
    lines.append("[extra.returns]")
    for k, v in fn["returns"].items():
        lines.append(f"{k} = {toml_value(v)}")
    # The link to the family's table, only once that family is in api.json, so zola check can
    # verify it rather than skip it.
    if fn["returns"].get("family") in families:
        lines.append(f'family_page = "reference/constants/{fn["returns"]["family"]}.md"')
    for p in fn["params"]:
        lines.append("")
        lines.append("[[extra.params]]")
        for k, v in p.items():
            lines.append(f"{k} = {toml_value(v)}")
    lines.append("+++")
    lines.append("")
    lines.append(fn["doc"])
    lines.append("")
    return "\n".join(lines)


def first_sentence(doc):
    import re
    m = re.search(r"^(.*?[.!?])(\s|$)", doc)
    return m.group(1) if m else doc


def section_index(stem, weight):
    title, description = SECTION_TITLES.get(stem, (stem, ""))
    return "\n".join(["+++", f"title = {toml_str(title)}", f"description = {toml_str(description)}",
                      'sort_by = "weight"', f"weight = {weight}", "+++", ""])


def constant_page(fam, weight):
    lines = ["+++", f"title = {toml_str(fam['family'])}", f"description = {toml_str(first_sentence(fam['doc']) if fam['doc'] else fam['prefix'] + '_*')}",
             'template = "reference-table.html"', f"weight = {weight}", "", "[extra]", 'kind = "constants"',
             f"family = {toml_str(fam['family'])}", f"prefix = {toml_str(fam['prefix'])}", f"cpp = {toml_value(fam['cpp'])}",
             f"count = {len(fam['values'])}"]
    for v in fam["values"]:
        lines.append("")
        lines.append("[[extra.rows]]")
        lines.append(f"name = {toml_str(v['name'])}")
        lines.append(f"cpp_name = {toml_value(v['cpp_name'])}")
        lines.append(f"value = {v['value']}")
        lines.append(f"doc = {toml_str(v['doc'])}")
    lines += ["+++", "", fam["doc"] or "", ""]
    return "\n".join(lines)


def struct_page(s, weight):
    lines = ["+++", f"title = {toml_str(s['c_type'])}", f"description = {toml_str(first_sentence(s['doc']))}",
             'template = "reference-table.html"', f"weight = {weight}", "", "[extra]", 'kind = "struct"',
             f"c_type = {toml_str(s['c_type'])}"]
    if "table" in s:
        lines.append(f"table_c = {toml_str(s['table']['c'])}")
        lines.append(f"table_class = {toml_str(s['table']['class'])}")
    for f in s["fields"]:
        lines += ["", "[[extra.rows]]", f"name = {toml_str(f['name'])}", f"type = {toml_str(f['c_type'])}",
                  f"doc = {toml_str(f.get('doc') or f.get('from', ''))}"]
    for f in s["flags"]:
        lines += ["", "[[extra.flags]]", f"name = {toml_str(f['name'])}", f"bit = {f['bit']}",
                  f"doc = {toml_str(f.get('doc', ''))}"]
    lines += ["+++", "", s["doc"], ""]
    return "\n".join(lines)


def emit(api, out_dir):
    # Everything but the checked-in _index.md is regenerated from scratch.
    for entry in os.listdir(out_dir) if os.path.isdir(out_dir) else []:
        path = os.path.join(out_dir, entry)
        if entry == "_index.md":
            continue
        shutil.rmtree(path) if os.path.isdir(path) else os.remove(path)
    os.makedirs(out_dir, exist_ok=True)

    sections = {}
    for fn in api["functions"]:
        sections.setdefault(fn["section"], []).append(fn)
    ordered = [s for s in SECTION_ORDER if s in sections] + sorted(s for s in sections if s not in SECTION_ORDER)
    families = {fam["family"] for fam in api["constants"]}
    count = 0
    for weight, stem in enumerate(ordered, start=1):
        d = os.path.join(out_dir, stem)
        os.makedirs(d)
        write(os.path.join(d, "_index.md"), section_index(stem, weight))
        for i, fn in enumerate(sections[stem], start=1):
            write(os.path.join(d, fn["c"] + ".md"), function_page(fn, i, families))
            count += 1
    weight = len(ordered) + 1
    d = os.path.join(out_dir, "constants")
    os.makedirs(d)
    write(os.path.join(d, "_index.md"), section_index("constants", weight))
    for i, fam in enumerate(api["constants"], start=1):
        write(os.path.join(d, fam["family"] + ".md"), constant_page(fam, i))
    if api["structs"]:
        d = os.path.join(out_dir, "structs")
        os.makedirs(d)
        write(os.path.join(d, "_index.md"), section_index("structs", weight + 1))
        for i, s in enumerate(api["structs"], start=1):
            write(os.path.join(d, s["name"] + ".md"), struct_page(s, i))
    return count, len(api["constants"]), len(api["structs"])


def write(path, text):
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--api", default=os.path.join(REPO, "api.json"))
    ap.add_argument("--out", default=os.path.join(REPO, "site", "content", "reference"))
    args = ap.parse_args()
    with open(args.api, encoding="utf-8") as f:
        api = json.load(f)
    if api.get("api_json_version") != 1:
        sys.exit(f"emit_docs: api_json_version {api.get('api_json_version')} is not the 1 this emitter reads")
    functions, families, structs = emit(api, args.out)
    print(f"emit_docs: {functions} function pages, {families} constant tables, {structs} struct tables under {os.path.relpath(args.out, REPO)}")


if __name__ == "__main__":
    main()
