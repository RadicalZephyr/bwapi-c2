# Draft: a proxy protocol to replace BWAPI client mode

> **Status: Draft.** Nothing here is settled and nothing is scheduled. This is a v2-or-later
> design that sits outside [the implementation plan](implementation-plan.md); phases 2–4 proceed
> on client mode unchanged. Six questions in §6 block a final decision, and one of them (§6.1)
> invalidates parts of §3 if it goes the wrong way. Expands the first bullet of
> [`possible-alternatives.md`](possible-alternatives.md) into a design.

## Purpose

Replace BWAPI's client mode with a protocol of our own between the game process and the bot
process, on one host. The measured groundwork is [R12](research/r12-proxy-transport.md); this
document is what the numbers argue for, what they argue against, and what still has to be
measured.

The framing that survived contact with the measurements is this: **a proxy is not a performance
change, it is a capability and supervision change.** Client mode maps `GameData` and exchanges a
4-byte token on a pipe — no encoding, no copy, a 0.25–28 µs handoff. Nothing beats that on one
host, and R12 §2 puts the entire handoff below 0.1% of a "fastest" frame. Anything built here
must therefore justify itself on what client mode *cannot do*, and be judged on how little
performance it gives back.

---

## 1. Goals, one non-goal, and one constraint

**1.0 What is *not* a motivating goal: batched reads.** Reading unit state one field per FFI
call costs 2.7–3.5 ms per frame, 6–8% of a "fastest" frame, against 2.8 µs for one bulk snapshot
(R12 §2) — a 950–1200× lever, and by far the largest number in the whole investigation. **It is
already fixed in the v1 plan and needs no proxy**: §5.10 specifies
`bwapi_game_snapshot_units()`/`_players()`, and §5.5 refuses per-cell FFI for map data. Recording
this first because it is the obvious thing to reach for a new protocol over, and it would be the
wrong reason. The proxy's case rests entirely on §1.1 and §1.2.

