# NekoSpace Reverb — Validation and Acceptance

This document prevents “more lines” or “a different colour” from being mistaken for an
improvement. Every candidate renders the same deterministic impulse-response and programme
material corpus, through the same analysis code, at matched output energy.

There are two classes of gates:

- **Binding engineering gates** are required from the first implementation.
- **Provisional acoustic targets** are starting hypotheses. They become release gates only
  after the analyzer has been run on respected reference reverbs and the threshold does
  not reject obviously useful results.

No single metric is treated as “reverb quality.” T60, density, decay surface, spectrum,
periodicity, spatial statistics, automation safety and listening answer different
questions.

## Implementation status

Phase 0 implements the offline parts needed to freeze the existing 8-line baseline:

- deterministic impulse rendering directly through Binaural's current `FdnReverb`;
- float WAV plus versioned JSON metadata;
- octave-centre T20/T30 using cascaded analysis high/low biquads and Schroeder integration;
- 20 ms-window, 5 ms-hop normalized echo density and project-defined `t90`;
- Hann-2048/hop-512 octave-band EDR;
- 1/12-octave spectrum and 1 ms-step autocorrelation from 10–100 ms;
- tests against a known exponential decay, supported sample rates and odd block sizes.

The Phase 0 band filters are a deterministic engineering approximation, not an
IEC/ISO-certified acoustics instrument. Their job is stable A/B regression. If later
release claims depend on standardized room-acoustic values, the analyzer must first be
validated against a trusted implementation and upgraded without rewriting old report
schema 1 results.

## Reproducible test render

The JUCE-free test tool must render at least:

- a unit impulse, with dry disabled and wet at a known gain;
- a sustained 1 kHz sine followed by silence;
- octave-band noise bursts;
- deterministic full-band noise bursts;
- short speech, whispered speech and a transient-rich sample whose redistribution is
  permitted for tests, or a generated equivalent.

Every artifact records:

```text
git commit
algorithm revision and deterministic seed
compiler and build type
sample rate and block sequence
complete parameter/state snapshot
input and output peak/RMS
analysis version
```

Private production recordings remain outside git. Generated fixtures need one narrow,
reviewed `.gitignore` exception; measurement summaries and small text/JSON results may be
versioned.

## Binding engineering gates

| Property | Required result |
| --- | --- |
| Audio-thread allocation | 0 allocation and 0 free after `prepare`, including a 30-minute automation stress run |
| Audio-thread synchronisation | 0 mutex or condition-variable acquisition |
| Numerical stability | 0 NaN/Inf sample; no runaway at any allowed control combination |
| Block-size invariance | with static parameters, block 1/16/64/127/511/1024 and mid-stream changes differ from the reference by max abs `< 1e-6` |
| Sample rates | 44.1, 48, 88.2, 96 and 192 kHz pass; acoustic metrics use a documented reference rate, initially 48 kHz |
| Channel safety | mono symmetry; no stereo channel swap; polarity-opposed side input remains represented in wet output |
| Bypass/mix | bypass and `Mix = 0` equal the correctly aligned dry path exactly; `Mix = 100%` contains no dry |
| State | save/reload is deterministic, including modulation seed and output mode |
| Automation | no click, discontinuity, stale-tail burst or topology rebuild in the callback |
| Tail | host tail length is finite, conservative and no shorter than the rendered active tail |
| CPU evidence | Release build records machine/CPU/compiler; at 48 kHz/64 samples callback p99 is at most 10% of buffer time and worst case at most 25% on the declared reference machine |

The CPU percentages are a project budget, not a cross-machine performance promise.

## Frequency-dependent T60

For each rendered IR, filter into octave or one-third-octave bands and use Schroeder
backward integration:

```text
E[n] = sum(k=n..N) h[k]^2
L[n] = 10 log10(E[n] / E[0])
```

Estimate:

- T20 from the regression over -5 to -25 dB, extrapolated to 60 dB;
- T30 from -5 to -35 dB when the rendered tail provides enough range;
- regression `R²` and whether the selected range touches truncation or the numerical
  floor.

Provisional targets:

| Property | Provisional target |
| --- | --- |
| T60 accuracy | every 125 Hz–8 kHz test band within `max(5%, 20 ms)` of target |
| Decay linearity | single-slope preset regression `R² >= 0.995` |
| Space/Decay independence | sweeping Space across the test range moves each T60 anchor by no more than 5% |

The 5% target is intentionally strict and is not presented as a universal just-noticeable
difference. The research reports stimulus-dependent sensitivity; this is our engineering
budget.

## Normalized Echo Density

For a sliding window with local standard deviation `sigma`, count samples satisfying
`abs(h[n]) > sigma` and divide the fraction by the Gaussian expectation `0.31731`:

