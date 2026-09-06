#!/usr/bin/env python3
"""The coverage audit (plan section 9; implementation plan 1.6): libclang over the headers in
audited-headers.txt, with the shared flags of clang_flags.py, reporting every public
method or free function that has neither a spec entry nor a rule-bearing skip:, and every spec
entry whose cpp: no longer matches a declaration. Exit non-zero on either.

Off the merge path (plan section 9): run by tools/abi/audit.sh, by hand and at a pin bump,
never per PR. The recorded backlog (tools/abi/backlog.txt) is the set of declarations phases 2
and 3 will burn down; with --backlog the audit passes when the unaccounted set is exactly that
list, and fails when a declaration joins it or a backlog entry is stale.

    tools/abi/check_coverage.py [--backlog tools/abi/backlog.txt] [--write-backlog] [-v]

Needs the libclang Python bindings (pip install libclang, the same major as clang++).
"""
import argparse
import os
import re
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import abispec  # noqa: E402
import clang_flags  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
HEADERS_TXT = os.path.join(HERE, "audited-headers.txt")
BACKLOG_TXT = os.path.join(HERE, "backlog.txt")


def load_libclang():
    try:
        from clang import cindex
    except ImportError:
        sys.exit("check_coverage: the libclang Python bindings are missing: pip install libclang")
    # The system library first, under the spellings distributions use, then whatever the pip
    # package bundles. A spelling that fails to load leaves nothing behind: cindex sets
    # Config.loaded and caches the library only once a load succeeds, so the next spelling
    # starts clean and no reset is needed between attempts.
    for candidate in ("libclang-18.so.1", "libclang.so.18", "libclang-18.so", "libclang.so.1", "libclang.so"):
        try:
            cindex.Config.set_library_file(candidate)
            cindex.Index.create()
            return cindex
        except Exception:  # noqa: BLE001 - try the next spelling
            continue
    cindex.Config.library_file = None
    cindex.Index.create()
    return cindex


def read_headers():
    audited, excluded = [], []
    with open(HEADERS_TXT, encoding="utf-8") as f:
        for raw in f:
            line = raw.split("#", 1)[0].strip()
            if not line:
                continue
            if line.startswith("exclude "):
                excluded.append(line[len("exclude "):].strip())
            else:
                audited.append(line)
    return audited, excluded


def resolve(header):
    for root in (clang_flags.BWAPI_INCLUDE, clang_flags.BWEM_INCLUDE):
        path = os.path.join(root, header)
        if os.path.exists(path):
            return os.path.realpath(path)
    sys.exit(f"check_coverage: audited header {header} not found under the include roots")


def qualified_class(cursor):
    """The class name the spec uses: unqualified for BWAPI, BWEM:: for BWEM, template arguments
    dropped (Type<UnitType, 233> is Type)."""
    parts = []
    c = cursor
    while c is not None and c.kind.name != "TRANSLATION_UNIT":
        if c.kind.name in ("CLASS_DECL", "STRUCT_DECL", "CLASS_TEMPLATE", "NAMESPACE"):
            parts.append(re.sub(r"<.*>$", "", c.spelling))
        c = c.semantic_parent
    parts.reverse()
    if parts and parts[0] == "BWAPI":
        parts = parts[1:]
    return "::".join(parts)


