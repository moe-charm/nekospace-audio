# HRTF Data Pipeline

## Runtime grid format (in-memory, v1)

- Direction grid: azimuth 0–355° step 5° (72), elevation -60…+90° step 15° (11) = 792 dirs
- Per direction, per ear: N-tap FIR, **time-aligned (ITD removed)**.
  N keeps the HRIR window at a constant ~2.7 ms so timbre does not depend on the session
  sample rate: `N = clamp(128 * sr/48000, 64, 256)` (128 @ 48 kHz, 256 @ 96 kHz and up).
  `quality.mode` Economy uses N/2 with an 8-tap raised-cosine fade on the truncation edge.
- ITD is not stored: interaural delay is computed geometrically at runtime
  (rigid-sphere path: straight line when the ear is visible, tangent+arc when occluded)
- Regenerated at `prepareToPlay` for the current sample rate (analytic profile is cheap)

## v1 profiles (both procedural, both built at `prepareToPlay`)

Both start with the same head-shadow first-order filter (Brown–Duda):
`H(s) = (1 + α·s/2ω0)/(1 + s/2ω0)`, `ω0 = c/a`, `α = 1 + cos(θ_inc)`, θ_inc = angle
source↔ear axis (ears at ±95°). They differ in the pinna/elevation stage.

### Analytic A (legacy, kept for A/B comparison)

Pinna notch at 5–12 kHz tracking elevation, plus a small HF peak for upward sources —
but the notch depth is scaled by `cos(elevation) × frontness`. That makes the height cue
**vanish at ±90° and behind the listener**, which is why "above" and "below" were hard to
hear. Retained only so the change is audible side by side.

### Analytic B (default)

Elevation cues that survive at the poles and behind:

1. **N1 notch** — centre sweeps geometrically 4.2 kHz (straight down) → 11.5 kHz
   (straight up), monotonic in elevation; depth 8–16 dB and never scaled to zero.
   Measured HRIRs show 10–20 dB here, so a shallow notch simply is not audible as height.
2. **P1 peak** — companion peak at `0.62 × f_N`, +2…+6 dB. The moving *pair* is what
   reads as height, not a lone notch (Hebrank & Wright; Langendijk & Bronkhorst).
3. **Elevation shelf** — high shelf at 8 kHz, ±6.5 dB: sources above gain air, sources
   below lose it to torso shadow.
4. Front/back weight floors at 0.45 (never 0) so the rear keeps most of the cue, and
   blends to a constant at the poles where azimuth is meaningless.

Impulse through the cascade → truncated to N taps.

Measured mean spectral separation over 4–14 kHz (`nsb_tests` prints this):
straight-up vs straight-down **5.8 dB** for B against **2.0 dB** for A.

Switching profiles is a pointer swap at a block boundary — both grids are resident
(~1 MB each) — and rides the normal filter crossfade.

## Future `.bhrtf` pack (TASK 6)

```
元の .sofa → 検証・座標正規化 → SR正規化 → 到達時間/残差フィルター分離
          → 方向グリッド作成 → 読み取り専用 .bhrtf パック（バージョン付き）
```

Plugin only ever reads versioned packs; SOFA parsing (libmysofa) stays on worker threads.
User-imported SOFA files are converted in the background and cached.

## Dataset licensing notes (for measured profiles later)

| Dataset | License | Commercial | Notes |
| ------- | ------- | ---------- | ----- |
| MIT KEMAR (compact/full) | free w/ attribution | ✅ | 128-tap 44.1k, classic |
| HUTUBS | CC BY 4.0 | ✅ | 96 subjects incl. KEMAR |
| TH Köln KU100 (Bernschütz) | CC BY-SA | ✅ (share-alike on data) | **matches the user's KU100 recording chain — priority for a measured profile** |
| Aachen high-res KEMAR | CC BY 4.0 | ✅ | dense grid |
| CHEDAR | CC BY-NC-SA | ❌ | non-commercial only — do not ship |

Every shipped/imported profile must display: dataset name, author, license, measurement
distance, sample rate, direction count, attribution text (HRTF panel in GUI).
