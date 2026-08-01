# NekoSpace Binaural — Architecture

Voice-focused binaural spatializer. v1 target: place one voice (or a linked stereo pair)
naturally at any azimuth / elevation / distance including extreme near-field ("ear whisper"),
with light room processing for externalization.

## Signal path

```
DAW Input (stereo, always)
   │
Input Router          source.mode = Mono Object | Linked Stereo
   │                  (mono = internal downmix; bus layout is stereo-fixed for FL Studio)
   ▼
Source Geometry       azimuth / elevation / distance / width
   ▼
Near-field Model      per-ear geometric path (sphere diffraction), per-ear gain,
   │                  per-ear fractional delay — this IS the ITD source
   ▼
HRTF Resolver         time-aligned (ITD-free) FIR grid, bilinear interpolation
   ▼
Crossfading FIR Convolvers   dual coefficient sets, ~10 ms output crossfade
   │
   ├────────────► Early Reflections (6 first-order shoebox images,
   │              │      each rendered through the HRTF at its own
   │              │      image direction + per-ear far-field ITD)
   │                    │
   │                    ▼
   │              Late Room FDN (8 lines, Hadamard)
   │                    │
   └────────┬───────────┘
            ▼
Output Trim / Meters
            ▼
Stereo Output
```

Key DSP decisions:

- **ITD is geometric, not stored.** HRIRs in the grid are time-aligned; interaural delay comes
  from exact per-ear path length on a rigid sphere (tangent + arc when the ear is occluded).
  This makes near-field ITD exaggeration emerge naturally and avoids double-counting.
- **`nearfield.amount`** blends between far-field equivalent paths (classic panner behavior)
  and exact per-ear geometry (full ear-whisper behavior).
- **v1 HRTF profile is analytic** (spherical-head shadow + pinna elevation cues), generated
  into the same runtime grid format that measured data (KEMAR / KU100) will use later.
  Swapping in measured HRIRs touches only the grid builder, not the runtime.
- **Direct-path FIR is time-domain (zero latency)** so FL Studio PDC stays trivial.

## Architecture Contract v1.1 (binding)

1. This product is an audio effect, not an instrument.
2. The canonical output is binaural stereo.
3. **The v1 bus layout is stereo-to-stereo, fixed.** Mono sources are handled by
   `source.mode`, never by bus negotiation (FL Studio's mixer is always stereo).
4. DSP code (`src/dsp`) must not include JUCE headers.
5. `processBlock` performs no allocation, locking, file access, network access, logging,
   or UI access.
6. HRTF datasets are immutable after publication to the audio thread.
7. SOFA parsing / resampling happens only on worker threads (TASK 6+).
8. Filter changes are published atomically and crossfaded.
9. Parameter IDs are permanent and must never be renamed after release
   (see parameter-contract.md).
10. Every serialized state carries an explicit schema version, and that version is
    actually read on load. Choice parameters are stored by name, never by index or
    normalised value. Unknown fields are ignored, never rejected. See state-format.md.
11. The plugin must process correctly while the UI is closed.
12. New DSP features require standalone unit tests (JUCE-free) before GUI integration.
13. Room processing must reduce to exact direct rendering at `room.amount = 0`.
14. No Dear Reality source, asset, preset, name, or proprietary implementation is copied.
15. The project is **AGPLv3-or-later**. Every source file carries an SPDX header, and
    every dependency must be AGPLv3-compatible; third-party terms are tracked in
    third-party-licenses.md (SSOT). Audio rendered through the plugin is not a covered
    work and carries no licence obligation.
16. **FL Studio (Windows) is the primary acceptance DAW**: variable/odd block sizes,
    "Fixed size buffers" both on and off, project save/reload, offline render.
17. **Latency must be reported accurately at all times** so FL PDC stays correct.
    v1: the renderer's fixed 2 ms base delay (near-field geometry headroom) is reported
    via `setLatencySamples`; the direct FIR itself adds zero. Any future partitioned-FFT
    quality mode must update the report when engaged.
18. Windows x64 ships first; the codebase stays cross-platform (CMake + JUCE) but macOS
    signing/notarization/AU are deferred until Mac hardware is in the loop.
