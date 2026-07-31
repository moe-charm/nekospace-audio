# Parameter Contract (permanent IDs)

IDs are frozen forever once released. Display names / ranges may evolve; IDs may not.
All parameters use JUCE `ParameterID` version hint 1 unless noted.

| ID                   | Type   | Range / Values                | Default | Notes |
| -------------------- | ------ | ----------------------------- | ------- | ----- |
| `global.bypass`      | bool   | Off / On                      | Off     | reported via `getBypassParameter()`; declared explicitly so the VST3 wrapper does not synthesise a hidden one. Bypassed audio is delayed by the reported latency so switching stays phase-aligned |
| `position.azimuth`   | float  | -180 … +180 °                 | 0       | 0 = front, positive = right; interpolate via shortest arc across ±180 |
| `position.elevation` | float  | -90 … +90 °                   | 0       | positive = up |
| `position.distance`  | float  | 0.05 … 20 m (log skew)        | 1.0     | from head center |
| `source.width`       | float  | 0 … 180 °                     | 60      | Linked Stereo only |
| `source.mode`        | choice | Mono Object / Linked Stereo   | Mono Object | discrete; short output fade on switch |
| `nearfield.amount`   | float  | 0 … 100 %                     | 75      | blends far-field ⇄ exact per-ear geometry |
| `head.radius`        | float  | 0.075 … 0.100 m               | 0.0875  | |
| `room.amount`        | float  | 0 … 100 %                     | 15      | 0 must equal exact direct render |
| `room.size`          | float  | 0 … 100 %                     | 35      | continuous shoebox + T60 scale |
| `room.damping`       | float  | 0 … 100 %                     | 50      | HF absorption |
| `room.early_late`    | float  | 0 … 100 %                     | 35      | 0 = all early, 100 = all late |
| `quality.mode`       | choice | Economy / Standard            | Standard | Standard = full HRIR window (~2.7 ms), Economy = half. Tap counts scale with sample rate — 128/64 at 48 kHz, 256/128 at 96 kHz and above |
| `output.gain`        | float  | -24 … +12 dB                  | 0       | |
| `output.bypass_room` | bool   | Off / On                      | Off     | A/B compare |

Reserved for future versions (do not reuse for anything else):

- **`hrtf.profile`** (choice) — ID reserved, **not exposed in v1**. A choice parameter
  with a single option has range 0..0, so its normalised value is 0/0 = NaN and no host
  can round-trip it (caught by pluginval strictness 10). It ships in TASK 6 once
  imported SOFA profiles make the list longer than one. Not for continuous automation:
  changing it rebuilds every filter on a worker thread, then crossfades on publish.
- `quality.mode` value `High` (partitioned FFT — must update reported latency).
- `source.mode` value `Mid-Side`.

State serialization: APVTS tree tagged with `schemaVersion` (integer, currently 1).
