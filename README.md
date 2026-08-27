![Abstract cat-ear spatial-audio waves in the NekoSpace Audio colours](docs/images/banner.jpg)

# NekoSpace Audio

[![Build](https://github.com/moe-charm/nekospace-audio/actions/workflows/ci.yml/badge.svg)](https://github.com/moe-charm/nekospace-audio/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/moe-charm/nekospace-audio?include_prereleases&label=release)](https://github.com/moe-charm/nekospace-audio/releases/tag/v0.2.0-alpha)
![Windows x64](https://img.shields.io/badge/platform-Windows%20x64-00a8d6)
![VST3 and Standalone](https://img.shields.io/badge/format-VST3%20%7C%20Standalone-f28c28)
[![AGPL-3.0-or-later](https://img.shields.io/badge/license-AGPL--3.0--or--later-8a8f98)](LICENSE)

Open-source spatial and restoration tools built for voice, ASMR and audio drama.

日本語版は [README.ja.md](README.ja.md) にゃ。

### [⬇ Download NekoSpace Binaural v0.2.0-alpha for Windows x64](https://github.com/moe-charm/nekospace-audio/releases/download/v0.2.0-alpha/NekoSpaceBinaural-v0.2.0-alpha-Windows-x64.zip)

VST3 effect and DAW-free Standalone included. Headphones or earphones recommended.
[Watch the 74-second demo](https://youtu.be/nk54N0w6FOE) ·
[Read the product guide](plugins/binaural/README.md)

## A voice-production path

| 1 — Clean | 2 — Place | 3 — Give it a room |
| --- | --- | --- |
| **CleanVoice** learns a noise-only region without collapsing the stereo image. | **Binaural** moves voice and effects around the listener, including strong near-ear placement. | **Reverb** adds early reflections and a natural tail while respecting the existing image. |

CleanVoice and Reverb are currently source-built development products. Binaural is the
packaged public alpha.

## Featured — NekoSpace Binaural

[![NekoSpace Binaural spatial audio interface](docs/images/gui-main.png)](plugins/binaural/README.md)

- Voice-focused left/right, front/back, distance and near-ear placement.
- Natural and Enhanced HRTF profiles, directional room assistance and factory scene presets.
- Current source builds VST3, Standalone and a file Player around the same processor and editor.

**[Download for Windows](https://github.com/moe-charm/nekospace-audio/releases/download/v0.2.0-alpha/NekoSpaceBinaural-v0.2.0-alpha-Windows-x64.zip)** ·
[Demo](https://youtu.be/nk54N0w6FOE) · [Details and limitations](plugins/binaural/README.md)

## In development

<table>
  <tr>
    <th width="50%"><a href="plugins/reverb/README.md">NekoSpace Reverb</a></th>
    <th width="50%"><a href="plugins/cleanvoice/README.md">NekoSpace CleanVoice</a></th>
  </tr>
  <tr>
    <td><a href="plugins/reverb/README.md"><img src="docs/images/reverb-main.png" alt="NekoSpace Reverb interface"></a></td>
    <td><a href="plugins/cleanvoice/README.md"><img src="docs/images/cleanvoice-main.png" alt="NekoSpace CleanVoice interface"></a></td>
  </tr>
  <tr>
    <td><strong>In development — DSP and UI working.</strong><br>Six early reflections, a deterministic 16-line tail and six factory starting points. VST3, Standalone and file Player share the real processor.</td>
    <td><strong>Prototype — offline app and CLI.</strong><br>Learn a noise-only region, then compare Original, Clean and Removed before processing the whole file.</td>
  </tr>
</table>

The Reverb feature-tour source is reproducible under [`video/`](video/README.md). A CleanVoice
demo waits for representative noisy voice material cleared for publication. Private
recordings, generated narration and rendered media stay outside Git.

## Honest engineering

- **Binaural:** horizontal and near-ear placement are the strengths. Elevation is useful
  production colour, not a guaranteed above/below cue with non-individualised static HRTFs.
- **Reverb:** its direction was accepted as natural in owner audition and its VST3 processed
  audio in FL Studio; CPU/memory evidence, packaging and the remaining release gates are not
  complete.
- **CleanVoice:** the fixed-profile workflow and shared channel gain work offline; it is not a
  realtime plug-in.

Measurements, limitations and validation boundaries remain in each product README instead
of being hidden or turned into marketing claims.

## Install

The current public Windows release is NekoSpace Binaural. Download its zip from
[Releases](https://github.com/moe-charm/nekospace-audio/releases), unpack it, and copy the
whole `NekoSpace Binaural.vst3` bundle to:

```text
C:\Program Files\Common Files\VST3\
```

Then rescan plug-ins. In FL Studio: *Options → Manage plugins → Find more plugins*.
`NekoSpace Binaural.exe` runs without a DAW. Headphones are required for binaural rendering.

Reverb and CleanVoice are development builds from source until their own release gates and
packages are complete.

## Build

Requires CMake 3.22 or newer, a C++17 compiler (Visual Studio 2022 on Windows), and git.
JUCE is fetched once by the top-level configure and shared by all products.

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release
```

## Repository layout

```text
nekospace-audio/
├─ plugins/
│  ├─ binaural/      DSP, VST3/Standalone, file Player, tests and product docs
│  ├─ reverb/        independent DSP, VST3/Standalone, file Player and analyzer
│  └─ cleanvoice/    JUCE-free DSP/CLI plus a JUCE offline editor
├─ shared/           product-neutral DSP proven by multiple consumers
├─ docs/             suite contracts and publication-safe images
├─ video/            Remotion source; private media and renders remain ignored
└─ cmake/            build helpers
```

See [repository layout](docs/repo-layout.md) for ownership rules and
[README structure](docs/readme-structure.md) for the landing-page contract.

## Shared contracts

- [Identity](docs/identity.md) — copyright holder, brand and permanent plug-in codes
- [State format](docs/state-format.md) — backwards-compatible host project state
- [Realtime contract](docs/realtime-contract.md) — audio-thread rules
- [Third-party licences](docs/third-party-licenses.md) — dependency licensing SSOT
- [IEM reference boundary](docs/reference-iem.md) — what was studied and the GPL line
- [Denoise research](docs/reference-denoise.md) — whispered-voice protection and shared gain
- [Video production](docs/video-production.md) — private-media boundary and reproducible demos

## Releases

Pushing a `v*` tag runs the release workflow, builds and tests the declared release product,
packages licences/notices and opens a draft GitHub Release. See
[CHANGELOG.md](CHANGELOG.md) for frozen IDs and release history.

## License

Free software under the **GNU Affero General Public License v3.0 or later** — see
[LICENSE](LICENSE).

    Copyright (C) 2026 charmpic

**Audio produced with these tools belongs to you.** The AGPL covers the software source and
binaries, not recordings or rendered audio created with it; see LICENSE section 2.
