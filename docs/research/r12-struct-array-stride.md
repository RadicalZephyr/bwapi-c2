# R12. The struct-array stride is destroyed by the callee, and buffer reuse corrupts

Run on Linux with clang 18 against the helpers as they stand in `src/abi_internal.h` at
`6701f6f`, while drafting the events how-to that §16.2 plans. Reproduction:
[r12/run-stride-reuse.sh](r12/run-stride-reuse.sh), [r12/stride_reuse.cpp](r12/stride_reuse.cpp).
No submodules and no BWAPI: the defect is in the boundary's own convention, not in anything
BWAPI does.

**Headline: element zero's `size` carries the caller's stride *in* and the callee's filled byte
count *out*, and the callee's write destroys the caller's value. §4's retry idiom tells a
consumer to size one buffer and reuse it every frame, and nothing tells it that the `size` field
must be re-set on every call. A consumer that sets it once reads corrupt rows from the second
frame onward, silently, and only when its header is newer than the DLL it loaded.** That is precisely the forward-compatibility case the size prefix exists to
serve, so the mechanism fails in the one situation it was added for. Eight exports ship this
convention today — the flat requiredUnits table and the seven per-class type tables — and they
escape it only because a type table is read once, not per frame.

## 1. What was run

`write_rows()`, `write_row()` and `write_struct()` copied verbatim out of `abi_internal.h`, then
driven the way a consumer is told to drive them: allocate once, set element zero's `size` once,
call every frame. §14's C example survives only because it re-sets `size` inside the frame loop,
beside a `realloc` it also repeats — and it says nothing about why, so a reader hoisting either
out of the loop, which is the obvious tidy-up, breaks it. Three consumer/library pairings, because the prefix exists for the mismatched
ones — a consumer whose row matches the library's, one compiled against a **newer** header than
the DLL it loaded, and one against an **older** header.

| Consumer's row | Library's row | Stride on frame 1 | Stride on frame 2 | Frame 2's rows |
|---|---|---|---|---|
| 28 B — matches | 28 B | 28 | 28 | correct |
| **32 B — newer than the library** | 28 B | **32** | **28** | **row 0 correct, row 1 shifted, row 2 lost** |
| 24 B — older than the library | 28 B | 24 | 24 | correct |
| 32 B — newer, single struct out | 28 B | 32 | 28 | correct |

The failing row in full: `type` on row 1 comes back `0` and its `size` comes back `201` — the
row's own `type` value read as a size, because the library laid the rows down at a 28-byte
stride while the consumer indexes at 32.

## 2. Why only that cell

`write_row()` sets `row.size = min(stride, sizeof(Row))`. The two safe cells are safe by
arithmetic, not by design:

- **Matched.** `min(28, 28) = 28`, which is what the caller wrote. The field is overwritten with
  its own value, so the aliasing is invisible.
- **Older consumer.** `min(24, 28) = 24`, again the caller's own value. A consumer smaller than
  the library never loses its stride because its stride is always the smaller of the two.
- **Newer consumer.** `min(32, 28) = 28`, which is *not* what the caller wrote. Frame 2 reads
  28 as the stride and packs rows at the library's size while the consumer indexes at its own.

The single-struct form escapes for a different reason: `write_struct()` re-reads `out->size`
each call, so the value shrinks once from 32 to 28 and is then a fixed point. The rows past the
prefix keep frame 1's zero fill rather than being re-zeroed, and `BWAPI_HAS_FIELD` against the
returned 28 correctly reports the v2 field absent. Sloppy, not wrong. **The defect is specific
to the array form**, where one field is read once for the whole array and written once per row.

## 3. What it costs to detect

Nothing in the current suite would catch it, and nothing in the plan warns about it: §4 says to
reuse the buffer and never says the field is load-bearing on every call. Every test calls a
table once. `bwapi_event`'s
suite sets `size` fresh on each call because each case builds its own struct. A consumer only
meets it after shipping, against a DLL older than its header, on the second frame — the
hardest possible reproduction, and the failure mode is misread data rather than a crash, so it
surfaces as wrong bot behaviour.

## 4. What it changes

§4 said, of the caller setting `size` on element zero: *"No separate `elem_size` parameter — one
mechanism, no redundancy."* The premise is wrong. It was never one mechanism; it was **two
directions aliased onto one field**, and the redundancy §4 refused was the thing keeping them
apart. Revision 4.7 separates them: `size` means "the bytes whoever wrote this struct filled",
in one direction only, and a reader's capacity travels as a parameter. See §4 and decision 25.

The three alternatives considered and rejected are recorded in decision 25: documenting a
"re-set `size` before every call" rule (leaves a silent-corruption footgun in every consumer, in
the case the prefix exists for); having the callee restore element zero's stride before
returning (makes row zero's `size` mean something different from every other row's, so
`BWAPI_HAS_FIELD` is wrong on exactly that row); and keeping the convention with a test
(a test proves the bug exists, it does not stop a consumer meeting it).
