# NekoSpace Reverb — Audition Shell

Status: **Phase 3.5 completed; retained as the audition-host contract for Room Body v2**. This
document defines the smallest honest way to hear the current processor before the
permanent product surface exists.

The owner selected the 16-line path on 2026-08-23. Room Body v1 kept the same three hosts
for its first Tail/Body comparison. The 2026-08-24 listening result was dominated by a
small level increase, so v2 adds isolation, fixed matching and wet-only Mono Input as
defined in [room-body.md](room-body.md).

## Purpose

The original Phase 3.5 build answered whether the 16-line late field was more useful on
voice than the retained 8-line fallback. That historical comparison is closed: 16 lines
are accepted, while 8 lines remain analysis/regression evidence only.

The same shell now auditions whether the implemented six-reflection Room Body adds a
useful boundary around that accepted tail. It is not presented as the finished room model.
Order-2/3 reflections, final Binaural output, ducking, production presets and the complete
control set remain later phases.

## One processor and one editor

The build has three hosts, but only one audio product implementation:

```text
DAW ───────────────► NekoSpaceReverbProcessor ─► NekoSpaceReverbEditor
JUCE Standalone ───► NekoSpaceReverbProcessor ─► NekoSpaceReverbEditor
Reverb Player ─────► NekoSpaceReverbProcessor ─► NekoSpaceReverbEditor
       └─ file/device transport only
```

- `NekoSpace Reverb.vst3` and the generated Standalone application are built from the
  same processor/editor target.
- `NekoSpace Reverb Player` hosts that exact processor and editor. It owns only Open,
  Play/Stop, Loop, timeline and audio-device controls.
- The Player must not duplicate DSP parameters, presets, meters or production UI.
- This slice has no offline export. Recording a demonstration is a host/video workflow,
  not part of the reverb's audio contract.
- File playback accepts formats supported by the registered JUCE readers (WAV, AIFF and
  FLAC in the first Windows build). Private source audio remains globally ignored and is
  never a repository fixture.

## Provisional listening controls

The current editor exposes only the controls required to judge the Room Body prototype:

- Bypass;
- Space;
- Distance;
- Definition;
- Pre-delay;
- Decay;
- Bass Tail;
- Air Tail;
- Mix;
- Mono Input, a saved/automatable wet-feed control whose dry path stays stereo.

These names follow [control-design.md](control-design.md), but their IDs, ranges and
defaults remain prototype data until Phase 6. Sessions saved with an audition build are
not a compatibility promise. The editor identifies this as `ROOM BODY PROTOTYPE` so the
missing later reflection orders and spatial modes cannot be mistaken for a finished
product.

## Current Tail / Body / ER comparison

The developer audition control exposes three views of the continuously running wet graph:

| Mode | Audible wet buses |
| --- | --- |
| `Tail Only` | accepted 16-line late tail |
| `Room Body` | six first-order reflections plus the same late tail |
| `ER Solo` | six first-order reflections only |

The mode contract is:

- it is not exposed to host automation;
- it is not serialized in plug-in state;
- `Room Body` is selected on startup;
- independent Early/Late output gains crossfade over 50 ms with no allocation, reset or
  rebuild in the callback;
- both buses keep processing in every mode;
- normal Mix remains active, so `ER Solo` needs Mix 100% when dry must be absent.

Tail/Body listening uses one recorded fixed audition gain and the same source/settings at
Mix 100%, matching integrated wet level within 0.1 dB. `ER Solo` may use a separately
recorded monitoring gain based on the first 50 ms; that gain is diagnostic only. No live
AGC or saved product Wet Trim is introduced.

Bypass is separate from that developer switch. It crossfades to dry over 50 ms, feeds
silence to the prepared room while bypassed so the tail cools down, and reaches exact dry
in steady state.

## Historical 8-line / 16-line comparison

The Phase 3.5 network selector was an unsaved developer control. Both networks were
prepared before audio start, and switching used a bounded crossfade without allocation or
rebuild in the audio callback. The accepted 16-line candidate started selected and the
8-line network was its comparison fallback. That control disappeared when Phase 3 closed.
Line count is not a marketing control or a permanent session dependency.

## Exit checks

This checkpoint is complete only when:

1. VST3, generated Standalone and Player build from the same processor/editor sources;
2. Player file playback reaches the actual plug-in processing path;
3. Mix at zero and Bypass produce the input exactly;
4. 8/16 switching stays finite, preallocated and click-bounded;
5. state round-trip restores provisional user controls but not the audition network;
6. Reverb and existing Binaural/CleanVoice tests still pass;
7. no private audio or rendered demonstration is tracked.

Those checks record the completed Phase 3.5 history. At `84a4df4`, Room Body v2's Mono
Input, three continuously running modes, fixed matching and bounded retune passed their
DSP/state tests; the complete Release CTest set passed 7/7 and pluginval 1.0.4 passed
strictness 10 for three randomised VST3 repeats. The real Player layout and each new
control were checked manually. Matched sighted owner listening was accepted on 2026-08-28,
and FL Studio loaded and processed the VST3; see [room-body.md](room-body.md). The full
Phase 7 host matrix and Steinberg validator remain incomplete.
