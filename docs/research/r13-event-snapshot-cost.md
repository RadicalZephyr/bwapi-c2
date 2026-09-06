# R13. The per-frame event snapshot costs 5 ns, and 70–95% of it is a walk that cannot be skipped

Measured on Linux with clang 18, Release, against this repository at `6da166d`, after a review
asked whether snapshotted lists should be filled lazily on the first call of their getter so a
bot that never reads them does not pay. Reproduction:
[r13/run-event-snapshot-bench.sh](r13/run-event-snapshot-bench.sh),
[r13/event_snapshot_bench.cpp](r13/event_snapshot_bench.cpp).

**Headline: an ordinary frame's snapshot costs 4.9 ns and a `MatchStart`-sized one 427 ns, of
which 70–95% is the `std::list` walk itself rather than the vector. Making it lazy would save a
bot that never reads events about 118 ns per second of game time, and would save nothing at all
once BWEM is initialised, because §8.2's `UnitDestroy` dispatch has to walk the same list every
frame whether the host asks or not.** The snapshot stays eager (§5.6, §7 row 18).

## 1. What was measured

`bwapi_c2::after_pump()` as it stands, against the bare list walk it must do anyway, and against
the `bwapi_game_get_events()` drain it enables. Best of five runs of 2,000 iterations each, over
the synthetic fixture. 2 events is an ordinary frame — BWAPI sends a `MatchFrame` every frame, so
zero never happens; 201 is `MatchStart`-sized on a populated map; 1,001 is past anything observed,
short of the 10,000 `GameData` allows.

| Events in the frame | Snapshot | Bare walk | Drain | Snapshot as % of the drain |
|---|---|---|---|---|
| 2 | **4.9 ns** | 3.4 ns | 26.1 ns | 19% |
| 11 | 16.4 ns | 11.9 ns | 128.3 ns | 13% |
| 51 | 91.1 ns | 87.1 ns | 544.8 ns | 17% |
| 201 | **426.8 ns** | 322.0 ns | 2,162 ns | 20% |
| 1001 | 2,282 ns | 1,802 ns | 10,542 ns | 22% |

An unoptimized build reports numbers about 20× larger and in the wrong proportion, which is why
the runner forces `CMAKE_BUILD_TYPE=Release`.

## 2. Why lazy is the wrong lever

**The vector is not the cost; the walk is.** Across every row the bare walk is 70–95% of the
snapshot — pointer-chasing a `std::list` dominates, and storing N node pointers into a vector
that has already reached its capacity is the cheap part. Any scheme that still has to look at
the frame's events pays almost the whole bill.

**And from 3.2 the walk is unconditional.** §8.2 drives BWEM's three destruction hooks from the
event pump, so once `bwapi_bwem_initialize()` has been called the ABI must walk the frame's
events every frame regardless of what the host calls. For those hosts a lazy snapshot saves the
vector fill alone: 1.5 ns on an ordinary frame.

**What it would actually save.** A bot that never reads events, with BWEM off, saves 4.9 ns per
frame — about 118 ns per second at 24 frames a second, or 0.00001% of a frame's budget. Against
that, `bwapi_client_update()` blocks on a `ReadFile` against the named pipe and a round trip
through the StarCraft process (R6, R7). That is not measured here, since these fixtures run with
no game, but it is many microseconds at best: the snapshot is lost in its noise.

**And it would cost something.** Lazy filling needs an invalidation flag set in `update()` and a
filled/unfilled state on the boundary. §7 row 18 already rejected keying laziness on the frame
count — menu-frame updates do not advance it, so the accessors would serve a stale list — and a
dirty flag avoids that, but it is still state where there is now none. The same row records that
a snapshot can outlive the `GameImpl` it points into between two fixtures in one binary; today
the teardown run of `after_pump()` empties it, and a lazily filled vector would need its own
answer.

## 3. The alternative that is not laziness

The drain of revision 4.6 removed the reason the vector exists. §5.6 justifies it with *"indexing
a `std::list` directly would be O(n²) over a frame's events"* — true of the per-index accessor,
which decision 24 deleted. The only index-taking event export left is `bwapi_game_event_text()`,
called for the handful of `SendText`, `ReceiveText` and `SaveGame` rows a game produces. So the
live list could be walked with no vector at all: the drain fills rows in one pass,
`bwapi_game_event_count()` is `getEvents().size()` (O(1) since C++11), and `event_text(i)`
advances an iterator.

**Not taken, on the numbers above.** The vector is 5–25% on top of a walk that §8.2 makes
unconditional anyway, `after_pump()` has to exist for 3.2 regardless, and removing the snapshot
now would empty that seam only to refill it one phase later. The O(n²) sentence in §5.6 is
nevertheless obsolete and is rewritten to say what the snapshot is actually for: one description
of the frame's events that both the drain and the BWEM dispatch read, with `event_text()`'s
index meaning the same position in both.

## 4. What it changes

Nothing in the code. §5.6 records the cost and why the snapshot is eager; §7 row 18 gains the
lazy-on-first-call variant beside the frame-count one it already rejected, so the question is
answered rather than re-raised. The rule the review was reaching for — *the ABI does no
per-frame work the host did not ask for* — already holds everywhere else by construction: §5.10's
unit and player snapshots and §6.3's bullets copy inside the call that asks for them. Events are
the one collection with a per-frame cost the host cannot decline, and it is 4.9 ns.
