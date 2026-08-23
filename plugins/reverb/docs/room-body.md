# NekoSpace Reverb — Room Body Prototype

Status: **Room Body v1 baseline recorded; v1 owner audition complete; Room Body v2
contract defined, implementation pending**.

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
- a controlled, deterministic early Side component for mono wet excitation;
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
              ├─ six first-order images ── fixed audition trim ── Early
              └─ bounded late-onset delay
                     └─ accepted 16-line/4-stage FDN ─────────── Late

Audition gains(Early, Late)
   Tail Only = (0, 1)   Room Body = (1, 1)   ER Solo = (1, 0)

Dry + Mix * selected wet buses ───────────────────────────────► Stereo output
```

Early and late buses always continue processing. Audition modes change only prepared
50 ms output-gain smoothers, so returning to a bus cannot expose reset or stale state.
`ER Solo` still obeys the normal Mix control; set Mix to 100% to hear no dry signal.

The fixed audition gain is calculated offline and recorded with its render conditions.
It is not a live level follower, saved parameter or future product Wet Trim. Tail/Body
comparison uses one fixed post-wet gain and must match integrated wet level within
`0.1 dB`; adaptive gain control is forbidden.

## Wet Mono Input

`Mono Input` is a real user parameter, separate from the unsaved audition modes.

| State | Wet excitation | Dry path and output bus |
| --- | --- | --- |
| Off | original L/R | original stereo L/R |
| On | both wet inputs receive `0.5 * (L + R)` | original stereo L/R; output remains stereo |

The Mid signal is unchanged and the wet Side input moves to/from zero over 50 ms. The dry
samples are captured before this operation and are never collapsed. A producer wanting
the entire track in mono should use the DAW's channel utility instead.

For mono wet excitation, v2 may derive a small decorrelated early Side signal from two
different prepared allpass paths. It must not use an unprocessed polarity-inverted copy.
The generated Side must cancel exactly under mono fold-down, keep L/R energy balanced and
remain bounded below the Mid energy. The accepted late field is not redesigned here.

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
may change only the existing six surface gains/cutoffs and the Definition/late-onset
mapping. It records the final constants and diagnostics after implementation. Adding
reflections, HRTF convolution, a new FDN or adaptive normalization requires a later phase.

Early reflections retain the complete Mid input and half of the original Side input
before L/R reconstruction. `Mono Input` removes the original Side only from the wet feed;
the bounded generated early Side described above can still give a centred mono source a
speaker-compatible room width.

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

At commit `b0c0475`, the Room Body v1 targeted checklist passed 7/7. Separately, the
complete Release CTest set passed 7/7 and pluginval 1.0.4 passed strictness 10 for three
randomised VST3 repeats. Those are v1 baseline results. They do not cover Mono Input,
three-bus isolation, matched audition or the bounded retune. The Steinberg validator was
not configured and its pluginval subtest was skipped.

| v2 gate | Required result | Docs-first status |
| --- | --- | --- |
| State | Mono Input saves/restores; missing old value becomes Off; audition mode is untouched | NOT RUN |
| Wet-only mono | On uses `0.5 * (L + R)` only for wet; Mix 0 and steady Bypass preserve original L/R exactly | NOT RUN |
| Mode identity | Tail, Body and ER Solo expose the defined buses while both DSP paths keep advancing | NOT RUN |
| Isolation | static `Room Body - Tail Only` equals isolated ER within numerical tolerance before fixed trim | NOT RUN |
| Transition | Mono Input and all mode changes remain finite, click-bounded and allocation-free | NOT RUN |
| Spatial safety | mono wet has balanced non-identical L/R, bounded Side and exact mono fold-down | NOT RUN |
| Matching | default Tail/Body integrated wet levels differ by no more than `0.1 dB`; fixed gain and render conditions are recorded | NOT RUN |
| Regression | prior Room Body gates, complete Release CTest and pluginval are rerun | NOT RUN |
| Listening | sighted, matched owner audition records accept/defer/remove; no blind test is required | NOT RUN |

The listening question is deliberately perceptual: does the matched Body sound more like
a believable room boundary than Tail Only, not merely different or louder? If one bounded
v2 retune still produces only level/tone change, Phase 4A stops for redesign instead of
accumulating more reflection features.
