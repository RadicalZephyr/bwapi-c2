# R12 — Transport floor for a BWAPI proxy protocol

Reproducible via [`r12/run-transport-bench.sh`](r12/run-transport-bench.sh). Unlike R1–R11 this
round needs no BWAPI checkout and no submodules: every number is a property of the OS primitives
and the host language, not of BWAPI.

**Headline: the transport is the smallest lever available. Reading 400 units × 20 fields one
call at a time across a Python FFI boundary costs 2.7–3.5 ms per frame — 6–8% of a "fastest"
frame. The entire client-mode frame handoff costs 0.25–28 µs, at most 0.07% of the same frame.
Client mode performs no serialization at all: it maps `GameData` and exchanges a 4-byte token on
a pipe. So a proxy redesign cannot be justified as a speed improvement over client mode, and
"which wire protocol" is the wrong first question. The two things that are actually broken are
the shape of the read API and the absence of any deadline on the server's blocking read.**

---

## 1. What client mode already is

Worth stating exactly, because "replacing" it has to mean replacing something.

`Client::connect()` calls `MapViewOfFile` on the 33 MB `GameData`
(`bwapi/BWAPIClient/Source/Client.cpp`). `Client::update()` is, in full: write the 4-byte int `1`
to a named pipe, block reading until a `2` comes back. **The bot reads the game's own memory in
place. There is no encoding, no copy, and no wire format anywhere in client mode.** Any format a
proxy introduces is added cost measured against zero.

`GameData` is also not mostly per-frame traffic. Partitioned from
[`tests/layout_dump/baseline.json`](../../tests/layout_dump/baseline.json):

| Region | Bytes | Share |
|---|---|---|
| One-shot static — `strings` (20 MB), `regions` (5.3 MB), walkability, ground height, map metadata | 27,358,189 | **82.9%** |
| Per-frame downstream — dominated by `units` (10000 × 336 B of slots) | 4,138,831 | 12.5% |
| Upstream — `shapes`, `commands`, `unitCommands` slots | 1,520,012 | 4.6% |

The per-frame *slots* are 4.1 MB; the per-frame *content* is far smaller. A realistic mid-game
army of 400 live units is 134 KB, and the `unitArray[1700]` cap puts the ceiling at 571 KB. The
four 256×256 tile bitmaps (`isVisible`, `isExplored`, `hasCreep`, `isOccupied`) add 262 KB.
Upstream, a busy frame is a few hundred `UnitCommand`s at 24 B each — under 10 KB.

---

## 2. The cost hierarchy

Everything below is a 4-core Linux VM. **Absolute numbers are inflated and run-to-run variance
is real** (§7); the ratios are the finding.

| Lever | Cost per frame | Share of a 42 ms frame |
|---|---|---|
| **Per-field FFI reads**, 400 units × 20 fields, ctypes | **2.70–3.49 ms** | **6.4–8.3%** |
| Whole 33 MB `GameData` over loopback TCP | 10.3–13.3 ms | 24–32% |
| All 4.1 MB of per-frame slots over loopback TCP | 1.29–1.61 ms | 3.1–3.8% |
| 571 KB (1700 units) over loopback TCP | 79–126 µs | 0.19–0.30% |
| 134 KB (400 units) over loopback TCP | 43–54 µs | 0.10–0.13% |
| Unix socket / pipe / TCP-loopback round trip, idle p50 | 21–33 µs | 0.05–0.08% |
| Socket round trip, contended p50 | 3.3–3.6 µs | 0.009% |
| **One bulk snapshot of 400 units** | **2.8–2.9 µs** | **0.007%** |
| Shared memory + spin round trip, p50 | 0.20–0.29 µs | 0.0006% |

**Bulk-versus-per-field is a 950–1200× lever. Shared-memory-versus-socket is a ~100× lever on a
quantity already below 0.1% of the frame.** Those two facts, together, are the whole argument for
where design effort belongs.

Two corollaries fall out of the same table. Sending all of `GameData` per frame is a hard 24–32%
tax and caps throughput near 75–100 fps, so a snapshot protocol must send the live fraction or
nothing at all. And every transport that crosses a syscall boundary lands within ~1.5× of every
other, so **the choice among pipe, Unix socket, eventfd and TCP-loopback is noise** — the wakeup
is the cost, not the mechanism.

---

