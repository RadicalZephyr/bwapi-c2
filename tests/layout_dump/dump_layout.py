#!/usr/bin/env python3
"""Dump BWAPI's GameData layout per target and diff it against the baseline (plan section 10.2).

R5's trick: compile a probe with `-fsyntax-only` and read the numbers out of clang's error
messages for an undefined template `SZ<N>`. Nothing links and no Windows SDK is needed, so the
MSVC targets are checked from Linux. For every field of GameData the probe asks for its offset
and size, plus sizeof each component struct, and the result is one JSON object per target.

Two sources of truth, and they must agree:
  - layout.cpp, the self-contained copy of the structs, compiled for every target;
  - the real headers of the pinned BWAPI, compiled for the host target only (they pull in the
    standard library, which the cross targets do not have).
A difference between the two on the host target means layout.cpp has drifted from the pin.

Usage:
  dump_layout.py --clang CLANG --bwapi-include DIR [--check | --update] [--json OUT]
  dump_layout.py --emit-static-asserts OUT.cpp

The second form needs no compiler: it turns baseline.json into a translation unit of
static_asserts over the real headers, one per field offset and size plus the component sizes,
which CMake compiles with whatever compiler builds the DLL. That closes the gap between "clang
predicts MSVC's layout" and "MSVC's layout": the baseline is asserted by MSVC itself in the
Windows job, and by clang on Linux.
"""
import argparse
import json
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
LAYOUT_CPP = os.path.join(HERE, "layout.cpp")
BASELINE = os.path.join(HERE, "baseline.json")

# The one number the whole plan rests on (section 10.2): sizeof(GameData) on every target.
EXPECTED_SIZEOF_GAMEDATA = 33017048

TARGETS = ["i386-pc-windows-msvc", "x86_64-pc-windows-msvc", "x86_64-unknown-linux-gnu"]
HOST_TARGET = "x86_64-unknown-linux-gnu"

COMPONENT_TYPES = [
    "BWAPI::GameData", "BWAPI::UnitData", "BWAPI::PlayerData", "BWAPI::ForceData",
    "BWAPI::BulletData", "BWAPI::RegionData", "BWAPI::unitFinder",
    "BWAPIC::Position", "BWAPIC::Event", "BWAPIC::Shape", "BWAPIC::Command",
    "BWAPIC::UnitCommand",
]

FIELD_RE = re.compile(
    r"^\s*(?!static\b)(?!//)([A-Za-z_][\w:]*(?:\s+[A-Za-z_][\w:]*)*)\s+([A-Za-z_]\w*)\s*"
    r"((?:\[[^\]]*\])*)\s*;")


def gamedata_fields(layout_cpp):
    """The field names of struct GameData, in declaration order, from layout.cpp."""
    with open(layout_cpp, encoding="utf-8") as f:
        text = f.read()
    start = text.index("struct GameData")
    body = text[text.index("{", start) + 1:]
    depth, end = 1, 0
    for i, ch in enumerate(body):
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                end = i
                break
    fields = []
    for line in body[:end].splitlines():
        m = FIELD_RE.match(line)
        if m:
            fields.append(m.group(2))
    if not fields:
        sys.exit("dump_layout: no fields found in struct GameData")
    return fields


def probe_source(fields, header_line):
    lines = [header_line,
             "#define BWAPI_C2_OFFSETOF(T, M) __builtin_offsetof(T, M)",
             "template <unsigned long long N> struct SZ;"]
    for i, t in enumerate(COMPONENT_TYPES):
        lines.append(f"SZ<sizeof({t})> sz_{i};")
    for i, f in enumerate(fields):
        lines.append(f"SZ<BWAPI_C2_OFFSETOF(BWAPI::GameData, {f})> off_{i};")
        lines.append(f"SZ<sizeof(BWAPI::GameData::{f})> fsz_{i};")
    return "\n".join(lines) + "\n"


NUM_RE = re.compile(r"variable has incomplete type 'SZ<(\d+)>'|undefined template 'SZ<(\d+)>'")


def run_probe(clang, target, source, include_dirs, extra_flags):
    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, "probe.cpp")
        with open(path, "w", encoding="utf-8") as f:
            f.write(source)
        cmd = [clang, "-x", "c++", "-std=c++17", "-fsyntax-only", "-target", target,
               "-ferror-limit=0", "-w"] + [f"-I{d}" for d in include_dirs] + extra_flags + [path]
        proc = subprocess.run(cmd, capture_output=True, text=True)
    numbers = []
    for line in proc.stderr.splitlines():
        m = NUM_RE.search(line)
        if m:
            numbers.append(int(m.group(1) or m.group(2)))
    return numbers, proc.stderr


def layout_for(clang, target, fields, header_line, include_dirs, extra_flags=()):
    numbers, stderr = run_probe(clang, target, probe_source(fields, header_line),
                                include_dirs, list(extra_flags))
    expected = len(COMPONENT_TYPES) + 2 * len(fields)
    if len(numbers) != expected:
        sys.stderr.write(stderr)
        sys.exit(f"dump_layout: {target}: expected {expected} numbers from clang, got "
                 f"{len(numbers)}; the probe did not compile as intended")
    sizes = dict(zip(COMPONENT_TYPES, numbers[:len(COMPONENT_TYPES)]))
    rest = numbers[len(COMPONENT_TYPES):]
    return {
        "sizeof": sizes,
        "fields": [{"name": f, "offset": rest[2 * i], "size": rest[2 * i + 1]}
                   for i, f in enumerate(fields)],
    }


