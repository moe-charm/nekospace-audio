# Parameter Contract (permanent IDs)

IDs are frozen forever once released. Display names / ranges may evolve; IDs may not.
All parameters use JUCE `ParameterID` version hint 1 unless noted.

| ID                   | Type   | Range / Values                | Default | Notes |
| -------------------- | ------ | ----------------------------- | ------- | ----- |
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
| `hrtf.profile`       | choice | Analytic A (v1)               | Analytic A | not for continuous automation; background rebuild + long crossfade |
| `quality.mode`       | choice | Economy / Standard            | Standard | Economy = 64-tap, Standard = 128-tap FIR |
| `output.gain`        | float  | -24 … +12 dB                  | 0       | |
| `output.bypass_room` | bool   | Off / On                      | Off     | A/B compare |

Reserved for future versions (do not reuse for anything else):
`quality.mode` value `High` (partitioned FFT — must update reported latency),
`hrtf.profile` values for measured KEMAR / KU100 profiles, `source.mode` value `Mid-Side`.

State serialization: APVTS tree tagged with `schemaVersion` (integer, currently 1).
