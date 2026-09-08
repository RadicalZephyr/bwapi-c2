# ADR 0002. Ship a launcher in BWAPI; do not port Chaoslauncher

> **Status: Draft.** Supersedes nothing. Extends
> [ADR 0001](0001-fork-and-invert.md) §3, which said the injection layer is inherited rather than
> rewritten, and settles a question that ADR left implicit: *who starts StarCraft.* The
> implementation plan is [`bwapi-launcher-plan.md`](../bwapi-launcher-plan.md). Two facts in §5
> are unverified on Windows and are marked; both are cheap experiments and neither changes the
> decision.

## Context

The opening proposal was to port the parts of Chaoslauncher that matter for BWAPI into the BWAPI
fork, so that the tournament runner setup gets simpler. Chaoslauncher is a Delphi 7 GUI
application, unmaintained since 2011, undocumented, and it is what BWAPI's own README still tells
a new bot author to install and click.

The proposal rests on three premises, and **all three are wrong in the same direction: the work is
already done, and Chaoslauncher is already gone.**

---

## Decision

> **Add a small standalone launcher to the BWAPI fork. Port nothing from Chaoslauncher, and
> delete the Chaoslauncher and MPQDraft integrations rather than replacing them.**
>
> The launcher starts `StarCraft.exe` suspended, injects `BWAPI.dll` by the same
> `VirtualAllocEx` + `CreateRemoteThread(LoadLibraryA)` sequence BWAPI already ships, optionally
> overrides the `InstallPath` registry read so instances run from separate directories, and
> resumes. Game setup — map, race, host/join — stays the runner's job. The reference for the
> mechanism is `bwheadless`, which is what the ecosystem actually uses; the reference is read,
> not vendored.

---

## 1. The three premises

### P1. "Chaoslauncher is the standard for tournament runners." — False since 2017

