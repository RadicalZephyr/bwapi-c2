+++
title = "Why a C ABI over the real Game"
description = "Why bwapi-c2 wraps BWAPI's own implementation instead of reimplementing the client protocol in each language."
weight = 1
+++

BWAPI's client protocol is small and its game logic is large. That single fact decides the shape
of this project.

## Two ways to reach BWAPI from another language

A BWAPI client is a separate process that reads the game's state out of a shared-memory buffer
and writes commands back into it. Two maintained projects, JBWAPI for the JVM and rsbwapi for
Rust, reach that buffer directly: they reimplement the protocol in their own language and read
the struct in place. Both work and both are in tournament use.

The alternative is to keep BWAPI's own C++ client and put a flat C layer over it, so that other
languages call C and never see the C++. That is what bwapi-c2 does.

## Where the cost actually is

Measured against the two reimplementations, the protocol is cheap: about nine hundred lines in
JBWAPI and about two hundred in rsbwapi, under five percent of either project. What is expensive
is everything the protocol does not carry. BWAPI's shared game rules, the code that decides
whether a unit *can* move, attack, build or research and with what error, run to some 5,600
lines. Its static type database, every unit, weapon, upgrade and tech with every attribute, is
about as large again. JBWAPI spent 16,500 lines porting those, 72% of its BWAPI package; rsbwapi
spent 31,500, 86% of its total.

Both ports got parts wrong, both skipped parts, and between them they record fourteen divergence
bugs from BWAPI's behaviour. The cost does not end at the port, either: when a rule is fixed in
BWAPI, the fix reaches no reimplementation. JBWAPI's own maintainers state it plainly in their
issue tracker: a fix in BWAPI will not fix it for JBWAPI.

A C ABI over the real `BWAPI::Game` never ports any of that. The rule engine is BWAPI's, the type
data is BWAPI's, and both are whatever the pinned BWAPI says they are.

## What it does not buy

It does not buy out of the server's own limits. Grouped commands, ordering several units at
once, are not implemented by the BWAPI server for any client, so no client bot in any language
can issue them; bwapi-c2 does not export them. Server-side bugs reach a C consumer unchanged, as
they reach every client.

## Why this and not the C ABI that already existed

One did: `bwapi-c` reached 1.0 in 2018 with 530 entry points and stopped. It shipped none of the
type database, so the one bot built on it hand-wrote 650 lines of enums; it passed BWAPI's
pointer-hashed set order straight through, so iteration order changed run to run; it let C++
exceptions unwind through `extern "C"`; and it has no license, which forecloses forking it.
bwapi-c2 takes its inventory of wrappable methods as a checked list and its issue tracker as a
list of the failure modes to design out, and reuses no code.

---

*The full argument:* plan [§1.6 Prior art](https://github.com/RadicalZephyr/bwapi-c2/blob/main/docs/c-abi-plan.md#16-prior-art),
[§2 Goals and non-goals](https://github.com/RadicalZephyr/bwapi-c2/blob/main/docs/c-abi-plan.md#2-goals-and-non-goals)
and [§3 Recommended shape](https://github.com/RadicalZephyr/bwapi-c2/blob/main/docs/c-abi-plan.md#3-recommended-shape-stated-against-the-prior-art);
research round [R4](https://github.com/RadicalZephyr/bwapi-c2/tree/main/docs/research) measured
the two reimplementations.