**1.1 Grouped commands, without module mode.** Grouped commands are not implemented by the BWAPI
server's client-mode path. JBWAPI #70, from `dgant`: *"Module bots can issue grouped commands.
But client bots can't, because BWAPI's server implementation doesn't support them… There's no way
to fix this on JBWAPI's end."* Our `bwapi_game_issue_command(ids, n, cmd)` is a loop, not a
grouped command; `GameImpl::issueCommand(const Unitset&, UnitCommand)` iterates and is annotated
`//FIX FIX FIX naive implementation` (§5.3, §15 #17). Seventeen `canXxxGrouped` declarations
under eleven names are suppressed for this reason.

This is a **server implementation gap, not a Brood War limit** — module bots have the capability.
Appendix A therefore makes it module mode's sole concrete motivation, and pays for it with
"x86-forever, no crash isolation, and a much harder test story." **A protocol that owns the
server-side command path closes the gap while keeping crash isolation and an x64 bot.** If that
holds, it removes the only stated reason module mode is on the roadmap at all.

The x86 half does not disappear, it stays where it already is. BWAPI's server lives in the DLL
injected into the 32-bit `StarCraft.exe` (`bwapi/BWAPI/Source/BWAPI/Server.cpp`), so owning the
command path means work inside that 32-bit process — exactly what `possible-alternatives.md`
means by "our own 32bit module mode that implements a separate client mode." What §10.2 settled
is that the **bot** process is x64, and that is what this design preserves and Appendix A's
module mode gives up.

**1.2 Tournament robustness against hangs.** Module bots crash the game process, which interferes
with replay saving and makes tournament hosting harder. Client mode already fixes *that*: on a
crashed client the pipe breaks, `ReadFile` fails, and the game disconnects and continues. What
neither mode fixes is a **hang**. `Server::callOnFrame()` (`Server.cpp:737`) blocks on `ReadFile`
with no deadline — `PIPE_TIMEOUT = 3000` governs `WaitNamedPipe`, not this read — so a bot stuck
in a loop wedges the game forever: no replay, no forfeit, no diagnosis.

Module mode cannot fix this even in principle: a hung module bot is an infinite loop inside the
game's own call stack, deadline-able only by a watchdog thread and a thread-kill against live
game state. **A separate process you can simply stop waiting for is the whole mechanism.** This
is the strongest argument for the project and it is about supervision, not speed.

**1.3 The constraint: competitive parity with module-mode bots.** Module bots will keep existing; a new protocol
cannot be premised on everyone migrating, and must not disadvantage the bots that adopt it.
§5 works through what parity actually requires, because the intuitive answer is wrong.

---

## 2. What client mode already gives us

Stated so the redesign does not rebuild it by accident (all from R12 §1):

- **Zero-copy reads.** `MapViewOfFile` on the 33 MB `GameData`. The bot reads the game's memory
  in place. No serialization exists to make faster.
- **Crash isolation.** Already solved, per §1.2.
- **Batched command *transport*.** `unitCommands[20000]` with a count is already a batch. What is
  missing is grouped *semantics* (§1.1), not batching of the wire.
- **A snapshot-shaped read API**, once §5.10 ships (§1.0).
- **82.9% of `GameData` is one-shot static** — terrain, regions, the 20 MB string table. Only
  ~134 KB (400 live units) to 571 KB (the `unitArray[1700]` cap) is live per-frame content.

And what it does not give us, beyond §1: no consistent snapshot (the bot reads a region the
server is free to mutate), no backpressure, and no sequence numbers to reason about staleness.

---

## 3. Proposed shape

Three planes over one shared-memory segment, plus an OS handle. **No network protocol, no wire
encoding.**

**3.1 State plane.** `GameData`-shaped, double-buffered (or seqlocked) with a frame sequence
number, so a bot reads a consistent snapshot without the server blocking on it. POD records at
generated offsets — the layout comes from `tools/abi/spec/*.yaml` through `regen.py`, the same
source that already produces the ABI, with `tests/layout_dump/baseline.json` pinning offsets
across three targets and `tests/regen_check.sh` failing CI on drift. A **layout hash exchanged at
connect** replaces `client_version`/`revision` version checking, so a mismatched peer fails
loudly instead of reading garbage.

**3.2 Command plane.** A ring of variable-length command records with sequence numbers and an
explicit "frame *N* complete" barrier. This is where §1.1 lives: a grouped command becomes one
record naming a set, rather than *n* records the server replays in a loop. The ring also gives
real backpressure and lets a bot stream commands as it computes rather than filling fixed slots.

**3.3 Control plane.** Connect, handshake, liveness, deadline, forfeit. **Keep an OS handle — the
existing named pipe is fine — open purely as a death detector**, because shared memory cannot
tell you your peer died and the OS guarantees an EOF. Deadlines live here, and so does the
"bot missed frame *N*" signal that §1.2 needs and §6.2 has to specify.

**3.4 Wait strategy: spin-then-block, spin budget as a deployment knob.** On Windows,
`WaitOnAddress`/`WakeByAddressSingle` — a futex in all but name — with `WaitForSingleObject` on
an Event where a timeout is wanted, which supplies §1.2's deadline for free.

R12 §3 is why this is a knob and not a constant, and it reversed my expectation. Shared memory
wins the mean round trip by ~100× (0.25 µs vs 21–28 µs), but **under CPU contention a blocking
socket wins the worst case by 40–80×** (~100 µs vs 4–8 ms), because a preempted spinner waits a
full scheduler quantum. So the right strategy is a function of core budget: dedicate a core and
spin; oversubscribe and block. One mechanism, one tunable, two regimes — rather than two
transports.

**3.5 Encoding: none.** Protobuf, FlatBuffers and Cap'n Proto all solve schema evolution across
independently-versioned endpoints. Both sides here are x86-64, both are generated from one spec,
and CI fails on drift — the problem those formats solve does not exist, and each would add
encode/decode cost against client mode's zero.

**3.6 Bindings never touch shared memory.** `bwapi_c2.dll` maps the region; the binding calls the
same C ABI as today. Mapping it from Python or C# directly would be marginally faster and would
make `GameData`'s layout a public ABI contract in every language — exactly what the function-ABI
design exists to avoid (§4). The proxy stays invisible to §4 and §4.1.

---

## 4. Decisions taken, and what they cost

| Decision | Rationale | What it gives up |
|---|---|---|
| Shared memory, not a network protocol | Same host by requirement; R12 §2 puts any syscall transport at 100× the handoff cost and R12 §4 rules out datagram protocols | Cross-host and cross-OS bots. If that becomes a goal, this design does not stretch — it is a different one |
| Server half inside the 32-bit game process | Where BWAPI's server already is; there is nowhere else to own the command path from | A 32-bit build target and its test story, for the game half only (§1.1) |
| No wire encoding; POD at generated offsets | §3.5 | Layout becomes a versioned contract; a pin bump that moves offsets is a protocol break, caught by the connect hash |
| Spin budget as a deployment knob | R12 §3; the two regimes have opposite optima | A knob is a support burden and a way to be misconfigured |
| Death detection via an OS handle, not shared memory | Shared memory cannot signal peer death | An extra handle to keep alive and to reason about at teardown |
| Bindings go through the DLL | §3.6 | The last ~µs of read latency |

**QUIC was the starting hypothesis and is rejected.** The intuition that it was overkill was
right; the reason was not. Encryption is the *cheap* part — AES-128-GCM measures 15–19 GB/s per
core, faster than the loopback path it would protect. The real cost is a 12–13× packetization tax
recoverable only through segmentation offload, which on the Windows side of the boundary is gated
on OS version and NIC support. And on one host QUIC would run congestion control, loss recovery
and pacing over a medium that cannot congest, reorder or drop, while none of its three headline
features apply: there is exactly one ordered stream, so head-of-line blocking *is* the semantics;
migration is meaningless for a process pinned beside a game; 0-RTT saves one handshake per
multi-minute game. QUIC solves transit problems, and this is not transit.

---

## 5. What competitive parity actually requires

The intuitive argument — that a lower-latency bot can micro at higher frequency — does not hold.
**BWAPI cannot act sub-frame.** `onFrame` fires once per logical frame and state advances only
per frame, so a lockstep bot and a zero-lag pipelined bot get *identical* decision opportunities.
Microseconds of handoff buy no additional decisions. Two things are genuinely at stake:

- **Frames of staleness.** Running *k* frames behind means reacting to old state, and that is a
  real and large disadvantage. It argues that **k = 0 must be the default and must always be
  available** — not that the transport must be fast. BWAPI's own `Latency::SinglePlayer = 2`
  through `BattlenetHigh = 24` describe how much slack exists if a deployment wants it, but a
  tournament should not.
- **Compute budget.** Handoff overhead is budget the bot cannot think in. At 42 ms/frame a 28 µs
  socket is 0.07% — negligible. Per-field FFI reads are 2.7–3.5 ms, or 6–8% (R12 §2). **The gap
  that actually exists is the read API shape, and it is ~100× larger than any transport
  difference.** A C++ module bot pays neither; a Python bot pays the FFI cost in module mode
  *and* in proxy mode. Bulk snapshot reads close it for both, and §5.10 already specifies them.

So parity is achievable on handoff and is not the binding constraint. **The binding constraint is
`k = 0` plus a snapshot-shaped read API**, and neither is a transport property.

One consequence worth designing for from the start: **frame-time accounting becomes a fairness
problem.** Tournament rules charge bots for time in `onFrame`. A module bot's clock is
unambiguous; a proxy bot measured handoff-to-reply is charged for IPC *and* for its own process's
scheduling delay — including R12 §3's 4–8 ms preemption tail. Two bots doing identical work would
get different bills. The protocol should carry timestamps from both sides so a tournament can
charge only the bot's own compute.

---

## 6. Open questions blocking a decision

**6.1 Do R12's numbers hold on Windows?** Every measurement is Linux on a shared 4-core VM. The
game side is Windows, where named pipes, `WaitOnAddress` and the scheduler quantum all differ.
§3.4's central claim — that the blocking path wins the tail under contention — is a *scheduler*
property and may not carry. **This is the largest unknown and it invalidates §3.4 if it goes the
wrong way.** Port `r12/jitter.c` to `WaitOnAddress` + named pipes and rerun on a tournament-spec
Windows box.

**6.2 On deadline expiry: drop-and-continue, or forfeit?** The one place this design changes
observable game semantics. Dropping a frame's commands and continuing keeps games running but
makes a slow bot silently lose actions; forfeiting is honest but harsh on a transient stall.
Either way a distinct "bot missed frame *N*" signal is needed so tournament rules can count
violations the way frame-limit violations are counted today. **Needs a decision before anything
is built, because the command plane's shape depends on it.**

**6.3 Is a grouped command actually representable end to end?** §1.1 rests on the gap being a
server implementation gap, which `dgant`'s quote supports and module mode's capability confirms.
But nobody has traced what the server would have to do to honour a grouped record against Brood
War's own command path. **Experiment:** read `GameImpl::issueCommand(const Unitset&,·)` and the
module-mode path that does support grouping, and establish whether the difference is a missing
protocol representation or missing server work. If §1.1 does not hold, module mode keeps its sole
motivation and this design loses a third of its case.

**6.4 How much of `UnitData` changes per frame?** Decides whether the state plane needs
dirty-tracking at all, or whether double-buffering is simply adequate — a 400-unit snapshot costs
2.8 µs (R12 §2), so dirty-tracking may be premature. **Experiment:** extend `tests/fixture` to run
a few thousand frames, XOR each frame's `units` array against the previous, and histogram nonzero
bytes per unit. Reasoning from the struct only gets as far as "336 B, 97 fields, 41 of them
`bool`, ~14 status timers zero for almost every unit" — suggestive, not a number.

**6.5 Does one mechanism really serve both regimes?** §3.4 asserts a single spin-then-block
mechanism with a tunable covers tournaments (dedicated cores, spin) and training (oversubscribed,
block). The alternative is admitting two transports. R12 §3 supports one mechanism but its
`spin 200` row is unstable across runs (p50 0.57 vs 8.01 µs), so the budget's sensitivity is not
characterised.

**6.6 What is the real read workload?** R12's "400 units × 20 fields" is modelled, not observed,
and ctypes is the pessimistic bound. The 950–1200× conclusion survives either way; the claim that
FFI reads cost 6–8% of a frame does not, and §5's parity argument leans on it. **Experiment:**
instrument a real bot's read pattern per frame.

---

## 7. Relationship to the existing plan

- **§4 and §4.1 are unchanged.** The frame loop the bot sees is `connect → update → poll →
  repeat` either way; §3.6 keeps the proxy behind the DLL. `bwapi_client_update()` still blocks.
- **§15 #17 (`canXxxGrouped` not exposed)** is the entry this design would retire. It currently
  reads "Revisit with module mode"; if §6.3 holds, the revisit is here instead.
- **Appendix A (module mode)** loses its sole concrete motivation if §1.1 holds. That is worth
  stating plainly: this design and module mode are alternatives, not complements, and this one
  keeps x64 and crash isolation.
- **§11's test story** would need a fourth substrate — the R7/R11.6 synthetic `GameData` fixture
  drives the client's read path but has no server side to deadline against.
- **The divergence register and decisions log are untouched** until §6's questions are answered.
  Nothing here has changed a v1 decision.
