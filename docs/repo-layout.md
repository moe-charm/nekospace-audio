# Repository Layout

```
nekospace-audio/
├─ CMakeLists.txt     fetches JUCE once, adds each product
├─ plugins/
│  ├─ binaural/       CMakeLists, src/, tests/, docs/, tools/, resources/
│  ├─ cleanvoice/     JUCE-free DSP/CLI plus a JUCE standalone GUI
│  └─ reverb/         design contract + Phase 0 analysis; independent DSP begins in Phase 1
├─ shared/            code proven to be needed by more than one plugin
├─ docs/              contracts that apply across products
├─ video/             Remotion source; private media and renders stay ignored
└─ cmake/             build helpers
```

## Why one repository

Suite products that may share DSP and UI. A monorepo lets a change to shared code and the
plugins that use it land in a single commit, with one CI run. Separate repositories would
need a submodule or a package for the shared layer — real overhead for a solo developer,
bought with no benefit at this size.

## Why `shared/` starts empty

**Nothing is promoted to `shared/` until a second plugin actually needs it.**

Promoting early creates a published API with exactly one consumer: every later change has
to be justified against a contract nobody is holding you to yet, and the code ends up
shaped for an imagined second user rather than the real one. Waiting costs one mechanical
move later; not waiting costs design freedom now.

Concretely, Binaural currently owns plenty that *looks* shareable — `FractionalDelay`,
`CrossfadeFir`, `FdnReverb`, the smoothers and biquad helpers. A Reverb design document is
not a second code consumer: those pieces stay put until Reverb Phase 1 actually uses them.
Extraction then lands as a behaviour-preserving commit with both products' tests green,
before any redesign of the primitive.

The genuinely binaural-specific parts — `HrtfDatabase`, `ElevationModel`,
`BinauralEngine`, the head-and-ear geometry — stay in the plugin permanently.

## Which docs live where

**Top-level `docs/`** — anything true of every product: the identity and plugin-code
reservations, the state-format rules, the realtime contract, third-party licensing.

**`plugins/<product>/docs/`** — anything true of one product: its architecture, its
parameter contract, its roadmap, its data formats.

Parameter IDs are per-plugin, so the *contract* is per-plugin; the *rules* those contracts
obey are shared. `docs/state-format.md` currently carries Binaural's layout as its worked
example; when a second plugin gains its own state, that layout section moves down to
`plugins/binaural/docs/` and the rules stay here.