## 3. Jitter: shared memory wins the mean, the socket wins the tail

"Lowest latency and lowest jitter" turn out to be in genuine tension, and the result reverses the
obvious answer. Round trip in µs, 30k iterations, idle and with 4 CPU hogs on 4 cores:

| Wait strategy | idle p50 | idle p99.9 | loaded p50 | loaded p99.9 | **loaded max** |
|---|---|---|---|---|---|
| shm + pure spin | 0.24 | 3.04–4.20 | 0.20–0.29 | 2.48–4.29 | **4018–4034** |
| shm + spin 2000 → futex | 0.37–0.61 | 4.73–7.10 | 0.53–0.67 | 4.38–14.50 | **4028–4037** |
| shm + spin 200 → futex | 0.41–0.55 | 11.79–14.93 | 0.57–8.01 | 33.76–42.01 | **188–8138** |
| shm + futex only | 0.50–21.58 | 35.74–66.06 | 2.06–2.30 | 22.52–4018 | **153–7826** |
| unix socketpair | 21.29–28.33 | 68.41–86.05 | 3.26–3.58 | 21.94–30.25 | **99–125** |

**Shared memory wins the mean by roughly 100×. Under contention the blocking socket wins the
worst case by 40–80×.** The ~4 ms figure is one CFS scheduling quantum: a spinner that gets
preempted by a competing runnable thread waits a full timeslice to be rescheduled, while a
blocking reader is woken with scheduler preference.

The consequence is that **the right wait strategy is a function of core budget, not of
transport**. With a core available per process, spin: 0.25 µs and a bounded tail. With cores
oversubscribed, don't: accept ~20 µs and keep the ~100 µs tail. Spin-then-block with a spin
budget of ~2000 pause iterations gets the spin p50 and cuts the futex-only p99 substantially,
which makes one mechanism with one tunable sufficient for both regimes.

For scale: even the 4–8 ms preemption tail is 10–19% of a 42 ms frame and would not by itself
breach a 55 ms tournament limit — but combined with a bot already using 40 ms it would. At
headless training speeds (~1 ms frames) it is catastrophic. Jitter is not a tournament problem;
it is a training-throughput problem.

---

## 4. The datagram tax, and why QUIC is miscategorised here

QUIC was the starting hypothesis, on the intuition that its encryption might be overkill. **The
encryption is the cheap part.** AES-128-GCM measures **15.5–18.9 GB/s per core** here (AES-NI +
PCLMULQDQ), which is 2.4–6× faster than the loopback TCP path itself. It would never be visible.

The cost is packetization. Same 571 KB payload, send side only:

| | µs/frame | GB/s | syscalls/frame |
|---|---|---|---|
| TCP, one `write`, kernel TSO | 175–197 | 2.90–3.25 | 1 |
| UDP, one `send` per 1200 B datagram | **2187–2318** | 0.25–0.26 | 476 |
| UDP + GSO, 64 segments per syscall | 26–31 | 18.7–22.3 | 8 |

An untuned datagram protocol pays **12–13×** over a stream protocol. A tuned one recovers it, but
only via segmentation offload — GSO/GRO on Linux, USO/URO on Windows, the latter gated on
Windows version and NIC support. **The game side of any BWAPI proxy is necessarily a Windows
process**, which is the side that would need it.

The decisive objection is simpler than any of this. On one host QUIC would run congestion
control, loss recovery, packet pacing and a connection state machine over a medium that cannot
congest, reorder or drop, and its three headline features do not apply: there is exactly one
ordered stream (frame *N* must precede *N+1*, so head-of-line blocking **is** the semantics);
connection migration is meaningless for a process pinned beside a game instance; and 0-RTT saves
one handshake per game, where games last minutes. **QUIC is not overkill here so much as
miscategorised — it solves transit problems, and this is not transit.**

---

## 5. What is actually broken

Two things, neither of them the wire format.

**The read API shape.** §2's top row. This is the one that costs real frame budget — and the
plan already fixes it in v1. §5.10 specifies `bwapi_game_snapshot_units()` and
`bwapi_game_snapshot_players()`, a field-select copy into a caller buffer with booleans packed as
bits in a `flags` word; §5.5 refuses per-cell FFI for map data on the same grounds. **A proxy
redesign does not create the batching win and is not needed for it.** The 950–1200× lever is
available in client mode today, and §5.10 is how it gets taken.

