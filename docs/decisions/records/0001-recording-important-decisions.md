# 0001 -- Recording important decisions

<details>
<summary><strong>Status:</strong> Drafted 2026-09-15</summary>

| Date | Transition |
| --- | --- |
| 2026-09-15 | Drafted |

</details>

*This record is its own first worked example. Because the rules it describes are
implemented by the same pull request that adopts them, the pre-merge commit logs
`Accepted` and `Implemented` on the same date -- the one case where those two
transitions collapse, and a demonstration that the log can say so plainly where
a single status field could not.*

*The argument was made once, elsewhere, and is adopted here as it stood; "we"
in the sections below is whoever made it, not a claim about this project's
past.*

## Context

This repository had nowhere to put reasoning. The code carries some of it in
comments, and a changelog records what changed without saying why. Neither
survives the question "we already settled this a year ago -- what did the first
answer cost?"

Two things constrain where it can go. The first is that most of the decisions
a project makes are about things it intends to change -- its internal
implementation, or how a requirement it does not control should be spelled in
this language. Reasoning about a structure that is about to be replaced turns
out to constrain how the reasoning can be recorded.

The second is what a decision leaves behind. Prototypes, measurements and probes
with nowhere to live end up in the test suite, where they fail for reasons
unrelated to the project ever after. That is the concrete failure this record
exists to prevent.

### This project already records decisions, and that part works

`docs/c-abi-plan.md` is roughly two thousand lines across numbered sections `§0`
to `§15`, it is the source of truth for every design decision the ABI rests on,
and it is *already a living document* -- revised in place, each revision carrying
a note saying what moved and which research round moved it, with a decisions log
at `§13` and a divergence register at `§15`. Research rounds R1-R11 back it with
code a reader can run. **None of that is being replaced**, and this record is not
a verdict on it. The plan remains the source of truth for decisions inside the
plan's scope.

### What it cannot hold is a decision that is not about the plan

The plan's shape is also its limit. A decision gets recorded by revising the
section it belongs to, which requires that a section belongs to it. A question
larger than the ABI -- *should BWAPI be forked and its trust boundary inverted?*
*who starts StarCraft?* -- has no section to be revised into, and revising the
plan to grow one would be the tail wagging the dog.

So those decisions went somewhere else, and the somewhere else was invented
twice. `docs/adr/0001-fork-and-invert.md` (2026-09-07) and
`docs/adr/0002-launch-and-inject.md` (2026-09-08) exist right now on four
unmerged branches, in a `docs/adr/` directory `main` has never had. Both open
with a hand-written `> **Status: Draft.**` and a paragraph explaining by hand
what they supersede, what they extend and what blocks them. That is a status
field, a supersession pointer and a dependency list being reinvented per file,
by whoever was writing that file that day -- which is the failure this template
exists to prevent, caught in the act rather than described in the abstract.

The second, quieter force: **evidence here is addressable by round, not by
decision.** `docs/research/rN-*.md` is organised by unit of work -- a question
raised, answered empirically, filed -- which is the right axis while the plan is
the only thing citing it. Once decisions have their own records, a record needs
to point at the code behind *its* numbers, and "R11.6" is an answer to a
different question than "what backs this claim".

## Numbering, and what it costs

This record is `0001`, so the two ADRs above become `0002` and `0003` when their
branches land, and `0002`'s internal cross-reference to "ADR 0001 §3" is
rewritten with them.

The template warns that renumbering existing records is usually wrong, on the
grounds that a number is an identifier and things point at identifiers. That
warning is about records someone could already have acted on. These two have
never been merged: nothing on `main`, no commit message, and no document outside
those four branches refers to either number. They are not yet identifiers of
anything, so the usual cost of renumbering is not being paid here -- what is
being paid is a rename and one cross-reference on branches that are being
rebased anyway.

Bought with it: `records/` reads as *the rules, then the decisions* from its
first commit, rather than opening with two records that predate the convention
they are filed under. Recorded here because it is a judgement that looked
arbitrary from outside and should not be re-litigated as an oversight.

**Landing one of those branches means**: move `docs/adr/NNNN-*.md` to
`docs/decisions/records/`, renumber, replace the hand-written status paragraph
with a status log whose `Drafted` row carries the record's original date above,
and open the body with a dated note saying it was written under the old
convention -- its body is a snapshot until someone revisits it. The template's
adoption guide calls for that note, and it is the honest thing to say about a
document written when nothing was going to revise it.

## Decision

Decisions live in `docs/decisions/records/`, one record per file, and they are
**living documents**: a record is edited to stay current rather than frozen at
the moment it was written.

The rules themselves live in [`README.md`](../README.md) one level up, beside
`records/`, and in [`CONTRIBUTING.md`](../../../CONTRIBUTING.md) for
contributors. This record holds the argument, what we turned down, and what we
are still unsure about. Keeping the two apart is deliberate: a record that
restates its own rulebook goes stale the first time the rulebook changes.

## Why living documents rather than immutable records

