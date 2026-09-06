#!/usr/bin/env python3
"""The one place the generator's clang invocations get their flags (implementation plan 1.3).

draft_spec.py (clang's JSON AST dump) and check_coverage.py (libclang) both parse the pinned
BWAPI and BWEM headers; if each carried its own include list the two would drift and the audit
would silently see a different tree from the one the drafts came from. So both import this.

Usage as a script prints the flags, one per line, for a shell that wants them:

    tools/abi/clang_flags.py [--audit]
"""
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))

# Both submodules nest their sources one directory down (docs/pins.md).
BWAPI_ROOT = os.path.join(REPO, "third_party", "bwapi", "bwapi")
BWEM_ROOT = os.path.join(REPO, "third_party", "bwem", "BWEM")
BWAPI_INCLUDE = os.path.join(BWAPI_ROOT, "include")
BWEM_INCLUDE = os.path.join(BWEM_ROOT, "include")


def clang_binary():
    """clang++ from CXX when that is a clang, else the first clang++ on PATH."""
    cxx = os.environ.get("CXX", "")
    if "clang" in os.path.basename(cxx):
        return cxx
    found = shutil.which("clang++")
    if not found:
        sys.exit("clang_flags: clang++ not found on PATH (set CXX)")
    return found


def check_submodules():
    for probe in (os.path.join(BWAPI_INCLUDE, "BWAPI.h"), os.path.join(BWEM_INCLUDE, "bwem.h")):
        if not os.path.exists(probe):
            sys.exit(f"clang_flags: {probe} is missing; run `git submodule update --init --depth 1` (never --recursive)")


def base_flags():
    """What every parse of the headers needs: C++17 as the closure is built, the include roots,
    NOMINMAX as cmake/closure.cmake defines it, and MSVC's delayed template parsing, which
    include/BWAPI/Client/CommandTemp.h:34 relies on (plan section 10.1)."""
    check_submodules()
    return [
        "-x", "c++",
        "-std=c++17",
        "-fdelayed-template-parsing",
        "-DNOMINMAX=1",
        "-I", BWAPI_INCLUDE,
        "-I", BWEM_INCLUDE,
    ]


def audit_flags():
    """The coverage audit parses v141_xp-era headers whole, which needs MSVC compatibility on
    top of the base flags (plan section 9). Off the merge path for exactly that reason."""
    return base_flags() + ["-fms-extensions", "-fms-compatibility"]


if __name__ == "__main__":
    flags = audit_flags() if "--audit" in sys.argv[1:] else base_flags()
    print("\n".join(flags))
