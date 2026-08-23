# NekoSpace Reverb — Room Body Prototype

Status: **Room Body v1 baseline recorded; Room Body v2 implemented and
engineering-validated at `84a4df4`; matched owner audition pending**.

The accepted 16-line late field remains unchanged. Room Body v1 placed six first-order
shoebox reflections in front of it. On 2026-08-24 the owner heard the difference, but
described `Room Body` mainly as slightly louder than `Tail Only`, not yet as a clearly
more realistic room boundary. That observation closes the v1 listening round without
accepting the six-reflection balance as a finished product.

## v1 diagnosis

The following numbers describe the reference/default v1 render. They are observations,
not product targets and not a loudness-matched superiority claim.

| Diagnostic | v1 result | Meaning |
| --- | ---: | --- |
| integrated `Room Body - Tail Only` wet level | about `+0.17 dB` | the comparison had no implemented wet compensation |
| `Room Body - Tail Only`, first 50 ms | about `+3.2 dB` | ER energy exists, but is concentrated near onset |
| six absolute ER arrival times | about `20.8–37.7 ms` | reproducible timing reference at default settings |
| duplicated-mono wet output | bit-identical L/R | safe and centred, but supplies no stereo room spread |
| isolated wet trim shown in the old signal diagram | not implemented | removed from the current contract |

The floor image was also the strongest and brightest v1 surface, and the late excitation
started before much of the explicit reflection cluster. Room Body v2 therefore performs
one bounded retune of the existing six images; it does not add more reflection orders or
replace the accepted late network.

## v2 scope

Room Body v2 delivers:

- the same six first-order images: left, right, front, back, floor and ceiling;
- reduced floor dominance and more clearly differentiated surface damping;
- a later, overlapping late-field onset rather than an early tail masking the images;
- a controlled, deterministic and mono-compatible early Side component for every wet
  excitation;
- three unsaved developer audition modes: `Tail Only`, `Room Body` and `ER Solo`;
- one fixed, offline-calibrated audition gain for matched `Tail Only` / `Room Body` level;
- a saved, automatable `Mono Input` parameter that sums only the wet feed;
- deterministic, fixed-capacity, JUCE-free and allocation-free callback processing.

It still excludes order-2/3 candidates, final HRTF output, factory scenes, a product
ER/Late trim and adaptive loudness normalization. Those must not be smuggled into this
bounded listening correction.

## Signal path

```text
Stereo input
   ├──────────────────────────────────────────────────────── Original dry L/R
   └─ wet input: original L/R or 0.5 * (L + R)
         └─ common wet pre-delay
              ├─ six first-order images ── general ER spread ─── Early
              └─ bounded late-onset delay
                     └─ accepted 16-line/4-stage FDN ─────────── Late

Audition gains(Early, Late)
   Tail Only = (0, 1)   Room Body = (k, k)   ER Solo = (1, 0)
   k = 0.9913 fixed audition trim

Dry + Mix * selected wet buses ───────────────────────────────► Stereo output
```

Early and late buses always continue processing. Audition modes change only prepared
50 ms output-gain smoothers, so returning to a bus cannot expose reset or stale state.
`ER Solo` still obeys the normal Mix control; set Mix to 100% to hear no dry signal.

The fixed audition gain is calculated offline and recorded with its render conditions.
It is not a live level follower, saved parameter or future product Wet Trim. Tail/Body
comparison applies the same fixed gain to the summed Early and Late buses and must match
integrated wet level within `0.1 dB`; adaptive gain control is forbidden.

## Wet Mono Input

`Mono Input` is a real user parameter, separate from the unsaved audition modes.

| State | Wet excitation | Dry path and output bus |
| --- | --- | --- |
| Off | original L/R | original stereo L/R |
| On | both wet inputs receive `0.5 * (L + R)` | original stereo L/R; output remains stereo |

