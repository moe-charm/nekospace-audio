# NekoSpace Reverb — Room Body Prototype

Status: **Phase 4A implementation contract**.

The 16-line late field has passed its engineering comparison and owner listening. This
slice keeps that tail unchanged and adds the first audible room boundary in front of it.
It is deliberately large enough to judge as a product feature, but small enough that a
bad reflection can still be identified rather than hidden inside 62 candidates.

## Scope

Phase 4A delivers:

- six first-order shoebox images: left, right, front, back, floor and ceiling;
- distance loss, surface-dependent damping and bounded stereo/per-ear arrival cues;
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
- Continuous control changes update bounded smoothers only.
- Processing accepts odd or larger-than-advertised blocks through prepared chunking at
  the JUCE edge.
- No file access, logging, string work, lock or allocation occurs in processing.
- Future order-2/3 geometry is an immutable off-thread snapshot and is not smuggled into
  this fixed first-order implementation.

## Exit checks

1. Six isolated impulse arrivals match their calculated order and remain finite at all
   supported sample rates.
2. `Distance`, `Definition` and `Pre-delay` move the intended timing/energy quantities.
3. `Mix = 0` and Bypass remain exact dry.
4. Tail-only comparison changes no serialized state and produces a bounded transition.
5. Static processing is block-size invariant.
6. The callback allocates nothing after `prepare`.
7. Existing 16-line density/T60 regressions and all other product tests still pass.
8. Owner listening finds a useful room boundary without a discrete slap or damaged voice.

