# NekoSpace Reverb parameter contract

Status: **freeze candidate for the first public alpha**.

Creating `reverb-v0.1.0-alpha` freezes every ID, meaning, range, default and host order in
this table. Until that tag exists the document records the candidate contract and may be
corrected together with source and migration tests. After the tag, an existing entry is
never renamed, reused, reordered or reinterpreted.

## Host parameter order

| Order | ID | Host name | Type / plain range | Default | versionHint | Meaning |
| ---: | --- | --- | --- | ---: | ---: | --- |
| 0 | `reverb.bypass` | Bypass | Bool | Off | 1 | 50 ms transition to exact original dry; the room receives silence while bypassed |
| 1 | `reverb.space` | Space | Float, 0–100%, step 0.1 | 35% | 1 | room scale and delay geometry; does not redefine Decay |
| 2 | `reverb.decay` | Decay | Float, 0.15–4.00 s, step 0.01 | 1.40 s | 1 | mid-band T60 target |
| 3 | `reverb.bassTail` | Bass Tail | Float, 25–200%, step 0.1 | 100% | 1 | low-band T60 as a ratio of Decay |
| 4 | `reverb.airTail` | Air Tail | Float, 25–200%, step 0.1 | 70% | 1 | high-band T60 as a ratio of Decay |
| 5 | `reverb.mix` | Mix | Float, 0–100%, step 0.1 | 35% | 1 | linear dry/wet blend; 0% exact dry, 100% selected wet bus only |
| 6 | `reverb.distance` | Distance | Float, 0–100%, step 0.1 | 25% | 2 | hidden wet excitation perspective and ER/late balance; never moves the dry image |
| 7 | `reverb.definition` | Definition | Float, 0–100%, step 0.1 | 65% | 2 | bounded early-boundary prominence and late-onset definition macro |
| 8 | `reverb.preDelay` | Pre-delay | Float, 0–120 ms, step 0.1 | 12 ms | 2 | common wet-room delay before early and late paths |
| 9 | `reverb.wetMonoInput` | Wet Mono Input | Bool | Off | 3 | feeds `0.5 * (L + R)` to wet processing only; dry L/R stays original |

The ranges are plain parameter values. Host automation may store normalised values; that
is why ranges and meanings are part of the compatibility contract.

## Rules after the first tag

- Additions use a new permanent ID, a higher `versionHint`, and are appended after order 9.
- A richer input-routing choice cannot replace `reverb.wetMonoInput`; it needs a new ID.
- Motion, Ducking, Envelopment, Output mode and advanced tone controls are not promised by
  this alpha. If accepted later, each is an appended parameter with its own tested default.
- `Tail Only`, `Room Body` and `ER Solo` are unsaved audition diagnostics, not parameters.
- Factory preset names are not serialized. Saved APVTS values are authoritative; changing
  a control makes the GUI display `Custom`.
- Bypass is the host-recognised bypass parameter. It is not removed in favour of wrapper
  bypass or a differently named control.

## Identity coupled to this contract

The plug-in identity is manufacturer `NkSp`, plug-in code `Nksr`, bundle ID
`audio.nekospace.reverb`. Changing any of those after the first tag creates a different
plug-in and breaks host project lookup.