The Mid signal is unchanged and the wet Side input moves to/from zero over 50 ms. The dry
samples are captured before this operation and are never collapsed. A producer wanting
the entire track in mono should use the DAW's channel utility instead.

For every wet excitation, v2 derives a small decorrelated early Side signal from two
different prepared allpass paths. This is a general ER-spread stage, not a feature that
appears only when `Mono Input` is On. With a mono wet feed it supplies the missing early
width; with stereo it remains bounded beside the retained original Side. It does not use
an unprocessed polarity-inverted copy. The generated Side cancels under mono fold-down,
keeps L/R energy balanced and remains below the Mid energy. The accepted late field is
not redesigned here.

The provisional saved-state contract is:

- bool APVTS parameter `reverb.wetMonoInput`, host name `Wet Mono Input`;
- default Off, appended after every existing parameter with `versionHint = 3`;
- missing old-state value restores Off; schema version remains 0 because this is an
  additive field;
- the three audition modes remain unsaved and unautomatable, starting at `Room Body`.

If future work needs Left/Right/Mid input choices, it adds a new ID rather than changing
this Bool into a Choice and reinterpreting saved normalized values.

## Geometry and bounded retune

The listener remains fixed at the centre of a hidden shoebox. `Space` scales bounded room
dimensions while `Distance` moves only the virtual wet excitation forward. The dry source
is never delayed, attenuated or repanned by this geometry.

For each image the renderer derives physical path length, inverse-distance level,
left/right projection, far-field per-ear offset and deterministic surface damping. v2
changed only the existing six surface gains/cutoffs and the Definition/late-onset
mapping. Adding reflections, HRTF convolution, a new FDN or adaptive normalization
requires a later phase.

Early reflections retain the complete Mid input and half of the original Side input
before L/R reconstruction. `Mono Input` removes the original Side only from the wet feed;
the bounded generated early Side described above can still give a centred mono source a
speaker-compatible room width.

## Recorded v2 implementation and measurements

The bounded retune uses these internal surface values. Cutoff interpolates geometrically
between the soft and hard endpoints as `Definition` moves from 0 to 1.

| Surface | gain | soft cutoff | hard cutoff |
| --- | ---: | ---: | ---: |
| Left | 0.68 | 6500 Hz | 12000 Hz |
| Right | 0.68 | 6500 Hz | 12000 Hz |
| Front | 0.64 | 6000 Hz | 11000 Hz |
| Back | 0.58 | 4500 Hz | 8000 Hz |
| Floor | 0.46 | 3200 Hz | 6000 Hz |
| Ceiling | 0.52 | 4000 Hz | 8000 Hz |

The late excitation offset after common pre-delay is
`6 + 18 * Definition + 4 * Space` ms. The ER-spread stage uses two prepared allpasses
(257 and 379 samples at 48 kHz, coefficient 0.55), then a 250 Hz high-pass, 8 kHz
low-pass and amount 0.22. Its `M + S` / `M - S` reconstruction is shared with the
floating-point-tolerance fold-down test.

The reference measurement used 48 kHz, block size 127, four seconds, default settings,
Mix 100% and a duplicated-mono stereo unit impulse. Energy is the raw sum of squared L/R
samples; it is diagnostic and not a loudness claim.

| Measurement | v2 result |
| --- | ---: |
| untrimmed integrated `Body - Tail` | +0.076579 dB |
| untrimmed 0–50 ms `Body - Tail` | +4.49981 dB |
| raw `ER Solo` L+R energy, 0–50 ms | 0.0179523 |
| fixed Body audition trim | 0.9913 |
| matched integrated `Body - Tail` | -0.000064 dB |
| first ER output above 1e-9 | 20.75 ms |
| first Late output above 1e-9 | 38.6042 ms |
| mono ER Side/Mid | -20.4056 dB |
| mono ER L/R correlation | 0.981956 |
| M/S fold-down maximum float error | 5.96046e-08 |
| maximum Wet Mono transition step | 0.000460103 |
| maximum of all six directed mode transitions | 0.000459019 |
| explicit all-mode 2400-sample ramp error | 0 |

