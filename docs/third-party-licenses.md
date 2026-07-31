# Third-Party Licenses (SSOT)

**This project is licensed under AGPLv3-or-later** (root `LICENSE`, verbatim FSF text).
Every dependency below must therefore be AGPLv3-compatible.

| Component | License | Status | Notes |
| --------- | ------- | ------ | ----- |
| JUCE 9.0.0 (pinned tag, fetched at configure) | AGPLv3 / commercial dual — **we use the AGPLv3 arm** | in use | Confirmed in the fetched tree's `LICENSE.md`: modules are dual-licensed AGPLv3 / JUCE 9 EULA. No paid tier needed while this project stays AGPLv3 |
| VST3 SDK (bundled inside JUCE 9.0.0) | MIT | in use | Verified in the pinned tree: `VST3_SDK/LICENSE.txt` reads "MIT License, Copyright (c) 2025, Steinberg Media Technologies GmbH". MIT is AGPLv3-compatible. "VST" remains a Steinberg trademark — the name may not be used to imply endorsement |
| Analytic A HRTF profile | original work (this repo), AGPLv3 | in use | procedural, no external data |
| libmysofa | BSD-3-Clause | planned (TASK 6) | AGPLv3-compatible; brings zlib dependency (zlib licence, also compatible) |
| HRTF datasets | per dataset (data, not code) | planned | see hrtf-format.md table; NC-licensed sets must never ship |

Rules:
- Any new dependency lands here in the same commit that introduces it.
- **Every code dependency must be AGPLv3-compatible.** Permissive licences (MIT, BSD,
  ISC, zlib, Apache-2.0) are fine; anything GPL-incompatible or NC is a blocker.
- HRTF data licensing is independent of code licensing — a CC-licensed dataset stays
  under its own terms as an aggregated data file and does not become AGPL. Track both.
- GUI HRTF panel must render the attribution for whichever dataset is active.
- JUCE bundles further third-party code (FLAC, Ogg Vorbis, zlib, HarfBuzz, LunaSVG,
  pnglib, jpeglib, …) — all permissive; JUCE's own `LICENSE.md` is the authority for
  that list. AAX and ASIO SDKs are proprietary-or-GPLv3 and are **not** enabled here.
