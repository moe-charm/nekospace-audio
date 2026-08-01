# Identity and Plugin Codes

Three different things that are easy to conflate, and one table that must never change
after a release.

| | Value | Why |
| --- | --- | --- |
| Copyright holder | **charmpic** | Copyright is held by a person. `NekoSpace Audio` is a brand, not a legal entity, so putting it in the copyright line would leave "who is that?" unanswered if the licence ever had to be enforced or a commercial licence offered alongside the AGPL. |
| Manufacturer (what DAWs show) | **NekoSpace Audio** | A brand groups the products together in a plugin list. An individual's handle there would just look untidy. |
| Repository | `moe-charm/nekospace-audio` | |

So source files carry `SPDX-FileCopyrightText: 2026 charmpic`, while CMake carries
`COMPANY_NAME "NekoSpace Audio"`.

## VST3 / AU identifiers — permanent after release

The manufacturer code and the plugin code together form the plugin's unique identity.
Change either after release and every existing project stops finding the plugin.

| Product | `PLUGIN_MANUFACTURER_CODE` | `PLUGIN_CODE` |
| --- | --- | --- |
| NekoSpace Binaural | `NkSp` | `Nksb` |
| NekoSpace Reverb (planned) | `NkSp` | `Nksr` |
| NekoSpace Room (planned) | `NkSp` | `Nksm` |
| NekoSpace Delay (planned) | `NkSp` | `Nksd` |

Rules:

- The manufacturer code is shared by every product and is **four characters with at least
  one non-lowercase**, because Audio Unit rejects an all-lowercase manufacturer code.
  `NkSp` satisfies that; `nksp` would not.
- Each plugin code is unique within the manufacturer and never reused, even if a product
  is discontinued.
- Reserve the code before shipping a product, not after. The table above is the reservation.

This changed once, before any release: the manufacturer was `Txvc` (TextureVoice), which
is a separate and unrelated project. That rename was free only because nothing had
shipped.
