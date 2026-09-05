# R11.7 — BWEM licensing and vendoring

**Not legal advice** — same posture as R9.

**Headline: BWEM's licensing is the cleanest of any dependency in this project. MIT/X11, one
copyright holder, a `LICENSE` file, and a consistent header on all 29 source files — in contrast
to BWAPI (zero per-file notices), `RnDome/bwapi-c` (no license) and `OpenBW/openbw` (no license).
Three things to record: GitHub misclassifies it as "Other", the X11 no-advertising clause is a
real if trivial obligation, and BWEM's unlicensed OpenBW dependency is test-only and we do not
take it.**

---

## 1. The license is MIT/X11, and it is properly applied

```
(MIT/X11 License)
Copyright (c) 2015, 2017, Igor Dimitrijevic
```

| Check | Result |
|---|---|
| `LICENSE` file present | Yes |
| Per-file headers | **29 of 29** source and header files, identical, naming the same holder |
| Files without a header | **none** |
| Contributors to the fork | 9 (N00byEdge 44, kant2002 27, Cmccrave 7, others 1–2) |
| Any contributor-added file with different terms | **none found** |

Every file carries:

> This file is part of the BWEM Library. BWEM is free software, licensed under the MIT/X11
> License. A copy of the license is provided with the library in the LICENSE file.

There is no CLA, so the fork's nine contributors are the usual inbound-equals-outbound situation
that every MIT project has. Nothing unusual, and nothing like the blocking problems R1 and R9
found in `bwapi-c` and `OpenBW/openbw`.

**Compatibility with LGPL-3.0 is not in question.** MIT is a permissive license explicitly
compatible with the GPL family; an LGPL-3.0 binary may contain MIT-licensed code. The reverse
would not hold, which is why the direction matters and is worth stating once: `bwapi_c2.dll` is
conveyed under LGPL-3.0 and *contains* MIT code, not the other way round.

---

## 2. Three things to record

**GitHub reports `spdx=NOASSERTION`, `name=Other`.** Not a problem — BWEM's `LICENSE` is a
reformatted MIT/X11 with the clauses as bullets, which GitHub's classifier does not match against
the canonical text. The file plainly identifies itself as MIT/X11. Recorded here so that a future
reader who runs the same API query does not conclude BWEM is unlicensed the way `OpenBW/openbw`
genuinely is.

**The X11 no-advertising clause is real.** Beyond standard MIT, BWEM's license adds:

> Except as contained in this notice, the name of the copyright holders shall not be used in
> advertising or otherwise to promote the sale, use or other dealings in this Software without
> prior written authorization from the copyright holders.

Practically trivial, but we will mention BWEM in the README and release notes. **Describing what
the library does is fine; using Igor Dimitrijevic's name to promote `bwapi-c2` is not.** Worth one
line in the contribution notes so nobody writes marketing copy around it.

**BWEM's own unlicensed dependency is test-only.** `.gitmodules` pulls three submodules:

| Submodule | License | Needed for |
|---|---|---|
| `external/googletest` | BSD-3 | `Tests/` only |
| `external/openbw-bwapi` | LGPL-3.0 | `Tests/` only |
| **`external/openbw`** | **none** (R9 §6) | `Tests/` only |

`BWEM/CMakeLists.txt` — the library target — globs `BWEM/src/*.cpp` and links `BWAPILIB`, nothing
else. The `external/` paths appear only in the **top-level** `CMakeLists.txt`, for the test
target. **We build `BWEM/` and never initialise those submodules**, so R9's unlicensed-OpenBW
blocker does not reach us through BWEM. Record it explicitly, because a naive `git submodule
update --init --recursive` on a pinned BWEM would pull an unlicensed repository into our tree for
no benefit.

---

## 3. Vendoring, and the patch we will carry

R11.4 recommended a **pinned submodule** rather than vendoring, for parity with how BWAPI comes
in. MIT permits either, so this is a maintenance preference and the licensing side is clear.

**R11.6 added a wrinkle: we need a one-line patch.** `BWEM-community` lacks `Map::ResetInstance()`,
and R11.6 showed that re-`Initialize` crashes on any map with neutrals. The fix is the one
Stardust's vendored variant already carries:

```cpp
void Map::ResetInstance() { m_gInstance = nullptr; }
```

MIT explicitly permits modification. The obligations are to retain the copyright notice and
permission text — which a patch on a submodule does automatically — and, as a matter of good
practice rather than license, to say what we changed.

**Recommendation:**

1. Pin `N00byEdge/BWEM-community` as a submodule at `third_party/bwem`.
2. Carry the `ResetInstance` addition as a patch applied at build time, or as a pinned fork branch
   if the patch grows. **Record it as the first entry in §15's divergence register**, which is
   exactly what that register is for.
3. Offer the patch upstream. Cost is one PR; the upstream is dormant (last commit 2021-06-01) so
   expect no response, and do not gate on it.
4. **Do not initialise BWEM's submodules.**

---

## 4. Release-asset obligations

Extending R9 §5's table:

| File | Clause | Note |
|---|---|---|
| `COPYING` (GPL-3.0) | LGPL §4(b) | not in BWAPI; fetch from gnu.org |
| `COPYING.LESSER` (LGPL-3.0) | LGPL §4(b) | copy of BWAPI's `LICENSE` |
| **`LICENSE.BWEM`** | **MIT/X11 attribution** | **new** — verbatim copy of BWEM's `LICENSE`, retaining the Igor Dimitrijevic notice |
| `NOTICE` | LGPL §4(a) | states BWAPI is used and LGPL-covered; **add a line naming BWEM and its MIT/X11 terms**, and note our `ResetInstance` modification |
| source pointer | GPL §6(d) | URL + pinned commits for **both** BWAPI and BWEM, plus our own tag |
| SPDX field | — | the package stays `LGPL-3.0-only` (R9 §5). The combined work's license is the strongest one; BWEM's MIT is an attribution obligation inside it, not a second package license |

---

## 5. Answers to the questions R11.7 asked

**Is the fork's license unmodified, and does any contributed file carry different terms?**
Unmodified, and no. 29 of 29 files carry the same header; nine contributors added none of their
own.

**MIT attribution requirement?** Ship `LICENSE.BWEM` verbatim in the release asset and name BWEM
in the `NOTICE`. Plus the X11 no-advertising clause, noted above.

**Combined distribution story?** An LGPL-3.0 binary containing MIT code is fine. Stated once so it
is not re-derived.

**Anything in `BWEM/external/` with its own terms?** Three submodules — googletest (BSD-3),
openbw-bwapi (LGPL-3.0) and **openbw (no license)** — all test-only, none reached by the library
target, and **none to be initialised**.

---

## 6. Feeds forward

| To | Finding |
|---|---|
| **§0 / R9 §5** | Add `LICENSE.BWEM` to the release-asset table and a BWEM line to `NOTICE` |
| **§10.1 build** | Pin `third_party/bwem`; **do not** recurse submodules |
| **§15 divergence register** | First entry: `Map::ResetInstance` added to pinned BWEM. Reason: upstream re-init crash (R11.6) |
| **§10.3 pin-bump** | The checklist now covers two pinned dependencies; re-apply the BWEM patch on bump |
| **README** | Describe BWEM's function, do not use the copyright holder's name promotionally (X11 clause) |
