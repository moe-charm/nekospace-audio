# NekoSpace Reverb — Research Basis

This is a reading record, not an architecture contract. It distinguishes published
evidence from NekoSpace product decisions and unproven hypotheses. The architecture and
validation documents win if wording here conflicts with them.

The original 2026 design survey contained internal citation tokens that do not work in a
public repository. The links below replace those tokens with primary papers or official
project material.

## Evidence map

| Source | What it supports here | What it does **not** establish |
| --- | --- | --- |
| [Prawda et al., *Improved Reverberation Time Control for Feedback Delay Networks* (DAFx-2019)](https://www.dafx.de/paper-archive/2019/DAFx2019_paper_46.pdf) | frequency-dependent T60 targets, line-length-dependent attenuation and measuring the rendered decay | that three UI controls, 6–10 internal anchors or a universal 5% threshold are optimal |
| [Abel and Huang, *A Simple, Robust Measure of Reverberation Echo Density* (AES 121)](https://secure.aes.org/forum/pubs/conventions/?elib=13819) | normalized echo density as a robust description of reflection buildup | NekoSpace's provisional 50/80/120 ms `t90` limits |
| [Jot, Cerveau and Warusfel, *Analysis and Synthesis of Room Reverberation Based on a Statistical Time-Frequency Model* (AES 103)](https://secure.aes.org/forum/pubs/conventions/?elib=7150) | time-frequency energy-decay analysis/EDR lineage | NekoSpace's window, hop or 1.5 dB RMSE target |
| [Dal Santo et al., *Optimizing Tiny Colorless Feedback Delay Networks* (2025)](https://doi.org/10.1186/s13636-025-00401-w) | modal excitation and perceived coloration; evidence that 4/6/8-line networks can be optimized; density difficulty below 16 lines; matrix comparisons with listening tests | that 16 lines are universally optimal, or that a matrix name determines quality |
| [Fagerström et al., *Velvet-Noise Feedback Delay Network* (DAFx-2020)](https://www.dafx.de/paper-archive/2020/proceedings/papers/DAFx2020_paper_23.pdf) | velvet-noise input/output filtering as one route to faster density buildup at low cost | that every FDN can halve its line count without penalty |
| [Fagerström et al., *Binaural Dark-Velvet-Noise Reverberator* (DAFx-2024)](https://www.dafx.de/paper-archive/2024/papers/DAFx24_paper_63.pdf) | the importance and controllability of frequency-dependent interaural coherence in a binaural late field | that placing a coherence stage after an FDN automatically improves it |
| [Schlecht and Habets, *Practical Considerations of Time-Varying Feedback Delay Networks*](https://www.sebastianjiroschlecht.com/publication/schlecht-2015-tj/) | time-varying FDN design and the need to evaluate modulation artefacts | NekoSpace's proposed Natural-mode rates/depths |
| [IEM official plug-in descriptions](https://plugins.iem.at/docs/plugindescriptions/) | FdnReverb's frequency-dependent RT controls and parallel-network fade-in; RoomEncoder's shoebox reflections and Ambisonics workflow | that IEM directly convolves every RoomEncoder reflection with an HRIR |
| [IEM Plug-in Suite source, fixed revision `39de1dd`](https://git.iem.at/audioplugins/IEMPluginSuite/-/tree/39de1dd5883f1bd8d65fe1662487f2470a1d7b55) | source-level reference for FWHT and the implementation as it existed at that revision | a timeless description of later IEM versions or a realtime-safety guarantee for NekoSpace |

Some AES full papers require library or member access; their public records still identify
the work. Do not copy unavailable details from secondary summaries and present them as if
verified in the primary paper.

## Product decisions derived from the evidence

These are NekoSpace engineering choices, not literature facts:

- begin with the existing 8-line FDN and compare a 16-line candidate;
- keep FWHT/Hadamard for v1 unless evidence shows a concrete failure;
- expose Mid Decay, Bass Tail and Air Tail while using a smoother internal T60 curve;
- generate at most 62 order-3 early-reflection candidates, then prune by time and energy;
- define the product-specific NED `t90` and provisional scene limits;
- use a common wet pre-delay and an overlapping ER/late transition;
- hide topology and express the result as Space, Definition, Envelopment and Motion;
- retain Stereo output while making Binaural wet rendering the hero mode.

Each decision may change after the measurement harness produces contradictory evidence.
The change must record the before/after result, not just “sounds better.”

## Explicit hypotheses

The largest differentiating idea is also the least established:

> Project the FDN to two channels, then control its frequency-dependent interaural
> coherence to create a more enveloping binaural late field.

The DAFx-2024 result is for a dark-velvet-noise reverberator. Applying its lesson to an
FDN output is our hypothesis. Phase 5 therefore retains an exact bypass/control render and
accepts the feature only with objective coherence, energy/mono safety and listening
evidence.

Likewise, order-3 HRTF early reflections are not advertised as novel. IEM, Steam Audio and
other systems already render directional room information. NekoSpace's product value is
the constrained voice workflow and the measurable connection from explicit early
directions to a statistically controlled late field—not the claim that directional
reflections were invented here.

## Competitive claims

Commercial products are useful listening references, but their unpublished internal
topologies are not design evidence. Public feature pages may establish exposed features;
they do not justify claims about hidden matrices, delay counts or filters.

The repository therefore avoids statements such as “Pro-R is an FDN,” “Bricasti uses this
matrix,” or “our direct HRTF path is better than an Ambisonics decoder” without primary
evidence. Architecture decisions are evaluated on NekoSpace's own rendered behaviour.

## Code and licence boundary

Papers, equations and public algorithm descriptions may be independently implemented.
Copying an implementation's control flow, constants or source structure is different and
requires file-level licence review and attribution.

IEM is GPLv3 and this repository is AGPLv3-or-later; GNU GPLv3 section 13 permits the
combination, but copied code would retain its notices and reduce future relicensing
freedom. The current decision remains: **read and credit IEM; implement independently**.
If code is ever imported, [third-party-licenses.md](../../../docs/third-party-licenses.md)
must change in the same commit.
