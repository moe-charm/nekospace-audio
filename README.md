# NekoSpace Audio

日本語版は [README.ja.md](README.ja.md) にゃ。

Audio plugins for voice work and audio drama. C++17 / JUCE / CMake, AGPLv3.

| Product | Status | |
| --- | --- | --- |
| **NekoSpace Binaural** | in development | voice-focused 3D binaural spatializer — [readme](plugins/binaural/README.md) |

| NekoSpace Reverb | planned | |
| NekoSpace Room | planned | |
| NekoSpace Delay | planned | |

## Status — honest version

Pre-release. Nothing here has shipped, so parameter IDs and plugin codes are settled but
the sound is not.

**NekoSpace Binaural** works: left/right, front/back, distance and near-field "at the ear"
placement all do what they claim, validated by 30 JUCE-free acceptance tests and
`pluginval --strictness-level 10`.

**Elevation is the weak axis.** It produces a real, audible change and a mild sense of
vertical movement, but it is not a dependable "that is above me" cue. This is the known
ceiling of static, non-individualised binaural rather than a bug we have not found yet:
the spectral cues that carry height depend on the shape of *your* pinnae, and headphones
colour exactly the 5–12 kHz band those cues live in. Four approaches were tried and
measured — an analytic redesign, a measured KU100 dataset, torso reflections plus early
reflections rendered through the HRTF, and per-listener tuning. Each improved the numbers;
none produced a decisive percept. Treat elevation as a colour, not as a load-bearing
narrative cue.

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
