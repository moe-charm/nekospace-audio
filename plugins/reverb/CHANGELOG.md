# NekoSpace Reverb changelog

Notable changes for NekoSpace Reverb only. Suite-wide and Binaural history remains in the
top-level changelog.

## [0.1.0-alpha] — Unreleased

First public-alpha candidate for Windows x64. This section describes the candidate; the
tag is not created until every binding release gate is resolved.

### Sound and workflow

- Six bounded first-order image reflections feeding a deterministic 16-line/FWHT late
  field with four input diffuser stages.
- Independent Mid Decay, Bass Tail and Air Tail shaping, plus Space, Distance,
  Definition and Pre-delay controls.
- `Tail Only`, `Room Body` and `ER Solo` audition paths, with fixed Tail/Body matching.
- Wet-only `Mono Input`; the dry stereo path remains unchanged.
- Six named factory starting points, available from both the GUI and the VST3 host
  program list.
- VST3, JUCE Standalone and a file Player that hosts the same processor/editor.

### Verification recorded so far

- Release CTest: 7/7.
- FL Studio load and audio-processing smoke test.
- Steinberg Validator 3.8.1 extensive/local-instance run: 537/537 after fixing an unnamed
  host-program entry.
- pluginval 1.0.4 strictness 10: three randomised repeats pass with the official
  Steinberg Validator returning 0 in every repeat.
- Formal schema-1 state plus a tested reader for pre-release schema-0 prototype states.
- Thirty-minute 48 kHz/64 callback run: zero allocation/free, finite output, no private
  memory growth and p99 at 2.7975% of the block budget.

### Open release gate

- The same callback run had one 28.815% worst-time sample against the binding 25% budget.
  The candidate is not tagged until that gate passes or its contract is explicitly revised
  with stronger scheduling/profiling evidence.
