# NekoSpace Binaural

Part of **NekoSpace Audio** — https://github.com/moe-charm/nekospace-audio

Voice-focused 3D binaural spatializer plugin. 音声作品向けの、耳元表現に強い軽量バイノーラル・パンナー。

- **Format (v1):** VST3 Effect (Windows x64) + Standalone
- **Buses:** Stereo in → Stereo out (mono handled internally via `source.mode`)
- **Stack:** C++17 / JUCE (pinned) / CMake
- **Primary test DAW:** FL Studio (Windows)

![NekoSpace Binaural](../../docs/images/gui-main.png)

▶ **[Watch the 74-second operation demo](https://youtu.be/nk54N0w6FOE)** — headphones
or earphones recommended.

## Player

`NekoSpace Binaural Player` plays a file through the plugin. The plugin's own Standalone
takes live input, which is the wrong shape for auditioning a take or filming a
demonstration; this is the other shape.

Open or drop a WAV/AIFF/FLAC, press Play (or Space), and drag the source around while it
runs. Loop, a position bar and an audio-device chooser are the whole transport.

**It hosts the real processor and the real editor.** Nothing below the transport strip is
reimplemented — the pad, the presets, the Elevation Lab and the help are the plugin's own,
so what is on screen is what ships, by construction. Rebuilding them would mean two of
everything drifting apart, and a demonstration filmed against a rebuilt interface would be
showing something nobody can download.

Reading and decoding run on a background thread, so `processBlock` still sees nothing but
audio, exactly as it does in a host.

Built alongside everything else; binary at
`build/plugins/binaural/NekoSpaceBinauralPlayer_artefacts/Release/`. A path on the command
line opens straight away.

Offline rendering is deliberately not here: screen capture with system audio covers the
demonstration case, and an exporter belongs with a reason to distribute renders.

## What works, and what does not

| Axis | State |
| --- | --- |
| Left / right | solid — geometric ITD from the exact rigid-sphere path, plus head shadow |
| Distance | solid — per-ear 1/r with a safety limiter |
| Near field ("at the ear") | solid — 5.6 dB ILD at 0 %, 24.2 dB at 100 %, az −90° / 12 cm |
| Front / back | usable |
| **Up / down** | **weak — see below** |

Elevation produces a real, audible change and a mild sense of vertical movement, but it is
not a dependable cue. Four approaches were built and measured: an analytic model redesign
(up/down spectral spread 2.0 → 5.8 dB), a measured TH Köln KU100 dataset (6.4 dB), torso
reflections plus early reflections rendered through the HRTF at their image directions,
and per-listener tuning via the Elevation Lab. Every one improved the numbers; none
produced a decisive percept on the author's own ears and headphones.

That is the ceiling of static, non-individualised binaural — height depends on the shape
of the listener's pinnae, and headphones colour the same 5–12 kHz band. The strongest
remaining lever, head tracking, does not apply when the deliverable is a rendered file.
Use elevation as a colour; convey height through the script and the room.

## Build

Requires: CMake ≥ 3.22, Visual Studio 2022, git (JUCE is fetched automatically on first configure).

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release
```

Outputs:

- VST3: `build/plugins/binaural/NekoSpaceBinaural_artefacts/Release/VST3/NekoSpace Binaural.vst3`
- Standalone: `build/plugins/binaural/NekoSpaceBinaural_artefacts/Release/Standalone/NekoSpace Binaural.exe`

Copy the `.vst3` folder to `C:\Program Files\Common Files\VST3\` (or add the artefacts dir to FL Studio's plugin search paths), then rescan in FL Studio.

## License

NekoSpace Binaural is free software, licensed under the
**GNU Affero General Public License v3.0 or later** (see [LICENSE](../../LICENSE)).

    Copyright (C) 2026 charmpic

    This program is free software: you can redistribute it and/or modify it under
    the terms of the GNU Affero General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option) any
    later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License for more
    details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

**Audio you produce with this plugin is yours.** The AGPL covers the plugin's own
source and binaries. Rendered audio is not a derivative work of the software —
see LICENSE section 2: *"The output from running a covered work is covered by this
License only if the output, given its content, constitutes a covered work."*
Recordings and audio dramas made with NekoSpace carry no AGPL obligation.

Choosing AGPLv3 also settles the JUCE question: JUCE 9 is dual-licensed AGPLv3 /
commercial, so an AGPLv3 release needs no paid JUCE tier. See
[third-party-licenses.md](../../docs/third-party-licenses.md).

### Optional: experimental measured HRTF (development only)

Not part of a normal build or of any distribution — see `elevation-findings.md`.

```bash
python tools/hrtf-pack/sofa_to_bhrtf.py external/hrtf-data/HRIR_L2702.sofa resources/hrtf/ku100_48k.bhrtf
cmake -B build -DNSB_WITH_KU100=ON
```

## Docs

- [architecture.md](docs/architecture.md) — signal path + Architecture Contract (binding)
- [parameter-contract.md](docs/parameter-contract.md) — permanent parameter IDs
- [state-format.md](../../docs/state-format.md) — what gets saved into a host project, and the rules that keep old projects loading
- [identity.md](../../docs/identity.md) — copyright holder vs brand, and the VST3/AU codes that are permanent after release
- [realtime-contract.md](../../docs/realtime-contract.md) — thread rules
- [hrtf-format.md](docs/hrtf-format.md) — HRTF data pipeline, licensing notes
- [roadmap.md](docs/roadmap.md) — task breakdown with acceptance criteria
- [elevation-findings.md](docs/elevation-findings.md) — four attempts at a height cue, and what they ruled out
- [third-party-licenses.md](../../docs/third-party-licenses.md) — dependency licensing SSOT