class Declarations:
    """Every public method and free function in the audited headers, keyed the way the spec
    keys them, plus the base classes of each class so an inherited member resolves."""

    def __init__(self, cindex, audited_paths, verbose=False):
        self.by_key = {}         # cpp key -> list of cursors, audited headers only: the universe
        self.by_class = {}       # class -> {method name: [cursors]}, every header, for resolution
        self.bases = {}          # class -> [base class names]
        self.audited = set(audited_paths)
        self.verbose = verbose
        self.cindex = cindex

    def in_audited(self, cursor):
        loc = cursor.location
        return loc.file is not None and os.path.realpath(loc.file.name) in self.audited

    def collect(self, tu):
        for cursor in tu.cursor.walk_preorder():
            kind = cursor.kind.name
            if kind in ("CLASS_DECL", "STRUCT_DECL", "CLASS_TEMPLATE") and cursor.is_definition():
                cls = qualified_class(cursor)
                for child in cursor.get_children():
                    if child.kind.name == "CXX_BASE_SPECIFIER":
                        base = re.sub(r"^(const\s+)?(BWAPI::|BWEM::)?", "", child.type.spelling)
                        self.bases.setdefault(cls, []).append(re.sub(r"<.*>$", "", base))
            if kind not in ("CXX_METHOD", "FUNCTION_DECL", "FUNCTION_TEMPLATE"):
                continue
            if kind == "CXX_METHOD" and cursor.access_specifier.name != "PUBLIC":
                continue
            name = cursor.spelling
            if name.startswith("operator") or name.startswith("~"):
                continue
            parent = cursor.semantic_parent
            if kind == "FUNCTION_TEMPLATE" and name.split("<")[0] == parent.spelling.split("<")[0]:
                continue  # a template constructor
            if parent.kind.name == "NAMESPACE" and kind == "FUNCTION_DECL":
                cls = qualified_class(parent)
            elif parent.kind.name in ("CLASS_DECL", "STRUCT_DECL", "CLASS_TEMPLATE"):
                cls = qualified_class(parent)
            else:
                continue
            if not (cls.startswith("BWEM::") or self.in_audited(cursor) or cursor.location.file is None
                    or "third_party" in os.path.realpath(cursor.location.file.name)):
                continue  # the standard library is not resolution material
            self.by_class.setdefault(cls, {}).setdefault(name, []).append(cursor)
        # The universe is what the audited headers declare; a base class outside them (Type<>,
        # Interface<>) resolves a spec entry but is not itself audited.
        for cls, methods in self.by_class.items():
            for name, cursors in methods.items():
                overloaded = len(cursors) > 1
                for cur in cursors:
                    if not self.in_audited(cur):
                        continue
                    key = abispec.cpp_key(cls, name, self.param_types(cur), overloaded)
                    self.by_key.setdefault(key, []).append(cur)

    @staticmethod
    def param_types(cursor):
        return [a.type.spelling for a in cursor.get_arguments()]

    def resolve(self, cpp):
        """The declarations a spec cpp: string names: exact key, or the bare name when the spec
        omitted the parameter list, walking up the base classes for an inherited member."""
        cls, method, params = abispec.split_cpp(cpp)
        for candidate in self.class_chain(cls):
            methods = self.by_class.get(candidate, {})
            if method not in methods:
                continue
            cursors = methods[method]
            if params is None:
                return cursors
            wanted = [abispec.normalize_type(p) for p in params]
            return [c for c in cursors if [abispec.normalize_type(t) for t in self.param_types(c)] == wanted]
        return []

    def class_chain(self, cls):
        seen, order = set(), []
        stack = [cls]
        while stack:
            c = stack.pop(0)
            if c in seen:
                continue
            seen.add(c)
            order.append(c)
            stack += self.bases.get(c, [])
        return order


def parse(cindex, audited_paths):
    """One translation unit including every audited header, under the audit flags."""
    src = "".join(f'#include "{p}"\n' for p in audited_paths)
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
    if errors:
        for d in errors[:20]:
            print(f"check_coverage: {d.location.file}:{d.location.line}: {d.spelling}", file=sys.stderr)
        sys.exit(f"check_coverage: {len(errors)} parse error(s); the audit flags in clang_flags.py may need updating")
    return tu


