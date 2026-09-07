#!/usr/bin/env python3
"""R12: what the coverage audit (plan section 9, pin-bump step 6 in docs/pins.md) would report
if the BWAPI pin moved from the 4.4.0-era tree to upstream's `develop` branch (BWAPI 5).

Parses the 24 BWAPI headers on tools/abi/audited-headers.txt under the audit's own flags
(tools/abi/clang_flags.py) against two include roots, collects every public declaration with
check_coverage.py's walker, and diffs the two universes: declarations removed, added, and
re-signatured, per header and per class. Then resolves every cpp: string in tools/abi/spec
against the develop tree, as the audit does at a pin bump, and reports how many still name a
declaration -- once as the audit would see it, and once after the five Interface -> value-type
renames, so the mechanical part of the churn is separated from the semantic part.

Usage:
    docs/research/r12/audit-diff.py <develop-include-root> [--pinned <pinned-include-root>]
                                    [--out <dir>]

<develop-include-root> is `<checkout of bwapi/bwapi@develop>/include`, and the checkout must
sit under a directory named `third_party` (a git worktree at `<scratch>/third_party/bwapi-develop`
does): check_coverage.py's walker admits a non-audited header as resolution material only from
under one, and develop's getID and the Type<> accessors live in headers the audit does not list
(IDs.h, Type.h). The pinned root defaults to the submodule's third_party/bwapi/bwapi/include.
--out writes the full key lists as text files beside the summary this prints. Needs the
libclang Python bindings at clang++'s major.
"""
import argparse
import os
import sys
import tempfile
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(REPO, "tools", "abi"))
import abispec  # noqa: E402
import check_coverage  # noqa: E402
import clang_flags  # noqa: E402

# BWAPI 5 collapses the pointer-to-interface typedefs into value types (include/BWAPI/IDs.h,
# InterfaceDataWrapper). The audit keys declarations by class, so every UnitInterface:: entry
# fails to resolve on develop for the rename alone; this map lets the diff say how much of
# the churn is that rename and how much is real.
RENAMES = {
    "UnitInterface": "Unit",
    "PlayerInterface": "Player",
    "ForceInterface": "Force",
    "RegionInterface": "Region",
    "BulletInterface": "Bullet",
}


def bwapi_audited_headers():
    audited, _ = check_coverage.read_headers()
    return [h for h in audited if h.startswith("BWAPI/")]


def parse_tree(cindex, include_root, headers):
    """One TU over the audited headers under the audit flags, with BWAPI_INCLUDE pointed at
    include_root. Diagnostics are reported, not fatal: a tree that no longer parses under the
    pinned flags is itself a result."""
    clang_flags.BWAPI_INCLUDE = include_root
    paths = []
    missing = []
    for h in headers:
        p = os.path.join(include_root, h)
        if os.path.exists(p):
            paths.append(os.path.realpath(p))
        else:
            missing.append(h)
    src = "".join(f'#include "{p}"\n' for p in paths)
    with tempfile.NamedTemporaryFile("w", suffix=".cpp", delete=False) as f:
        f.write(src)
        path = f.name
    try:
        index = cindex.Index.create()
        tu = index.parse(path, args=clang_flags.audit_flags(),
                         options=cindex.TranslationUnit.PARSE_SKIP_FUNCTION_BODIES)
    finally:
        os.remove(path)
    errors = [d for d in tu.diagnostics if d.severity >= cindex.Diagnostic.Error]
    decls = check_coverage.Declarations(cindex, paths)
    decls.collect(tu)
    return decls, paths, missing, errors


def universe(decls, rename):
    """{(class, method, (param types...)): header basename} for every audited declaration,
    with parameter types always spelled so an overload gained or lost still compares."""
    out = {}
    for key, cursors in decls.by_key.items():
        for cur in cursors:
            cls = check_coverage.qualified_class(cur.semantic_parent)
            if rename:
                cls = RENAMES.get(cls, cls)
            params = tuple(abispec.normalize_type(t) for t in decls.param_types(cur))
            header = os.path.basename(cur.location.file.name)
            out[(cls, cur.spelling, params)] = header
    return out


