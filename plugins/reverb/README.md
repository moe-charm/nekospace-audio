# NekoSpace Reverb

Phase 3 engineering candidate, voice-focused algorithmic reverb for music, audio drama and headphone
listening. It is **headphone-first, not headphone-only**: binaural wet rendering is the
hero mode, while a conventional stereo wet output keeps the plug-in useful on sends and
speakers.

The JUCE-free independent DSP core now exists. Its stereo wet excitation preserves Mid
and Side in separate deterministic 8-line networks. Each feedback line now follows a
smooth three-band decay curve driven by provisional Mid Decay, Bass Tail and Air Tail
settings. The Phase 0 analyzer continues to render the exact Binaural baseline for
comparison. A 16-line FWHT network with four input allpasses is now the measured default;
the 8-line fallback remains available until owner listening confirms the decision.

Phase 3.5 now provides an explicitly provisional listening surface: the same Reverb
processor/editor builds as VST3 and JUCE Standalone, while Reverb Player adds file and
device transport around those exact objects. The screen is labelled `LATE TAIL
PROTOTYPE`; directional early reflections, Binaural output and the permanent Phase 6
parameter contract do not exist yet.

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
- [8-line baseline findings](docs/baseline-8line.md) — the first reproducible 48 kHz
  measurement and what it says about the current tail.
- [Phase 3 network decision](docs/phase3-network-decision.md) — level-matched 8/16-line
  density, periodicity, decay, CPU and memory evidence.
- [Audition shell](docs/audition-shell.md) — the shared VST3/Standalone/Player boundary,
  provisional controls and unsaved 8/16 comparison.
- [Roadmap](docs/roadmap.md) — vertical implementation slices and the exit condition for
  each one.
- [Research basis](docs/research-basis.md) — what is evidence, what is a product decision
  and what remains a hypothesis.

Repository-wide state, thread and licensing rules remain authoritative:
[state format](../../docs/state-format.md),
[realtime contract](../../docs/realtime-contract.md),
[repository layout](../../docs/repo-layout.md), and
[third-party licences](../../docs/third-party-licenses.md).

## Phase 0 analyzer and Phase 3 core

Build the repository, then run:

```powershell
build\plugins\reverb\Release\nekospace_reverb_analyze.exe `
  --output reverb-analysis-output `
  --sample-rate 48000 --duration 6 --block-size 256 `
  --size 0.35 --decay 1.4 --damping 0
```

It writes a stereo float WAV plus JSON/CSV reports for band T20/T30, normalized echo
density, EDR, a 1/12-octave spectrum and 10–100 ms autocorrelation. The output directory
and all audio formats remain ignored; reports are local evidence, not production media.
The JSON embeds the git commit, dirty-state flag, compiler, build configuration, complete
settings and deterministic seed.

The independent core is covered by `reverb_dsp`: exact dry identity, exact mono symmetry,
pure-Side preservation, block-size invariance, allocation-free coefficient updates and
five supported sample rates. Rendered T20 tests cover opposite Bass/Air slopes and prove
that Space preserves a neutral T60. The selected 16-line/4-stage candidate reaches 45 ms
NED t90 and 0.140 autocorrelation at matched RMS; its 8-line/4-stage fallback measures
65 ms and 0.180. Owner listening is the remaining Phase 3 gate.

## Phase 3.5 audition applications

A Release build produces:

```text
build/plugins/reverb/NekoSpaceReverb_artefacts/Release/VST3/NekoSpace Reverb.vst3
build/plugins/reverb/NekoSpaceReverb_artefacts/Release/Standalone/NekoSpace Reverb.exe
build/plugins/reverb/NekoSpaceReverbPlayer_artefacts/Release/NekoSpace Reverb Player.exe
```

Use the Player to open or drop a WAV, AIFF or FLAC take, then compare the 8-line fallback
and 16-line candidate. That comparison is deliberately not automatable or saved. Audio
files and rendered demonstrations are globally ignored and must not be committed.

License: **AGPLv3-or-later**, like the rest of NekoSpace Audio.
