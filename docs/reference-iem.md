# Reference: IEM Plug-in Suite

What we read it for, what we deliberately do not take from it, and where the licence line
sits. This is a reading note, not a contract — the contracts are
[realtime-contract.md](realtime-contract.md), [state-format.md](state-format.md) and
[repo-layout.md](repo-layout.md), and where they disagree with anything here, they win.

The IEM Plug-in Suite is an open-source spatial-audio suite from the Institute of
Electronic Music and Acoustics in Graz — Ambisonics encoders and decoders, a shoebox room
encoder, an FDN reverb, delays and metering, in one JUCE/CMake project.

| | |
| --- | --- |
| Licence | **GPLv3** |
| Version read | v1.15.0 (released 2025-04-10) |
| Site | https://plugins.iem.at/ |

## The licence position

This project is **AGPLv3-or-later**, and that specific pairing matters.

AGPLv3 **section 13 explicitly permits** combining an AGPL work with GPLv3 code. So
"we cannot use GPL code" is not true for us — it is legally available. What it costs is
bookkeeping: the GPL-licensed portion keeps its own licence and notices, the combination
has to be described accurately, and the option of ever relicensing that part (a permissive
DSP core, a dual GPL/commercial arrangement) is gone for good.

**Decision: read IEM to understand the algorithms, implement independently.** Not because
copying is forbidden, but because the freedom to relicense our own DSP later is worth more
than the few days copying would save — and because the algorithms below are published
techniques, not IEM inventions. Where a specific IEM design choice influences ours, it is
credited in the source comment.

If that ever changes and IEM code is pulled in directly, it lands in
[third-party-licenses.md](third-party-licenses.md) in the same commit, per the rule there.

## What is worth taking, against what we already have

### FdnReverb — the direct teaching material

Our late reverb today: 8 delay lines, Hadamard butterfly mixing, one one-pole lowpass per
line, delay lengths spread near primes, each line slowly modulated, and `room.decay`
setting a single broadband T60.

IEM's, from the official description: an FDN that works on both plain audio and Ambisonic
signals, with **two shelving filters** giving frequency-dependent reverberation time, and a
fade-in control that improves diffusion by running **a second network in parallel**.

The gap that matters is the first one. **We have a single broadband T60 and one damping
lowpass**, which can only make high frequencies die sooner. Real rooms usually ring
*longer* at the bottom, and a single decay time cannot express that. IEM's two shelving
filters in the feedback path are the standard way to buy a low band and a high band their
own T60, and that is the next thing to try in our FDN.

The parallel-network fade-in is a second idea worth remembering, and cheaper to reason
about than adding lines.

### RoomEncoder — early reflections and externalisation

A virtual shoebox with **over 200 wall reflections**, per-wall absorption, and Doppler when
source or listener moves. IEM call it the most computation-intensive plug-in in the suite.

We render **6 first-order images**, each through the HRTF at its own image direction. That
choice is already paying for itself — it is what gives a raised source a floor bounce that
arrives from below, which is one of the few elevation cues that survived our testing.

RoomEncoder is useful as the **upper bound of the same idea**, not as a target. For the
light externalisation assist inside **NekoSpace Binaural**, direct sound plus the six
first-order walls/floor/ceiling and an FDN tail remain a deliberate CPU/product tradeoff.

That decision does not define the dedicated **NekoSpace Reverb**. Its design may generate
up to third-order candidates (62 in a shoebox lattice), then prune or cluster them by
arrival time, level, cumulative energy and CPU budget. See
[Reverb architecture](../plugins/reverb/docs/architecture.md). Candidate count and active
render count are explicitly different.

### BinauralDecoder — different entry point, same dataset

IEM converts **Ambisonic** signals straight to binaural using pre-processed KU 100 HRTFs
with the MagLS approach, plus headphone EQ from Bernschütz et al.

Ours takes a mono or stereo source and places it directly, with no Ambisonic bus in
between. So the decoder is not something we can adopt as-is.

