#!/usr/bin/env python3
"""Run every emitter (implementation plan 1.4): headers, sources, .def, api.json, and the
reference pages so a local `zola serve` is current. The first four write checked-in files;
CI runs this and fails on `git diff --exit-code` over them (tests/regen_check.sh), which is
what makes every generator change a reviewable diff in the output.

    tools/abi/regen.py            # regenerate everything
    tools/abi/regen.py --check    # exit 1 if any checked-in output would change; writes nothing
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))

CHECKED_IN = ["emit_header.py", "emit_source.py", "emit_def.py", "emit_json.py"]


def run(script, args):
    return subprocess.run([sys.executable, os.path.join(HERE, script), *args], cwd=REPO).returncode


def main():
    check = "--check" in sys.argv[1:]
    failed = False
    for script in CHECKED_IN:
        if run(script, ["--check"] if check else []) != 0:
            failed = True
    if failed:
        sys.exit(1)
    if not check:
        run("emit_docs.py", [])
        static = os.path.join(REPO, "site", "static")
        os.makedirs(static, exist_ok=True)
        with open(os.path.join(REPO, "api.json"), "rb") as src, open(os.path.join(static, "api.json"), "wb") as dst:
            dst.write(src.read())


if __name__ == "__main__":
    main()
