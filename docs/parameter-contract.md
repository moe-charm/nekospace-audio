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
| `nearfield.amount`   | float  | 0 … 100 %                     | 75      | how much each ear gets its *own* distance to the source. 0 % attenuates both ears by the same 1/r, so the level difference comes only from the head-shadow filter — a conventional panner. 100 % uses the exact per-ear sphere path, so a source at one ear is dramatically louder there. ITD is always per-ear and is never switched off; this only moves it from the Woodworth approximation to the exact path. Measured ILD at az −90°, 12 cm: **5.6 dB at 0 %, 24.2 dB at 100 %** |
| `head.radius`        | float  | 0.075 … 0.100 m               | 0.0875  | |
| `room.amount`        | float  | 0 … 100 %                     | 15      | 0 must equal exact direct render |
| `room.size`          | float  | 0 … 100 %                     | 35      | continuous shoebox + T60 scale |
| `room.damping`       | float  | 0 … 100 %                     | 50      | HF absorption |
| `room.early_late`    | float  | 0 … 100 %                     | 35      | 0 = all early, 100 = all late |
| `hrtf.profile`       | choice | Analytic A (legacy) / Analytic B | Analytic B | B has the working height cue; A is kept for comparison. Both grids stay resident, so switching is a block-boundary swap + crossfade. Not meant for continuous automation |
| `quality.mode`       | choice | Economy / Standard            | Standard | Standard = full HRIR window (~2.7 ms), Economy = half. Tap counts scale with sample rate — 128/64 at 48 kHz, 256/128 at 96 kHz and above |
| `output.gain`        | float  | -24 … +12 dB                  | 0       | |
| `output.bypass_room` | bool   | Off / On                      | Off     | A/B compare |

Reserved for future versions (do not reuse for anything else):

- `quality.mode` value `High` (partitioned FFT — must update reported latency).
- `source.mode` value `Mid-Side`.

State serialization: see [state-format.md](state-format.md). In short — IDs are permanent,
new parameters are appended with an incremented version hint, and **choice parameters are
stored by name**, because adding an option changes what every previously saved normalised
value decodes to.