```text
NED(t) = fraction(abs(h) > sigma) / 0.31731
```

A Gaussian-like dense tail approaches 1. Define `t90` for this project as the first time
the 20 ms-smoothed NED reaches at least 0.9 and stays there for at least 20 ms.

Provisional preset targets:

| Scene | `t90` |
| --- | ---: |
| Small | <= 50 ms |
| Medium | <= 80 ms |
| Hall | <= 120 ms |
| Sustained late field | median NED 0.9–1.1 after mixing |

These times are not AES/ISO limits. They must be calibrated against references and may be
revised by room class before release.

## Energy Decay Relief

Compute a time-frequency energy decay surface from the wet IR. The initial common analyzer
configuration is:

```text
sample rate: 48 kHz
window: Hann, 2048 samples
hop: 256 or 512 samples, frozen by the analyzer version
frequency region: 125 Hz–8 kHz
decay region: -5 to -35 dB
```

Record the surface and flag narrow ridges, discontinuous frequency slopes and isolated
bins with abnormally slow decay. A provisional single-slope target is RMSE <= 1.5 dB from
the intended decay surface. This value is a product calibration target, not a published
standard.

## Colouration and periodicity

Keep a test-only static-network mode with modulation disabled. It exposes problems that
modulation can disguise.

Measure:

- modal-excitation spread where pole/residue analysis is available;
- narrow-peak prominence against a local 1/6-octave-smoothed baseline;
- baseline-removed spectral-ripple RMS from 200 Hz to 10 kHz;
- normalized autocorrelation side peaks in the 10–100 ms lag region after mixing time.

Provisional warning thresholds are:

| Metric | Provisional target |
| --- | ---: |
| Narrow peak prominence | <= 8 dB |
| Spectral-ripple RMS | <= 2.5 dB |
| Off-zero autocorrelation side peak | < 0.15 |

There is no accepted single “metallicness number.” A failure is diagnostic evidence, not
a proof of perceived quality; a pass never substitutes for listening.

## Early/late transition

Analyze bandwise RMS/energy envelopes around the overlap. The provisional gate is no jump
greater than 1 dB across a 20 ms region around the crossover. Also retain arrival-time and
cumulative-energy plots so a smooth total envelope cannot hide a missing early field.

For Binaural early rendering, compare retained reflections against an offline reference:

| Metric | Provisional target |
| --- | ---: |
| ITD error | <= 20 microseconds |
| Octave-band ILD error | <= 1 dB |

The existing above/below reflection-spectrum regression from NekoSpace Binaural should be
ported as a guard that the renderer still produces direction-dependent cues. It is not a
promise that every listener identifies elevation.

## Late spatial field

For Binaural mode, estimate interaural coherence per test band from the left/right wet
tail and compare it with the preset target curve. The provisional bandwise mean absolute
error is <= 0.05.

Also test:

- bounded L/R energy and no channel dominance for symmetric input;
- mono fold-down without severe cancellation;
- Stereo mode with the binaural-coherence stage exactly absent;
- Binaural mode against the identical FDN projection with coherence control bypassed.

The coherence stage is accepted only if it improves the intended envelopment without
moving the dry source or producing unstable combing.

## Modulation

On a sustained 1 kHz input, isolate the late tail and measure pitch track and sidebands.
The Natural-mode provisional budget is:

- pitch wander <= 3 cents RMS;
- pitch wander <= 10 cents peak;
- no discrete sideband judged objectionable at the shipping maximum `Motion` setting.

The first two are project budgets. The final condition still needs listening because a
single sideband threshold has not been established here.

## Comparative listening

Owner listening is required throughout because the product is meant to sound good, not
merely graph well. Each comparison must:

- use the same source and rendered duration;
- match integrated wet loudness or the relevant isolated bus to within 0.1 dB;
- hide brand/topology when a superiority claim is being evaluated;
- retain both the result and the settings needed to reproduce it.

A formal multi-listener test is **not a gate for early prototypes**. It becomes necessary
only before publishing a broad claim such as “better than Pro-R” or “more realistic than
Valhalla.” Without that evidence, documentation describes NekoSpace's measured behaviour
and avoids comparative superiority claims.

## Calibrating the provisional gates

Before acoustic thresholds are frozen for v1:

1. run the analyzer on the current 8-line baseline;
2. run it on the 16-line candidate at matched T60 and wet energy;
3. run it on at least one transparent commercial reference, one character reference and
   one published/open implementation that can legally be evaluated;
4. confirm the gates do not reject every respected reference or reward an obviously bad
   one;
5. record threshold changes with before/after plots and the listening reason.

Safety and realtime gates never wait for this calibration.
