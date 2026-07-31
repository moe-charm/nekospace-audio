# Third-Party Licenses (SSOT)

| Component | License | Status | Notes |
| --------- | ------- | ------ | ----- |
| JUCE (pinned tag, fetched at configure) | AGPLv3 / commercial dual | in use | AGPLv3 OK while source is open; JUCE Starter covers closed-source ≤ $20k/yr revenue — re-check before closed distribution |
| VST3 SDK (bundled inside JUCE) | MIT (current official steinbergmedia/vst3sdk; older releases were GPLv3/proprietary dual) | in use | "VST" is a Steinberg trademark; verify the exact terms of the SDK copy bundled in the pinned JUCE tag at release time |
| Analytic A HRTF profile | original work (this repo) | in use | procedural, no external data |
| libmysofa | BSD-3-Clause | planned (TASK 6) | brings zlib dependency |
| HRTF datasets | per dataset | planned | see hrtf-format.md table; NC-licensed sets must never ship |

Rules:
- Any new dependency lands here in the same commit that introduces it.
- HRTF data licensing is independent of code licensing — track both.
- GUI HRTF panel must render the attribution for whichever dataset is active.
