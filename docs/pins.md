# Pinned dependencies

The two submodules under `third_party/`, the commit each is pinned to, and the procedure for
moving a pin. Plan §10.3 is the rationale; this file is the record. Update it in the same
commit that moves a submodule.

## Current pins

| Dependency | Path | Commit | What it is |
|---|---|---|---|
| `bwapi/bwapi` | `third_party/bwapi` | `d727fed68558c506163048ea889131d8cbb33915` | upstream `master` at 2026-05-08, after the `v4.4.0` tag; `CLIENT_VERSION` 10003; `SVN_REV` 5030 from `revisionUpdate.vbs` |
| `N00byEdge/BWEM-community` | `third_party/bwem` | `9a63141f301f7830e9e09a2bae95c304fcc03cc5` | `master` at 2021-06-01, the fork's last commit; MIT/X11 |

**Both repositories nest their sources one level down.** BWAPI's tree root is
`third_party/bwapi/bwapi/` (so the include root is `third_party/bwapi/bwapi/include` and the
client sources are `third_party/bwapi/bwapi/BWAPIClient/Source`), and BWEM's is
`third_party/bwem/BWEM/` (`include/` holds `map.h` and friends directly, `src/` the fourteen
translation units). Every path in the plan's §10.1 is relative to those inner roots.

**BWEM's own submodules are never fetched.** `external/googletest`, `external/openbw-bwapi`
and `external/openbw` (the unlicensed engine) are test-only; `.gitmodules` sets
`fetchRecurseSubmodules = false` on `third_party/bwem`, and no script in this tree passes
`--recursive` (plan §0, §10.1, Appendix B).

## Carried patches

| Dependency | Patch | Plan |
|---|---|---|
| `third_party/bwapi` | `patches/bwapi-convenience-va-list.patch` — `BWAPIClient/Source/Convenience.h:33`, `va_list &ap` → `va_list ap` | §15.2 |
| `third_party/bwem` | `patches/bwem-reset-instance.patch` — adds `void Map::ResetInstance()` | §15.2 |

Applied into the submodule working trees by `tools/apply-patches.sh`, which CMake configure
runs (implementation plan 0.3, 0.5). Idempotent; `--reverse` removes them, which is the first
thing to do when moving a pin. Each patch file opens with prose stating what it changes and
why; `git apply` ignores everything before the first `diff --git` line.

## Moving a pin

Moving a pin is a deliberate act, and every check attaches to it (plan §10.3). There is no
scheduled drift job.

1. Move the submodule (`git -C third_party/<dep> checkout <commit>`), and update the table
   above in the same commit.
2. Re-apply the carried patches. A patch that no longer applies is the bump's first finding,
   not a surprise.
3. For BWAPI: run `cscript.exe revisionUpdate.vbs` in the pinned checkout on Windows and commit
   the regenerated `vendor/svnrev.h`. Never synthesise it.
4. Run the layout dump and the derived-closure test; diff both against the checked-in baselines.
5. Run `check_coverage.py`; resolve every added, removed or changed declaration.
6. Rebuild; run every suite in plan §11. Record the new revision and `CLIENT_VERSION` here.
