#!/usr/bin/env python3
"""The golden .def diff (plan sections 4 and 11): the library exports exactly the names
bwapi_c2.def lists, undecorated, and nothing else.

On Windows the names come from `dumpbin /exports` (found on PATH or through vswhere); elsewhere
from `nm -D --defined-only`, keeping the T symbols. Either way the set must equal the .def's
EXPORTS. A name in the DLL and not in the .def is a leak past hidden visibility; a name in the
.def and not in the DLL is a declaration without a definition.

Usage: check_exports.py --lib <dll or .so> --def bwapi_c2.def
"""
import argparse
import glob
import os
import re
import shutil
import subprocess
import sys


def def_exports(path):
    names = set()
    in_exports = False
    with open(path, encoding="utf-8") as f:
        for raw in f:
            line = raw.split(";", 1)[0].strip()
            if not line:
                continue
            if line.upper() == "EXPORTS":
                in_exports = True
                continue
            if in_exports:
                names.add(line.split()[0].split("=")[0].split("@")[0])
    return names


def find_dumpbin():
    exe = shutil.which("dumpbin")
    if exe:
        return exe
    vswhere = os.path.join(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"),
                           "Microsoft Visual Studio", "Installer", "vswhere.exe")
    if os.path.exists(vswhere):
        out = subprocess.run([vswhere, "-latest", "-products", "*", "-property", "installationPath"],
                             capture_output=True, text=True).stdout.strip()
        hits = glob.glob(os.path.join(out, "VC", "Tools", "MSVC", "*", "bin", "Hostx64", "x64", "dumpbin.exe"))
        if hits:
            return sorted(hits)[-1]
    sys.exit("check_exports: dumpbin not found on PATH or through vswhere")


def library_exports_windows(path):
    out = subprocess.run([find_dumpbin(), "/exports", path], capture_output=True, text=True, check=True).stdout
    names = set()
    # Rows look like "          1    0 00001000 bwapi_abi_version"; stop at the summary.
    for line in out.splitlines():
        m = re.match(r"^\s+\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]{8}\s+(\S+)", line)
        if m:
            names.add(m.group(1))
    return names


def library_exports_elf(path):
    out = subprocess.run(["nm", "-D", "--defined-only", path], capture_output=True, text=True, check=True).stdout
    names = set()
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[1] == "T":
            names.add(parts[2].split("@")[0])
    return names


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--lib", required=True)
    ap.add_argument("--def", dest="def_path", required=True)
    args = ap.parse_args()

    expected = def_exports(args.def_path)
    actual = library_exports_windows(args.lib) if sys.platform == "win32" else library_exports_elf(args.lib)

    extra = sorted(actual - expected)
    missing = sorted(expected - actual)
    if extra:
        print("check_exports: exported but not in the .def:\n  " + "\n  ".join(extra), file=sys.stderr)
    if missing:
        print("check_exports: in the .def but not exported:\n  " + "\n  ".join(missing), file=sys.stderr)
    if extra or missing:
        return 1
    print(f"check_exports: {len(actual)} export(s), exactly the .def")
    return 0


if __name__ == "__main__":
    sys.exit(main())