It is worth noting that this is **the same measured dataset we already experimented with**
(TH Köln KU100, Bernschütz) and drew a conclusion about — see
[elevation-findings.md](../plugins/binaural/docs/elevation-findings.md). Our finding was
that swapping to that dataset did not fix elevation, and IEM using it does not change that;
they are solving a different problem (decoding a soundfield) where its accuracy pays off
in ways it does not for a single panned source.

MagLS is still worth reading about if an internal Ambisonic bus is ever considered.

### Project structure

IEM keeps shared components — a processor base class, FDN, GUI widgets, Ambisonic helpers —
in a `resources/` directory, pins JUCE, and builds any subset of plugins from one CMake
project.

We already do the pinned-JUCE, one-configure, subset-buildable part. We deliberately do
**not** pre-populate a shared directory: see [repo-layout.md](repo-layout.md) for why
`shared/` starts empty and only gains something once a second product actually needs it.
IEM has many plugins and has earned its shared layer; we have one.

## What we deliberately do not take

- **Ambisonics as the internal representation.** It is the right answer for a suite that
  encodes, rotates and decodes soundfields. For placing one voice near one ear it adds a
  bus, an order parameter and a decoding stage between the source and the result.
- **Line count as a quality claim or user control.** A well-optimised small network can
  outperform a poorly configured large one, but that does not prove 8 universally beats
  16 or 32. Reverb freezes the current 8-line network as a baseline and compares a 16-line
  candidate at matched T60, energy and total-order intent. Density, coloration, CPU and
  listening decide; the winning line count remains hidden.
- **The research-instrument GUI.** IEM's controls are stated in the terms of the field —
  Ambisonic order, reflection coefficients, azimuth in degrees — because their users think
  in those terms. Ours say what a control is *for*. That difference is the product.

## Real-time safety is ours, not theirs

A source review at fixed IEM revision
[`39de1dd`](https://git.iem.at/audioplugins/IEMPluginSuite/-/tree/39de1dd5883f1bd8d65fe1662487f2470a1d7b55)
confirmed that size/delay changes can enter update work from the processing path. That is
not an end-to-end realtime audit and we do not use it to label IEM unsafe.

The conclusion does not depend on IEM's implementation:
[realtime-contract.md](realtime-contract.md) requires NekoSpace structural changes to be
built off the audio thread and published as immutable snapshots. The audio thread swaps at
a block boundary and crossfades using preallocated paths. Anything learned from IEM is
rebuilt to that rule rather than copied into it.

## Status of the claims here

Verified against IEM's own site and plugin descriptions:

- GPLv3; v1.15.0.
- RoomEncoder: shoebox room, 200+ reflections, most computation-intensive in the suite.
- FdnReverb: FDN for audio and Ambisonic signals; **two shelving filters** for
  frequency-dependent reverberation time; fade-in via a second parallel network.
- BinauralDecoder: KU 100 HRTFs, MagLS, direct Ambisonic-to-binaural, Bernschütz headphone
  EQ.

Verified in the fixed source revision above, but not claimed from the product page:

- `FeedbackDelayNetwork.h` uses a Fast Walsh–Hadamard transform for feedback mixing.
- RoomEncoder's reflection table contains 237 image sources through order 7.

Reported by an earlier research summary but still **not verified at the fixed source
revision**, so not relied on:

- A Freeze control.

Do not record one universal “maximum FDN size”: public documentation and source revisions
have changed. Anyone acting on a source-level detail must cite the exact revision — and
note that reading GPL source to learn an algorithm is exactly the use this note endorses.

## The next concrete uses

For Binaural's light room, the next contained experiment remains splitting the feedback
decay by frequency while retaining `room.decay` as the mid-band reference.

For the dedicated Reverb, the full design is broader: user-facing Mid Decay plus Bass/Air
ratios become a smooth internal T60 curve, measured on the rendered IR; an 8/16-line
comparison and explicit ER/late transition follow. The authoritative order is in the
[Reverb roadmap](../plugins/reverb/docs/roadmap.md), not this reading note.
