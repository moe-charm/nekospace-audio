# Realtime Contract

## Threads

| Thread       | Responsibility |
| ------------ | -------------- |
| Audio Thread | FIR, delays, reflections, FDN, meter value production |
| UI Thread    | GUI, preset selection, file dialogs |
| DSP Model Worker | HRTF/SOFA load and resampling; future Reverb reflection sets, filter banks and structural network builds |
| CleanVoice Worker | Offline noise-profile learning and file rendering |
| File Worker  | (later) preset / cache saving |

## Forbidden on the Audio Thread

```
new / delete            malloc / free
mutex / locks           file I/O
SOFA parsing            network
string building         logging
vector growth           UI object access
```

## Rules

- All buffers are allocated in `prepareToPlay` for `maxBlockSize`; `processBlock` only
  indexes into them. If the host delivers a larger block than prepared, process in chunks.
- Meter values leave the audio thread via `std::atomic<float>` (relaxed).
- New HRTF filter sets (TASK 6+) are published as ref-counted immutable objects; the audio
  thread swaps a pointer at block boundaries, never locks.
- Reverb reflection sets, T60 filter banks, coherence configurations and any structural
  FDN replacement follow the same publication rule: build an immutable snapshot away
  from the callback, swap it at a block boundary, and use a bounded preallocated
  crossfade. Reclaim the old snapshot away from the audio thread.
- Ordinary coefficient/delay automation uses preallocated smoothers. It must not be
  misclassified as permission to rebuild topology inside `processBlock`.
- Random modulation has a deterministic definition. A project reload and an offline
  regression render must not acquire a different room merely because a new seed was
  generated.
- Standalone monitoring follows the same rule: the callback acquires one ref-counted,
  immutable audio snapshot at the start of a block. File loading and completed offline
  renders publish a new snapshot; they never mutate a vector the callback may be reading.
- Worker completion must not capture a GUI component as a raw pointer. Publish through a
  safe/weak component reference and discard callbacks belonging to cancelled or superseded
  jobs.
- Progress crosses from a worker through an atomic value. A `double&` consumed by a UI
  widget is updated on the UI timer, not written by the worker directly.
- Hosts may deliver any block size at any time (FL Studio especially: odd sizes, size
  changes mid-playback, tiny slices during automation). Never assume a fixed block size.
- Parameter reads in `processBlock` are atomic loads from APVTS raw values; smoothing
  happens inside the engine.
