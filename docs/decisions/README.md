# Architecture Decision Records

An ADR records a decision that shaped the project: what we chose, what we
turned down, and why. The point is to save the next person -- usually us, a
year later -- from re-litigating a question that was already settled, or from
re-settling it without knowing what the first answer cost.

Not every change needs one. Write an ADR when the reasoning is the valuable
part: a trade-off between two workable designs, a constraint that is not
obvious from the code, a decision whose alternatives will look tempting again
later.

## Files

One record per file in [`records/`](records/), `NNNN-kebab-case-title.md`,
numbered from `0001` in the order they are written. Numbers are never reused. A
number is an identifier, not a chronology -- it is what supersession references
point at, which is why it survives a retitle. This directory holds the rules
and, in [`experiments/`](experiments/), the code that backs a record; the
records themselves are everything under `records/` and nothing else.

Suggested sections, though the record should follow the argument rather than the
template:

- **Status** -- the transition log, described below. It replaces a separate
  `Date` field, because when the record was written is its first transition.
- **Context** -- the forces in play, including anything measured.
- **Decision** -- what we are doing.
- **Consequences** -- what this buys and what it costs, including the parts we
  are not happy about.
- **Alternatives considered** -- and why each was turned down.

Where discussion produced a conflict or a trade-off, write it into the section it
belongs to rather than into a changelog at the bottom. The disagreement is
usually the most useful thing in the document.

## Records are living documents

A record is **edited to stay current**, not frozen on acceptance. Read
`docs/decisions/records/` as the current state of the decisions made in this
repository.

Git is the log; the document is the projection. Every revision is kept and dated
already, and `git log -p` on a record is there for anyone who wants the diff, so
nothing is lost by keeping the file readable as it stands today.

What makes that safe is that new information arrives as a **dated addition,
marked as arriving after the decision** -- never as a silent revision of the
original reasoning:

> **2027-02-14 (after the decision):** re-running the benchmark on the next
> toolchain release closes the gap to 4%, which weakens but does not reverse
> the argument below.

Accretion, not overwriting. Without that discipline a mutable record drifts
toward what we now think we thought, and stops being evidence of a commitment
made under specific information.

A record whose last row is still `Drafted` is exempt: while its pull request is
open it is being written, not revised, so edit it freely without dating
anything. The dated-addition rule protects a decision that has already been
made from being quietly reworded in hindsight, and a draft has not made one
yet.

[`records/0001-recording-important-decisions.md`](records/0001-recording-important-decisions.md)
argues for all of this.

## Status is a transition log

State changes are exactly the information that would otherwise live only in git,
so they go in the record. Each record opens with a collapsed log: the summary
line carries the current state, expanding it gives the history.

```markdown
<details>
<summary><strong>Status:</strong> Implemented 2026-11-03</summary>

| Date | Transition |
| --- | --- |
| 2026-09-10 | Drafted |
| 2026-09-24 | Accepted |
| 2026-11-03 | Implemented |

</details>
```

The last row is the current state and the summary restates it, so there is one
source of truth and a reader who never expands the block still knows where the
record stands. The gap between `Accepted` and `Implemented` is the interesting
number in there: a decision the code never honoured shows up as a row that never
arrived.

| Transition | Logged when |
| --- | --- |
| `Drafted` | the record is written -- this is what a `Date` field used to say |
| `Accepted` | a commit before the record's pull request merges |
| `Implemented` | the pull request that finishes the work |
| `Superseded by NNNN` | a commit before the superseding record merges |
| `Deprecated` | when the reversal is written into the record |

`Superseded` points outward, to the record that replaced this one.
`Deprecated` points inward: the summary links to the section of this record that
explains the reversal, which is why a withdrawn decision needs no successor to
stay accountable.

### Two things the log is not

**It is not an edit log.** Only state transitions go in it. Changes to the
*content* of a record are dated additions in the body, where the reasoning they
belong to is. A log that grows a row every time someone fixes a sentence is a bad
reimplementation of `git log`, and it will be skipped for the same reason the
real one is.

**It is not a changelog at the bottom.** It records state, never reasoning --
which is what keeps it consistent with writing conflicts and trade-offs into the
section they concern. If a row starts wanting a sentence of explanation, that
sentence is a dated addition in the body and the row just records that the
transition happened.

## Editing versus superseding

> Edit the record when the decision it documents is still the decision. Write a
> new one when someone following the old record would now be doing the wrong
> thing.

The mechanical form of the same test, which is usually quicker to apply:

> Can the change be written as a dated addition? It is an edit. Does it require
> deleting a claim someone might have acted on? The old claim deserves to
> survive as its own record.

New data supporting the existing choice, a widened scope, a clarity rewrite: all
edits. A reversal, or a decision that stands while its mechanism changes out from
under it: a new record. Deliberately a judgement about a reader rather than
something countable -- diff size measures effort and gets this wrong in both
directions.

## Dating claims that will not age well

Anything in a record that is true *as of* rather than true: benchmark numbers,
costs, quoted toolchain output, toolchain behaviour, third-party capabilities.
Say when it was measured, and against what.

This is not a separate convention from the version stamp on quoted toolchain
output below -- that is this rule's first and strictest instance.

## Research

Code that produces concrete data used in the argumentation of an ADR --
benchmarks, memory measurements, probes into toolchain behaviour -- **must** be
committed somewhere a reader can run it. A number quoted in an ADR should be
re-derivable by anyone with a checkout; if the experiment only ever existed in
a scratch buffer, the ADR is asserting rather than arguing.

