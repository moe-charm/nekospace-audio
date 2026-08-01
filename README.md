# NekoSpace Binaural

Voice-focused 3D binaural spatializer plugin. 音声作品向けの、耳元表現に強い軽量バイノーラル・パンナー。

- **Format (v1):** VST3 Effect (Windows x64) + Standalone
- **Buses:** Stereo in → Stereo out (mono handled internally via `source.mode`)
- **Stack:** C++17 / JUCE (pinned) / CMake
- **Primary test DAW:** FL Studio (Windows)

## Build

Requires: CMake ≥ 3.22, Visual Studio 2022, git (JUCE is fetched automatically on first configure).

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release
```

Outputs:

- VST3: `build/NekoSpaceBinaural_artefacts/Release/VST3/NekoSpace Binaural.vst3`
- Standalone: `build/NekoSpaceBinaural_artefacts/Release/Standalone/NekoSpace Binaural.exe`

Copy the `.vst3` folder to `C:\Program Files\Common Files\VST3\` (or add the artefacts dir to FL Studio's plugin search paths), then rescan in FL Studio.

## License

NekoSpace Binaural is free software, licensed under the
**GNU Affero General Public License v3.0 or later** (see [LICENSE](LICENSE)).

    Copyright (C) 2026 TextureVoice

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
[third-party-licenses.md](docs/third-party-licenses.md).

### Optional: experimental measured HRTF (development only)

Not part of a normal build or of any distribution — see `current_task.md`.

```bash
python tools/hrtf-pack/sofa_to_bhrtf.py external/hrtf-data/HRIR_L2702.sofa resources/hrtf/ku100_48k.bhrtf
cmake -B build -DNSB_WITH_KU100=ON
```

## Docs

- [architecture.md](docs/architecture.md) — signal path + Architecture Contract (binding)
- [parameter-contract.md](docs/parameter-contract.md) — permanent parameter IDs
- [state-format.md](docs/state-format.md) — what gets saved into a host project, and the rules that keep old projects loading
- [realtime-contract.md](docs/realtime-contract.md) — thread rules
- [hrtf-format.md](docs/hrtf-format.md) — HRTF data pipeline, licensing notes
- [roadmap.md](docs/roadmap.md) — task breakdown with acceptance criteria
- [third-party-licenses.md](docs/third-party-licenses.md) — dependency licensing SSOT