| Runner | What it uses | Chaoslauncher? |
|---|---|---|
| [StarcraftAITournamentManager](https://github.com/davechurchill/StarcraftAITournamentManager) (AIIDE) | `injectory` | Dropped Aug 2017: *"ChaosLauncher is no longer used for injecting BWAPI; injectory is used instead."* |
| [sc-docker](https://github.com/basil-ladder/sc-docker) (BASIL, and the SSCAIT lineage) | `bwheadless.exe` | **Zero references in the tree.** `docker/scripts/launch_game` is one command |
| [BWAIShotgun](https://github.com/Bytekeeper/BWAIShotgun) | a `bwheadless` fork | No |
| [DropLauncher](https://github.com/adakitesystems/DropLauncher) | its own injector | Advertises *not* needing it |
| **BWAPI's own README** | Chaoslauncher | **Yes — and this is the only place it survives** |

sc-docker's entire launch is:

```bash
bwheadless.exe -l "$BWAPI_DATA_DIR/BWAPI.dll" -e "$SC_DIR/StarCraft.exe" --installpath "$SC_DIR"
```

**The premise is inverted.** Chaoslauncher is not the tournament standard that the fork should
absorb; it is a developer-onboarding path that the tournament world abandoned nine years ago and
that only BWAPI's documentation still holds open. Porting it forward would make the fork the last
consumer of a dependency everyone else has already dropped.

### P2. "Does BWAPI already ship an injector?" — Yes, and it is 44 lines

`bwapi/BWAPI_PluginInjector/` is 345 lines total. The injection itself is
`chaoslauncher.cpp:50-93`, `ApplyPatchSuspended`: get the target from `bwapi.ini`, `VirtualAllocEx`
a `MAX_PATH` buffer, `WriteProcessMemory` the DLL path into it, `CreateRemoteThread` on
`LoadLibraryA`, wait, check the exit code. `valloc.cpp` (30 lines) and `remotethread.cpp` (27
lines) are RAII wrappers over exactly that. It is correct, it is complete, and **it is the only
part of any of this that is hard to get right.**

What it is not is a program. `main.cpp` is eight lines of `DllMain` returning `TRUE`. The tree
builds one DLL and copies it to two names (`BWAPI_PluginInjector.vcxproj:109-110`):
`.bwl` for Chaoslauncher, `.qdp` for MPQDraft. Both are *plugin* entry points, and a plugin
cannot start a process. Chaoslauncher's contribution to the arrangement is `CreateProcess` with
`CREATE_SUSPENDED`, a call to `ApplyPatchSuspended(hProcess)`, and `ResumeThread`.

**So the gap between what BWAPI has and what it needs is not a port. It is a `main()`.** The
launcher is a new front end over code already in the tree, and the delta is tens of lines, not a
Delphi translation.

### P3. "Port the important parts of Chaoslauncher." — There are none

Taking the parts one at a time:

| Chaoslauncher does | Worth porting? |
|---|---|
| Start SC suspended, inject, resume | The injection half is already in the fork (P2). The launch half is `CreateProcess` |
| Multi-instance | `Experiments/MultipleInstance/MultipleInstance.dpr` is **six bytes** written to absolute address `$004DFFF0` to jump over the mutex check. Version-locked to 1.16.1 and blind — and `bwheadless` gets the same result without patching code, by hooking the `InstallPath` registry read (`main.cpp:786-807`) so each instance resolves its own directory |
| W-Mode (windowed) | Not a tournament concern; and a rendering hack we would then own |
| Latency changer, APM alert, hack detector, replay tools, Chaosplugin, ICCup support, auto-updater | Not BWAPI's business at all |
| A plugin host that loads arbitrary `.bwl` DLLs into the game process | **See §2** |

The residue that is both wanted and not already present is: `CreateProcess(CREATE_SUSPENDED)`,
`ResumeThread`, and a way to point an instance at its own directory. None of it is Chaoslauncher's
insight, and none of it needs Pascal read.

---

## 2. The architectural objection, which is the real one

Chaoslauncher is a **plugin host**: its purpose is to load third-party DLLs into StarCraft's
address space, and its GUI is a checklist of things to inject. That is the same architecture
[ADR 0001 §2](0001-fork-and-invert.md) defect 2.4 indicted and the defect series deleted — the
commit is `Stop the game process from loading bots`, and its message ends *"A bot is a separate
process or it is nothing."*

**Porting a plugin loader into BWAPI would re-import, as a supported feature, the architecture the
previous pull request just removed.** The fork spent seventeen commits establishing that nothing
in `BWAPI.dll` calls `LoadLibrary` on bot-supplied code; adopting a launcher whose organising
idea is user-configurable in-process DLL loading gives that property back at the front door.

This is also why the launcher's scope is *inject and resume* and not *inject plugins*. The one
DLL it loads is `BWAPI.dll` — the trusted side — and the path comes from the runner's command
line, not from a user-editable plugin directory.

---

## 3. Why not just depend on bwheadless or injectory

Both were considered and both are reasonable; the case for a first-party launcher is narrow and
worth stating honestly, because **this decision is the closest call in the ADR.**

**`injectory`** is a general-purpose injector. It would work. It is an external binary the runner
has to acquire, version and trust, to do 44 lines the fork already contains. Depending on it to
call code we ship is the wrong shape.

**`bwheadless`** is the incumbent, it is CC0, and it is genuinely more capable: 4,087 lines that
stub graphics, sound and input; hook the game clock; drive host/join, map, race and network
provider; and hold `--headful` for when a human watches. Against adopting it: it is unmaintained
(`v0.1`), sc-docker vendors three different prebuilt copies of it (`bwheadless.exe`,
`bwheadless_lf3.exe`, `bwheadless_lf6.exe`) which is what an unversioned binary dependency looks
like in practice, and the bulk of its size is the headless rendering stubbing — **which this
project does not need, because ADR 0001 §3 already commits to Wine on Linux, where a virtual
display is free.**

So the split is: take the ~200 lines of bwheadless's design that overlap the need (suspended
launch, injection, `InstallPath` hooking), leave the ~3,800 that solve a problem Wine already
solves, and keep it in the tree where the fork's own tests can reach it. **Read as reference,
under CC0, with attribution — not vendored, and not forked.**

**The rejected fourth option is porting Chaoslauncher**, and it is rejected on all three counts at
once: it is the least capable of the four, it is the only one with no license file anywhere in its
tree (its readme says only *"USE AT YOUR OWN RISK"*, which is not a grant, and BWAPI is LGPL-3 —
so vendoring its Pascal is the one option with an actual legal question attached), and it is the
only one that carries the §2 architecture back in.

---

## 4. What this decides, and what it does not

**Decided.** A `BWAPILauncher` project in the fork; injection reuses the existing
`valloc`/`remotethread`/`config` code rather than duplicating it; the Chaoslauncher and MPQDraft
plugin entry points and the bundled third-party binaries come out of the tree, the installer and
the README together with it.

**Not decided, and deliberately.** Game setup — host/join, map, race, player name, network
provider — stays out. That is match policy, it belongs to the referee ADR 0001 §4 defers, and
putting it in the launcher now would prejudge it. The runner drives the menus, as sc-docker's
`play_common.sh` already does.

**Left open.** Whether the launcher eventually absorbs Storm/SNP wiring for LAN play. The fork
already ships `SNP_DirectIP`, so the pieces are present, but nothing today needs them joined.

---

## 5. Honest bounds

Two claims here rest on reading, not running, because this session has no Windows host and no
retail 1.16.1:

1. **That `ApplyPatchSuspended` works unchanged when called from an `.exe` rather than a `.bwl`.**
   It takes `HANDLE hProcess` and touches no Chaoslauncher state, so it should; the plan's stage 1
   proves it before anything is built on it.
2. **That overriding `InstallPath` is sufficient for multi-instance, without the six-byte mutex
   patch.** `bwheadless` relies on this and BASIL runs three concurrent games on it, which is good
   evidence, but it is evidence about `bwheadless`'s manual PE loading path, not necessarily about
   `CreateProcess`. **If it turns out `CreateProcess` still trips the mutex, the fallback is the
   six-byte patch after all** — the one piece of Chaoslauncher that would then be worth taking,
   and the plan's stage 3 is where that gets settled.

Neither changes the decision. Both change how much code stage 3 contains.