Which of the two homes it gets is decided by what it depends on:

| The experiment needs | It lives in |
| --- | --- |
| this project's code | the sub-project in [`experiments/`](experiments/), as an entry point named after the record |
| only the language's toolchain and standard library | a share link on the playground, with the source in the ADR |

The split enforces itself in one direction: a playground cannot depend on this
project, so anything that fits in one is necessarily a minimal reproduction. It
is also the only route open to a build-time experiment -- a case that must
*fail* to compile or type-check cannot live in the sub-project, because a
sub-project that does not build breaks the project's build. A project with no
playground at all loses that route; the next section says what it does instead.

See [`experiments/README.md`](experiments/README.md) for the sub-project,
including how an experiment is retired.

### What the playground has to be

"The playground" is a hosted service that gives back a **durable share link**
to a snippet, **runs the snippet on demand against the language's real
toolchain** -- so a reader is one click from a running compiler or interpreter
rather than from a listing -- and **says which toolchain version ran**, so the
provenance line below can be honest. Most languages have a service that meets
all three, and this project's is:

> **Playground:** [Compiler Explorer](https://godbolt.org)

"Share" mints a permanent short link, the *Execute the code* pane runs the
snippet, and the compiler picker names the exact build -- `x86-64 clang 18.1.0`,
`x64 msvc v19.40` -- which is the version the provenance line quotes.

It carries every compiler this project's CI does, which is the reason it is
worth having on top of the sub-project. The suite runs Linux/clang and MSVC at
both x86 and x64 (`.github/workflows/ci.yml`), and a question about the *C ABI*
-- a calling convention, a struct return, whether a bitfield is laid out the way
the spec assumes -- is usually a question about the difference between two of
those, put to both of them side by side. That is a Compiler Explorer pane, not a
CMake target.

The other route only the playground has: **a case that must fail to compile.**
An experiment proving that `bwapi_c2.h` rejects something -- that the header is
not C-clean under `-std=c99 -pedantic-errors`, that a type mismatch is caught at
the boundary rather than at runtime -- cannot live in `experiments/`, because a
sub-project that does not build breaks the build. Note the line between that and
a test: `tests/header_hygiene` asserts the headers *are* clean and is a promise
this project keeps, so it is a test. A record arguing about what the boundary
should reject is evidence, and goes here.

A language with nothing that meets all three closes the second route.
Everything that builds then goes in `experiments/`, and a case that has to
*fail* to build is recorded in the ADR as the four parts below with the
provenance line cut to `TOOL VERSION (released DATE)` -- there is no link to
re-check, so no checked date.

### The playground is for recording, not for iterating

A share link is minted on someone else's infrastructure, and it publishes a
snippet that nobody can collect afterwards. So an experiment is developed
**locally** -- the toolchain against a file in a scratch directory, as many
times as it takes -- and goes to the playground only once it produces the
result the record is going to quote. Nothing is minted to find out what happens.
A link is minted to publish what already happened.

Two consequences, stated outright because the natural working rhythm violates
both:

- **No iterative development against playground resources.** An experiment
  that took four attempts should leave one link behind, not four. Reworking
  after minting turns the earlier links into litter that stays live and wrong.
- **At most one link a minute.** A record needing several is a record that
  should mint them at that pace.

### Playground experiments carry four things, not one

A share link is live, not frozen. A link that names a channel -- `stable`,
`latest`, whatever the service calls it -- rather than a version re-runs against
whatever that channel points at on the day someone clicks it: the wording of a
diagnostic will drift out from under the record, and the same code may
eventually build clean. So a playground experiment is recorded as four parts, in
this order, each doing one job:

1. **A label**, bolded, saying what the experiment demonstrates -- so a reader
   skimming knows whether to stop.
2. **The source, in a code block** -- the frozen record. Not a backup against
   the snippet being deleted; it is what the ADR actually argued from.
3. **Its output, in a `text` code block** -- what the quoted claim rests on.
4. **A provenance line**, as a blockquote directly beneath, carrying the
   toolchain version that produced the output, the date it was last checked,
   and the share link -- live convenience, one click to a running toolchain.

````markdown
**Experiment -- what it shows**

```LANGUAGE
...
```

```text
...
```

> TOOL VERSION (released DATE) - output checked DATE - [PLAYGROUND](URL)
````

`LANGUAGE` is the fence for whichever language the snippet is in. `TOOL` is the
command that produced the output -- the compiler, the interpreter, the type
checker -- `VERSION` is what its `--version` reports, and `PLAYGROUND` is the
service named above. The blockquote renders muted, which is the point:
provenance is needed but is not what the reader came for. Keep the fences bare
-- wrapping the whole block in a blockquote would offset it more strongly, at
the cost of `> ` on every line and source no longer copyable out of a checkout.

Both dates earn their place by answering different questions. The **released**
date belongs to the toolchain that produced the output quoted here. The
**checked** date is when someone last confirmed the link still produces it, and
is what tells a later reader how stale the live half has gone.

The version stamp is **required, and reviewers check for it**. Without it a
reader who clicks through to different output cannot tell whether the toolchain
moved or the record was always wrong. With it, that disagreement is itself the
signal that a superseding record is due.

Research is evidence, not a test. An ADR arguing for different internals should
not bring tests with it -- they would be written against the structure the
record exists to replace. See
[`CONTRIBUTING.md`](../../CONTRIBUTING.md#tests-and-research-are-not-the-same-thing).