The usual ADR practice freezes a record on acceptance and supersedes it with a
new one. We are not doing that, and the reason is sharper than "immutability is
a computer-science habit applied to a human process."

An append-only log is half of a pattern. Event-sourced systems work because
they keep the log *and* materialise a view from it. Immutable ADR practice
keeps the log and then tells you to read the log -- so reconstructing the
current state means walking a supersession chain, which in practice nobody
will ever do.

The synthesis was already available: **git is the log, the document is the
projection.** Making records mutable does not discard history, it finally
builds the view that makes the history usable. Git keeps every revision, dated,
and `git log -p` on a record is there for anyone who wants the diff.

The strongest argument for immutability is not preservation but cost --
write-once records need no maintenance. That is an accounting trick. A frozen
record that has gone stale still costs; the cost just moves to read time and is
charged to every reader instead of once to an author. For a directory we want
to function as documentation, read cost is the one that matters.

## Dated strata are what make mutability safe

The real hazard of editable records is not lost history, it is **hindsight
contamination**: once a record can be revised, it drifts toward "what we now
think we thought," and stops being evidence of a commitment made under specific
information. Git does not protect against this, because nobody reads git.

So new information enters a record as a **dated addition, marked as arriving
after the decision** -- the practice [the ADR community calls
timestamps](https://github.com/architecture-decision-record/architecture-decision-record).
That single constraint converts "living document" from overwriting into
accretion, which preserves exactly what immutability was protecting while
leaving the file readable as current state.

The same discipline applies to any claim that will not age well: benchmark
numbers, toolchain behaviour, quoted diagnostics. The version stamp on quoted
toolchain output, in *Evidence* below, is this rule's first and strictest
instance.

## The status field

A record's status is a **dated transition log**, collapsed at the top of the
file, with the current state in the summary line. [`README.md`](../README.md)
has the shape and the set of transitions.

The reasoning that got us here is worth keeping, because we tried a simpler thing
first. The original design was a single `Status` line that recorded *deviation
from the expected lifecycle*: made-and-built was the terminal state and carried
no line at all, on the argument that a field answering "what is unusual about
this record?" does not need to say "nothing".

That argument holds, and it is still why we do not want a bare `Implemented`
line on forty records. But it defended an asymmetry rather than removing one, and
it left a real cost: absence cannot distinguish "done" from "the author forgot."

The log dissolves both problems instead of trading between them. `Implemented`
becomes a row rather than an absence, so the special case stops existing; and
because every row is dated, the line is never noise -- `Accepted 2026-09-10` then
`Implemented 2026-11-03` is a fact about how this project actually moves.

It is also the more consistent application of our own principle. We adopted
living documents because forcing a reader to reconstruct state from git is the
failure we were trying to escape, and then put state transitions in exactly that
place. The gap between `Accepted` and `Implemented` is the hot-air metric a
decisions directory most needs: a decision the code never honoured shows up as a
row that never arrived, visible in the file rather than derivable from
`git log`.

## Editing versus superseding

The rule we needed was when to revise a record and when to write a new one that
supersedes it. Our first instinct was to ground it in diff size -- substantially
rewriting a record means writing a new one instead.

That does not survive contact. Changing "we will use X" to "we will not use X"
is four characters and unambiguously a new decision; rewriting an entire record
for clarity without touching a conclusion is a total diff and unambiguously not.
Diff size measures effort, and correlates with the thing we care about in
neither direction. It is also, ironically, the *more* rigid option: counting
lines looks precise while being wrong.

The rule is grounded in consequence to a reader instead:

> Edit the record when the decision it documents is still the decision. Write a
> new one when someone following the old record would now be doing the wrong
> thing.

With a mechanical companion that makes this rule and the dated-strata rule
enforce each other:

> Can the change be written as a dated addition? It is an edit. Does it require
> deleting a claim someone might have acted on? The old claim deserves to
> survive as its own record.

`Superseded` and `Deprecated` differ in where the reasoning lives, not whether
it exists. **Superseded points outward** -- the argument is in the successor
record. **Deprecated points inward** -- the argument is a dated addendum in the
record itself, and the status line links to it.

We looked for a case where a record would be deprecated with no reasoning worth
capturing anywhere, and could not construct one. Even the degenerate case --
accepting a decision and then simply not doing it -- has a reason attached, and
that reason is a dated addendum rather than a whole new record, because nothing
was built for anyone to have acted on. A useful intuition, though not a rule:
`Deprecated` is mostly what happens to a record that died in the not-yet-
implemented state, and `Superseded` is what happens to one that shipped.

## Evidence

A number quoted in a record has to be re-derivable by a reader, or the record is
asserting rather than arguing. Experiments are routed by what they depend on --
anything needing this project's code goes in the
[`experiments/`](../experiments/) sub-project beside `records/`, anything
needing only the language's toolchain and standard library goes in a playground
share link recorded in the document. [`README.md`](../README.md) has the
routing table, what a playground has to provide to qualify, and what a project
with none does instead; [`experiments/README.md`](../experiments/README.md) has
the retirement rule.

Two things about that arrangement are decisions rather than mechanics.

**The playground constraint is a feature.** A playground cannot depend on this
project, so anything that fits in one is necessarily a minimal reproduction. It
is also the only route open to a build-time experiment, because a sub-project
that does not build breaks the project's build.

The alternative for a build-time result is a snapshot test: capture the
toolchain's diagnostic once and assert it on every run. We turned that down for
ADR evidence. Such a test is written against diagnostic wording the toolchain
does not promise to keep stable, so it asserts something the project neither
controls nor set out to promise. Pinning the wording is not incidental to the
technique, it *is* the technique: a snapshot with nothing to compare against is
not a snapshot, and a test that only asserts the build failed demonstrates
nothing a reader can quote. And the cost lands on contributors rather than on
CI: anyone whose toolchain is a release ahead of or behind the one the wording
was captured from gets a red build out of a diff they did not write. Evidence
near-certain to break buys nothing a playground link does not.

**A share link is live, not frozen**, which is why a playground record is
written as four parts rather than one. A link that names a channel rather than
a version re-runs against whatever that channel points at on the day it is
clicked. The link is convenience, the code block is the frozen record, and the
toolchain version that produced the quoted output is what makes the record
falsifiable later. [`README.md`](../README.md) states the shape as the rule.

That last requirement is there because the failure it prevents is quiet. A
diagnostic quoted from one toolchain release can differ from the next by a
single word, and whoever re-records it on the older release commits that
wording in a diff nobody looks at twice. Without a version stamp, a reader
cannot tell whether the toolchain moved or the record was always wrong.

## Tests are not evidence

A decision arguing for different internals does **not** bring tests with it. Such
a test is written against the structure the record exists to replace, so landing
the change means rewriting it: it guarded nothing and only enlarged the diff. The
existing suite is what checks that behaviour did not change while the internals
did; the argument for changing them belongs in an experiment.

## Alternatives considered

**Immutable records with a supersession chain.** The conventional practice, and
what we have used on other repositories. Rejected because it optimises for the
writer at the reader's expense, and because in practice it produced records
permanently marked "draft" -- a symptom of having no criterion for when a draft
ends. Making acceptance a merge gate dissolves that.

**Diff size as the edit-versus-supersede criterion.** Rejected above. It
measures effort rather than consequence and fails in both directions.

**Collapsing the status field entirely**, with implementation state inferred
from the code. Rejected: it reproduces the failure it was meant to fix, making a
reader reconstruct current state from somewhere else. It also assumed a reader
looking at a pull request on GitHub, where the surrounding interface supplies
the context. A record is frequently read from a local checkout at a commit --
especially in a repository where the reason to check out is to run the
experiments a record cites -- and there it must speak for itself.

**A single `Status` line with no marker for the implemented state.** Our own
first design, replaced by the transition log before any record used it. The
argument for it is in *The status field* above; what it could not do was
distinguish a finished record from a forgotten one, and it kept state
transitions in git after we had just finished arguing that git is where state
goes to be ignored.

**A plain `## History` section at the bottom of the record** rather than a
collapsed block at the top. Rejected because it puts the current state at the
opposite end of the document from where a reader starts, and duplicates it
between a top-line status and a bottom-row log. The collapsed block keeps one
source of truth and puts it first.

## Consequences

Living records converge on **explanation**. A record that is continuously
updated becomes a description of why the architecture is the way it is, which is
[Diataxis](https://diataxis.fr/) explanation in all but name. That is the
mechanism by which this directory becomes useful documentation rather than an
archive, not drift to be corrected.

The shape of the directory is a design signal. Few records edited often means a
small number of load-bearing decisions being refined -- healthy. Many records
edited rarely means the decisions are independent, and the directory is an
archive that will want an index. **Many records edited often is the warning**:
decisions are entangled, so the code's seams do not match the decisions' seams.
It looks like health from inside, because everything is current and active. The
first place to look is whichever boundary nearly every decision touches on both
sides.

`experiments/` should trend toward empty. Every experiment in it has a scheduled
exit -- deleted once the internals it measured are gone, or promoted to the
benchmark suite or the test suite once it turns out to answer a live question.
Accumulation means something was miscategorised.

## Open questions

**How explanation and decisions divide.** In a codebase that already exists
when this convention arrives, the reasoning behind its current shape may not be
in the repository's history at all. An explanation document would often have to
say "this is what the code does, and we do not know why it came to be this way."
Deriving that history from the code alone would be fabrication.

So explanation and decisions will overlap, from opposite directions: explanation
describes a present nobody can account for, while decisions accumulate the
richer history from here forward. When a decision is later made about something
an explanation document covers, some of that document is context for the record
-- but probably not all of it, and the record should not swallow the whole
thing.

We are deliberately not settling this in advance. A file of undigested
observations with no decision attached would be the case in miniature: it may
graduate into a record, or into explanation, or legitimately stay where it is.
That last option is the disanalogy with `experiments/` -- an experiment has no
valid resting state, an observation does. We will have a better feel for the
boundary with a real case in front of us.
