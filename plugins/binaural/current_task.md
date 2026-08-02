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
- [x] **Listening round 1 — inconclusive for KU100, and the same verdict for A/B/KU100**

## Listening round 1 result (2026-08-01)

Setup: Sony MDR-Z1R, dedicated DAC and amp.

> "音の変化ははっきりとあるけど 上下がやはりわかりにくい"

All three profiles produce a **clear timbre change** with elevation, but none produce a
convincing **up/down percept**. KU100 is not obviously better than Analytic B by ear,
despite measuring slightly wider.

This is the textbook outcome for static, non-individualised spectral elevation cues, so
it is not evidence that the KU100 conversion is broken — the tests show it loaded, is
oriented correctly and is level-matched. It is evidence that **spectral cues alone are
not the lever**, and that swapping datasets will not fix it.

Consequences:

- KU100 is **not promoted**. It stays behind `NSB_WITH_KU100` as a reference point.
- The next work is on cues that do not depend on matching the listener's pinnae:
  torso/shoulder reflections, and early reflections rendered through the HRTF at their
  real image directions instead of being panned.
- Round 2 must be listened to **with the room engine on**, not dry. The dry test isolates
  the HRTF, which is exactly the cue that turns out to be weak.

## Round 2 — pinna-independent elevation cues (implemented, awaiting listening)

Two changes, both chosen because they do **not** depend on the listener's pinnae
matching the dataset:

1. **Early reflections now render through the HRTF** at each image's own direction, with
   a per-ear far-field ITD, instead of being equal-power panned. A raised source now
   produces a floor bounce that genuinely arrives from below. Under the old panning a
   source above and a source below shared one pan angle (`atan2(x, z)` is identical for
   the two) and differed only in arrival time — there was no directional information in
   the reflections at all. Measured up-vs-down room-response spread: **5.9 dB**.
   Cost: 6 images × 2 ears × 32 taps, a quarter of the direct path's length, which is
   still ample for a Q=3.5 notch at 4 kHz.
2. **Torso/shoulder reflection in Analytic B** — a delayed copy whose comb notches track
   elevation across 440 Hz–1.2 kHz. Elevation spread in the 0.7–3 kHz band: **2.2 dB**
   for B against 0.1 dB for A. The KU100 is a head without a torso, so the measured
   profile has no equivalent; this is one reason it did not win round 1.

Also: **all three profiles are now level-matched** at the frontal direction, not just
KU100 against B. Round 1 compared A and B at slightly different loudness.

New preset **"Height Check (room)"** — the one to judge height with. Round 1's dry
preset is kept for isolating the HRTF, but it removes the cue that actually works.

## Planned GUI restructure — deferred until after round 2 (agreed)

The DSP for this already exists; it is a naming and grouping job, not new features.
Deliberately **not** done yet: the labels would be dishonest while KU100 is 48 kHz-only
and silently falls back to Analytic B at other rates.

```
Height Character:  Natural / Enhanced
Room Assist:       OFF ───── 100%
```

| Product control | Backed by today |
| --- | --- |
| Height Character = `Natural` | KU100 measured profile |
| Height Character = `Enhanced` | Analytic B (torso/shoulder cue, exaggerated notch sweep) |
| Room Assist | existing `room.amount` + `room.early_late`, now with direction-rendered reflections |
| Analytic A | moves to an Advanced/debug section — comparison only |

Intended combinations: `Natural + Room OFF` dry and natural; `Natural + Room 30%`
natural in a room; `Enhanced + Room OFF` height pushed by timbre; `Enhanced + Room 30%`
the maximum-effect setting for audio drama.

Prerequisites before renaming anything:

1. Round 2 listening decides whether KU100 is actually good enough to be called
   "Natural". If it is not, the whole two-axis naming needs rethinking.
2. KU100 must work at every sample rate, not just 48 kHz — a `Natural` label that
   silently becomes Analytic B at 44.1 kHz is worse than the current honest wording.
3. `hrtf.profile` keeps its permanent ID and index order; only display names change.

## Round 3 — Elevation Lab (implemented, awaiting tuning)

Round 2 did not resolve it either: still hard to tell which is up. Rather than add
another general-purpose model, the approach changes — **tune the curve by ear for this
listener and these headphones**, and freeze the result.

Criterion is explicitly *"does it feel like something is there"*, judged on the user's
own Z1R with real material. Not a blind hit-rate.