def fmt(k):
    cls, name, params = k
    return f"{cls}::{name}({', '.join(params)})"


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("develop_include")
    ap.add_argument("--pinned", default=clang_flags.BWAPI_INCLUDE)
    ap.add_argument("--out", default=None, help="directory for the full key lists")
    args = ap.parse_args()

    for root in (args.develop_include, args.pinned):
        if "third_party" not in os.path.realpath(root).split(os.sep):
            sys.exit(f"audit-diff: {root} must sit under a directory named third_party (see the usage note)")

    cindex = check_coverage.load_libclang()
    headers = bwapi_audited_headers()

    pinned, _, pinned_missing, pinned_errors = parse_tree(cindex, args.pinned, headers)
    develop, _, dev_missing, dev_errors = parse_tree(cindex, args.develop_include, headers)

    print(f"audited BWAPI headers: {len(headers)}")
    print(f"pinned  ({args.pinned}): {len(pinned.by_key)} keys, {len(pinned_errors)} parse errors, missing headers: {pinned_missing or 'none'}")
    print(f"develop ({args.develop_include}): {len(develop.by_key)} keys, {len(dev_errors)} parse errors, missing headers: {dev_missing or 'none'}")
    for d in dev_errors[:10]:
        print(f"  develop parse error: {d.location.file}:{d.location.line}: {d.spelling}")

    p_raw, d_raw = universe(pinned, rename=False), universe(develop, rename=False)
    p_ren, d_ren = universe(pinned, rename=True), universe(develop, rename=True)

    # --- names, per header: the coarse diff a reader wants first ---------------------------
    def names(u):
        s = defaultdict(set)
        for (cls, name, _), header in u.items():
            s[header].add((cls, name))
        return s

    pn, dn = names(p_ren), names(d_ren)
    print("\nPer header, after the Interface -> value-type rename (class::name, overloads folded):")
    print(f"{'header':18} {'pinned':>7} {'develop':>8} {'kept':>6} {'removed':>8} {'added':>6}")
    tot = defaultdict(int)
    for h in headers:
        b = os.path.basename(h)
        a, c = pn.get(b, set()), dn.get(b, set())
        kept, removed, added = a & c, a - c, c - a
        print(f"{b:18} {len(a):7} {len(c):8} {len(kept):6} {len(removed):8} {len(added):6}")
        tot["pinned"] += len(a); tot["develop"] += len(c); tot["kept"] += len(kept)
        tot["removed"] += len(removed); tot["added"] += len(added)
    print(f"{'total':18} {tot['pinned']:7} {tot['develop']:8} {tot['kept']:6} {tot['removed']:8} {tot['added']:6}")

    all_p = set().union(*pn.values()) if pn else set()
    all_d = set().union(*dn.values()) if dn else set()
    removed_names = sorted(all_p - all_d)
    added_names = sorted(all_d - all_p)

    # --- signatures: same class::name on both sides, different parameter sets ----------------
    def sigs(u):
        s = defaultdict(set)
        for (cls, name, params) in u:
            s[(cls, name)].add(params)
        return s

    ps, ds = sigs(p_ren), sigs(d_ren)
    resig = []
    for cn in sorted(set(ps) & set(ds)):
        if ps[cn] != ds[cn]:
            resig.append((cn, sorted(ps[cn]), sorted(ds[cn])))

    print(f"\nremoved names: {len(removed_names)}; added names: {len(added_names)}; "
          f"names kept with a changed parameter set: {len(resig)}")

    # --- the spec, resolved against develop as the audit would do at a pin bump --------------
    spec = abispec.Spec()
    entries = [(stem, e) for stem, e in spec.entries if e.get("cpp") and not e["cpp"].endswith("::*")]
    raw_fail, ren_fail, ambiguous = [], [], []
    for stem, e in entries:
        cpp = e["cpp"]
        cls = abispec.split_cpp(cpp)[0]
        if cls == "Client":
            continue
        if not develop.resolve(cpp):
            raw_fail.append(cpp)
            cls2 = RENAMES.get(cls, cls)
            cpp2 = cls2 + cpp[len(cls):]
            m = develop.resolve(cpp2)
            if not m:
                ren_fail.append(cpp)
            elif len(m) > 1:
                ambiguous.append(cpp)
    wildcard = [e["cpp"] for _, e in spec.entries if e.get("cpp", "").endswith("::*")]
    wildcard_missing = [w for w in wildcard if RENAMES.get(w[:-3], w[:-3]) not in develop.by_class]
    print(f"\nspec entries with a cpp: (Client:: excluded): {len(entries)}; "
          f"unresolved on develop as written: {len(raw_fail)}; "
          f"still unresolved after the rename: {len(ren_fail)}; ambiguous after the rename: {len(ambiguous)}; "
          f"wildcard skips whose class is gone: {len(wildcard_missing)} of {len(wildcard)}")

    if args.out:
        os.makedirs(args.out, exist_ok=True)
        def dump(name, lines):
            with open(os.path.join(args.out, name), "w", encoding="utf-8") as f:
                f.write("\n".join(lines) + ("\n" if lines else ""))
        dump("removed-names.txt", [f"{c}::{n}" for c, n in removed_names])
        dump("added-names.txt", [f"{c}::{n}" for c, n in added_names])
        dump("resignatured.txt", [f"{c}::{n}  pinned {[', '.join(p) for p in a]}  develop {[', '.join(p) for p in b]}" for (c, n), a, b in resig])
        dump("spec-unresolved-raw.txt", raw_fail)
        dump("spec-unresolved-after-rename.txt", ren_fail)
        dump("spec-ambiguous-after-rename.txt", ambiguous)
        dump("pinned-keys.txt", sorted(fmt(k) for k in p_raw))
        dump("develop-keys.txt", sorted(fmt(k) for k in d_raw))
        print(f"\nfull lists written under {args.out}")
    else:
        print("\nremoved:"); [print("  " + f"{c}::{n}") for c, n in removed_names]
        print("added:"); [print("  " + f"{c}::{n}") for c, n in added_names]
        print("re-signatured:"); [print(f"  {c}::{n}: {a} -> {b}") for (c, n), a, b in resig]
        print("spec unresolved after rename:"); [print("  " + x) for x in ren_fail]


if __name__ == "__main__":
    main()