def read_backlog(path):
    keys = set()
    if not os.path.exists(path):
        return keys
    with open(path, encoding="utf-8") as f:
        for raw in f:
            line = raw.split("#", 1)[0].strip()
            if line:
                keys.add(line)
    return keys


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--backlog", help="a file of cpp keys known to be unaccounted (phases 2 and 3)")
    ap.add_argument("--write-backlog", action="store_true", help="rewrite tools/abi/backlog.txt with the unaccounted set")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    cindex = load_libclang()
    audited, _ = read_headers()
    paths = [resolve(h) for h in audited]
    tu = parse(cindex, paths)
    decls = Declarations(cindex, paths, args.verbose)
    decls.collect(tu)

    spec = abispec.Spec()
    accounted = set()
    problems = []
    # Every spec entry with a cpp: must name exactly one declaration (a wildcard skip, Class::*,
    # names every member of the class).
    for stem, e in spec.entries:
        cpp = e.get("cpp")
        if not cpp:
            continue
        if cpp.endswith("::*"):
            if "skip" not in e:
                problems.append(f"{stem}: {cpp}: a wildcard is only for skip: entries")
                continue
            cls = cpp[:-3]
            members = decls.by_class.get(cls, {})
            if not members:
                problems.append(f"{stem}: {cpp}: no class {cls} in the audited headers")
            for name, cursors in members.items():
                for cur in cursors:
                    accounted.add(abispec.cpp_key(cls, name, decls.param_types(cur), len(cursors) > 1))
            continue
        matches = decls.resolve(cpp)
        cls = abispec.split_cpp(cpp)[0]
        if not matches:
            if cls == "Client":
                continue  # BWAPI::Client lives in Client/Client.h, excluded as not public; client.gen.cpp's static_asserts cover it
            problems.append(f"{stem}: {e.get('c', 'skip')}: cpp {cpp!r} matches no declaration")
            continue
        if len(matches) > 1:
            problems.append(f"{stem}: {e.get('c', 'skip')}: cpp {cpp!r} is ambiguous ({len(matches)} overloads); spell the parameter types")
            continue
        cur = matches[0]
        owner = qualified_class(cur.semantic_parent)
        overloaded = len(decls.by_class.get(owner, {}).get(cur.spelling, [])) > 1
        accounted.add(abispec.cpp_key(owner, cur.spelling, decls.param_types(cur), overloaded))
        # A generated entry's parameters must match the declaration's count, receiver aside.
        if "c" in e and "body" not in e and "source" not in e:
            n_spec = len(e.get("params") or [])
            if e["self"] == "none":
                n_spec -= 1  # the receiver
            if n_spec != len(list(cur.get_arguments())):
                problems.append(f"{stem}: {e['c']}: {n_spec} spec parameter(s) against {len(list(cur.get_arguments()))} in {cpp}")

    unaccounted = sorted(k for k in decls.by_key if k not in accounted)
    backlog = read_backlog(args.backlog) if args.backlog else set()
    new = [k for k in unaccounted if k not in backlog]
    stale = sorted(k for k in backlog if k in accounted or k not in decls.by_key)

    total = len(decls.by_key)
    print(f"check_coverage: {total} declarations in {len(paths)} audited headers; "
          f"{len(accounted & set(decls.by_key))} accounted (entry or skip), {len(unaccounted)} unaccounted"
          + (f", {len(backlog)} on the backlog, {len(new)} new, {len(stale)} stale" if args.backlog else ""))
    if args.write_backlog:
        with open(BACKLOG_TXT, "w", encoding="utf-8") as f:
            f.write("# The declarations the coverage audit reports as unaccounted: the phase-2 and phase-3\n"
                    "# backlog (implementation plan 1.6). check_coverage.py --backlog passes while the\n"
                    "# unaccounted set is exactly this list; a declaration that gains a spec entry or a skip\n"
                    "# must leave it, and a new one must be decided, not added. Rewritten by --write-backlog.\n")
            for k in unaccounted:
                f.write(k + "\n")
        print(f"check_coverage: wrote {len(unaccounted)} entries to {os.path.relpath(BACKLOG_TXT)}")
    for p in problems:
        print(f"check_coverage: {p}", file=sys.stderr)
    if args.backlog:
        for k in new:
            print(f"check_coverage: unaccounted and not on the backlog: {k}", file=sys.stderr)
        for k in stale:
            print(f"check_coverage: stale backlog entry: {k}", file=sys.stderr)
    elif args.verbose:
        for k in unaccounted:
            print(f"  {k}")
    failed = bool(problems) or (bool(new) or bool(stale) if args.backlog else bool(unaccounted))
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
