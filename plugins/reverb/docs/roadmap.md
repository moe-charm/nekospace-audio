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

## Phase 2 — Frequency-dependent decay ✅

Add a smooth internal T60 curve driven by Mid Decay, Bass Tail and Air Tail. Fit a stable
per-line attenuation filter and verify the rendered result rather than coefficients only.

**Exit:** provisional T60 accuracy/linearity gates pass; Space automation preserves T60;
no coefficient update allocates or clicks.

**Completed at `bac48a0`.** Each feedback line uses a complementary three-band attenuation
filter fitted at 125 Hz, 1 kHz and 8 kHz. Opposite Bass/Air slopes meet the calibrated
prototype tolerance, neutral curves remain within 5%, and Space 10%/90% comparisons stay
within 5%. Settings ramp for 50 ms; extreme automation has no click-sized step and performs
zero allocation in the instrumented process test. Five sample rates remain finite under
five-second extreme-setting renders.

## Phase 3 — Density and network selection ✅

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

**Engineering candidate selected at `0ff4200`.** Four diffuser stages move the 8-line NED
t90 from 355 ms to 65 ms. The level-matched 16-line/4-stage candidate reaches 45 ms and
reduces autocorrelation from 0.180 to 0.140 while retaining the 1.4 s T20 target. Local
Release throughput is 1.10% of realtime versus 0.64%; estimated stereo FDN storage rises
by about 33 kB. See [phase3-network-decision.md](phase3-network-decision.md).

**Owner listening completed on 2026-08-23.** The owner heard the actual Player path and
reported that the 16-line result sounded natural. Sixteen lines are now the product path;
the 8-line type stays compiled as regression evidence. Static modulation was
intentionally disabled for the topology comparison, so sustained-vowel Motion tuning
remains part of the later listening/control prototype.

## Phase 3.5 — Owner audition shell ✅

Before adding a directional early field, make the current late-field candidate easy to
hear in its real product path. Build VST3 and generated Standalone from one provisional
processor/editor, plus a small Player that adds file/device transport around that exact
processor/editor.

Expose only the late-tail controls needed for listening. Keep 8/16 selection as an
unsaved, non-automatable developer comparison with a bounded preallocated crossfade.
This checkpoint does **not** freeze the Phase 6 parameter contract and does not claim to
be the finished reverb. See [audition-shell.md](audition-shell.md).

**Exit:** all three hosts build, Player audio reaches the real processor, dry/bypass and
state tests pass, 8/16 switching is real-time safe, and no private audio is tracked.

**Completed at `b5956f6`; listening decision recorded in
[phase3-network-decision.md](phase3-network-decision.md).** The temporary network switch
is removed as Phase 4A lands.

## Phase 4 — Directional early field

Generalise the six-image baseline to compile-time-bounded order-3 candidates. Add
arrival/energy/level pruning, per-surface filters, Stereo projection and Binaural HRTF/ITD
rendering. Establish an overlapping ER/late transition.

**Exit:** arrival references, ER HRTF tolerances and transition continuity pass; changing
Space or a preset never allocates in the callback.

Phase 4 begins with the bounded first-order Room Body prototype defined in
[room-body.md](room-body.md). Order-2/3 generation, pruning and final HRTF output follow
only after the six-image timing and energy balance survives owner listening.

### Phase 4A — Room Body prototype

The implemented slice wraps the accepted 16-line/4-stage tail in six first-order shoebox
reflections. Its early field retains Mid fully and Side at 0.5, and all pre-delay and
geometry-delay automation is limited to 0.5 sample of delay per processed sample.
`setSettings` is audio-thread-owned and operates only on prepared storage. Bypass fades to
dry over 50 ms, feeds silence into the room while bypassed so the tail cools down, and is
exact dry in steady state.

**v1 result:** at `b0c0475`, the seven targeted gates, complete seven-test Release CTest
set and pluginval 1.0.4 strictness 10/repeat 3 passed. On 2026-08-24 the owner heard Body
mainly as slightly louder than Tail rather than as a clearly realistic boundary. The
default comparison was not matched: its integrated wet difference was about `+0.17 dB`.

### Phase 4A.1 — Room Body v2 listening correction

Before adding reflection orders or HRTF, keep the same six images and accepted tail while
adding:

- unsaved `Tail Only`, `Room Body` and `ER Solo` audition modes;
- a saved wet-only `Mono Input` Bool, with original dry stereo unchanged;
- fixed offline Tail/Body level matching and explicit ER isolation;
- one bounded retune of surface gain/damping and late onset;
- controlled, mono-compatible early spread for centred wet excitation.

**Docs-first status:** contract defined; implementation and every v2-specific gate are
NOT RUN. Exit requires state/transition/realtime tests, 0.1 dB default Tail/Body matching,
complete Release CTest and pluginval reruns, followed by one sighted matched owner
audition. If the result is still only level/tone change, Phase 4A pauses for redesign.
Order-2/3 and HRTF do not begin before this decision. The Steinberg validator remains an
unconfigured Phase 7 gate.

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
