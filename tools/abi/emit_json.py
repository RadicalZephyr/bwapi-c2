#!/usr/bin/env python3
"""The api.json emitter (plan sections 7, 9 and 16; implementation plan 1.4): the machine-readable
description of the ABI that the Python, C# and Rust raw layers and the reference pages consume,
so no binding author re-parses C. Every field is documented in docs/api-json.md; a field that
is not documented there is not written here.

    tools/abi/emit_json.py            # writes api.json
    tools/abi/emit_json.py --check    # exits 1 if it would change
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import abispec  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
API_JSON = os.path.join(REPO, "api.json")

API_JSON_VERSION = 1


def param_json(p):
    return {"name": abispec.c_param_name(p), "type": p["type"], "c_type": abispec.c_param_type(p["type"])}


def function_json(stem, e):
    sig = abispec.signature(e)
    params = []
    if e["self"] in abispec.HANDLE_KINDS:
        params.append({"name": f"{e['self']}_id", "type": f"handle:{e['self']}", "c_type": f"bwapi_{e['self']}_id"})
    params += [param_json(p) for p in e.get("params") or []]
    # The buffer pair the return kind implies, as the raw layers must declare it.
    for c_type, name in sig[len(params):]:
        kind = {"char*": "string_buf", "int32_t*": "array_out", "bwapi_position*": "array_out"}.get(c_type, "array_out")
        if name in ("buf_len", "cap"):
            kind = "int32"
        params.append({"name": name, "type": kind, "c_type": c_type})
    r = e["returns"]
    base, _, arg = r.partition(":")
    returns = {"kind": r, "c_type": abispec.c_return_type(r), "neutral": abispec.neutral_doc(r)}
    if base == "type":
        returns["type_class"] = arg
        returns["constants"] = "BWAPI_" + abispec.family_prefix_of(arg)
    if base == "handle":
        returns["handle_kind"] = arg
    if base in ("position", "tile_position", "walk_position"):
        returns["scale"] = {"position": "pixel", "tile_position": "tile", "walk_position": "walk"}[base]
    out = {
        "c": e["c"],
        "section": stem,
        "signature": abispec.c_signature(e),
        "cpp": e.get("cpp"),
        "self": e["self"],
        "params": params,
        "returns": returns,
        "reentrant": e.get("reentrant", "allowed"),
        "legit_none": bool(e.get("legit_none", False)),
        "generated": "body" not in e and "source" not in e,
        "doc": " ".join(e["doc"].split()),
    }
    for optional in ("guides", "since", "divergences"):
        if optional in e:
            out[optional] = e[optional]
    return out


def constants_json(spec):
    families = []
    for fam in abispec.ABI_CONSTANTS:
        families.append({
            "family": fam["family"], "prefix": fam["prefix"], "cpp": None, "doc": fam["doc"],
            "values": [{"name": f"{fam['prefix']}_{n}", "cpp_name": None, "value": v, "doc": d}
                       for n, v, d in fam["values"]],
        })
    for fam in spec.families:
        families.append({
            "family": fam["family"], "prefix": fam["prefix"], "cpp": fam["cpp"], "doc": fam.get("doc", ""),
            "values": [{"name": abispec.constant_name(fam["prefix"], v["name"], fam.get("strip", "")),
                        "cpp_name": v["name"], "value": v["value"], "doc": ""}
                       for v in fam["values"] if abispec.exportable_enumerator(v["name"])],
        })
    return families


def structs_json(spec):
    return [{"name": s["name"], "c_type": f"bwapi_{s['name']}", "doc": " ".join(s["doc"].split()),
             "fields": s["fields"], "flags": s.get("flags") or []} for s in spec.structs]


def render(spec):
    doc = {
        "api_json_version": API_JSON_VERSION,
        "abi_version": abispec.abi_version(),
        "functions": [function_json(stem, e) for stem, e in spec.exports()],
        "constants": constants_json(spec),
        "structs": structs_json(spec),
    }
    return json.dumps(doc, indent=2, ensure_ascii=False) + "\n"


def main():
    text = render(abispec.Spec())
    old = open(API_JSON, encoding="utf-8").read() if os.path.exists(API_JSON) else None
    if "--check" in sys.argv:
        if old != text:
            print("emit_json: api.json is out of date", file=sys.stderr)
            sys.exit(1)
        return
    if old != text:
        with open(API_JSON, "w", encoding="utf-8") as f:
            f.write(text)
        print("wrote api.json")


if __name__ == "__main__":
    main()
