# Pinned dependencies

The two submodules under `third_party/`, the commit each is pinned to, and the procedure for
moving a pin. Plan §10.3 is the rationale; this file is the record. Update it in the same
commit that moves a submodule.

## How the pins work

Each submodule points at a **fork under `RadicalZephyr/`**, not at upstream. The fork carries a
`bwapi-c2-pin` branch: the upstream commit we pin, plus the few commits of our own that
`bwapi-c2` needs (plan §15.2). The forks exist so that those commits are ordinary commits with
ordinary history rather than patch files applied into a working tree at configure time; the
fork's default branch tracks upstream untouched, so the diff between the two branches is
exactly what we carry. Nothing in this repository modifies a submodule working tree.

## Current pins

| Dependency | Path | Pinned commit (fork, `bwapi-c2-pin`) | Upstream base | What it is |
|---|---|---|---|---|
| [`RadicalZephyr/bwapi`](https://github.com/RadicalZephyr/bwapi), fork of `bwapi/bwapi` | `third_party/bwapi` | `621ef61e0d423b5ed48f75f0126cd1dfb7efb47a` | `d727fed68558c506163048ea889131d8cbb33915` | upstream `master` at 2026-05-08, after the `v4.4.0` tag; `CLIENT_VERSION` 10003; `SVN_REV` 5030 |
| [`RadicalZephyr/BWEM-community`](https://github.com/RadicalZephyr/BWEM-community), fork of `N00byEdge/BWEM-community` | `third_party/bwem` | `70d7b34a2ae2043e1bf42e024461ef26dca6967c` | `9a63141f301f7830e9e09a2bae95c304fcc03cc5` | `master` at 2021-06-01, the community fork's last commit; MIT/X11 |

**Both repositories nest their sources one level down.** BWAPI's tree root is
`third_party/bwapi/bwapi/` (so the include root is `third_party/bwapi/bwapi/include` and the
client sources are `third_party/bwapi/bwapi/BWAPIClient/Source`), and BWEM's is
`third_party/bwem/BWEM/` (`include/` holds `map.h` and friends directly, `src/` the fourteen
translation units). Every path in the plan's §10.1 is relative to those inner roots.

**BWEM's own submodules are never fetched.** `external/googletest`, `external/openbw-bwapi`
and `external/openbw` (the unlicensed engine) are test-only; `.gitmodules` sets
`fetchRecurseSubmodules = false` on `third_party/bwem`, and no script in this tree passes
`--recursive` (plan §0, §10.1, Appendix B).

## Carried commits

What each `bwapi-c2-pin` branch adds on top of its upstream base. Each commit's message states
what it changes and why, and names the upstream issue once one is filed.

| Fork | Commit | Change | Plan |
|---|---|---|---|
| `bwapi` | `4b77d6e` | `bwapi/revisionUpdate.sh`: a POSIX port of upstream's `revisionUpdate.vbs` (`2383 + git rev-list HEAD --count` into `include/svnrev.h`) | §10.3 |
| `bwapi` | `1b54de4` | `bwapi/include/svnrev.h` committed, generated against the upstream base (`SVN_REV = 5030`). Upstream gitignores it; it is force-added | §10.3 |
| `bwapi` | `621ef61` | `BWAPIClient/Source/Convenience.h:33`, `va_list &ap` → `va_list ap` | §15.2 |
| `bwem` | `70d7b34` | adds `static void Map::ResetInstance()` to `include/map.h` and `src/map.cpp` | §15.2 |

`svnrev.h` living in the fork means there is no `vendor/svnrev.h` in this tree and no Windows
step at a pin bump: the shell port runs anywhere git does. `bwapi_revision()` reads `SVN_REV`
from the fork's `include/svnrev.h` through the ordinary include path.

## Moving a pin

Moving a pin is a deliberate act, and every check attaches to it (plan §10.3). There is no
scheduled drift job. The work happens in the fork first and in this repository second.

1. In the fork, fast-forward the default branch to the new upstream commit, then rebase
   `bwapi-c2-pin` onto it. A carried commit that no longer applies is the bump's first finding,
   not a surprise.
2. For BWAPI, regenerate `svnrev.h` **with the upstream commit checked out**, not the tip of
   `bwapi-c2-pin`: `revisionUpdate.sh` counts the commits reachable from `HEAD`, and the number
   must match what upstream's own build of the same commit reports (plan §10.3). Run
   `sh revisionUpdate.sh` from `bwapi/` at the upstream
   commit, then on `bwapi-c2-pin` replace the committed header with that output
   (`git add -f bwapi/include/svnrev.h`; upstream's `.gitignore` excludes it). Never synthesise
   it.
3. Push `bwapi-c2-pin` to the fork. Rewriting that branch is expected; this repository pins a
   commit, not the branch name, so old checkouts keep working.
4. In this repository, `git -C third_party/<dep> checkout <new tip>`, and update the tables
   above in the same commit.
5. Run the layout dump and the derived-closure test; diff both against the checked-in baselines.
6. Run `check_coverage.py`; resolve every added, removed or changed declaration.
7. Rebuild; run every suite in plan §11. Record the new revision and `CLIENT_VERSION` here.
