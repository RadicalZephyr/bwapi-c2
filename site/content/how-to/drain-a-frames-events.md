+++
title = "Drain a frame's events"
description = "Read the whole frame's event list in one call, and hand it to your own dispatch."
weight = 3
+++

Every frame carries a list of events — units discovered, units destroyed, text received, the
match ending. Read the whole list in **one call**, then dispatch it however your host language
already dispatches things.

Three functions:

```c
int32_t bwapi_game_event_count(void);
int32_t bwapi_game_get_events(bwapi_event* out, int32_t cap, int32_t stride);
int32_t bwapi_game_event_text(int32_t index, char* buf, int32_t buf_len);
```

`bwapi_game_get_events()` fills up to `cap` rows, `stride` bytes apart, and returns how many the
frame actually has. `bwapi_game_event_text()` gets the text of the three types that carry one.
There is no per-index accessor for the fields: draining is the way in.

## 1. Size the buffer once, outside the frame loop

`GameData` caps the list at 10,000 events and `bwapi_event` is 28 bytes, so the whole worst case
is 10,000 × 28 = 280,000 bytes. Allocate it once and stop thinking about it:

```c
enum { BWAPI_MAX_EVENTS = 10000 };
static bwapi_event events[BWAPI_MAX_EVENTS];
```

If that is too much, size from `bwapi_game_event_count()` instead and grow on overflow, the
same retry idiom as every other collection. Do not preflight with `cap == 0` every frame — that
runs the snapshot twice.

## 2. Drain it, once per frame

Inside the frame loop, after the state is yours to read and before `bwapi_client_update()`:

```c
void poll_events(void)
{
    int32_t n = bwapi_game_get_events(events, BWAPI_MAX_EVENTS, sizeof events[0]);
    for (int32_t i = 0; i < n; ++i)
        dispatch(&events[i], i);          /* your own fan-out */
}
```

Pass `sizeof events[0]` as the stride on **every** call. It is a parameter, not something you
write into the buffer, so reusing one buffer across frames needs no other ceremony.

The snapshot is taken by `bwapi_client_update()` and lasts until the next one, so drain once per
frame, anywhere before you call it again.

## 3. Fan out to your own subscribers

The ABI polls rather than calling you back, so the dispatch is yours to shape. In Python, over
the generated `ctypes` raw layer:

```python
import ctypes
from bwapi_c2 import _raw

MAX_EVENTS = 10_000
STRIDE = ctypes.sizeof(_raw.bwapi_event)
_buf = (_raw.bwapi_event * MAX_EVENTS)()          # allocated once

_handlers = {}                                     # BWAPI_EVENT_* -> [callable]

def on(event_type):
    def register(fn):
        _handlers.setdefault(event_type, []).append(fn)
        return fn
    return register

def pump():
    """Drain this frame and call the registered handlers. One FFI crossing for the lot."""
    n = _raw.bwapi_game_get_events(_buf, MAX_EVENTS, STRIDE)
    for i in range(min(n, MAX_EVENTS)):
        row = _buf[i]
        for fn in _handlers.get(row.type, ()):
            fn(row, i)

@on(_raw.BWAPI_EVENT_UNIT_DESTROY)
def forget_unit(row, index):
    squads.release(row.unit_id)
```

The array is passed straight to a `POINTER(bwapi_event)` parameter — `ctypes` converts it — and
`STRIDE` is just an `int`. C# is the same shape: `[Out] Event[] buf`, `int cap`, `int stride`.

## 4. Get the text of the three types that carry it

`SendText`, `ReceiveText` and `SaveGame` carry a string, which is not in the row because rows are
fixed width. Ask for it by the row's index, under the usual string convention — call once to
learn the length, once to fill:

```python
def event_text(index):
    n = _raw.bwapi_game_event_text(index, None, 0)
    if n <= 0:
        return ""
    buf = ctypes.create_string_buffer(n + 1)
    _raw.bwapi_game_event_text(index, buf, n + 1)
    return buf.value.decode("utf-8", "replace")
```

These arrive at human typing rate, so the extra crossings are a handful per game.

## Three things to get right

**The rows are in BWAPI's order, not sorted.** Unlike every other collection the ABI returns,
events come back in the order the game produced them, because that order is information —
`UnitCreate` before `UnitDestroy` for one unit is a fact you can rely on. Do not re-sort them.

**An index is a position in this frame, not a handle.** It is valid until the next
`bwapi_client_update()` and means nothing afterwards. If you need to remember an event, copy the
row and its text; do not keep the index. Passing a stale one to `bwapi_game_event_text()` latches
`BWAPI_ERR_INVALID_HANDLE`.

**Check `size` before reading a field you added recently.** Each row's `size` comes back as the
bytes the library filled. If your header is newer than the DLL you loaded, `BWAPI_HAS_FIELD`
tells you whether a field is really there:

```c
if (BWAPI_HAS_FIELD(bwapi_event, is_winner, events[i].size))
    /* safe to read events[i].is_winner */;
```

## Related

- [`bwapi_game_get_events`](@/reference/game/bwapi_game_get_events.md),
  [`bwapi_game_event_count`](@/reference/game/bwapi_game_event_count.md) and
  [`bwapi_game_event_text`](@/reference/game/bwapi_game_event_text.md) in the reference
- [`bwapi_event`](@/reference/structs/event.md) for the row's fields
