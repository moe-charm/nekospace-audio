# NekoSpace Reverb — Audition Shell

Status: **Phase 3.5 completed; retained as the audition-host contract for Phase 4A**. This
document defines the smallest honest way to hear the current processor before the
permanent product surface exists.

The owner selected the 16-line path on 2026-08-23. Phase 4A keeps the same three hosts
and replaces the temporary network selector with the Room Body comparison defined in
[room-body.md](room-body.md).

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
- Mix.

These names follow [control-design.md](control-design.md), but their IDs, ranges and
defaults remain prototype data until Phase 6. Sessions saved with an audition build are
not a compatibility promise. The editor identifies this as `ROOM BODY PROTOTYPE` so the
missing later reflection orders and spatial modes cannot be mistaken for a finished
product.

## Current Tail-only / Room-body comparison

The current developer audition control compares the accepted tail with the same tail plus
the six first-order reflections:

- it is not exposed to host automation;
- it is not serialized in plug-in state;
- `Room body` is selected on startup;
- switching crossfades the early contribution over 50 ms and performs no allocation or
  rebuild in the audio callback;
- the 16-line tail is processed once in both positions.

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

Those checks record the completed Phase 3.5 history. For Phase 4A, the current automated
engineering gate and complete Release CTest set are both 7/7; pluginval 1.0.4 passes
strictness 10 for three randomised VST3 repeats. Owner listening is pending; see
[room-body.md](room-body.md). pluginval skipped the Steinberg VST3 validator because no
validator path was configured.
