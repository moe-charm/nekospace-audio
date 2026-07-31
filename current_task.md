# Current task — experimental KU100 profile (listening prototype)

**Goal:** decide, by ear, whether a measured KU100 HRTF beats the procedural Analytic B.
Nothing ships until it wins a level-matched blind comparison.

Status: **in progress** (started 2026-08-01)

## Scope

| | |
| --- | --- |
| Dataset | TH Köln KU100, far-field, Bernschütz 2013 — `HRIR_L2702.sofa` (Lebedev 2702-point full sphere, 3.4 MB) |
| Sample rate | **48 kHz only** — the dataset's native rate, no resampler in the prototype |
| Distribution | **not bundled.** Behind the CMake option `NSB_WITH_KU100` (default OFF); the SOFA and the converted pack stay untracked |
| Exposure | third entry in `hrtf.profile`: Analytic A / Analytic B / KU100 (48k) |
| Promotion | only after KU100 clearly wins by ear → then multi-SR, bundling, full licence display |

## Conversion requirements (all of these, or the comparison is meaningless)

1. **Remove the dataset's own ITD.** The engine generates ITD geometrically from the
   rigid-sphere path; leaving the measured delay in would double-count it. Done by
   converting each HRIR to **minimum phase** (real-cepstrum fold), which removes all
   delay while preserving the magnitude response exactly — no onset-detection heuristics,
   and it concentrates energy at the start so truncation to 256 taps is clean.
2. **Verify the coordinate system and ear order.** SOFA `SimpleFreeFieldHRIR` uses
   azimuth counter-clockwise (positive = left) while ours is positive = right, and
   receiver 0 is meant to be the left ear. Both are asserted by a test that checks a
   source at az +90 is louder in the right ear.
3. **Match overall gain to Analytic B** at the frontal reference direction, so switching
   profiles is a timbre comparison and not a loudness comparison.
4. **Resample the direction grid** from 2702 Lebedev points to the engine's 72 × 13 grid
   (3-nearest inverse-distance weighting on the unit sphere; safe because the HRIRs are
   time-aligned first).
5. **Attribution**: dataset name, author, licence (CC BY-SA), source URL and the fact
   that we modified it (min-phase + regridded) must be visible in the plugin and recorded
   in `docs/third-party-licenses.md`.

## Listening protocol

Conditions: **Room OFF, Mono Object, Distance 1 m, Near Field 0** — the `Height Check
(dry)` preset. Test azimuths 0°, ±45°, ±75°, and elevation sweeps at each.

Material: voice **plus** pink noise and hand claps — broadband transients make the
5–12 kHz height cues far easier to judge than speech alone.

Method: level-matched, and do not look at which profile is selected while judging.

Judge: externalisation, front/back certainty, height, and whether the voice stays clear.

## Correction to an earlier claim

I previously said the KU100 recording chain makes a KU100 HRTF a natural fit. That holds
only for **mono sources being spatialised**. Material already recorded binaurally on the
KU100 must not be run through this plugin at all — that would apply the head twice.

## Task list

- [x] Write this plan
- [x] Fetch the SOFA, dump its metadata — confirmed 48 kHz native, `SimpleFreeFieldHRIR`,
      2702 directions × 2 receivers × 128 taps, radius 3.25 m, full sphere including the
      poles, `License = CC 3.0 BY-SA`
- [x] ~~libmysofa~~ — not needed for an offline conversion. The converter is Python
      (h5py reads the SOFA's HDF5 directly), which avoids dragging libmysofa + zlib into
      the build for a step that runs once. libmysofa still belongs in the real TASK 6,
      where users import their own SOFA at runtime.
- [x] `tools/hrtf-pack/sofa_to_bhrtf.py` → versioned `.bhrtf` pack (958 500 bytes)
- [x] `HrtfDatabase::loadPack`, third profile wired through the engine
- [x] Tests (22 total, all passing) — loader rejects every malformed/mismatched pack
      without half-loading, L/R orientation, level match within 0.1 dB, min-phase energy
      front-loading, graceful fallback off 48 kHz, and an end-to-end check of the real
      pack that is skipped when it has not been generated
- [x] Build with `NSB_WITH_KU100=ON`, verified in the standalone: the profile is
      selectable, actually engages at 48 kHz, and the footer shows the BY-SA attribution
- [ ] **← you are here: listen and decide**

## Measured results so far (numbers only — the ear decides)

| | Analytic A | Analytic B | KU100 48k |
| --- | --- | --- | --- |
| up vs down spectral spread, 4–14 kHz | 2.0 dB | 5.8 dB | **6.4 dB** |
| ILD at az +90 | — | — | 12.7 dB |
| frontal level | reference | reference | matched to B within 0.1 dB |

The numbers say KU100 is at least as differentiated as Analytic B, but they cannot say
whether it *externalises* better, which is the main thing measured data is supposed to
buy. That is what the listening test is for.

## If KU100 wins

1. Decide licensing: ship under BY-SA with attribution, or switch to a permissive
   dataset (HUTUBS is CC BY 4.0 and includes a KU100-class dummy head).
2. Multi-sample-rate packs (or a resampler in the loader).
3. Move `hrtf.profile` defaults over, keep the analytic profiles as fallback.

## If it does not

Keep Analytic B, and spend the effort on torso/shoulder reflections instead — a
low-frequency elevation cue the current model has no representation of at all.
