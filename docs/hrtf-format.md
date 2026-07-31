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

## v1 profile: "Analytic A"

Procedural spherical-head model, generated per direction/ear:

1. Head-shadow first-order filter (Brown–Duda): `H(s) = (1 + α·s/2ω0)/(1 + s/2ω0)`,
   `ω0 = c/a`, `α = 1 + cos(θ_inc)`, θ_inc = angle source↔ear axis (ears at ±95°)
2. Pinna elevation notch: biquad, center ≈ 5–12 kHz tracking elevation, frontal emphasis
3. Elevation shelf: small HF peak for upward sources
4. Impulse through the cascade → truncated to N taps

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
