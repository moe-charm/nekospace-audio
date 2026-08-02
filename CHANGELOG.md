# Changelog

Notable changes per release. Dates are ISO. This project follows the compatibility rules
in [docs/state-format.md](docs/state-format.md) and [docs/identity.md](docs/identity.md).

## [0.2.0-alpha] — 2026-08-03

Minor rather than patch: this adds a parameter and bumps the state schema.

### Room: decay is its own control

- **`DECAY` is separate from `SIZE`.** They used to be one knob — the tail was derived
  from the room's dimensions as `0.25 + 2.4·size²` s — which made the most useful room in
  audio drama impossible to build. A tiled bathroom is a *small* room with a *long*,
  bright tail; deriving one from the other forced you to pick. The shipped `Bathroom`
  preset rang for 0.37 s, which is why it never sounded like tile. It is now 1.15 s in a
  room that is still small.
  Measured: at size 20 %, decay 0.3 s gives a 0.18 s tail and decay 2.0 s gives 1.05 s;
  at size 80 %, decay 0.3 s still gives 0.18 s — size no longer touches tail length.
- **The FDN delay lines are modulated.** Eight fixed-length lines beat against each other
  and ring metallically. Each line now wanders by a fraction of its length on its own slow
  LFO, at rates chosen not to share factors so the lines never fall back into step. Depth
  is small on purpose: enough to break up flutter, well short of audible pitch movement on
  a sustained vowel.
- Presets retuned around the split — `Stairwell` 2.3 s, `Next Room` 0.95 s through a wall,
  `Ambience / Music` 1.4 s.

### Compatibility

- **State schema 3.** Adding a parameter would have needed no bump, but taking decay away
  from `room.size` changes what an existing `room.size` value *means*, which is rule 8.
  Projects saved under schema 2 have their decay reconstructed from the stored size with
  the old curve, so they reload sounding as they were mixed. The new parameter's default
  (0.54 s) is what that curve gave at the default size, so new instances are unchanged
  too. See [docs/state-format.md](docs/state-format.md).
- `room.decay` carries `versionHint` 2 and is appended, so host display order does not
  shift for anyone who already has v0.1.0-alpha.
- Choice lists, plugin codes and every existing parameter ID are unchanged, so
  v0.1.0-alpha projects open normally.

### Fixed

- **The right-hand column drew over the knob row on a short window.** Its layout was built
  from fixed heights and nothing checked they fit; below about 640 px the JUMP TO grid ran
  past the end of its own area and painted over the SPACE captions and the OUTPUT knob.
  Rows and buttons now give way together as the window gets shorter, the way the bottom
  bar already did. The normal size is untouched.
- The window's resize grip was drawn straight through the end of "see LICENSE" in the
  footer — the AGPL notice being the text least suited to being scribbled on.

### Docs

- Screenshots in both READMEs: the main window, the grouped preset menu, the Elevation
  Lab, and the built-in manual in English and Japanese.
- Install instructions written for someone holding the release zip, in both languages.

## [0.1.0-alpha] — 2026-08-03

First public build of **NekoSpace Binaural**. Windows VST3 (x64) and Standalone.

### What it does

- Places a mono or linked-stereo source at any azimuth, elevation and distance, including
  extreme near field — a voice at the ear rather than merely hard-panned.
- ITD is geometric, taken from the exact rigid-sphere path per ear, so near-field
  exaggeration emerges instead of being faked.
- Near Field spans a conventional panner (0 %, 5.6 dB ILD at az −90° / 12 cm) to full
  per-ear geometry (100 %, 24.2 dB).
- Room: six first-order shoebox reflections, each rendered through the HRTF at its own
  image direction, plus an 8-line FDN late reverb. `room.amount = 0` is bit-exact direct.
- **Late-only Voice Duck**: the late reverb is held down while the voice speaks and opens
  at the end of a phrase, so a close voice stays close in a live room. Direct sound and
  early reflections are never touched, and the FDN keeps being fed at full level — what
  appears at the end of a line is a tail that was building all along. No threshold to
  set: detection normalises against recent level, so one setting suits a whisper and a
  shout. Measured on the isolated room bus: −13.1 dB during a phrase, −0.7 dB after it.
- Four HRTF profiles: Analytic A (legacy), Analytic B (default), an experimental measured
  KU100 pack (development builds only), and Custom via the Elevation Lab.
- Zero-latency direct path; the reported 2 ms is the near-field base delay, so host PDC
  stays correct. Host bypass is delay-compensated.
- Safety limiter after the output trim, with a gain-reduction meter.

### Using it

- **Four macros, not twenty-four numbers.** The elevation model is driven by Up, Down,
  Body and Focus; every raw anchor value stays reachable in the Elevation Lab. At 1.00 the
  macros reproduce the default profile bit-exactly, so the simple control is not a
  lossy wrapper around the detailed one.
- **Presets are complete scenes.** Each one sets all twelve sound-shaping parameters, so
  the same name always gives the same sound rather than inheriting whatever was set
  before. Grouped as Close, Rooms, Beds and Reference; the Reference pair are diagnostics.
  Output gain, HRTF profile and quality are deliberately left alone — those belong to the
  person, not the scene.
- **JUMP TO moves the source and nothing else**, so a room you have built survives an A/B
  of position. This is the one thing a preset cannot do.
- **Built-in manual** in English and Japanese, switched from inside the help window. The
  language follows the OS on first run and is stored per user, not per project — opening
  an old session never brings back the wrong language.
- **Double-click any knob to reset it** to its default.
- **ROOM BYPASS dims exactly what it silences**, so what the switch covers is visible
  rather than something to remember.

### Known limitations

- **Elevation is weak.** It produces a real, audible change and a mild sense of vertical
  movement, but it is not a dependable cue. See
  [elevation-findings.md](plugins/binaural/docs/elevation-findings.md) for the four
  approaches tried and what each measured.
- The measured KU100 profile is 48 kHz only, is not bundled (CC BY-SA), and requires
  `-DNSB_WITH_KU100=ON` plus a locally generated pack.
- Windows only. macOS (VST3 + AU, Universal 2, notarisation) is deferred.
- No user SOFA import yet.
- **Headphones only.** Binaural rendering assumes each ear hears only its own channel;
  over speakers the crosstalk removes the effect. No crossfeed or speaker mode.

### Frozen at this release

Per [docs/identity.md](docs/identity.md) and
[docs/parameter-contract.md](plugins/binaural/docs/parameter-contract.md), the following
can no longer change without breaking existing projects:

- Manufacturer code `NkSp`, plugin code `Nksb`.
- Every parameter ID.
- The option lists of `source.mode`, `quality.mode` and `hrtf.profile` — host automation
  stores a normalised index, which no plugin-side state format can repair. Adding an
  option later requires a new parameter ID.

State schema is 2, and schema 1 remains readable.

### Verification

- 30 JUCE-free DSP acceptance tests (`ctest`), covering mirror symmetry, distance
  monotonicity, room-zero identity, automation sweeps, block-size invariance, in-place
  processing, reported latency, and the state-format choice-key resolution.
- `pluginval --strictness-level 10`, clean.
- FL Studio (Windows) is the primary acceptance DAW.
