# NekoSpace Reverb — Audition Shell

Status: **Phase 3.5 completed; retained as the audition-host contract**. This document defines the smallest honest way
to hear the current late-field candidate before early reflections and the permanent
product surface exist.

The owner selected the 16-line path on 2026-08-23. Phase 4A keeps the same three hosts
and replaces the temporary network selector with the Room Body comparison defined in
[room-body.md](room-body.md).

## Purpose

The audition build answers one question: does the measured 16-line late field sound more
useful on voice than the retained 8-line fallback? It is not presented as the finished
room model. Directional early reflections, Binaural output, ducking, production presets
and the complete control set remain later phases.

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

The first editor exposes only the controls required to judge the existing late tail:

- Bypass;
- Space;
- Decay;
- Bass Tail;
- Air Tail;
- Mix.

These names follow [control-design.md](control-design.md), but their IDs, ranges and
defaults remain prototype data until Phase 6. Sessions saved with an audition build are
not a compatibility promise. The editor must label the build as `LATE TAIL PROTOTYPE` so
missing early reflections or spatial modes cannot be mistaken for a finished product.

## 8-line / 16-line comparison

Network selection is a developer audition control, not a product parameter:

- it is not exposed to host automation;
- it is not serialized in plug-in state;
- both networks are prepared before audio starts;
- switching uses a bounded crossfade and performs no allocation or rebuild in the audio
  callback;
- the 16-line candidate is selected on startup, while 8-line remains the comparison
  fallback.

The control disappears when the Phase 3 listening decision closes. Line count never
becomes a marketing control or a permanent session dependency.

## Exit checks

This checkpoint is complete only when:

1. VST3, generated Standalone and Player build from the same processor/editor sources;
2. Player file playback reaches the actual plug-in processing path;
3. Mix at zero and Bypass produce the input exactly;
4. 8/16 switching stays finite, preallocated and click-bounded;
5. state round-trip restores provisional user controls but not the audition network;
6. Reverb and existing Binaural/CleanVoice tests still pass;
7. no private audio or rendered demonstration is tracked.
