# NekoSpace Audio

日本語版は [README.ja.md](README.ja.md) にゃ。

Open-source audio tools for voice, ASMR and audio drama. C++17 / JUCE / CMake,
licensed under AGPLv3-or-later.

| Product | Purpose | Status |
| --- | --- | --- |
| [**NekoSpace Binaural**](plugins/binaural/README.md) | place voice and effects around the listener | `v0.2.0-alpha` |
| [**NekoSpace Reverb**](plugins/reverb/README.md) | build a natural room around an existing stereo image | owner-audition prototype |
| [**NekoSpace CleanVoice**](plugins/cleanvoice/README.md) | learn and remove steady noise from whispered voice | offline prototype |

<table>
  <tr>
    <th width="33%">NekoSpace Binaural</th>
    <th width="33%">NekoSpace Reverb</th>
    <th width="33%">NekoSpace CleanVoice</th>
  </tr>
  <tr>
    <td><img src="docs/images/gui-main.png" alt="NekoSpace Binaural interface"></td>
    <td><img src="docs/images/reverb-main.png" alt="NekoSpace Reverb interface"></td>
    <td><img src="docs/images/cleanvoice-main.png" alt="NekoSpace CleanVoice interface"></td>
  </tr>
  <tr>
    <td>Voice-focused 3D placement with near-ear distance control.</td>
    <td>Six early reflections, a deterministic 16-line tail and six starting presets.</td>
    <td>Noise-region learning with Original / Clean / Removed monitoring.</td>
  </tr>
</table>

## Demos

- ▶ [NekoSpace Binaural — 74-second operation demo](https://youtu.be/nk54N0w6FOE)
  — headphones or earphones recommended.
- **NekoSpace Reverb** — narrated feature-tour render is ready locally; the reproducible
  Remotion/VOICEVOX composition is in [`video/`](video/README.md). A published link will
  replace this note after upload.
- **NekoSpace CleanVoice** — demonstration planned after representative noisy voice material
  is cleared for publication.

Recordings, generated narration and rendered videos are ignored globally. Only reproducible
code, narration text and timing are versioned.

## Honest status

### NekoSpace Binaural

The Windows VST3 and Standalone provide left/right, front/back, distance and strong near-ear
placement. Elevation creates an audible colour and mild vertical movement, but it is not a
dependable above/below cue: static non-individualised HRTFs cannot know the listener's pinnae,
and headphones colour the same 5–12 kHz band. Treat height as supporting production colour,
not a load-bearing narrative cue. The measurements and full explanation live in the
[Binaural README](plugins/binaural/README.md).

### NekoSpace Reverb

The real VST3, JUCE Standalone and file Player share one processor and editor. The current
Room Body combines bounded first-order reflections with the accepted low-colour 16-line tail.
Factory starting points are **Default, Voice Booth, Small Wood Room, Dialogue Stage, Soft
Chamber and Open Hall**; editing a value changes the selector to `Custom`, and Reset restores
Default.

On 2026-08-28 the owner accepted the current direction as natural in sighted audition and the
VST3 loaded and processed audio in FL Studio. This is not a public alpha yet: CPU/memory
evidence, final parameter freeze, release packaging and the remaining validator gate are
still explicit work.

### NekoSpace CleanVoice

The offline desktop app and CLI learn a fixed profile from a selected noise-only region. The
app previews the exact same range as **Original / Clean / Removed Noise** before whole-file
processing. One shared spectral gain is applied to every channel, so noise removal does not
dissolve a binaural image. It is not a realtime plug-in yet; see the
[CleanVoice workflow](plugins/cleanvoice/README.md).

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
