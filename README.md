# NekoSpace Audio

日本語版は [README.ja.md](README.ja.md) にゃ。

Audio plugins for voice work and audio drama. C++17 / JUCE / CMake, AGPLv3.

| Product | Status | |
| --- | --- | --- |
| **NekoSpace Binaural** | v0.2.0-alpha | voice-focused 3D binaural spatializer — [readme](plugins/binaural/README.md) |
| **NekoSpace CleanVoice** | prototype | noise removal for whispered voice, app + CLI — [readme](plugins/cleanvoice/README.md) |
| **NekoSpace Reverb** | Room Body v2 in progress | accepted 16-line tail, matched first-order room audition and wet-only Mono Input; VST3, Standalone and Player — [design](plugins/reverb/README.md) |
| NekoSpace Room | planned | |
| NekoSpace Delay | planned | |

![NekoSpace Binaural](docs/images/gui-main.png)

▶ **[Watch the 74-second NekoSpace Binaural demo on YouTube](https://youtu.be/nk54N0w6FOE)**
— headphones or earphones recommended.

## Status — honest version

Alpha. Parameter IDs, plugin codes and the choice lists were frozen at `v0.1.0-alpha`
and have not changed since — see [CHANGELOG.md](CHANGELOG.md). The sound is not frozen.

**NekoSpace Binaural** works: left/right, front/back, distance and near-field "at the ear"
placement all do what they claim, validated by 33 JUCE-free acceptance tests and
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

## CleanVoice prototype

**NekoSpace CleanVoice** is an offline desktop editor and CLI for whispered and breathy
voice — not a realtime plugin yet. Select a noise-only region, learn its fixed profile,
then audition the same preview range as **Original / Clean / Removed Noise** before
processing the full take. One shared spectral gain is applied to every channel so noise
removal does not dissolve the binaural image. See the
[CleanVoice readme](plugins/cleanvoice/README.md) for the workflow and current limits.

## A closer look

| | |
| --- | --- |
| **Presets are complete scenes.** Each one sets every sound-shaping parameter, so a name always means the same sound. The JUMP TO buttons in the main window do the opposite job — they move the source and leave the room you built alone. | ![Preset menu](docs/images/presets.png) |

**Elevation Lab** — four macros over the height model, tuned by ear on your own headphones
and then frozen. `Advanced…` opens the 24 raw anchor values behind them, and `Copy as C++`
emits the block that makes a curve permanent.

![Elevation Lab](docs/images/elevation-lab.png)

**The manual is in the plugin**, in English and Japanese, switched from inside the window.
The choice follows the OS on first run and is stored per user, so an old session never
opens in the wrong language.

![Help, English](docs/images/help-en.png)

![Help, Japanese](docs/images/help-ja.png)

## Install

Download the Windows zip from [Releases](https://github.com/moe-charm/nekospace-audio/releases)
and unpack it. It contains a `.vst3` folder, a standalone `.exe`, and the licence and
notices.

1. Copy the whole **`NekoSpace Binaural.vst3` folder** — it is a bundle, not a single file —
   into `C:\Program Files\Common Files\VST3\`.
2. Rescan plugins in your DAW. In FL Studio: *Options → Manage plugins → Find more plugins*.
3. `NekoSpace Binaural.exe` runs without a DAW if you just want to hear it.

Needs headphones. This is a binaural renderer; over speakers the effect does not survive.

If the plugin was installed under an earlier name and your DAW still refuses to load it,
the DAW is caching the old plugin ID — clear its plugin database and rescan.

## Releases

Builds are published from a tag: pushing `v*` runs the release workflow, which configures,
builds, **runs the acceptance tests**, packages the VST3 and Standalone with the licence
and notices, and opens a draft GitHub Release. A build that has not passed its own tests
is not released.

See [CHANGELOG.md](CHANGELOG.md) for what each release contains and what it freezes.

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
│  ├─ binaural/      src, tests, docs, tools, resources
│  ├─ cleanvoice/    JUCE-free DSP/CLI plus a JUCE standalone GUI
│  └─ reverb/        independent JUCE-free core + analysis harness
├─ shared/           product-neutral DSP proven by multiple consumers
├─ docs/             contracts that apply across products
├─ video/            Remotion source; private media and renders stay ignored
└─ cmake/            build helpers
```

See [docs/repo-layout.md](docs/repo-layout.md) for what belongs where, and in particular
how code earns promotion into `shared/`.

## Docs that apply to every product

- [identity.md](docs/identity.md) — copyright holder vs brand; VST3/AU codes, permanent after release
- [state-format.md](docs/state-format.md) — what gets written into a host project and the rules that keep old projects loading
- [realtime-contract.md](docs/realtime-contract.md) — thread rules
- [third-party-licenses.md](docs/third-party-licenses.md) — dependency licensing SSOT
- [reference-iem.md](docs/reference-iem.md) — what we read the IEM Plug-in Suite for, what we deliberately do not take, and where the GPL line sits
- [reference-denoise.md](docs/reference-denoise.md) — noise reduction for whispered and breathy voice: why harmonic-based methods do not apply, and why binaural material needs one gain for both ears

## License

Free software under the **GNU Affero General Public License v3.0 or later**
(see [LICENSE](LICENSE)).

    Copyright (C) 2026 charmpic

**Audio you produce with these plugins is yours.** The AGPL covers the plugins' own source
and binaries. Rendered audio is not a derivative work of the software — see LICENSE
section 2. Recordings and audio dramas made with NekoSpace carry no AGPL obligation.
