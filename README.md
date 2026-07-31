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

## Docs

- [architecture.md](docs/architecture.md) — signal path + Architecture Contract (binding)
- [parameter-contract.md](docs/parameter-contract.md) — permanent parameter IDs
- [realtime-contract.md](docs/realtime-contract.md) — thread rules
- [hrtf-format.md](docs/hrtf-format.md) — HRTF data pipeline, licensing notes
- [roadmap.md](docs/roadmap.md) — task breakdown with acceptance criteria
- [third-party-licenses.md](docs/third-party-licenses.md) — dependency licensing SSOT
