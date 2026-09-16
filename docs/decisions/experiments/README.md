# Experiments

Experiments backing the decision records in
[`docs/decisions/records/`](../records/).

Any code that produces concrete data used to argue an ADR lives here rather
than in a scratch file or a gist. An ADR that cites a measurement is only as
good as a reader's ability to re-run it, and a benchmark that lived in
someone's working tree cannot be re-run at all.

## Layout

This directory is a sub-project of the main build. It depends on the project by
path rather than on a published version, is never published itself, and is
covered by the same checks -- lint, type check, tests -- as anything else in the
repository, because an experiment is still code someone runs, and not publishing
it is no reason to skip that. How it joins the build, and what it is called, the
project says here:

> **Sub-project:** a CMake subdirectory of the main build, added by
> `add_subdirectory(docs/decisions/experiments)` from the root `CMakeLists.txt`
> and gated on `BWAPI_C2_BUILD_EXPERIMENTS`, which defaults on when `bwapi_c2`
> is the top-level project -- the same shape as `BWAPI_C2_BUILD_TESTS`.

It depends on the project by path, through the same `bwapi_c2_abi` object
library the test suite links -- and `bwapi_c2_closure` beside it, because object
libraries do not propagate their objects transitively and linking only the ABI
fails at link on `BWAPI::Type<>::typeNames`. So an experiment sees the real ABI
over the real BWAPI and BWEM closure rather than a copy of either. It gets the same warning
flags as the rest of the tree and the same sanitizer configuration, so
`-DBWAPI_C2_SANITIZERS=address,undefined` covers it too.

Each experiment is one entry point named after the ADR it serves:

```text
docs/decisions/experiments/0001-some-decision.cpp
```

declared in [`CMakeLists.txt`](CMakeLists.txt) with one line --
`bwapi_c2_add_experiment(0001-some-decision)` -- and run from the repository root
with one command:

```shell
cmake --build build --target run-0001-some-decision
```

The `run-` target builds the entry point and executes it; the bare target name
builds it without running. Experiments are **not** registered with CTest. They
are evidence, not tests, and nothing asserts on their output -- they print, a
record quotes what they printed, and the `run-` target is how a reader
re-derives the quote.

### An experiment that needs a running game

Most will. The synthetic-`GameData` builder that makes one exist without
StarCraft is `tests/fixture`, and it is deliberately not wired in here yet,
because the template's rule is that a shared module stays empty until something
is actually shared. When the first experiment needs it, make the fixture target
usable from outside the suite rather than copying it in -- a second builder that
drifts from the one every test uses would make every number this directory
produces suspect.

The record's name is the only thing that varies from one experiment to the
next. Print results in whatever shape the ADR needs to quote them, and link the
entry point from the section that relies on the numbers. Fixtures shared between
experiments -- a data builder two of them both need, a timing harness -- get a
shared module, and it stays empty until something is actually shared. Resist
putting an experiment's own scaffolding there.

## What does not go here

Research produces *evidence*, not tests. It is not asserting that the project
is correct, and it is not expected to keep passing -- it answers a question
that was open at the time an ADR was written.

An experiment that needs nothing from this project does not belong here either.
If it depends only on the language's toolchain and standard library -- a probe
into type inference, a diagnostic worth quoting -- it goes in a playground share
link recorded in the ADR instead, which is also the only route available to a
case that has to *fail* to build. That holds while
[`../README.md`](../README.md) names a playground; where it names none, a
toolchain-only experiment that builds lives here after all, and a case that has
to *fail* to build is recorded in the ADR with no link. The same file has the
routing rule, what the playground has to provide, and the version stamp such a
record has to carry.

## Retirement

An experiment is maintained while the decision it serves is still being argued
or built. It leaves the moment that decision is done -- which the record dates
exactly, in the `Implemented` row of its status log, or in the `Deprecated` row
if the decision was withdrawn instead of built -- through one of two exits:

- **Deleted** -- it measured internals the ADR replaced. An entry point that
  no longer builds against the new code is deleted rather than repaired; cite
  the commit that produced the numbers in the ADR and history keeps it. Most
  records here argue for changing the internals an experiment was measuring, so
  breaking is how it ends rather than a regression.
- **Promoted** -- it still answers a live question, which means it stopped
  being research. A measurement worth re-running is a benchmark and moves to
  the benchmark suite; something asserting a property we promise is a test and
  moves to the test suite. A project with no benchmark suite starts one the
  first time this happens; the experiment does not stay here as the substitute.

What an experiment never does is linger. This sub-project is a **staging area,
not an archive**: everything in it has a scheduled exit, and a healthy one
trends toward empty. Accumulation is the signal to look for something
miscategorised -- a benchmark that was never promoted, or an experiment whose
ADR quietly landed months ago.


## The imported research rounds

`r5/` through `r11/` are the exception, and it is worth being plain that they
are one rather than pretending they fit. They are the scripts, probes and
fixtures behind research rounds R1-R11, which predate this convention. They
moved here from `docs/research/rN/` when the template was adopted; the
write-ups they produce numbers for stayed at
[`docs/research/rN-*.md`](../../research/), and the plan goes on citing those as
`RN`.

Three ways they differ from everything else here:

- **They are named after a research round, not a record.** A round is a unit of
  work -- a question raised in the plan, answered empirically, filed. A record
  is a decision. The two do not correspond one-to-one, and renaming them to
  pretend otherwise would break every `RN` citation in a 2,000-line plan for
  nothing.
- **They back the plan, not a record.** `docs/c-abi-plan.md` is still the source
  of truth for every decision inside its scope, and `§13`'s decisions log
  indexes what these rounds settled. Nothing in `records/` cites them yet.
- **They are not on a record's clock.** The retirement rule keys on a record
  logging `Implemented` or `Deprecated`, and these have no record to key on. So
  they do not trend toward empty on their own, and their presence is *not* the
  accumulation signal the section above describes. Read that signal off the
  `NNNN-` entry points only.

That is an exception with an end, not a permanent carve-out. **The exit these
take is the same one, reached differently**: when a decision inside the plan
graduates into a record, the round that settled it becomes that record's
evidence and rejoins the normal rule; when it does not, the round is promoted or
deleted on its own merits.

**Promotion has already happened here three times, before there was a word for
it.** `tests/layout_dump` is R5 grown up, `tests/derive_closure` is R6, and
`tests/fixture` -- the synthetic-`GameData` builder every suite now uses -- is
R7's harness. Each stopped being a question and became something the project
promises, which is exactly the promoted exit. What is left in `rN/` is the part
that was only ever a question, so the honest expectation is that this directory
shrinks as rounds are revisited, and never that it is tidied in one pass.
