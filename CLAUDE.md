# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## ALWAYS commit incrementally

**Commit work as you go, never as one batch at the end.** Each commit must be one coherent
change to the repository: one research result filed, one section of the plan revised, one
script added with the doc that reports its result. If a task touches two unrelated things, that
is two commits. Before starting the next piece of work, the previous piece should already be
committed.

Commit messages follow the existing history: an imperative subject line naming the change
(`Add R11.6 results - synthetic terrain drives BWEM, and finds a crash`), then a body that says
what changed and why in prose. For plan revisions, the body walks the sections that moved.

## What this repository is

`bwapi-c2` is a flat C ABI over BWAPI (the C++ StarCraft: Brood War bot API) and BWEM (map
analysis), so languages with a C FFI can drive both without C++. **Phases 0 and 1 of the
implementation plan are done and phase 2 is under way: the generator exists and the ABI has 248
exports** (the ABI's own surface, `Player`, the static type data, the bulk tables, and the
events of 2.1). The repository holds the design
plan, the research that settled its premises, the license files, the two pinned submodules
under `third_party/` (see `docs/pins.md`; both nest their sources one directory down, and
BWEM's own submodules must never be fetched), the CMake build of the BWAPI+BWEM closure and
`bwapi_c2.dll`, the generator under `tools/abi/`, the generated headers, sources, `.def` and
`api.json`, the hand-written boundary under `src/`, the raw-layer generators under
`bindings/`, the test suite under `tests/`, the Zola site under `site/`, and two workflows.
Phases 2 to 4 add the read surface, the write surface with BWEM, and the consumers; do not
write ahead of the implementation plan's step order.

**The spec is the source of truth and the generated files are output.** `include/bwapi_c2.h`,
`include/bwapi_c2_types.h`, `src/*.gen.cpp`, `bwapi_c2.def` and `api.json` are written by
`tools/abi/regen.py` from `tools/abi/spec/*.yaml` and checked in; never edit them by hand. A
change to the ABI is a change to a spec file (`docs/spec-format.md`), then `regen.py`, then a
commit of both; CI fails on any diff (`tests/regen_check.sh`). `bwapi_c2_bwem.h` is still the
phase-0 skeleton until phase 3 generates it. The raw layers under `bindings/` are generated
from `api.json` by each binding's `gen.py` and are not committed.

## Layout

- `docs/c-abi-plan.md` — the plan, currently **revision 4.7**. ~2,000 lines, numbered sections
  (`§0` licensing through `§15` divergence register, plus Appendices A and B). It is the source
  of truth for every design decision. Cross-references throughout the repo use `§N` notation
  against this file.
- `docs/implementation-plan.md` — the phase-by-phase execution sequence: commit-sized steps,
  per-step checks, exit checklists, and a table of the judgment calls the plan leaves open. When
  starting work on a phase, begin here; when a step turns out wrong, edit it in place.
- `docs/pins.md` — the pinned commits, the commits our forks carry on top of upstream, and the
  pin-bump checklist. Update it in the same commit that moves a submodule.
- `docs/bwapi-c-abi-plan-revision-4-changes.md` — the directive set that produced revision 4.
  Historical; section numbers in it refer to revision 3.
- `docs/research/rN-<topic>.md` — results of research round N (R1–R11; R11 has sub-rounds
  R11.1–R11.8 as `r11-K-<topic>.md`, with `r11-bwem-research-plan.md` as the plan for them).
- `docs/research/rN/` — the scripts, fixtures and probe sources behind round N's results. Each
  runnable script has its usage line in its header comment.
- `docs/research/rev4-review.md` and `research-vs-rev4-review.md` — critical reviews whose
  findings have been folded into the plan. The decisions log in `§13` of the plan indexes them.

## How the plan and research relate

The workflow that produced everything here: a question is raised in the plan or a review, a
numbered research round answers it empirically (a measured surface, a script that links, a
fixture that runs), the result is filed as `docs/research/rN-*.md`, and the plan is revised in
place with a revision note at the top. Reviews get a `Status:` line once applied. When a
research result changes a decision, update the divergence register (`§15`) and the decisions
log (`§13`) in the plan, not just the research file.

Key settled conclusions to not re-litigate (each has its section and research round):

- The `§4` ABI conventions are the stable core; they survived every revision and hold for BWEM
  unchanged (R11.3).
- Forking `RnDome/bwapi-c` is foreclosed by its missing license; it is read-only reference (R1, `§3`).
- No mock server. Tests run BWAPI's real client `GameImpl` and BWEM's full analysis over a
  synthetic `GameData` on Linux, with no StarCraft, no MPQs and no Windows (R7, R11.6, `§11`).
- x64 client mode is settled, 32-bit Linux is a non-goal, Linux via OpenBW is parked (R5, R6, `§10.2`, Appendix B).
- SWIG's C target was tried and rejected; clang's AST dump drafts the generator spec (R8, `§9`).
- Python and C# are the primary consumers; Rust is the proof-of-concept only (`§7`).
- License is LGPL-3.0-only because the DLL embeds BWAPI's object code (R9, `§0`).

## Building and testing

Set up the checkout as the README's "Working on bwapi-c2" says: `git submodule update --init
--depth 1` (never `--recursive`). The submodules point at our forks, whose `bwapi-c2-pin`
branches already carry the `§15.2` fixes and `svnrev.h` (`docs/pins.md`); nothing is patched
into a working tree and `git status` stays clean. Then:

```sh
CXX=clang++ cmake -B build -G Ninja      # clang only on Linux; g++ is refused at configure (§10.1)
cmake --build build
ctest --test-dir build --output-on-failure
```

- `cmake/closure.cmake` names every translation unit of the closure; never glob.
- `tests/` is one directory per row of `§11`'s table: `header_hygiene`, `layout_dump`,
  `derive_closure`, `exports`, `fixture` (the shared synthetic-`GameData` builder every suite
  uses), `read_write`, `errors`, `bwem`, plus `types_test.cpp` (the static type data, no game)
  and `regen_check.sh` (a CI step). New tests use `bwapi_c2_add_test()` and the `Fixture`
  builder; do not build a `GameData` by hand. The exports are tested through the C boundary
  (`read_write/player.cpp` is the model), with the ABI linked as objects beside the fixture. The one exception is `tests/bwem/overlap_asserts.cpp`: it
  must die inside BWEM's `Neutral::PutOnTiles()` to prove the assertions are live, so it cannot
  be a doctest runner and plants its overlapping neutrals past the builder on purpose. Do not
  copy its shape for tests that can use the helper.
- `tests/layout_dump/baseline.json` changes only at a pin bump, via the `layout_dump_update`
  target.
- `-DBWAPI_C2_SANITIZERS=address,undefined` is CI's second Linux configuration. Before trusting a
  green sanitized run, `tools/test-sanitizers.sh <build-dir>` proves the sanitizers are live in
  that build: every object instrumented, both runtimes linked into every image, and a
  use-after-free, a signed overflow and a leak each diagnosed.
- The generator: `tools/abi/draft_spec.py` drafts a spec from clang's AST (never the source of
  truth; drafts are gitignored), `tools/abi/regen.py` runs the five emitters, and
  `tools/abi/audit.sh` runs the coverage audit against `tools/abi/backlog.txt`, off the merge
  path (needs `pip install libclang` at clang's major, and PyYAML for the emitters). The audit's
  numbers are recorded in `docs/pins.md` at each pin.
- The site: `cd site && zola check && zola build`, with Zola at the version pinned in
  `.github/workflows/docs.yml`. Reference pages under `site/content/reference/` are generated
  and gitignored.

## Running the research experiments

The R1–R11.8 experiment scripts under `docs/research/rN/` predate the submodules and expect
sibling checkouts that are **not** in this repo:

- `bwapi/bwapi` (the upstream BWAPI tree) — passed as the first argument, or found at
  `../../../bwapi` relative to the script. `third_party/bwapi/bwapi` is the same tree and works.
- `N00byEdge/BWEM-community` — passed as the second argument to the R11 scripts.

R11.9's `docs/research/r11/run-bwem-teardown.sh` is the first to build from the submodules
directly and takes no arguments.

They need `clang++` and `g++` on Linux, build into a `mktemp -d` scratch directory, and clean
up after themselves. Examples:

```sh
# R6: derive BWAPI's client-mode link closure and link upstream's ExampleAIClient
docs/research/r6/derive-closure.sh /path/to/bwapi/bwapi

# R7: drive the real client against a synthetic GameData
docs/research/r7/run-fixture-harness.sh /path/to/bwapi/bwapi

# R11.6: BWEM full analysis on synthetic terrain (flags: --exit-clean | --reinit)
docs/research/r11/run-bwem-fixture.sh /path/to/bwapi/bwapi /path/to/BWEM-community/BWEM

# R5: regenerate the GameData layout matrix across MSVC/GNU x86/x64 targets (clang only, no linking)
docs/research/r5/run-layout-dump.sh
```

The R6 `shim/` (a fake `Windows.h`) and `patched/Convenience.h` (a `va_list` by-value fix) are
shared by the later rounds; they are what makes the closure compile on Linux and are the
origin of the `va_list` fix our BWAPI fork carries (`§15.2`).

## Writing conventions for the docs

Match the existing voice: direct, measured, numbers stated rather than hand-waved, decisions
stated as decisions. Tables for comparisons and registers. Bold the headline of a paragraph
when it carries the finding. Reference sections as `§N.M` and research as `RN` or `RN.K`. A
plan revision gets a blockquote revision note under the Purpose section summarising what moved
and pointing at the research that moved it.
