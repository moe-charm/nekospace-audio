# NekoSpace Reverb — Control Design

Status: **long-term control research plus a frozen-alpha subset**.

This document still describes controls that may be researched after the first alpha. The
ten controls implemented for that alpha are defined normatively in
[parameter-contract.md](parameter-contract.md); this broader design must not silently
change those IDs, ranges or meanings. Once the first release freezes them, the
repository-wide [state rules](../../../docs/state-format.md) apply permanently.

## Main controls

| Control | User promise | Internal responsibility |
| --- | --- | --- |
| **Space** | changes the apparent room scale without silently lengthening the decay | ER geometry, FDN delay scale and density compensation; feedback attenuation is recalculated to hold T60 |
| **Decay** | changes the mid-band reverberation time | reference mid-band T60 |
| **Bass Tail** | makes low frequencies leave sooner or linger longer | low/mid T60 ratio, not a wet-output bass EQ |
| **Air Tail** | makes high frequencies leave sooner or linger longer | high/mid T60 ratio, not brightness EQ |
| **Distance** | moves the listener's perspective from intimate to across the room without repanning the dry source | direct-to-ER timing relationship and ER/late energy balance; dry location is unchanged |
| **Definition** | moves from crisp, legible room boundaries to a blended onset | bounded macro over ER prominence and late-input diffusion |
| **Envelopment** | changes how much the late room surrounds the listener | binaural coherence target in Binaural mode; bounded width/decorrelation mapping in Stereo mode |
| **Motion** | reduces static ringing or adds an intentional living tail | bounded modulation rate/depth macro |
| **Pre-delay** | separates the whole wet room from the dry event | common delay before ER and late paths; physical ER timing remains relative to that origin |
| **Mono Input** | sends a centred mono signal into the room while leaving the original dry stereo image intact | 50 ms transition from original wet Side to zero; both wet channels receive `0.5 * (L + R)` when On |
| **Ducking** | keeps speech clear and lets the tail open after phrases | late-output attenuation only; no threshold control in the primary UI |
| **Mix** | blends aligned dry with the complete wet field | 0% is exact dry; 100% contains no dry and is valid on an aux send |
| **Output** | chooses the listening target | `Stereo` speaker-compatible wet renderer or `Binaural` headphone-optimised wet renderer |

`Tone` is an Advanced control because output equalisation and decay spectrum are distinct.
A dark `Tone` attenuates high frequencies at every time; a short `Air Tail` changes how
quickly those frequencies disappear.

## Controls deliberately hidden

The following are implementation details, not user goals:

- reflection order and pruning count;
- individual wall absorption and reflection filters;
- FDN line count and raw delay lengths;
- feedback matrix type;
- diffuser stage count and allpass coefficients;
- per-line filter anchors;
- per-line modulation rate, depth and phase;
- interaural-coherence frequency anchors.
- Room Body's fixed offline audition-matching gain and `ER Solo` monitoring gain.

They may be visible in developer diagnostics, but they are never automatable production
parameters in v1.

## Presets versus knobs

Presets own correlated structure that cannot be described by one continuous gesture:

```text
room proportions and listener perspective
surface absorption curves
ER pruning and onset shape
base diffusion and density target
late coherence target
default modulation character
wet output tone
```

Knobs own quantities a producer may automate continuously:

```text
Space, Decay, Bass Tail, Air Tail, Distance,
Definition, Envelopment, Motion, Pre-delay, Mono Input, Ducking, Mix
```

Like Binaural, a factory preset must set every sound-shaping control it owns. Loading the
same preset from two different starting states must produce the same room.

## Initial factory presets

The owner-audition build ships a deliberately small set. These are useful starting points,
not claims that the room models have completed final voicing:

| Preset | Space | Distance | Definition | Pre-delay | Decay | Bass Tail | Air Tail | Mix |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Default | 35% | 25% | 65% | 12 ms | 1.40 s | 100% | 70% | 35% |
| Voice Booth | 18% | 18% | 88% | 4 ms | 0.38 s | 85% | 58% | 18% |
| Small Wood Room | 32% | 30% | 70% | 8 ms | 0.85 s | 125% | 62% | 28% |
| Dialogue Stage | 45% | 38% | 82% | 18 ms | 1.15 s | 95% | 55% | 24% |
| Soft Chamber | 52% | 22% | 48% | 10 ms | 1.85 s | 135% | 42% | 34% |
| Open Hall | 78% | 58% | 42% | 32 ms | 3.20 s | 120% | 78% | 38% |

Every factory preset sets every sound-shaping parameter, turns Bypass off, turns Wet Mono
Input off, and returns the unsaved audition bus to Room Body. `RESET` applies `Default`.

The saved state remains parameter-authoritative: no preset name is serialized. Therefore a
session recalls its exact values even if a factory preset is renamed or retuned later. Moving
any control away from a factory tuple makes the selector display `Custom`.

## Automation rules

- Every continuous control is smoothed in the DSP core.
- `Space` automation preserves Decay within the tolerance in
  [validation.md](validation.md).
- Output-mode changes use two prepared paths or an off-thread rebuild plus bounded
  crossfade; they never replace topology abruptly.
- Preset loads are one coherent transaction. The audio thread must not hear a sequence of
  partially updated room states.
- Random modulation is reproducible after save/reload.
- No control may allocate, lock or rebuild a variable-size container from the host's
  parameter callback or audio callback.

## Questions for later appended controls

These questions remain relevant to controls that are not in the alpha contract:

1. Does `Distance` remain distinct enough from `Definition` in real voice material?
2. Does `Envelopment` need one shared range for Stereo and Binaural, or separate IDs so
   host automation never changes meaning with output mode?
3. Is an Advanced ER/Late trim necessary, or do presets plus `Definition` cover it?
4. What mix law preserves useful A/B loudness without making correlated ER too loud at
   the midpoint?
5. Which output mode is the safe default for a new instance?

Answers come from prototypes and saved listening notes. Names and ranges are not frozen
because a research document suggested them.

Room Body v2's fixed Tail/Body matching gain does not answer question 3. It is an unsaved,
offline-calibrated comparison correction, not a product ER/Late control. The provisional
`Mono Input` implementation is a saved Bool with ID `reverb.wetMonoInput`, default Off and
`versionHint = 3`; future multi-choice routing must use a new ID rather than reinterpret
that Bool. Its dry path remains stereo. The deterministic ER-spread stage is always part
of the early renderer and is not another user control coupled to this Bool.
