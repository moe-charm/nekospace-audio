# NekoSpace Reverb — Control Design

Status: **provisional names and semantics; not a parameter contract**.

No parameter ID, range, default, choice order or preset format is frozen by this document.
Those are created only after the DSP baseline and listening prototypes exist. Once the
first release freezes them, the repository-wide [state rules](../../../docs/state-format.md)
apply permanently.

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
Definition, Envelopment, Motion, Pre-delay, Ducking, Mix
```

Like Binaural, a factory preset must set every sound-shaping control it owns. Loading the
same preset from two different starting states must produce the same room.

## Candidate factory scenes

These names are design probes, not a frozen shipping list:

- **Voice Booth** — very short, defined, minimal late field;
- **Small Wood Room** — compact geometry, warm early field, controlled tail;
- **Tiled Bathroom** — small Space with longer bright Decay, proving those controls are
  independent;
- **Stairwell** — long, narrow, reflection-led onset;
- **Dialogue Stage** — clear late-only ducking and restrained envelopment;
- **Soft Chamber** — diffuse, dark, intimate;
- **Open Hall** — slower density buildup and broad late field;
- **Headphone Embrace** — binaural envelopment demonstration, never the default speaker
  preset.

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

## Before IDs are frozen

The Phase 6 parameter contract must answer and test these open questions:

1. Does `Distance` remain distinct enough from `Definition` in real voice material?
2. Does `Envelopment` need one shared range for Stereo and Binaural, or separate IDs so
   host automation never changes meaning with output mode?
3. Is an Advanced ER/Late trim necessary, or do presets plus `Definition` cover it?
4. What mix law preserves useful A/B loudness without making correlated ER too loud at
   the midpoint?
5. Which output mode is the safe default for a new instance?

Answers come from prototypes and saved listening notes. Names and ranges are not frozen
because a research document suggested them.
