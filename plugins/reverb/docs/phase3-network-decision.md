# Reverb Phase 3 — Network Decision

Status: **16-line selected for the product path after engineering and owner listening**.

## Compared configurations

All results use the same independent core, 48 kHz, 64-sample blocks, a neutral 1.4 s
decay, six-second impulse render and matched wet RMS. The deterministic comparison tool is
`nsr_phase3_compare`; timings below are one local MSVC Release run and are not a
cross-machine performance promise.

| Network | Diffuser | NED t90 | Max autocorrelation | Peak | RMS | CPU for 10 s | Realtime |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 8-line | 0 | 355 ms | 0.1829 | 0.1392 | 0.001685 | 50.2 ms | 0.50% |
| 8-line | 2 | 110 ms | 0.1796 | 0.0592 | 0.001685 | 56.9 ms | 0.57% |
| 8-line | 4 | 65 ms | 0.1795 | 0.0526 | 0.001685 | 63.6 ms | 0.64% |
| **16-line** | **4** | **45 ms** | **0.1401** | **0.0504** | **0.001685** | **110.3 ms** | **1.10%** |

The selected candidate also retains the decay target: its 1 kHz T20 is 1.415 s with
R² 0.99954, versus 1.405 s and R² 0.99989 for 8-line/4-stage. The difference from the
1.4 s request is about 1.1%.

## Why 16 lines won the engineering comparison

- It reaches the 50 ms Small-scene density target; the best 8-line result reaches 65 ms.
- Its 0.140 autocorrelation passes the provisional 0.15 periodicity warning; all measured
  8-line variants remain around 0.18.
- Wet RMS is matched and the peak is nearly matched, so the result is not an output-level
  trick.
- The complete Phase 2 three-band implementation remains below 1.2% of realtime in this
  local throughput measurement.
- Estimated prepared FDN delay storage at 48 kHz is about 426 kB for stereo Mid/Side,
  versus 393 kB for eight lines. Four stereo diffuser stages add about 12 kB to either.

The 16 lengths total 17,010 samples at the 48 kHz reference before Space scaling, compared
with 14,628 for the 8-line set. This is a comparable-order candidate, not a doubled copy
of all eight delays. FWHT feedback remains orthogonal and costs `N log2(N)` butterflies.

## Owner listening decision

On 2026-08-23 the owner auditioned the actual shared Processor/Editor through Reverb
Player and reported that the 16-line result sounded natural. This closes the Phase 3
selection gate. The production path may now process only the 16-line network; the 8-line
type remains compiled as regression evidence rather than a user-facing option.

This is a project listening decision, not a universal claim that 16 lines are always
better. Modulation remains disabled in the static comparison, so sustained-vowel Motion
limits still belong to the later control prototype.
