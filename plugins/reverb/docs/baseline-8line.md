# Reverb Phase 0 — 8-line Baseline

This is the first frozen measurement of the late FDN currently shipped inside NekoSpace
Binaural. It describes a baseline, not a pass/fail verdict on the future Reverb.

## Reproduction

Implementation commit: `4e29a9d9768e`

```powershell
build\plugins\reverb\Release\nekospace_reverb_analyze.exe `
  --output build\reverb-baseline-4e29a9d `
  --sample-rate 48000 --duration 6 --block-size 256 `
  --size 0.35 --decay 1.4 --damping 0
```

The run was a clean MSVC 19.44 Release build. Its JSON reported
`working_tree_dirty: false`. Generated WAV/CSV/JSON artifacts remain under the ignored
`build/` tree and are not committed.

## Summary

| Metric | Result | Interpretation |
| --- | ---: | --- |
| Output peak | 0.09999 | finite, ample measurement headroom |
| Output RMS | 0.0007588 | recorded for future level matching |
| NED `t90` | **260 ms** | substantially slower than the provisional 50/80/120 ms scene targets |
| Maximum normalized autocorrelation, 10–100 ms | **0.355** | above the provisional 0.15 periodicity warning |
| T30 regression R² | 0.9977–0.9999 | decay slopes are extremely regular within each measured band |

## Band decay

Target mid decay was 1.4 seconds. `damping = 0` still leaves the current FDN's fixed
13 kHz one-pole low-pass in every feedback line, so the top end is expected to die sooner.

| Band | T20 | T30 |
| ---: | ---: | ---: |
| 125 Hz | 1.374 s | 1.392 s |
| 250 Hz | 1.362 s | 1.397 s |
| 500 Hz | 1.385 s | 1.390 s |
| 1 kHz | 1.363 s | 1.359 s |
| 2 kHz | 1.263 s | 1.276 s |
| 4 kHz | 1.087 s | 1.125 s |
| 8 kHz | 0.847 s | 0.904 s |

The current network therefore already does one thing well: its low/mid decay is smooth,
stable and close to the requested broadband value. The high-frequency curve is not an
independent design target; it is whatever one damping low-pass produces.

The largest measured weaknesses are elsewhere:

1. **Density builds slowly.** A 260 ms `t90` is too late for the intended Small/Medium
   voice rooms under the current provisional targets.
2. **Periodic structure remains visible.** The 0.355 autocorrelation peak explains why a
   static or lightly modulated 8-line tail can sound patterned or metallic.
3. **Decay colour has only one direction.** High frequencies can be shortened, but low,
   mid and high T60 cannot be designed independently.

These findings justify the roadmap order. Phase 1 adds an independent Reverb core and
Phase 2 adds frequency-dependent T60 without changing line count. Phase 3 then measures
input diffusion and the 8/16-line candidates. Increasing line count before preserving this baseline would
make it impossible to know which change helped.

## Regression coverage

The Phase 0 tests establish:

- exact baseline agreement across block sizes 1, 127 and 512;
- finite, non-empty rendering at 44.1, 48, 88.2, 96 and 192 kHz;
- T20 recovery from a known 0.8-second exponential-noise decay;
- dense-noise NED behaviour;
- FFT round-trip accuracy;
- valid RIFF/WAVE, JSON and every CSV artifact;
- end-to-end CLI smoke rendering through CTest.

The provisional thresholds in [validation.md](validation.md) are deliberately not marked
as universal standards. This baseline is the first datum used to calibrate them.
