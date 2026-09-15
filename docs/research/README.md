# Research

Results of the numbered research rounds R1-R11: the empirical answers behind
[`../c-abi-plan.md`](../c-abi-plan.md), one write-up per round as
`rN-<topic>.md`, with R11's sub-rounds as `r11-K-<topic>.md`. The plan's
decisions log (`§13`) indexes what each round settled, and cross-references
throughout the repository name them as `RN` or `RN.K`.

## The code that produced them is not here

It is in [`../decisions/experiments/`](../decisions/experiments/), under `r5/`,
`r6/`, `r7/`, `r8/` and `r11/` -- the same directory names, one level over. They
moved there when this repository adopted a decisions template that puts every
runnable piece of evidence in one place, so that a number quoted in an argument
has exactly one place a reader looks to re-derive it. The write-ups stayed here
because they are not evidence; they are what the evidence was read to mean.

Links from a write-up into its scripts point across, so following one works. A
bare mention like `` `r6/derive-closure.sh` `` names the round's directory, not
a path relative to this one.

[`../decisions/experiments/README.md`](../decisions/experiments/README.md#the-imported-research-rounds)
says how these rounds sit inside a directory whose other occupants are named
after decision records and are deleted when those records land -- the short
version being that they are an exception, deliberately, and why.

## This directory is an archive, and that is allowed

R1-R11 settled questions that are now built on. The write-ups are not
maintained, not expected to be re-derived, and not going anywhere: a round is
a snapshot of what was true when it was measured, and the plan is where the
current answer lives. Anything here that is still *open* is open in the plan's
divergence register (`§15`), not in these files.

That is the difference between this directory and `../decisions/records/`.
Records are living documents, edited to stay current. These are dated findings,
and editing one to agree with today would destroy the only thing it is for.
