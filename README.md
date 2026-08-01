# NekoSpace Audio

Audio plugins for voice work and audio drama. C++17 / JUCE / CMake, AGPLv3.

| Product | Status | |
| --- | --- | --- |
| **NekoSpace Binaural** | in development | voice-focused 3D binaural spatializer — [readme](plugins/binaural/README.md) |
| NekoSpace Reverb | planned | |
| NekoSpace Room | planned | |
| NekoSpace Delay | planned | |

## Build

Requires CMake ≥ 3.22, a C++17 compiler (Visual Studio 2022 on Windows), and git — JUCE
is fetched automatically on first configure.

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release
```

Every plugin builds from the one top-level configure; JUCE is fetched once and shared.

## Layout

```
nekospace-audio/
├─ plugins/          one directory per product, self-contained
│  └─ binaural/      src, tests, docs, tools, resources
├─ shared/           code proven to be needed by more than one plugin
├─ docs/             contracts that apply across products
└─ cmake/            build helpers
```

See [docs/repo-layout.md](docs/repo-layout.md) for what belongs where, and in particular
why `shared/` starts empty.

## Docs that apply to every product

- [identity.md](docs/identity.md) — copyright holder vs brand; VST3/AU codes, permanent after release
- [state-format.md](docs/state-format.md) — what gets written into a host project and the rules that keep old projects loading
- [realtime-contract.md](docs/realtime-contract.md) — thread rules
- [third-party-licenses.md](docs/third-party-licenses.md) — dependency licensing SSOT

## License

Free software under the **GNU Affero General Public License v3.0 or later**
(see [LICENSE](LICENSE)).

    Copyright (C) 2026 charmpic

**Audio you produce with these plugins is yours.** The AGPL covers the plugins' own source
and binaries. Rendered audio is not a derivative work of the software — see LICENSE
section 2. Recordings and audio dramas made with NekoSpace carry no AGPL obligation.