**The server's blocking read has no deadline.** `Server::callOnFrame()`
(`bwapi/BWAPI/Source/BWAPI/Server.cpp:737`):

```cpp
WriteFile(pipeObjectHandle, &code, sizeof(int), &writtenByteCount, NULL);
while (code != 1) {
  BOOL success = ReadFile(pipeObjectHandle, &code, sizeof(int), &receivedByteCount, NULL);
  if (!success) { DisconnectNamedPipe(pipeObjectHandle); connected = false; ... break; }
}
```

`PIPE_TIMEOUT = 3000` is `CreateNamedPipe`'s `nDefaultTimeOut`, which governs `WaitNamedPipe`,
not this read. So:

- Bot **crashes** → the pipe breaks → `ReadFile` fails → the game disconnects and continues.
  **Client mode already solves crash isolation today.**
- Bot **hangs** → the game blocks forever. No replay, no forfeit, no diagnosis; tournament
  infrastructure must kill it out of band.

That asymmetry is the tournament-robustness gap, and it is a missing deadline plus a missing
supervision signal — not a property of the transport. It is also the thing module mode
fundamentally *cannot* fix: a hung module bot is an infinite loop inside the game's own call
stack, deadline-able only by a watchdog thread and a thread-kill against live game state.

---

## 6. What this rules in and out

**Ruled out.** QUIC, and any network protocol, for a same-host boundary (§4). Protobuf,
FlatBuffers and Cap'n Proto, which solve schema evolution across independently-versioned
endpoints — a problem this project does not have, since both sides are x86-64 and both are
generated from `tools/abi/spec/*.yaml` with `tests/regen_check.sh` failing CI on any drift.
Whole-`GameData` snapshots per frame, at 24–32% of a frame (§2).

**Ruled in.** Shared memory for the state and command planes, with POD records at generated
offsets and a layout hash exchanged at connect — a stricter version of what `client_version` and
`revision` half-do today, and directly supported by the three-target
`tests/layout_dump/baseline.json`. A spin-then-block wait with the spin budget as a deployment
knob (§3); on Windows that is `WaitOnAddress`/`WakeByAddressSingle`, a futex in all but name. An
OS handle — the existing named pipe is fine — kept open purely as a death detector, because
shared memory cannot tell you your peer died while the OS guarantees an EOF.

**Unchanged.** §4's ABI conventions and §4.1's frame loop describe a protocol the bot sees
through the DLL. Bindings should never touch shared memory directly: doing so would make
`GameData`'s layout a public ABI contract in every language, which is precisely what the
function-ABI design exists to avoid. `bwapi_c2.dll` maps the region; the binding calls the same C
ABI as today; the proxy stays invisible to §4.

---

## 7. What remains unmeasured

Four gaps, in the order they block a decision.

1. **Every number here is Linux on a shared 4-core VM.** The game side is Windows. Named pipes,
   `WaitOnAddress`, and Windows' scheduler quantum all differ, and §3's central result — that the
   blocking path wins the tail under contention — is a scheduler property that may not carry.
   **Re-measure on Windows before any final decision.**
2. **Run-to-run variance is large.** The futex-only idle p50 moved from 0.50 to 21.58 µs between
   two runs of the same binary, and the FFI call from 337 to 436 ns. Tables above give observed
   ranges rather than single figures for this reason. Any decision resting on a factor under ~2×
   is not yet supported.
3. **Per-frame delta density is unknown.** Whether the state plane needs dirty-tracking at all
   depends on what fraction of `UnitData` changes between consecutive frames, and that has not
   been measured — only reasoned about from the struct (336 B, 97 fields, 41 of them `bool`, ~14
   status timers that are zero for almost every unit, ~10 mostly-`-1` ID references). At 2.8 µs
   for a 400-unit snapshot, double-buffering may simply be adequate and dirty-tracking premature.
   **Experiment:** extend `tests/fixture` to run a few thousand frames, XOR each frame's `units`
   array against the previous, and histogram nonzero bytes per unit.
4. **The FFI workload is modelled, not observed.** "400 units × 20 fields" is a plausible
   mid-game read pattern, not one measured from a real bot, and ctypes is the pessimistic bound
   (cffi is ~5–10× faster). The conclusion survives either way — 950× is not a close call — but
   the specific percentage of frame budget does not.
