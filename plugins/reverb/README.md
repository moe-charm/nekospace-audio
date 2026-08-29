# NekoSpace Reverb

Room Body prototype, voice-focused algorithmic reverb for music, audio drama and headphone
listening. It is **headphone-first, not headphone-only**: binaural wet rendering is the
hero mode, while a conventional stereo wet output keeps the plug-in useful on sends and
speakers.

The JUCE-free independent DSP core now exists. Its stereo wet excitation preserves Mid
and Side in separate deterministic 16-line networks. Each feedback line now follows a
smooth three-band decay curve driven by provisional Mid Decay, Bass Tail and Air Tail
settings. The Phase 0 analyzer continues to render the exact Binaural baseline for
comparison. The 16-line FWHT network with four input allpasses passed the measured
comparison and owner listening; the historical 8-line type remains only for analysis and
regression evidence.

The explicitly provisional listening surface uses the same Reverb
processor/editor builds as VST3 and JUCE Standalone, while Reverb Player adds file and
device transport around those exact objects. Room Body v1 implements a bounded six-image
early field in front of the accepted tail. Owner listening found its first comparison
mainly slightly louder. Room Body v2 is now implemented at `84a4df4`: matched
Tail/Body/ER isolation, a wet-only Mono Input, one bounded surface/onset retune and a
general mono-compatible ER spread stage. Engineering validation and the sighted matched
owner audition are complete. The 2026-08-28 owner session accepted the current result as
natural, and the same VST3 loaded and processed audio in FL Studio. This does not freeze the
permanent Phase 6 parameter contract or make the prototype a public alpha.

![NekoSpace Reverb](../../docs/images/reverb-main.png)

▶ [Watch the 101-second feature tour on YouTube](https://youtu.be/lT10UuXTyAE)
— headphones or earphones recommended. This is an operation and feature tour, not the final
sound-quality comparison.

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
  invariants, including longer-term controls outside the first alpha.
- [Parameter contract](docs/parameter-contract.md) — the ten host-facing alpha controls,
  permanent IDs, ranges, defaults and order.
- [State format](docs/state-format.md) — formal schema 1 plus the schema-0 development-state
  bridge.
- [Validation](docs/validation.md) — measurement harness, hard safety gates and provisional
  acoustic targets.
- [30-minute realtime benchmark](docs/realtime-benchmark-2026-08-29.md) — actual processor
  CPU distribution, callback allocation, process memory and the still-open worst-time gate.
- [8-line baseline findings](docs/baseline-8line.md) — the first reproducible 48 kHz
  measurement and what it says about the current tail.
- [Phase 3 network decision](docs/phase3-network-decision.md) — level-matched 8/16-line
  density, periodicity, decay, CPU and memory evidence.
- [Audition shell](docs/audition-shell.md) — the shared VST3/Standalone/Player boundary
  and current Room Body audition path, with the completed 8/16 gate retained as history.
- [Room Body prototype](docs/room-body.md) — the first-order reflection, control,
  real-time and listening contract for Phase 4A.
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
65 ms and 0.180. Phase 3 owner listening accepted the 16-line result; 8 lines remain a
historical regression path rather than a live product choice.

## Room Body audition applications

A Release build produces:

```text
build/plugins/reverb/NekoSpaceReverb_artefacts/Release/VST3/NekoSpace Reverb.vst3
build/plugins/reverb/NekoSpaceReverb_artefacts/Release/Standalone/NekoSpace Reverb.exe
build/plugins/reverb/NekoSpaceReverbPlayer_artefacts/Release/NekoSpace Reverb Player.exe
```

Use the Player to open or drop a WAV, AIFF or FLAC take. Room Body v2 compares `Tail Only`,
`Room Body` and `ER Solo` through unsaved developer modes. `Mono Input` is different: it
is a saved, automatable Bool that feeds `0.5 * (L + R)` to the wet room only, leaving dry
stereo and the output bus unchanged. All mode and input transitions are specified at
50 ms. Bypass still feeds silence to the room and becomes exact dry in steady state.

The owner-audition UI also provides six complete factory starting points: `Default`,
`Voice Booth`, `Small Wood Room`, `Dialogue Stage`, `Soft Chamber` and `Open Hall`.
`Reset` applies Default. Preset names are not serialized; the authoritative APVTS values
are saved, and editing any member of a known tuple displays `Custom`. The same six names
are exposed to VST3 hosts as factory programs.

At `84a4df4`, Room Body v2 passed the complete Release CTest set 7/7 and pluginval 1.0.4
at strictness 10 for three randomised VST3 repeats. The matched default Body/Tail
difference is `-0.000064 dB`; every 50 ms mode/Mono transition remained far below the
`0.06` click-sized step guard. GUI layout and all four controls were also checked in the
real Player. On 2026-08-28 the owner accepted the current direction as natural in sighted
audition, and FL Studio loaded and processed the VST3 successfully. Exact conditions and
the v1 comparison history are in [room-body.md](docs/room-body.md). Audio and rendered
demonstrations remain globally ignored. CPU/memory evidence, release packaging and the
Steinberg validator remain public-alpha gates. The first 30-minute Release run records
p99 at 2.7975%, zero callback allocation/free and no memory growth, but formally fails the
25% worst-time gate because one of 1,350,000 callbacks reached 28.815%. Steinberg
Validator 3.8.1 extensive mode passes 537/537 after its first run exposed and prompted a
fix for an unnamed factory-program entry.

License: **AGPLv3-or-later**, like the rest of NekoSpace Audio.
