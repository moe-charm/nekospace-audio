# Third-Party Licenses (SSOT)

**This project is licensed under AGPLv3-or-later** (root `LICENSE`, verbatim FSF text).
Every dependency below must therefore be AGPLv3-compatible.

| Component | License | Status | Notes |
| --------- | ------- | ------ | ----- |
| JUCE 9.0.0 (pinned tag, fetched at configure) | AGPLv3 / commercial dual — **we use the AGPLv3 arm** | in use | Confirmed in the fetched tree's `LICENSE.md`: modules are dual-licensed AGPLv3 / JUCE 9 EULA. No paid tier needed while this project stays AGPLv3 |
| VST3 SDK (bundled inside JUCE 9.0.0) | MIT | in use | Verified in the pinned tree: `VST3_SDK/LICENSE.txt` reads "MIT License, Copyright (c) 2025, Steinberg Media Technologies GmbH". MIT is AGPLv3-compatible. "VST" remains a Steinberg trademark — the name may not be used to imply endorsement |
| Remotion 4.0.512 | Remotion License | development only | Used under Remotion's free licence for an individual/small team to author and render the demo. It is installed from npm, is not linked into a plugin or shipped in release archives, and remains under its own terms: `https://www.remotion.dev/license`. The composition source in this repository is AGPLv3-or-later |
| React 19.2.3 / React DOM 19.2.3 | MIT | development only | Runtime for the Remotion composition; installed from npm and not included in audio-plugin release archives |
| Analytic A HRTF profile | original work (this repo), AGPLv3 | in use | procedural, no external data |
| libmysofa | BSD-3-Clause | planned (TASK 6) | AGPLv3-compatible; brings zlib dependency (zlib licence, also compatible) |
| HRTF datasets | per dataset (data, not code) | planned | see hrtf-format.md table; NC-licensed sets must never ship |
| TH Köln KU100 HRIRs (`HRIR_L2702.sofa`, Benjamin Bernschütz, Technische Hochschule Köln) | **CC BY-SA 3.0** (stated in the file's own `License` attribute) | **development only — NOT distributed** | Behind `NSB_WITH_KU100=OFF` by default; the SOFA and the converted `.bhrtf` are untracked. Source: `https://sofacoustics.org/data/database/thk/`. **Modified**: converted to minimum phase and regridded to 72×13 — both must be declared if it is ever distributed, and the pack would then have to ship under BY-SA with attribution. The plugin already renders the attribution whenever the profile is active. Decide licensing again before promoting it (see docs/elevation-findings.md) |

Rules:
- Any new dependency lands here in the same commit that introduces it.
- **Every dependency linked into or distributed with a covered binary must be
  AGPLv3-compatible.** Permissive licences (MIT, BSD, ISC, zlib, Apache-2.0) are fine;
  anything GPL-incompatible or NC is a blocker. Separately installed development tools
  may have their own terms, but must be listed here and must never leak into release archives.
- HRTF data licensing is independent of code licensing — a CC-licensed dataset stays
  under its own terms as an aggregated data file and does not become AGPL. Track both.
- GUI HRTF panel must render the attribution for whichever dataset is active.
- JUCE bundles further third-party code (FLAC, Ogg Vorbis, zlib, HarfBuzz, LunaSVG,
  pnglib, jpeglib, …) — all permissive; JUCE's own `LICENSE.md` is the authority for
  that list. AAX and ASIO SDKs are proprietary-or-GPLv3 and are **not** enabled here.
