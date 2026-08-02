# Changelog

Notable changes per release. Dates are ISO. This project follows the compatibility rules
in [docs/state-format.md](docs/state-format.md) and [docs/identity.md](docs/identity.md).

## [0.1.0-alpha] — unreleased

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
- Four HRTF profiles: Analytic A (legacy), Analytic B (default), an experimental measured
  KU100 pack (development builds only), and Custom via the Elevation Lab.
- Zero-latency direct path; the reported 2 ms is the near-field base delay, so host PDC
  stays correct. Host bypass is delay-compensated.
- Safety limiter after the output trim, with a gain-reduction meter.

### Known limitations

- **Elevation is weak.** It produces a real, audible change and a mild sense of vertical
  movement, but it is not a dependable cue. See
  [elevation-findings.md](plugins/binaural/docs/elevation-findings.md) for the four
  approaches tried and what each measured.
- The measured KU100 profile is 48 kHz only, is not bundled (CC BY-SA), and requires
  `-DNSB_WITH_KU100=ON` plus a locally generated pack.
- Windows only. macOS (VST3 + AU, Universal 2, notarisation) is deferred.
- No user SOFA import yet.

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
