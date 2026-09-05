# bwapi-c2

A C ABI for [BWAPI](https://github.com/bwapi/bwapi) and
[BWEM](https://github.com/N00byEdge/BWEM-community): one `bwapi_c2.dll`, two flat
`extern "C"` headers, no C++ toolchain required on the consuming side. Everything routes
through the real `BWAPI::Game` and `BWEM::Map` — this is a thin, mechanical layer over the
real implementation, not a re-implementation of the shared-memory protocol. BWEM's function
here is map analysis — areas, chokepoints, bases and paths — exposed through `bwapi_c2_bwem.h`.

The design is [docs/c-abi-plan.md](docs/c-abi-plan.md). The research
it rests on is under [docs/research/](docs/research/). The detailed
implementation plan is
[docs/implementation-plan.md](docs/implementation-plan.md).

**Status: phase 0 (bootstrap) of the implementation plan is complete, pending its first CI
run.** The tree builds `bwapi_c2.dll` with one export, `bwapi_abi_version()`, over the full
BWAPI and BWEM closure; the three public headers carry the ABI's conventions and nothing else
yet; the test suite runs BWAPI's real client and BWEM's full analysis over a synthetic
`GameData` on Linux, with no StarCraft. Phase 1, the generator, is next. BWAPI and BWEM are
pinned as submodules under `third_party/`, each pointing at a fork that carries the few commits
we need on top of upstream; the commits, what they carry and the pin-bump procedure are in
[docs/pins.md](docs/pins.md).

## Working on bwapi-c2

Start with [docs/implementation-plan.md](docs/implementation-plan.md); it says what the next
step is and which plan section decides it. The toolchain is clang++ with CMake and Python 3 on
Linux for everything that is not the DLL itself, and MSVC on Windows for the DLL. Not g++: one
BWAPI header needs MSVC's template semantics, which only clang emulates, and configure says so.

```sh
CXX=clang++ cmake -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

The site under `site/` builds with Zola, pinned to the version and checksum in
`.github/workflows/docs.yml`; `zola check` there must pass before a docs change lands. The one
repository setting not in the tree is Pages' source, which must be "GitHub Actions".

**Fetch the submodules without `--recursive`.** BWEM's own submodules are googletest, OpenBW's
BWAPI fork and the unlicensed OpenBW engine; nothing in this tree may fetch them, and
`.gitmodules` cannot stop a recursive update on its own (`fetchRecurseSubmodules = false` only
governs `git fetch`). So:

```sh
git clone https://github.com/RadicalZephyr/bwapi-c2.git
cd bwapi-c2
git submodule update --init --depth 1     # NOT --recursive, and NOT git clone --recurse-submodules
```

That is the whole checkout. The submodules point at our forks of BWAPI and BWEM, whose
`bwapi-c2-pin` branches carry the two source fixes we need and BWAPI's generated `svnrev.h` as
ordinary commits on top of the upstream pins ([docs/pins.md](docs/pins.md)), so nothing is
patched into a working tree and `git status` stays clean. `--depth 1` keeps the BWAPI checkout
to a few hundred megabytes; the one job that needs full history is regenerating `svnrev.h` at a
pin bump, which happens in the fork, not here. Both repositories nest their sources one
directory down (`third_party/bwapi/bwapi/`, `third_party/bwem/BWEM/`).

## What a client-mode bot cannot do

`bwapi-c2` is client mode only: the bot is a separate process talking to BWAPI over shared
memory and a pipe. That buys crash isolation, x64, and a Linux-native test suite, and it
inherits one gap that is the server's, not this library's: **grouped commands are not
implemented by the BWAPI server**, so no client bot in any language can issue them. The
`canXxxGrouped` predicates are therefore not exported, and `bwapi_game_issue_command()` over an
id array is a loop of single-unit commands, not a group. Module mode, which would close the gap,
is a scoped v2 item (plan Appendix A).

## Names

Every artifact carrying a project identity uses `bwapi-c2` or `bwapi_c2`; the exported symbol
prefix stays `bwapi_` because it names the API being wrapped, not the project wrapping it
(plan §3).

| Thing | Name |
|---|---|
| Repository | `bwapi-c2` |
| CMake target | `BWAPI_C2` |
| Shared library | `bwapi_c2.dll` (`libbwapi_c2.so` if a Linux target lands) |
| Import lib / module def | `bwapi_c2.lib`, `bwapi_c2.def` |
| Public headers | `bwapi_c2.h`, `bwapi_c2_bwem.h`, `bwapi_c2_types.h` |
| Include guards | `BWAPI_C2_H`, `BWAPI_C2_BWEM_H`, `BWAPI_C2_TYPES_H` |
| Exported symbol prefix | `bwapi_` (BWAPI), `bwapi_bwem_` (BWEM) |
| Internal C++ namespace | `BWAPI::CApi` — never exported |
| Packages | crates.io `bwapi-c2-sys` / `bwapi-c2`; PyPI `bwapi-c2`; NuGet `BwapiC2` |
| Release asset | `bwapi-c2-<ver>-win32.zip`, `-win64.zip` |
| SPDX | `LGPL-3.0-only` |

Do **not** name anything `BWAPIC`: `namespace BWAPIC` already exists in BWAPI's client headers
for the shared-memory PODs.

## License

`bwapi-c2` is **LGPL-3.0-only**. `bwapi_c2.dll` contains BWAPI's object code (LGPL-3.0)
and BWEM's (MIT/X11, an attribution obligation), so it is conveyed as an LGPL Combined
Work. See `COPYING`, `COPYING.LESSER`, `LICENSE.BWEM` and `NOTICE`.

What that means for a bot author: **your bot's own code stays yours; your *distribution*
carries these files and this notice.** A tournament zip containing your bot and
`bwapi_c2.dll` is a distribution, and it must include `COPYING`, `COPYING.LESSER`,
`LICENSE.BWEM` and `NOTICE` (which contains a snippet to copy verbatim). Consume the DLL
dynamically; a static variant is deliberately not offered, so that closed-source bots stay
within LGPL §4(d)(1).