**Elevation Lab** (button in the bottom bar) opens a tuning bench with three independent
anchors — `Below −60`, `Level 0`, `Above +60` — each carrying eight values: notch
frequency / depth / Q, companion peak ratio and height, HF shelf, torso delay and amount.
Intermediate angles interpolate between anchors, and beyond ±60° the trend is
extrapolated rather than flattened.

The point of the anchor form is that **up and down are no longer tied to one symmetric
expression**. Analytic B derives everything from sin/cos of elevation, so any change to
"above" drags "below" with it. Verified by test: moving the above anchor from 9.7 kHz to
14 kHz moved the rendered above notch to 13.4 kHz and left below at 4850 Hz exactly.

- Defaults reproduce Analytic B, so tuning starts from the current sound. Extrapolating
  the log-frequency line to ±90° lands on 4.2 / 11.5 kHz, which are B's own endpoints.
- Selected as profile `Custom (Elevation Lab)`; opening the Lab switches to it, because
  tuning a profile you are not listening to is pointless.
- Rebuilds happen on the message thread into a double buffer and are published with one
  atomic pointer store, so the audio thread only ever swaps a pointer and crossfades.
- Level-matched to Analytic B like every other profile.
- Values persist in the plugin state, and **Copy as C++** emits the anchor block ready to
  paste into `ElevationModel::analyticBDefaults`, which is how a good curve becomes
  permanent.

### Controls

The Lab opens with **four** controls, because 24 raw values turned out to be unusable —
correct as a bench, meaningless as something to turn while listening:

| | |
| --- | --- |
| **UP** | how far a raised source departs from ear level (0 = flat, 1 = Analytic B, 2 = double) |
| **DOWN** | the same for a lowered source, independently |
| **BODY** | shoulder-reflection strength — the low-frequency cue that survives headphone colouration |
| **FOCUS** | notch width: narrow spectral colouring vs broad tonal shift |

A macro of exactly 1.00 restores Analytic B bit-for-bit, so Reset really resets.
**Advanced…** reveals the 24 raw anchor values for finishing a curve by hand; moving a
macro afterwards rebuilds from the macros and discards those edits — one direction of
authority, no silent conflict.

## Listening round 3 result (2026-08-02)

Setup: Sony MDR-Z1R, dedicated DAC and amp, Elevation Lab macros.

> "一応上下に動いた感はあったけど まだまだ弱いかんじはした"

Some sense of vertical movement, but still weak. Better than rounds 1 and 2 — the
direction is no longer absent — but not yet a confident "it is above me".

**Accepted status: the feature exists and is honest about its strength.** Elevation is
shipped as a working control that produces a real, audible change, not as a reliable
localisation cue. That framing goes in the README rather than being quietly implied.

What this rules out: more spectral modelling on the direct path. Three rounds of it
(analytic redesign, measured KU100, torso + HRTF-rendered reflections, per-listener
tuning) each produced a measurable improvement and none produced a decisive percept.
That is the known ceiling of static, non-individualised binaural, and for a distributed
audio drama the strongest remaining lever — head tracking — is unavailable in principle,
because the listener plays back a file.

Remaining options, in the order worth trying:

1. Leave elevation as a colour and lean on production convention (script, footsteps,
   room character) for height — what audio drama actually does.
2. Individual measurement or a per-listener calibration flow, if this ever becomes a
   product other people use.
3. Head tracking, only if a real-time/interactive use case appears.

### Tuning order

1. Room OFF, real voice material, `Height Check (dry)`.
2. Work on `Above +60` until it reads as **out of the head and up**, not merely brighter.
3. Work on `Below −60` **independently** until it reads as toward the floor, not merely
   duller.
4. Sweep elevation through 0° and check the movement is continuous.
5. Only then add Room, and see whether floor/ceiling reflections reinforce the picture.
6. **Copy as C++** and hand the block back so it can be frozen as the default.

### Round 2 listening protocol

1. `Height Check (room)`, Analytic B, sweep ELEVATION -90 → 0 → +90 slowly.
2. Compare against `Height Check (dry)` at the same setting — the room version should be
   clearly more convincing. If it is not, the reflection path is not helping and the next
   lever is head tracking, not more spectral modelling.
3. Then A vs B vs KU100 within `Height Check (room)`.
4. Pink noise and claps as well as voice.

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
