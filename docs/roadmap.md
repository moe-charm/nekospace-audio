# Roadmap / Task Breakdown

Vertical slices; each task lands complete with tests before the next starts.

## TASK 0 — Skeleton ✅ (this repo's first milestone)
C++17, JUCE pinned, CMake, VST3 (Win x64) + Standalone, stereo→stereo, state save,
resizable GUI shell.
**Accept:** loads in FL Studio, params restore after project reload, ctest green.

## TASK 1 — Fixed-direction HRTF ✅ (folded into TASK 2 engine)
**Accept:** L/R mirror test passes, no allocation in processBlock.

## TASK 2 — Continuous panning ✅
Azimuth/elevation/distance, grid interpolation, crossfading convolvers.
**Accept:** no clicks on 360° sweeps, shortest-arc across ±180°, block-size invariant.

## TASK 3 — GUI & presets ✅ (v1 scope: pad, elevation, numerics, factory presets)
**Accept:** DSP runs with GUI closed, state consistent on reopen, high-DPI clean.
Deferred within task: Undo/Redo UI, A/B.

## TASK 4 — Near Field ✅
Per-ear distance/gain/fractional delay, head size, ear snap presets.
**Accept:** approaching left ear shortens/strengthens left path, continuous at boundaries,
symmetry preserved.

## TASK 5 — Room Engine ✅
6 first-order reflections, size/damping/early-late, 8-line FDN.
**Accept:** never diverges, no NaN at any setting, room 0% == exact direct,
tail length reported to host.

## TASK 6 — SOFA Import (next)
libmysofa, background conversion, `.bhrtf` pack + cache, license display.
Priority dataset: TH Köln KU100 (matches production recording chain).
**Accept:** playback never stalls during load, bad SOFA never crashes, failed load keeps
old HRTF, project reload resolves the same HRTF.

## TASK 7 — Distribution
Windows installer, code signing, CI artifacts. macOS (VST3+AU, Universal 2, notarization)
when Mac hardware is available. Then: AAX, CLAP, Mid-Side mode, OSC head tracking.

## Test matrix (standing)

- Sample rates: 44.1 / 48 / 88.2 / 96 / 192 kHz
- Block sizes: 1, 16, 64, 127, 256, 1024 + mid-stream changes
- FL Studio: Fixed size buffers ON/OFF, save/reload, offline render, fast automation,
  GUI closed long-run, bypass toggling
- pluginval + Steinberg VST3 validator before any release
- Listening: 方向/前後/高さ/耳元距離/外在化/明瞭度/移動時の音色/疲労 — dearVR無償版と
  音量一致ブラインド比較
