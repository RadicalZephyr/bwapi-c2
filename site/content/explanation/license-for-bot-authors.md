+++
title = "What the license asks of a bot author"
description = "bwapi-c2 is LGPL-3.0-only because it contains BWAPI's object code. Here is what that means for a bot that uses it."
weight = 2
+++

bwapi-c2 is licensed under the GNU Lesser General Public License, version 3.0 only. The short
version for a bot author: **your bot's own code stays yours; your distribution carries a few
files and a notice.** The rest of this page says why, and exactly which files.

## Why LGPL, and why it is not merely inherited

`bwapi_c2.dll` is built by compiling BWAPI's own source files into it. The DLL therefore
*contains* BWAPI's object code, and BWAPI is LGPL-3.0. Under the LGPL that makes the DLL a
Combined Work, conveyed under the license's section 4, and it owes the corresponding source for
the BWAPI portion. bwapi-c2's own wrapper code could have been licensed differently, but shipping
it under the same license collapses the question, so that is what was done. BWEM, the map
analysis library also compiled in, is MIT/X11; that is an attribution obligation inside the
combined work, not a second license to reason about.

## What this asks of you

**Consume the DLL dynamically.** The LGPL gives a bot author two ways to let users substitute
their own build of the library: ship your object files so they can relink, or load the library
through a shared-library mechanism at run time. Loading `bwapi_c2.dll` is the second. A static
library would force every closed-source bot into the first, so bwapi-c2 deliberately does not
offer one, however often it is asked for.

**Your bot's code is yours.** Calling the library does not make your bot LGPL. Nothing about the
license reaches into code that merely uses the ABI.

**A distribution carries the license files and a notice.** A tournament zip containing your bot
and `bwapi_c2.dll` is a distribution. It must give prominent notice that the library is used,
include copies of both the GPL and the LGPL alongside it, and, if your bot displays copyright
notices, include the library's. In practice: copy `COPYING`, `COPYING.LESSER`, `LICENSE.BWEM` and
`NOTICE` from the release into your zip, and paste the notice paragraph from `NOTICE` into your
README. The release ships that paragraph for you to copy verbatim.

**Do not vendor the headers under another license.** The headers are part of the library and
carry its license. Include them from the release; do not copy them into an MIT project.

## What the release itself carries

| File | Why |
|---|---|
| `COPYING` | The GPL-3.0 text, which the LGPL incorporates by reference |
| `COPYING.LESSER` | The LGPL-3.0 text |
| `LICENSE.BWEM` | BWEM's MIT/X11 text with its copyright line |
| `NOTICE` | What the DLL contains and under what terms, the modifications bwapi-c2 carries on BWAPI and BWEM, and the paragraph for bot authors to copy |
| A source pointer | The repository URL, the release tag, and the exact commits of BWAPI and BWEM the release was built from. This line is the corresponding-source offer the license requires |

---

*The full argument:* plan [§0 Licensing](https://github.com/RadicalZephyr/bwapi-c2/blob/main/docs/c-abi-plan.md#0-licensing),
verified in research rounds R9 and R11.7; the repository's
[`NOTICE`](https://github.com/RadicalZephyr/bwapi-c2/blob/main/NOTICE) is the file described above.