Both transition measurements use continuous low-level stereo tones, the exact 2400-sample
50 ms ramp and a click-sized guard of 0.06. After the ramp, every switched mode matches a
reference instance whose target bus had run continuously, proving that muted buses are
not frozen or stale.

## Provisional controls

These values are audition data, not a frozen release contract.

| Control | Prototype range/default | Promise in this slice |
| --- | --- | --- |
| `Distance` | 0–100%, default 25% | changes wet source distance, reflection timing and perspective without moving dry |
| `Definition` | 0–100%, default 65% | changes boundary clarity, damping and late-onset overlap without changing Decay |
| `Pre-delay` | 0–120 ms, default 12 ms | delays the complete wet room; physical spacing stays relative to that origin |
| `Mono Input` | Off/On, default Off | sums only the wet feed to `0.5 * (L + R)`; dry and output remain stereo |

`Space`, `Decay`, `Bass Tail`, `Air Tail` and `Mix` retain their Phase 3.5 meanings.

## Real-time contract

- All delay, reflection, allpass and work storage is allocated in `prepare`.
- Six reflection slots and any decorrelation stages use fixed capacity.
- `RoomBodyCore::setSettings` is audio-thread-owned; UI and worker threads do not mutate
  its signal state concurrently.
- Continuous control and audition changes update bounded smoothers only.
- Pre-delay, per-ear reflection delays and late-excitation delay move by no more than
  0.5 sample per processed sample.
- Processing accepts odd or larger-than-advertised blocks through prepared chunking.
- No file access, logging, string work, lock, allocation or topology rebuild occurs in
  processing.

Bypass crossfades toward dry over 50 ms. As soon as bypass is requested the room receives
silence, while the output transition releases the existing tail. Steady bypass is exact
dry and re-enabling cannot reveal programme material accumulated while bypassed.

## Validation history and v2 exit checks

At commit `84a4df4`, the complete Release CTest set passed 7/7. pluginval 1.0.4 passed
strictness 10 for three randomised VST3 repeats, including editor automation and state
restoration. The VST3, generated Standalone and Player all built. In the actual Player,
the three audition buttons and Mono Input control were visible without overlap; `ER Solo`
and `Mono Input` were clicked and their selected states/notices updated. The Steinberg
validator was not configured and its pluginval subtest was skipped.

| v2 gate | Required result | Status |
| --- | --- | --- |
| State | Mono Input saves/restores; missing old value becomes Off; audition mode is untouched | PASS |
| Wet-only mono | On uses `0.5 * (L + R)` only for wet; Mix 0 and steady Bypass preserve original L/R exactly | PASS |
| Mode identity | Tail, Body and ER Solo expose the defined buses while both DSP paths keep advancing | PASS |
| Isolation | static Body equals `(Tail + ER Solo) * 0.9913` within numerical tolerance | PASS |
| Transition | Mono Input and all mode changes remain finite, 50 ms, click-bounded, allocation-free and free of stale returns | PASS |
| Spatial safety | mono wet has balanced non-identical L/R, bounded Side and M/S fold-down within floating-point tolerance | PASS |
| Matching | default Tail/Body integrated wet levels differ by no more than `0.1 dB`; fixed gain and render conditions are recorded | PASS |
| Regression | prior Room Body gates, complete Release CTest and pluginval are rerun | PASS |
| GUI | real Player layout, selected states and mode notices respond correctly | PASS (manual) |
| Listening | sighted, matched owner audition records accept/defer/remove; no blind test is required | PENDING |

The listening question is deliberately perceptual: does the matched Body sound more like
a believable room boundary than Tail Only, not merely different or louder? If one bounded
v2 retune still produces only level/tone change, Phase 4A stops for redesign instead of
accumulating more reflection features.
