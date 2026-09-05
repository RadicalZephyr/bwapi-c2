# R9. Licensing verification

Read the actual license files in every repository this project would touch, and checked §0's
three assertions against the operative clauses.

**Not legal advice.** This is an engineering read of primary texts, done to find the places where
the plan is wrong or silent. Two items below (§4, §6) are genuinely questions for a lawyer and
are marked as such.

**Headline: §0's three claims are broadly right, but it is silent on the one that actually binds
us. `bwapi_c.dll` does not merely link BWAPI — R6 established it compiles BWAPI's own source into
itself, so the DLL contains the Library and must be conveyed under LGPL-3.0 with a Corresponding
Source offer. Separately, the README wording §0 proposes ("the license does not propagate to bots
that merely call it") is imprecise enough to be wrong: it does not propagate to a bot's *code*,
but a distributed bot still owes LGPL §4(a)–(c). And two dependencies in the research have no
license at all: `RnDome/bwapi-c` (R1) and, newly, `OpenBW/openbw` itself.**

---

## 1. BWAPI's license: verified

| | |
|---|---|
| `LICENSE` | LGPL-3.0, 165 lines, verbatim FSF text |
| `LICENSE.md` | the same text, Doxygen-marked-up |
| `bwapi/COPYING` | **byte-identical to `LICENSE`** |
| BWAPI-specific exception or extra permission | **none** — the only "Exception" is the LGPL's own §1 |
| License statement in `README.md` | **none** |
| Per-file copyright headers in the R6 closure | **0 of 44** |
| Third-party notices in the tree | one: `Util/Sha1.{h,cpp}`, 3-clause BSD, Micael Hildenborg |

So §0's "LGPL-3.0, inherited from BWAPI (`LICENSE`, `LICENSE.md`)" is **confirmed**.

Three details worth carrying forward:

- **BWAPI does not ship the GPL-3.0 text.** All three license files are the LGPL-3.0 *additional
  permissions* document, which incorporates GPL-3.0 by reference but does not reproduce it. The
  installer (`Installer/Installer.iss:43,62`) ships only `LICENSE`. We must source GPL-3.0
  ourselves rather than copying it out of the dependency — §10.4 already says so, which makes the
  plan more complete than upstream.
- **There are no per-file notices to preserve.** Zero of the 44 closure translation units carry a
  copyright header. LGPL §4(a)/(c) and GPL §5 obligations to preserve notices therefore reduce to
  preserving the top-level license and stating that BWAPI is used.
- **The one third-party notice is outside our closure.** `Util/Sha1.*` is BSD-3, and R6 confirmed
  `Util/` is not in the client path. OpenBW's *server* build does compile it
  (`BWAPI/CMakeLists.txt:48`), so anyone shipping an OpenBW-based server inherits that notice; we
  do not.

Copyright is held by 41 git contributors, led by Adam Heinermann (1,177 commits), `lowerlogic`
(747) and `kovarex` (323). There is no CLA, no AUTHORS file, and no copyright assignment — so the
work is jointly held, which matters only if anyone ever wanted it relicensed. Nobody does.

---

## 2. The claim §0 misses: `bwapi_c.dll` contains the Library

§0 reasons about `bwapi_c.dll` as an Application linked against BWAPI. R6 showed that is not what
we are building. The closure compiles **`BWAPILIB/Source/*.cpp`, `Shared/*.cpp` and
`BWAPIClient/Source/*.cpp`** — BWAPI's own source — into a static archive that goes inside
`bwapi_c.dll`. The DLL therefore *contains* the Library's object code.

Consequences, from LGPL-3.0 §4 as it appears in `LICENSE`:

1. **`bwapi_c.dll` must be conveyed under LGPL-3.0** (or GPL-3.0 via §2). We cannot ship the
   binary under a permissive license, whatever we choose for our own source. Declaring the whole
   project LGPL-3.0, as §0 does, is the simple and correct answer — it just needs this reason
   attached rather than "inherited".
2. **We owe Corresponding Source for the BWAPI portion.** Satisfiable under GPL-3.0 §6(d) by
   offering access from a network server — which is exactly §10.4's "a link to the source at the
   exact tagged commit". That provision is doing more work than the plan realises and should not
   be dropped as boilerplate.
3. **Our own ABI source is an Application** under LGPL §0 ("any work that makes use of an
   interface provided by the Library"). The combined binary is the Combined Work. Shipping both
   under LGPL-3.0 collapses the question.

**Recommended edit to §0:** replace "inherited from BWAPI" with a sentence saying the DLL embeds
BWAPI's compiled source, so the binary is conveyed under LGPL-3.0 and the release must carry a
Corresponding Source pointer to the pinned commit.

---

## 3. §0 claim 1 — dynamic-only consumption: correct, and under-argued

§0: *"`bwapi_c.dll` must be consumed dynamically, never statically linked into a consumer… the
user must be able to relink against a modified `bwapi_c.dll`."*

**Confirmed.** LGPL §4(d) gives a downstream bot author exactly two options:

- **4(d)(0)** — ship Minimal Corresponding Source *and* the application in a form that permits the
  user to relink it against a modified Library. In practice: publish object files.
- **4(d)(1)** — "Use a suitable shared library mechanism", defined as one that uses a copy of the
  Library already present at run time and works with an interface-compatible modified version.

Loading `bwapi_c.dll` at runtime is 4(d)(1). A closed-source bot that statically linked a
`bwapi_c.a` would be forced into 4(d)(0). So the design rule is right and the reason is right.

**But §0's README wording is wrong as stated.** It proposes telling users that "the ABI's license
does not propagate to bots that merely call it." That is true of the bot's *source code* and
false of the bot's *distribution*. §4(a)–(c) still bind anyone who conveys a bot together with
`bwapi_c.dll`:

- **4(a)** prominent notice that the Library is used and is covered by the LGPL;
- **4(b)** a copy of **both** the GNU GPL and the LGPL alongside the work;
- **4(c)** if the work displays copyright notices at runtime, include the Library's among them.

For a tournament submission — a zip with a bot binary and `bwapi_c.dll` in it — 4(a) and 4(b) are
live obligations. **Recommended:** ship a `NOTICE` snippet in the release asset that a bot author
can copy verbatim, and make the README say "your bot's own code stays yours; your *distribution*
must carry these two files and this notice." That is both accurate and more useful than a
reassurance.

---

## 4. §0 claim 2 — the static CRT: right conclusion, wrong mechanism

§0: *"The static CRT (`/MT`) is fine. LGPL-3.0's System Libraries exception covers the MSVC
runtime."*

The definition lives in **GPL-3.0 §1**, which the LGPL incorporates (and which, per §1, BWAPI
does not ship). It has two prongs: a System Library is included in the normal packaging of a
Major Component but is not part of it, and either serves only to enable use with that Major
Component **or** implements a Standard Interface with a publicly available source implementation.
GPL-3.0 §1 names "a compiler used to produce the work" as a Major Component.

The MSVC CRT satisfies both prongs on a straightforward reading: it ships with Visual Studio, and
it implements the C standard library, for which public source implementations exist. So the
conclusion holds.

**The mechanism is misdescribed, and the distinction matters.** The System Libraries carve-out
governs what must be included in **Corresponding Source** — it means we need not ship the CRT's
source. It is **not a permission to statically link**. That permission comes from Microsoft's
Visual Studio distributable-code terms, which are a separate contract and not affected by the
LGPL at all. Both must hold independently.

**Two things to do**, neither large:

- Restate §0 claim 2 as: *the LGPL does not require us to ship CRT source (GPL-3.0 §1 System
  Libraries); separately, Microsoft's VS distributable-code terms permit redistributing the
  statically linked CRT — verify against the terms for the specific VS edition used to build the
  release.*
- **Flag for a lawyer:** whether static-CRT redistribution is permitted under the VS edition
  actually used (Community vs Professional terms differ), and whether LGPL §4's
  "effectively do not restrict… reverse engineering for debugging such modifications" is in
  tension with the VS terms' reverse-engineering clause. This is the standard LGPL-on-Windows
  question and it has a standard answer, but it is not mine to give.

---

## 5. §0 claim 3 — release contents: correct, and stricter than upstream

§0: *"Every release asset ships `COPYING.LESSER`, `COPYING`, and a link to the source at the exact
tagged commit."*

**Confirmed correct**, and it is precisely what LGPL §4(b) requires (both texts) plus GPL §6(d)
(network-server source offer). Upstream BWAPI's own installer ships only the LGPL text, so we
cannot lift the file set from the dependency — the GPL-3.0 text must be added deliberately.

Minimum contents of a release asset, derived from the clauses rather than assumed:

| File | Clause | Note |
|---|---|---|
| `COPYING` (GPL-3.0) | LGPL §4(b) | **not present in BWAPI**; fetch from gnu.org |
| `COPYING.LESSER` (LGPL-3.0) | LGPL §4(b) | copy of BWAPI's `LICENSE` |
| `NOTICE` | LGPL §4(a) | states BWAPI is used and LGPL-covered; reusable by bot authors (§3) |
| source pointer | GPL §6(d) | URL + exact pinned BWAPI commit **and** our own tagged commit |
| `README` license section | — | records BWAPI revision and `CLIENT_VERSION` (§10.4 already) |

The crates.io and npm packages carry the same set. Note that crates.io and npm both expect an
SPDX `license` field: `LGPL-3.0-or-later` or `LGPL-3.0-only` — **decide which**, because BWAPI's
`LICENSE` does not state an "or later" preference and the LGPL's own §? default applies
("you may choose any version… ever published" only when no version is specified — here version 3
*is* specified). `LGPL-3.0-only` is the defensible reading. R10 should carry this into the
package metadata.

---

## 6. The unlicensed dependencies: two, not one

R1 found that `RnDome/bwapi-c` has no license — no file, no header, no README statement, nothing
in 148 commits, and GitHub reports `license: null`. That foreclosed direction A.

**R7's substrate has the same problem.** `OpenBW/openbw` — the engine — has:

- no `LICENSE`, `COPYING` or equivalent file;
- no license statement in `README.md`;
- **zero copyright headers** in any of its source files;
- GitHub API: `license = NONE`.

Only `OpenBW/bwapi` (the BWAPI fork) is licensed, LGPL-3.0, with a `LICENSE` byte-identical to
upstream's. The engine it depends on is not.

That is a live problem for R7's recommendation, and it is a second reason — beyond the MPQ
problem — not to build on OpenBW. It also means:

- **`basil-ladder/openbw`** (R6 §13's required pairing) inherits the same absence.
- **`awest813/OpenSnowstorm---Brood-War`** (R6 §12) **added an LGPL-3.0 `LICENSE` on 2026-03-01**
  (commit `e2e90a6`) to a codebase whose upstream has none. A fork cannot license code it does not
  own. *(This corrects R6 §12, which described that license as "inherited"; it is asserted.)*

**Recommendation:** treat OpenBW as read-only reference, exactly as R1 recommended for bwapi-c. If
OpenBW ever becomes a build target, the missing license is a precondition to resolve with the
authors first — which the research plan's no-outreach constraint forbids, so for now it is simply
a blocker.

Permissive and unproblematic: SDL2 and SDL2_mixer are both Zlib; Boost (BSL-1.0) is not in the
closure at all (R6 §3).

---

## 7. The Blizzard files: MPQs, maps, and the fixture question

**The MPQs are Blizzard's and cannot be redistributed.** R7 established that OpenBW hardcodes
`Patch_rt.mpq`, `BrooDat.mpq`, `StarDat.mpq`, and that the headless simulation reads eleven small
tables out of them. Eleven small files is not a licensing improvement — they are still Blizzard's
copyrighted game data. R7 also found the ecosystem's only public download URL is now a 404 and
that both JBWAPI e2e workflows were disabled in 2022. Nothing here changes that: **no CI we
operate can obtain them.**

R7 handed R9 an open question: **are recorded `GameData` frame-0 fixtures redistributable?**
Reading `GameData.h` field by field gives a sharper answer than "unclear".

**`GameData` contains no Blizzard static tables.** No unit stats, no weapon damage, no iscript —
those live in `BWAPILIB/Source/UnitType.cpp` et al., which is BWAPI's own LGPL transcription. What
a frame-0 buffer does contain:

- session state — frame count, players, `units[10000]`, bullets, commands. Facts about a game.
- **map terrain**: `getGroundHeight[256][256]`, `isWalkable[1024][1024]`, `isBuildable[256][256]`,
  `hasCreep`, `isOccupied`, and `regions[5000]` (BW's pathfinding mesh). This is derived from the
  `.scx`/`.scm` map file.
- `mapName[33]`, `mapFileName[261]`, `mapHash[41]`.

So the exposure is **the map, not the game**. And JBWAPI's fifteen fixtures are all
community-made ladder maps — Benzene, Destination, Fighting Spirit, Python, Andromeda, Circuit
Breaker, La Mancha, Tau Cross, Neo Moon Glaive, Roadrunner, Electric Circuit, Heartbreak Ridge,
Empire of the Sun — whose rights sit with individual mapmakers, generally without any stated
license. JBWAPI ships them inside an MIT repository; whether that is sound is exactly the
question, and I have not resolved it.

**The mitigation makes the question moot, and R7 already built it.** The
[`r7/fixture_harness.cpp`](r7/fixture_harness.cpp) demonstration used a **fully synthetic**
`GameData` — `calloc` plus a few dozen hand-set fields — and successfully exercised the read path,
BWAPI's real rule engine, and the command write path. Synthetic fixtures have **no provenance
problem at all**: no map, no Blizzard data, nothing derived from anything.

**Recommendation:**

1. **Default to synthetic fixtures**, checked in, as the CI substrate. Proven working in R7.
2. **Recorded fixtures are optional and contributor-local** — a `GameStateDumper` equivalent that
   a developer runs against their own StarCraft copy, output `.gitignore`d. Costs a little realism
   in CI, costs nothing legally.
3. **Do not vendor JBWAPI's `.bin` fixtures**, however convenient. Different project, unresolved
   provenance, and we would be adopting their risk without their reasons.

One related note carried from R3: `ceverettkoop/oscar_c` vendored `RnDome/bwapi-c`'s **unlicensed**
headers and prebuilt `BWAPIC.dll` into an MIT-licensed repository. It is a small hobby project and
nobody is going to litigate it, but it is R1's licensing gap already materialising downstream, and
it is the pattern our own release notes should help people avoid.

---

## 8. Answers to the questions as asked

**Confirm BWAPI's license version and any exceptions.** LGPL-3.0, verbatim, in three identical
copies (`LICENSE`, `LICENSE.md`, `bwapi/COPYING`). **No BWAPI-specific exception.** The GPL-3.0
text that the LGPL incorporates is **not** shipped. No per-file copyright headers anywhere in the
closure. One third-party notice (`Util/Sha1`, BSD-3), outside the closure.

**Read `RnDome/bwapi-c`'s license.** There is none (R1). Fork compatibility fails at the
precondition.

**Read OpenBW's license.** `OpenBW/bwapi` is LGPL-3.0. **`OpenBW/openbw`, the engine, has no
license at all** — no file, no README statement, no headers, `license: NONE`. This is new and it
is a second, independent reason not to adopt OpenBW as a build target.

**Does the System Libraries exception cover the MSVC static CRT?** On a straightforward reading of
GPL-3.0 §1, yes — but the carve-out governs Corresponding Source obligations, not permission to
link. Microsoft's VS distributable-code terms govern that separately and must be checked against
the specific edition. Flagged for a lawyer.

**What must a release asset contain?** `COPYING` (GPL-3.0, sourced ourselves), `COPYING.LESSER`
(LGPL-3.0), a `NOTICE` satisfying §4(a) that bot authors can reuse, and a network-server source
pointer to both the pinned BWAPI commit and our own tag. Plus an explicit SPDX identifier —
`LGPL-3.0-only` is the defensible choice.

**The MPQ problem.** Unchanged and unfixable: Blizzard's, non-redistributable, and the one public
source is gone. Constrains CI (R7) and any "just run the example bot" onboarding story. The
onboarding answer is the synthetic harness, not a game.

---

## 9. Recommended edits to §0

1. **Add the missing premise.** `bwapi_c.dll` embeds BWAPI's compiled source (R6), so the binary conveys the Library and must be LGPL-3.0 with a Corresponding Source offer. Say that instead of "inherited".
2. **Keep claim 1, fix the README wording.** Dynamic-only consumption is right. But a distributed bot still owes LGPL §4(a)–(c): notice plus both license texts. Ship a reusable `NOTICE` snippet and say "your code stays yours; your distribution carries these files."
3. **Restate claim 2's mechanism.** The System Libraries carve-out removes a *source-shipping* obligation; Microsoft's terms are what permit static CRT redistribution. Verify the latter for the VS edition used.
4. **Extend claim 3 with the file table in §5**, and note that the GPL-3.0 text is absent from BWAPI and must be sourced.
5. **Pick an SPDX identifier now** — `LGPL-3.0-only` — and carry it into R10's package metadata.
6. **Add a fourth consequence: test data.** Fixtures are synthetic by policy; recorded buffers are contributor-local and gitignored; JBWAPI's fixtures are not vendored.
7. **Add a non-goal or caveat about OpenBW's missing license**, alongside R7's MPQ finding.
