# Implementation plan: a launcher for the BWAPI fork

> **Status: draft. Nothing is implemented.** This plan executes
> [ADR 0002](adr/0002-launch-and-inject.md): add a standalone launcher to the fork, and remove the
> Chaoslauncher and MPQDraft integrations rather than porting them. It stacks on
> [the defect series](bwapi-fork-defect-plan.md) (PR 2) for the reason in [§0](#0-where-the-work-happens)
> — stage 4 deletes plugin entry points whose architectural justification that series removed.
>
> **Two facts are unverified and gate stages 1 and 3** ([ADR 0002 §5](adr/0002-launch-and-inject.md#5-honest-bounds)).
> Both are cheap and both are the first thing their stage does.

## Scope

**In.** A `BWAPILauncher.exe` that starts `StarCraft.exe` suspended, injects `BWAPI.dll`, can
point an instance at its own install directory, and resumes. Removal of the Chaoslauncher and
MPQDraft plugin entry points, the third-party binaries the installer ships, and the README path
that depends on them.

**Out.** Game setup (host/join, map, race, name, network provider) — [ADR 0002 §4](adr/0002-launch-and-inject.md#4-what-this-decides-and-what-it-does-not).
True headless operation; the target is Wine with a virtual display, per ADR 0001 §3. Anything in
`BW/Offsets.h`. Any Pascal.

## 0. Where the work happens

Two repositories, one branch name, and the docs land first.

| Repo | Branch | Carries |
|---|---|---|
| `bwapi-c2` | `claude/chaoslauncher-bwapi-port-s66nye` off `claude/bwapi-defect-fixes-gvul5m` | This plan and ADR 0002 |
| `bwapi` | `claude/chaoslauncher-bwapi-port-s66nye` off `claude/bwapi-defect-fixes-gvul5m` | Stages 1–5 |
| `Chaoslauncher` | — | **Nothing. It is a reference, and after this it is not even that** |

The fork branch stacks on the defect series rather than on `main` because stage 4's deletions only
make sense downstream of `Stop the game process from loading bots`. If PR 2 is rejected, stages
1–3 still stand alone and stage 4 becomes a separate argument.

## 1. The steps

### Stage 1 — prove the injector detaches from its host

**The experiment first.** Build the existing `BWAPI_PluginInjector` sources into a throwaway
console `.exe` that does nothing but `CreateProcess(CREATE_SUSPENDED)` on a trivial target,
call `ApplyPatchSuspended(pi.hProcess, 0)`, and `ResumeThread`. If the injected DLL's `DllMain`
runs, the 44 lines are host-independent and every later stage is a packaging exercise. If it does
not, this plan is wrong on its central claim and stops here for redesign.

Then, and only then:

- New `BWAPILauncher` vcxproj, console subsystem, x86, matching the toolset PR 3 retargeted to.
- Move `valloc.{h,cpp}`, `remotethread.{h,cpp}`, `common.{h,cpp}` and `config.{h,cpp}` to a place
  both the launcher and (for now) the plugin can compile against. **Move, not copy** — two
  divergent copies of the injection sequence is the failure mode this stage exists to avoid.
- Extract the body of `ApplyPatchSuspended` into `bool InjectInto(HANDLE hProcess, const std::string& dllPath)`.
  `chaoslauncher.cpp`'s export becomes a one-line forward, so the plugin keeps working through
  stages 1–3 and the deletion in stage 4 is clean.

**Exit:** the launcher injects `BWAPI.dll` into a real 1.16.1 and a bot connects, with the plugin
still working unchanged.

### Stage 2 — the command line

Small and deliberately boring:

```
BWAPILauncher.exe --exe PATH [--dll PATH]... [--installpath PATH] [--wait] [--] [args...]
```

- `--exe` the target. No default; the runner is explicit or it gets an error.
- `--dll` repeatable, defaults to `BWAPI.dll` resolved as `config.cpp` already resolves it.
- `--installpath` stage 3.
- `--wait` block until the game exits and forward its exit code, so a runner can treat one match
  as one process.
- Everything after `--` is passed to `StarCraft.exe`.

Exit code and `stderr` discipline: a runner has to distinguish "launcher failed" from "game
failed". Nonzero-and-message on every `BWAPIError` path; the game's own code only under `--wait`.

**Exit:** `--help` documented in the README, and the failure paths return distinguishable codes.

### Stage 3 — multiple instances

**The experiment first**, and it is the one with a real chance of going the other way. Launch two
instances from two directories with `CreateProcess` and no patching, and see whether the second
one comes up. Three outcomes:

1. **They both run.** `--installpath` is a `CreateRemoteThread`-time environment or registry
   redirect and the stage is small.
2. **The second is refused by the mutex.** Then take the six bytes at `$004DFFF0` from
   `Experiments/MultipleInstance/MultipleInstance.dpr` — with attribution, as an absolute address
   guarded by the same version check the fork's other patches use, and with a comment saying it is
   1.16.1-only. This is the **one** thing this plan may take from Chaoslauncher, and taking it is
   a fallback, not the goal.
3. **Something else.** Write it down before coding around it.

`--installpath` itself: BWAPI reads `InstallPath` from `HKCU\Software\Blizzard Entertainment\Starcraft`
to find `bwapi-data/`. `bwheadless` hooks the read (`main.cpp:786-807`). The cheaper option to try
first is setting it per-process before resume; the hook is the fallback.

**Exit:** two instances, two `bwapi-data` directories, two bots, one machine.

### Stage 4 — remove Chaoslauncher and MPQDraft

Only after 1–3 are green, and this is the stage that delivers the simplification the whole exercise
is for:

- Delete `chaoslauncher.cpp/.h`, `mpqdraft.cpp/.h`, `QDPlugin.{h,def}`, and the `.bwl`/`.qdp`
  post-build copies (`BWAPI_PluginInjector.vcxproj:109-110,186-187`). The project either goes away
  or becomes the launcher.
- Delete `Release_Binary/Chaoslauncher/` (13 third-party binaries) and `Release_Binary/MPQdraft/`.
- `Installer/Installer.iss`: drop the two `Source:` lines (47–48), the two `Components` (107–108),
  and the two `postinstall` `Filename:` entries (111–112).
- Rewrite `README.md:44-66` and `Release_Binary/README` around the launcher. This is the
  user-visible half of the change and it is the half most likely to be done carelessly; the new
  text should be a command a reader can paste.

**Exit:** `grep -ri chaoslauncher` over the fork returns nothing but changelog.

### Stage 5 — tests

Following the defect series' pattern: host-buildable where possible, Windows-gated where not.

- **Argument parsing** — a dependency-free `LauncherArgs` header with a Linux-buildable test, the
  way `ClientInput.h` was done. Malformed input, missing values, repeated `--dll`, `--` handling.
- **Injection** — Windows-only, gated. A tiny target `.exe` and a marker DLL that writes a file
  from `DllMain`; assert the launcher injected it and that a bad path fails with the right code.
- **No regression in what BWAPI writes.** PR 3's tier-2 shadow-StarCraft harness compares the
  linked DLL's patching against upstream v4.4.0. Stage 1 moves files between translation units,
  which should move nothing — that harness is how we know, and it already caught one defect that
  review missed.

**Exit:** green on Linux for the parse tests; green on Windows for the rest; tier 2 unchanged.

## 2. TODO

Ordered. Each line is a commit-sized step; `[!]` marks the two that can invalidate the plan.

**Stage 1 — detach the injector**
- [ ] `[!]` Experiment: call `ApplyPatchSuspended` from a console `.exe`; confirm `DllMain` runs
- [ ] Add `BWAPILauncher.vcxproj`, console, x86, PR 3's toolset; add to `bwapi.sln` and CI
- [ ] Move `valloc`/`remotethread`/`common`/`config` to shared compilation; no second copy
- [ ] Extract `InjectInto(HANDLE, const std::string&)`; make the `.bwl` export forward to it
- [ ] End-to-end: launcher → 1.16.1 → bot connects, plugin still working

**Stage 2 — command line**
- [ ] `LauncherArgs` parser, no Windows dependencies
- [ ] Wire `--exe`, `--dll`, `--wait`, `--`; distinguishable exit codes and `stderr`
- [ ] `--help`

**Stage 3 — multiple instances**
- [ ] `[!]` Experiment: two `CreateProcess` instances, two directories, unpatched — does the second run?
- [ ] `--installpath`: per-process registry value first; hook only if that fails
- [ ] *Conditional on the experiment:* the six-byte mutex patch, attributed and version-gated
- [ ] End-to-end: two instances, two bots, one machine

**Stage 4 — removal**
- [ ] Delete the Chaoslauncher and MPQDraft plugin sources and post-build copies
- [ ] Delete `Release_Binary/Chaoslauncher/` and `Release_Binary/MPQdraft/`
- [ ] Installer: remove both components, sources and postinstall entries
- [ ] Rewrite `README.md` and `Release_Binary/README` around the launcher
- [ ] `grep -ri chaoslauncher` is clean

**Stage 5 — tests**
- [ ] `launcher_args_test` on Linux, g++ and clang++, ASan/UBSan
- [ ] Windows-gated injection test with a marker DLL
- [ ] Re-run PR 3 tier 1 and tier 2; confirm the file moves changed nothing

## 3. Where this stops short

**It does not make the fork headless.** A virtual display is assumed. If a Windows-native
tournament runner without a display ever becomes a requirement, that is `bwheadless`'s 3,800 lines
and it is a different project — ADR 0002 §3.

**It does not drive a match.** Something still has to click through the menus. sc-docker does it
with autoclicking in `play_common.sh`; the referee ADR 0001 §4 defers is where that properly
lives. The launcher deliberately does not prejudge it.

**It does not verify on Windows from this environment.** Every `[!]` and both end-to-end exits
need a machine with retail 1.16.1. Until those run, stages 1 and 3 are proposals, and the plan
says so rather than discovering it in review.
