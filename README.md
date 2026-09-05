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

**Status: phase 0 (bootstrap) of the implementation plan is in progress.** There is no
buildable code yet. BWAPI and BWEM are pinned as submodules under `third_party/`; the commits,
the patches carried on them and the pin-bump procedure are in [docs/pins.md](docs/pins.md).

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
