# NekoSpace Reverb — Roadmap

Each phase is a vertical slice with an explicit exit condition. A later phase does not
begin merely because the previous code compiles.

## Phase 0 — Freeze the baseline ✅

Build a JUCE-free offline IR renderer and analysis harness around the current Binaural
8-line FDN without changing its sound.

Deliver:

- deterministic impulse and programme-material rendering;
- T20/T30, NED, EDR, spectrum and autocorrelation reports;
- manifest containing commit, seed, parameters, sample rate and block sequence;
- baseline results at every supported sample rate and representative block sizes.

**Completed at `4e29a9d`.** The unchanged network is reproducible and block-size invariant;
the analyzer produces float WAV, JSON, NED, EDR, spectrum and autocorrelation artifacts.
Tests cover block sizes 1/127/512, the 44.1/48/88.2/96/192 kHz standing matrix, a known
synthetic decay and end-to-end report writing. See [baseline-8line.md](baseline-8line.md).
No GUI exists yet.

## Phase 1 — Establish the independent core ✅

Create `plugins/reverb/src/dsp` and the minimum command-line/test host. Decide and test the
stereo-to-wet excitation mapping. Extract only the generic primitives now proven to have a
second consumer.

Extraction happens in its own behaviour-preserving commit:

1. characterize the primitive in Binaural;
2. move it to `shared/` without redesign;
3. keep Binaural bit-equivalent;
4. consume it from Reverb;
5. run both products' tests.

**Exit:** mono symmetry, stereo side preservation, exact dry bypass and both product test
suites pass.

**Completed at `64b3f8a`, after the behaviour-preserving extraction at `e84844e`.** The
JUCE-free core uses independent Mid and Side 8-line networks. Tests prove exact mono
symmetry, non-collapsing pure-Side input, exact zero-mix dry identity and identical output
for block sizes 1 and 512. Binaural and Reverb suites pass together.

## Phase 2 — Frequency-dependent decay (next)

Add a smooth internal T60 curve driven by Mid Decay, Bass Tail and Air Tail. Fit a stable
per-line attenuation filter and verify the rendered result rather than coefficients only.

**Exit:** provisional T60 accuracy/linearity gates pass; Space automation preserves T60;
no coefficient update allocates or clicks.

## Phase 3 — Density and network selection

Measure the 8-line network, add 2–4 late-input diffuser stages, then build a 16-line FWHT
candidate with comparable decay and a justified total order/delay set.

Compare:

- NED and mixing time;
- EDR and decay surface;
- narrow peaks and temporal periodicity;
- sustained-vowel modulation;
- Release CPU and memory;
- level-matched owner listening.

**Exit:** record a decision to ship 8 or 16 lines. Sixteen is selected only when its gain
survives matched comparison; otherwise the simpler network remains.

## Phase 4 — Directional early field

Generalise the six-image baseline to compile-time-bounded order-3 candidates. Add
arrival/energy/level pruning, per-surface filters, Stereo projection and Binaural HRTF/ITD
rendering. Establish an overlapping ER/late transition.

**Exit:** arrival references, ER HRTF tolerances and transition continuity pass; changing
Space or a preset never allocates in the callback.

## Phase 5 — Late-field envelopment

Implement the measured frequency-dependent coherence stage behind Binaural output mode.
Keep an identical bypassed projection for comparison and a separate Stereo path.

**Exit:** coherence error, energy, mono compatibility and listening checks pass. If the
stage only changes tone or creates combing, remove it rather than shipping the feature
name.

## Phase 6 — Product controls, state and GUI

Resolve the open questions in [control-design.md](control-design.md), then create the
permanent parameter contract. Only at this point are IDs, ranges, defaults and choice
lists frozen for the first release.

Build a simple voice-production UI around perceptual controls. Developer graphs may show
T60 and density but matrix/line internals remain hidden from the production surface.

**Exit:** automation, preset determinism, state migration fixtures, GUI-closed processing,
high-DPI layout and accessibility checks pass.

## Phase 7 — Host and release validation

Build VST3 and Standalone, exercise FL Studio with fixed-size buffers on/off, save/reload,
offline render, bypass, odd/changing blocks and fast automation. Run pluginval and the
Steinberg validator when available.

**Exit:** every binding gate in [validation.md](validation.md) passes, provisional acoustic
targets are either calibrated and frozen or explicitly waived with evidence, release
artifacts contain licences/notices, and no private media is tracked.

## Post-v1 research queue

These items require a separate design decision and are not allowed to inflate v1:

- internal scattering or filter-feedback matrices if density still fails;
- velvet-noise excitation or a DVN late renderer;
- multi-slope/coupled-room decay;
- imported IR analysis and algorithm fitting;
- Freeze and creative infinite decay;
- head tracking;
- Ambisonics/surround buses;
- explicit room geometry, which belongs to the separate Room product decision.

## Stop conditions

The project pauses for redesign rather than accumulating features if any of these persists:

- frequency-dependent decay cannot meet stability and T60 tolerances;
- ER and late paths cannot overlap without a measurable energy seam;
- Binaural mode offers no benefit beyond a stereo tone change;
- the chosen network needs callback allocation or locking;
- CPU targets require hiding overruns with a limiter or silently reducing quality;
- controls cannot be explained in terms a voice producer can predict by ear.
