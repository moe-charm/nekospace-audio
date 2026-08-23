# NekoSpace Reverb — Room Body Prototype

Status: **Phase 4A implemented; engineering gates 7/7, owner listening pending**.

The 16-line late field has passed its engineering comparison and owner listening. This
slice keeps that tail unchanged and adds the first audible room boundary in front of it.
It is deliberately large enough to judge as a product feature, but small enough that a
bad reflection can still be identified rather than hidden inside 62 candidates.

## Scope

Phase 4A delivers:

- six first-order shoebox images: left, right, front, back, floor and ceiling;
- distance loss, surface-dependent damping and bounded stereo/per-ear arrival cues;
- M/S early-field input with full Mid and `earlySideRetention = 0.5`;
- a common wet pre-delay followed by separate early and late paths;
- a later late-field excitation so sparse early arrivals overlap the dense 16-line tail;
- provisional `Distance`, `Definition` and `Pre-delay` controls;
- an unsaved `Tail only` / `Room body` owner-audition switch;
- deterministic, fixed-capacity, JUCE-free processing.

Phase 4A does **not** yet deliver order-2/3 candidates, final HRTF output modes, factory
scenes, late-field coherence or the permanent parameter contract. Those require the
first-order timing and level balance to survive listening first.

## Signal path

```text
Stereo input
   ├──────────────────────────────────────────── Dry
   └─ common wet pre-delay
         ├─ six first-order images ───────────── Early
         └─ bounded late-onset delay
                └─ existing 16-line/4-stage FDN ─ Late

Early + Late ─► wet trim ─► dry/wet mix ─► Stereo output
```

The late diffuser is not placed before the early path. Early and late are never switched
at a hard time boundary: the first FDN energy is delayed into the early-arrival region
and both paths coexist. `Definition` changes their relationship without changing Decay.

The accepted late path is one 16-line network per Mid/Side channel with four late-input
allpasses. The former 8-line implementation is retained only for historical measurement
and regression; it is not processed in parallel and is not a current product option.

## Geometry and projection

The listener is fixed at the centre of a hidden shoebox. `Space` scales bounded room
dimensions while `Distance` moves a virtual wet-field source forward from the listener.
The dry signal is never delayed, attenuated or repanned by this geometry.

For each image the renderer derives:

- physical path length and arrival time;
- inverse-distance reflection gain with a bounded floor;
- left/right projection from horizontal direction;
- a small far-field per-ear path offset;
- deterministic surface damping, including distinct floor, ceiling and rear character.

This is a stereo/dual-ear first-order renderer, not the final measured-HRTF mode. It must
remain speaker-compatible and give headphones useful lateral and surface contrast. The
dedicated `Stereo` / `Binaural` choice and reflection FIR renderer remain Phase 4B after
this timing/energy prototype is accepted.

Early reflections retain the complete Mid component and half of the Side component
(`earlySideRetention = 0.5`) before L/R reconstruction. This keeps duplicated mono exact
and carries existing stereo difference information without isolating the two input
channels into unrelated rooms.

## Provisional controls

These values are audition data, not frozen release contracts.

| Control | Prototype range/default | Promise in this slice |
| --- | --- | --- |
| `Distance` | 0–100%, default 25% | changes wet-field source distance, reflection timing and early/late perspective without moving dry |
| `Definition` | 0–100%, default 65% | higher values expose clearer early boundaries and delay the dense-tail onset; lower values blend them |
| `Pre-delay` | 0–120 ms, default 12 ms | delays the complete wet room; physical reflection spacing remains relative to that origin |

`Space`, `Decay`, `Bass Tail`, `Air Tail` and `Mix` retain their Phase 3.5 meanings. The
new controls remain provisional APVTS parameters so they can be exercised in a DAW, but
their current IDs/ranges do not carry a release compatibility promise.

## Owner comparison

`Tail only` / `Room body` is a developer audition switch:

- not exposed to host automation;
- not serialized;
- starts in `Room body`;
- crossfades the early contribution over 50 ms;
- never rebuilds or reallocates in the callback.

The old 8/16 switch is removed from the editor. `ReverbCore8` remains compiled only as a
regression/reference type; the shipping path now processes the accepted 16-line network
once rather than paying for two networks continuously.

## Real-time contract

- All reflection and pre-delay storage is allocated in `prepare`.
- Six reflection slots are fixed arrays; no callback container mutation is permitted.
- `RoomBodyCore::setSettings` is owned and called only by the audio thread; UI and worker
  threads never mutate the fixed first-order core concurrently.
- Continuous control changes update preallocated targets and bounded smoothers only.
- Pre-delay, per-ear reflection delays and late-excitation geometry delay slew by no more
  than 0.5 sample of delay per processed sample.
- Processing accepts odd or larger-than-advertised blocks through prepared chunking at
  the JUCE edge.
- No file access, logging, string work, lock or allocation occurs in processing.
- Future order-2/3 geometry is an immutable off-thread snapshot and is not smuggled into
  this fixed first-order implementation.

Bypass crossfades the processed output to dry over 50 ms. While bypass is requested, the
core receives silence rather than new input, so its existing tail cools down naturally
without clearing large buffers. Steady bypass is exact dry, and re-enabling cannot expose
programme material accumulated while bypassed.

## Phase 4A validation status

The current automated engineering gate is **7/7**:

1. All six calculated paths are finite and mirror-consistent; at 48 kHz the rendered
   earliest isolated ER arrives at sample 420 for a 421.168-sample minimum target (within
   the two-sample Hermite-interpolation allowance).
2. `Distance`, `Definition` and `Pre-delay` move the intended timing/energy quantities.
3. `Mix = 0` and steady Bypass remain exact dry.
4. Tail-only comparison changes no serialized state and produces a bounded transition.
5. Static processing is block-size invariant.
6. The callback allocates nothing after `prepare`.
7. Existing 16-line density/T60 regressions and the current product tests pass.

The sustained-sine extreme automation test measured a maximum adjacent output step of
`0.0123987` against its `0.06` ceiling. A four-second worst-gain ER+tail impulse measured
`0.158439` peak against its `1.0` runaway/headroom ceiling. The complete repository CTest
set passes 7/7 in Release, and pluginval 1.0.4 passes strictness 10 for three randomised
repeats on the VST3.

The only open Phase 4A product decision is owner listening: confirm that the room boundary
is useful on voice, without a discrete slap or damaged stereo image. The provisional
bandwise 1 dB ER/late seam analysis in [validation.md](validation.md) has not yet been
automated and remains pre-release acoustic work; no seam result is claimed here. The
Steinberg VST3 validator subtest was skipped because no validator path was configured, so
that separate Phase 7 host/release gate has **not** passed implicitly.