def emit_static_asserts(out_path):
    """baseline.json -> a TU asserting every number against the real headers, for any compiler."""
    with open(BASELINE, encoding="utf-8") as f:
        baseline = json.load(f)
    layouts = list(baseline.values())
    if any(l != layouts[0] for l in layouts[1:]):
        sys.exit("dump_layout: the baseline's targets disagree; nothing sane to assert")
    layout = layouts[0]
    lines = [
        "// Generated by tests/layout_dump/dump_layout.py --emit-static-asserts from baseline.json.",
        "// Every field of GameData, asserted against the real pinned headers by the compiler that",
        "// builds this tree (plan section 10.2). Do not edit; regenerate at a pin bump.",
        "#include <BWAPI/Client/GameData.h>",
        "#include <cstddef>",
        "",
    ]
    for t, n in layout["sizeof"].items():
        lines.append(f'static_assert(sizeof({t}) == {n}, "sizeof({t}) changed: revisit plan section 10.2");')
    lines.append("")
    for fld in layout["fields"]:
        name = fld["name"]
        lines.append(f'static_assert(offsetof(BWAPI::GameData, {name}) == {fld["offset"]}, "GameData::{name} moved");')
        lines.append(f'static_assert(sizeof(BWAPI::GameData::{name}) == {fld["size"]}, "GameData::{name} resized");')
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print(f"dump_layout: wrote {len(layout['fields'])} field assertions to {out_path}")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--clang", help="clang++ to run (any host; cross targets are free)")
    ap.add_argument("--bwapi-include", help="the pinned BWAPI's include/ directory, for the real-header check")
    ap.add_argument("--check", action="store_true", help="fail on any difference from baseline.json")
    ap.add_argument("--update", action="store_true", help="rewrite baseline.json from this run")
    ap.add_argument("--json", help="also write the full dump here")
    ap.add_argument("--emit-static-asserts", metavar="OUT.cpp",
                    help="write a static_assert TU from baseline.json and exit; needs no compiler")
    args = ap.parse_args()

    if args.emit_static_asserts:
        return emit_static_asserts(args.emit_static_asserts)
    if not args.clang or not args.bwapi_include:
        ap.error("--clang and --bwapi-include are required unless --emit-static-asserts is given")

    fields = gamedata_fields(LAYOUT_CPP)
    dump = {}
    for target in TARGETS:
        dump[target] = layout_for(args.clang, target, fields, f'#include "{LAYOUT_CPP}"', [])

    # The real headers on the host target. GameData.h needs the client include directory for
    # its unqualified includes and NOMINMAX for the Windows-isms in the BWAPI headers.
    real = layout_for(args.clang, HOST_TARGET, fields, "#include <BWAPI/Client/GameData.h>",
                      [args.bwapi_include, os.path.join(args.bwapi_include, "BWAPI", "Client")],
                      ["-DNOMINMAX=1"])

    failures = []
    for target, layout in dump.items():
        got = layout["sizeof"]["BWAPI::GameData"]
        if got != EXPECTED_SIZEOF_GAMEDATA:
            failures.append(f"{target}: sizeof(GameData) is {got}, expected {EXPECTED_SIZEOF_GAMEDATA}")
    if real != dump[HOST_TARGET]:
        failures.append(f"{HOST_TARGET}: layout.cpp disagrees with the pinned BWAPI headers; "
                        f"layout.cpp has drifted from the pin")
        for a, b in zip(real["fields"], dump[HOST_TARGET]["fields"]):
            if a != b:
                failures.append(f"  first difference at field {a['name']}: real {a} vs copy {b}")
                break
    # Every target must agree with every other: that is what makes x64 client mode sound.
    for target in TARGETS[1:]:
        if dump[target] != dump[TARGETS[0]]:
            failures.append(f"{target} differs from {TARGETS[0]}")

    if args.json:
        with open(args.json, "w", encoding="utf-8") as f:
            json.dump(dump, f, indent=1)
            f.write("\n")

    if args.update:
        with open(BASELINE, "w", encoding="utf-8") as f:
            json.dump(dump, f, indent=1)
            f.write("\n")
        print(f"dump_layout: wrote {BASELINE}")
    elif args.check:
        with open(BASELINE, encoding="utf-8") as f:
            baseline = json.load(f)
        if baseline != dump:
            failures.append("dump differs from baseline.json; run with --update if the pin moved "
                            "on purpose and the plan's section 10.2 has been revisited")

    for target, layout in dump.items():
        print(f"{target:28s} sizeof(GameData)={layout['sizeof']['BWAPI::GameData']} "
              f"fields={len(layout['fields'])}")
    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1
    print("dump_layout: all targets agree, and layout.cpp matches the pinned headers")
    return 0


if __name__ == "__main__":
    sys.exit(main())
