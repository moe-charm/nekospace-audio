# Third-Party Licenses (SSOT)

| Component | License | Status | Notes |
| --------- | ------- | ------ | ----- |
| JUCE (pinned tag, fetched at configure) | AGPLv3 / commercial dual | in use | AGPLv3 OK while source is open; JUCE Starter covers closed-source ≤ $20k/yr revenue — re-check before closed distribution |
| VST3 SDK (bundled inside JUCE) | GPLv3 / Steinberg proprietary dual | in use | "VST" is a Steinberg trademark; check current SDK license text at release time |
| Analytic A HRTF profile | original work (this repo) | in use | procedural, no external data |
| libmysofa | BSD-3-Clause | planned (TASK 6) | brings zlib dependency |
| HRTF datasets | per dataset | planned | see hrtf-format.md table; NC-licensed sets must never ship |

Rules:
- Any new dependency lands here in the same commit that introduces it.
- HRTF data licensing is independent of code licensing — track both.
- GUI HRTF panel must render the attribution for whichever dataset is active.
