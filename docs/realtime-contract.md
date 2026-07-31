# Realtime Contract

## Threads

| Thread       | Responsibility |
| ------------ | -------------- |
| Audio Thread | FIR, delays, reflections, FDN, meter value production |
| UI Thread    | GUI, preset selection, file dialogs |
| HRTF Worker  | (TASK 6+) SOFA load, resample, filter build |
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
- Hosts may deliver any block size at any time (FL Studio especially: odd sizes, size
  changes mid-playback, tiny slices during automation). Never assume a fixed block size.
- Parameter reads in `processBlock` are atomic loads from APVTS raw values; smoothing
  happens inside the engine.
