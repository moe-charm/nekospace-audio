# NekoSpace Reverb

Design-stage, voice-focused algorithmic reverb for music, audio drama and headphone
listening. It is **headphone-first, not headphone-only**: binaural wet rendering is the
hero mode, while a conventional stereo wet output keeps the plug-in useful on sends and
speakers.

No plug-in or DSP target exists yet. This directory freezes the product boundary and the
order in which evidence must be gathered before implementation starts.

## What it is

NekoSpace Reverb builds the space around an already positioned sound:

- image-source early reflections with direction, distance and surface filtering;
- a low-colour FDN tail with independently controlled low-, mid- and high-frequency
  decay;
- a headphone mode whose late field controls frequency-dependent interaural coherence;
- a speaker-compatible stereo mode;
- late-only voice ducking, so words stay close while the charged tail opens between
  phrases.

It is not a larger copy of the light room inside NekoSpace Binaural, and it is not a
shoebox simulator exposing dozens of engineering parameters.

| Product | Owns | Does not own |
| --- | --- | --- |
| **NekoSpace Binaural** | direct-source azimuth, elevation, distance, near-ear geometry and HRTF | a full production reverb |
| **NekoSpace Reverb** | early/late balance, decay spectrum, density, motion and envelopment | direct-source 3D placement |
| **NekoSpace Room** (possible later product) | explicit room, source and listener geometry if that workflow proves useful | musical macro reverb controls |

The dry path is never spatialised by Reverb. A source placed by Binaural or by the DAW
keeps that direct image; Reverb generates only the room around it.

## v1 promise

> A directionally legible early field and a dense, low-colour late field whose decay and
> binaural envelopment can be measured, automated safely and understood through musical
> controls.

The v1 design is intentionally narrower than the research backlog. It excludes imported
IRs, IR-to-algorithm fitting, user room geometry, Ambisonics, head tracking, freeze,
multi-slope decay and a configurable feedback matrix.

## Design documents

- [Architecture](docs/architecture.md) — product boundary, signal path and binding DSP
  decisions.
- [Control design](docs/control-design.md) — provisional user controls and their DSP
  invariants; parameter IDs are deliberately not frozen yet.
- [Validation](docs/validation.md) — measurement harness, hard safety gates and provisional
  acoustic targets.
- [Roadmap](docs/roadmap.md) — vertical implementation slices and the exit condition for
  each one.
- [Research basis](docs/research-basis.md) — what is evidence, what is a product decision
  and what remains a hypothesis.

Repository-wide state, thread and licensing rules remain authoritative:
[state format](../../docs/state-format.md),
[realtime contract](../../docs/realtime-contract.md),
[repository layout](../../docs/repo-layout.md), and
[third-party licences](../../docs/third-party-licenses.md).

## First implementation

The first deliverable is **not a GUI**. It is an offline impulse-response and measurement
harness around the existing 8-line Binaural FDN, frozen as a baseline. Frequency-dependent
T60 comes next. A 16-line network is adopted only if the same harness shows a useful
improvement over the 8-line baseline.

License: **AGPLv3-or-later**, like the rest of NekoSpace Audio.
