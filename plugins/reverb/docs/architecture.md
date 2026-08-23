# NekoSpace Reverb — Architecture

Status: **design contract; 16-line late field selected, Phase 4A engineering complete and
owner listening pending**.

This document defines the product boundary and the signal-processing architecture. It is
binding unless a later measured result is recorded here with the reason for the change.
Numerical acoustic targets live in [validation.md](validation.md); user-facing controls
live in [control-design.md](control-design.md).

## Product contract

1. NekoSpace Reverb is an audio effect, not an instrument and not a room-measurement tool.
2. It is voice-focused and suitable for inserts and auxiliary sends.
3. It is **headphone-first, not headphone-only**. `Binaural` is the hero wet renderer;
   `Stereo` remains a first-class, speaker-compatible renderer.
4. The dry path is passed unchanged apart from mix/bypass alignment. Reverb never takes
   ownership of direct-source azimuth, elevation, distance or HRTF.
5. Early reflections and late reverberation are separate fields with an intentional
   overlap. There is no hard ER-stop/late-start seam.
6. `Space` changes geometry and delay scale; `Decay` changes T60. One must not silently
   redefine the other.
7. Low-, mid- and high-frequency decay are independently controllable. Output tone is a
   different operation from high-frequency decay and remains a separate control.
8. The measured 8-line FDN remains a regression baseline. The accepted product path uses
   the 16-line FDN; line count is an engineering choice, never a marketing control.
9. DSP is JUCE-free. JUCE belongs at the plug-in, state and GUI edges.
10. The repository-wide [realtime contract](../../../docs/realtime-contract.md),
    [state rules](../../../docs/state-format.md) and
    [licensing SSOT](../../../docs/third-party-licenses.md) apply without exception.
11. Windows x64 VST3 is the first shipping target. The core remains portable C++17.
12. No claim that this reverb is more realistic or sounds better than another product is
    made from topology or metrics alone. Such a claim needs level-matched listening
    evidence; see [validation.md](validation.md).

## Shared audition hosts

The first audible product slice deliberately has one implementation behind three hosts.
VST3 and JUCE Standalone are formats of the same `NekoSpaceReverbProcessor` and editor.
The separate Reverb Player embeds those exact objects and contributes file transport and
audio-device selection only. It does not maintain a second set of controls or a second
DSP graph.

This shell is an owner-listening tool, not an early declaration that Phase 6 is complete.
Its reduced controls and saved state are provisional. Phase 4A replaces the completed
8/16 comparison with an unsaved `Tail only` / `Room body` comparison. The host boundary
is defined in [audition-shell.md](audition-shell.md), and the current DSP slice in
[room-body.md](room-body.md).

## Responsibility boundary

The three spatial products must not collapse into one ambiguous plug-in.

| Concern | Binaural | Reverb | Possible future Room |
| --- | :---: | :---: | :---: |
| Place the direct source around the head | **yes** | no | optional, geometry-led |
| Extreme near-ear ITD/ILD | **yes** | no | no |
| Musical early/late room effect | light assist | **yes** | secondary |
| Independent size and decay spectrum | limited | **yes** | physically constrained |
| Explicit source/listener/wall geometry | internal approximation | hidden preset structure | **yes** |
| Stereo speaker output | no | **yes** | likely |
| Binaural wet output | yes | **yes** | likely |

This boundary lets a DAW or NekoSpace Binaural establish the dry location first. Reverb
then adds a room without moving that direct image.

## Signal path

```text
Stereo input
   │
   ├──────────────────────────────────────────────────────────── Dry
   │
   └─► input conditioning / common wet pre-delay
          │
          ├─► Image-source early field
          │      Phase 4A: six first-order images
          │      future: order <= 3, maximum 62 candidates + pruning
          │      distance loss + per-surface filtering
          │      │
          │      ├─ Stereo projection ───────────────┐
          │      └─ per-reflection HRTF/ITD ────────┤ Output mode
          │                                         │
          └─► late excitation diffuser              │
                 2–4 short allpass stages            │
                 │                                   │
                 ▼                                   │
              accepted 16-line FDN                  │
                 four late-input allpass stages      │
                 FWHT/Hadamard feedback mixing       │
                 smooth T60(f) attenuation per line  │
                 deterministic slow modulation      │
                 │                                   │
                 ├─ Stereo L/R projection ───────────┤
                 └─ Binaural coherence stage ────────┘
                                    │
                              late-only ducking
                                    │
                       wet tone / ER-late energy trim
                                    │
                         Dry/Wet sum, output safety
                                    │
                              Stereo output
```

### Input and dry path

The shipping bus is stereo-to-stereo so FL Studio and ordinary send workflows remain
predictable. Mono material may arrive duplicated by the host. The dry path is not summed,
HRTF-filtered or widened.

Phase 1 established an energy-bounded Mid/Side transform with two independent 8-line
networks. Phase 3 retained that channel architecture and replaced each historical 8-line
network with the accepted 16-line/4-stage path. Mid and Side wet outputs are reconstructed
as `L = Mid + Side`, `R = Mid - Side`. This deliberately spends more state than a mono
feed to make these invariants exact:

- mono input produces a centred, symmetric field;
- stereo input does not collapse or reverse the existing image;
- polarity-opposed side information does not disappear from the wet feed;
- input and output energy stay bounded and comparable across mono and stereo material;
- `Mix = 0` and room bypass reduce to the aligned dry signal exactly.

A simple `(L + R) / 2` downmix is therefore not used. The 8-line implementation remains
available only as analysis and regression evidence; it is not a second shipping path or
a current owner control.

### Early field

Phase 4A implements six first-order shoebox images around the accepted 16-line tail. The
early renderer converts the stereo input to Mid/Side, retains Mid at unity and retains
Side at `earlySideRetention = 0.5`, then reconstructs L/R per reflection. This preserves
mono symmetry and existing stereo difference information without turning the first-order
room into two disconnected hard-panned reverbs.

Pre-delay, per-ear reflection delays and the late-excitation geometry delay change through
a delay smoother capped at **0.5 sample of delay per processed sample**. This bound applies
even when the requested geometry move is larger than the ordinary smoothing interval.

The six-image renderer is the accepted Phase 4A implementation, not the final Phase 4
ceiling. Later work may generate image candidates through reflection order 3:

| Maximum order | New images | Cumulative candidates |
| ---: | ---: | ---: |
| 1 | 6 | 6 |
| 2 | 18 | 24 |
| 3 | 38 | 62 |

The candidate ceiling is fixed at compile time. The renderer retains reflections by
arrival time, level, cumulative energy and the measured ER/late transition—not by order
alone. Per-wall surface data controls reflection gain and filtering. Presets own those
correlated structural values; the main UI does not expose 62 paths or six absorption
coefficients.

Sixty-two is a **geometry-candidate capacity, not a promise to run 62 two-ear FIRs**. The
active render count has a separate CPU budget established by Release benchmarks at 48 and
192 kHz. Weak or late candidates are pruned; perceptually adjacent late candidates may be
clustered only if arrival energy and direction tests stay within tolerance. The active
limit is recorded in the implementation contract after Phase 4, not guessed here.

In `Binaural` output mode, every retained reflection is rendered from its image direction
with a short HRTF and its physical far-field ITD. In `Stereo` mode, the same geometry is
projected to a speaker-compatible stereo field without HRTF. The two modes share arrival
times, surface losses and pruning so they describe the same room rather than two unrelated
algorithms.

No diffuser is placed before the early field: its sparse arrivals and direction are the
information the early renderer exists to preserve.

### ER/late overlap

The late path is excited independently from the predelayed input to avoid feeding every
explicit reflection into the FDN a second time. It fades in while the early field is still
active.

A medium-room starting region—not a frozen universal constant—is:

- late excitation begins around 15–30 ms;
- late energy fades in across roughly 20–60 ms;
- useful explicit reflections may continue to 80–100 ms.

Preset geometry can move these times. The acceptance condition is continuity of the
bandwise energy envelope, not any one millisecond boundary.

### Late field

The current 8-line, modulated Hadamard FDN in Binaural is the first baseline because it is
working code with regression coverage. It is not copied and declared finished.

The late-field development order is fixed:

1. measure the unmodified 8-line baseline;
2. add frequency-dependent T60 while keeping eight lines;
3. measure density, decay surface, periodicity and coloration;
4. build a 16-line FWHT candidate with comparable total order and decay targets;
5. adopt the candidate only if measurements, CPU and listening all justify it.

Phase 3 selects 16 lines with four late-input allpasses as the product path after owner
listening. At
matched wet RMS it reaches 45 ms NED t90 and 0.140 maximum autocorrelation, versus 65 ms
and 0.180 for the best 8-line candidate. The production alias points to 16 lines, while
the complete 8-line type remains available for regression evidence only.

The feedback matrix stays orthogonal and densely connected. FWHT/Hadamard is the v1
default because it is deterministic, already understood in this codebase and scales as
`O(N log N)`. Matrix type, line count, raw delay lengths and per-line modulation are not
user parameters.

Delay-set quality is judged by total system order, low-order integer dependencies,
spacing and modal excitation. “Nearest primes” is a construction technique, not an
acceptance criterion by itself.

### Frequency-dependent decay

For a target decay time `T60(f)`, sample rate `fs` and a delay line of `m_i` samples, the
required loop magnitude is

```text
G_i(f) = 10 ^ (-3 m_i / (fs T60(f)))
```

or in decibels:

```text
G_i,dB(f) = -60 m_i / (fs T60(f)).
```

The UI supplies three perceptual degrees of freedom—mid T60, low/mid ratio and high/mid
ratio. Internally they become a smooth target curve with enough frequency anchors to
avoid three disconnected shelf regions. Each delay line receives a stable causal filter
approximating its own target loop magnitude. The rendered impulse response, not the
coefficient calculation alone, is the acceptance evidence.

Phase 2 implements this as a complementary low/mid/high split in every feedback line.
The three bands sum exactly to the unfiltered input when both tail ratios equal one, so a
neutral decay curve has no hidden damping. A bounded six-iteration fit solves the common
gain and low/high ratios against 125 Hz, 1 kHz and 8 kHz complex responses. It allocates
nothing and runs only when settings change. Delay length and the fitted gains ramp over
50 ms.

Static target lengths are rounded to integer samples. Leaving them fractional introduced
Space-dependent Hermite interpolation loss at high frequencies. Automation still crosses
fractional positions through the length smoother, avoiding a stepped pitch/time change.
Whenever `Space` changes a delay length, all three attenuation targets are recomputed so
measured T60 remains constant. This separation is a product invariant.

### Modulation

Modulation exists to suppress static ringing, not to announce itself as chorus. Natural
presets begin their search around 0.07–0.35 Hz and 0.25–1.5 samples, with deterministic
per-line phases and rates. These are tuning seeds, not standards. Sustained-tone pitch
wander and sidebands decide the final range.

All random or pseudo-random choices use a stable seed in tests and a serialized or
otherwise reproducible definition in production. Reloading a project must not invent a
new room.

### Late spatial field

Stereo mode uses energy-normalised L/R projections and must remain useful on speakers.
Binaural mode additionally controls the late field's frequency-dependent interaural
coherence. It does **not** render thousands of virtual HRTF reflections.

Applying a binaural-coherence method to an FDN projection is a product hypothesis, not an
already proven improvement. The feature lands only with:

- a measured interaural-coherence curve per test band;
- mono compatibility and bounded output energy;
- an exact bypass/reference path;
- listening against the same tail without the stage.

`Envelopment` controls this stage through a bounded perceptual macro. It must not become
an unrestricted phase randomiser.

### Ducking and output

Ducking acts on the late bus only. The early field and dry signal remain intelligible;
the FDN continues to receive full excitation while its output is held down, so the tail
opens naturally after a phrase. The existing Binaural detector is the behavioural
reference, not automatically the final implementation.

Wet tone follows spatial rendering and remains separate from decay filtering. Safety
limiting, if retained, must expose gain reduction and must not hide instability in the
network.

## Latency and tail

The v1 target is **zero reported processing latency**. Pre-delay and physical reflection
arrival are part of the effect and are not compensated away. Any future look-ahead,
partitioned convolution or other true processing delay must be reported to the host and
the bypass/dry path must remain aligned.

Tail length is reported from the common wet pre-delay, the longest retained reflection and
the **maximum active-band T60**, plus a documented safety margin. It is not derived from
mid-band Decay alone.

The Phase 4A Bypass transition crossfades to dry over 50 ms. While bypass is requested,
the preallocated Room Body receives silence instead of new programme material, so its
existing tail cools down naturally without a callback reset. After the fade the output is
exact dry; re-enabling does not release material that arrived during steady bypass.

## Real-time state publication

All maximum buffers and candidate reflection slots are allocated during `prepare`.
Ordinary parameter automation updates preallocated coefficients through bounded smoothers.
For the fixed Phase 4A graph, `RoomBodyCore::setSettings` is owned and called only by the
audio thread. It updates preallocated state and fixed-cost targets; no UI or worker thread
may call it concurrently. Pre-delay and geometry-delay targets obey the 0.5
sample-per-sample slew cap.

Any change that requires rebuilding a filter bank, reflection topology or network state
is constructed away from the audio thread, published as an immutable object and swapped
at a block boundary through a bounded crossfade. The old object is reclaimed away from
the audio thread.

The audio callback performs no allocation, deallocation, locking, file access, network
access, string work, logging or UI access. Hosts may provide odd and changing block sizes;
processing therefore uses prepared chunks and is block-size invariant at static settings.

Phase 4A's seven automated engineering gates and the complete seven-test Release CTest set
pass. pluginval 1.0.4 also passes strictness 10 for three randomised VST3 repeats. Owner
listening remains the open Phase 4A product decision, while the provisional bandwise
ER/late seam analysis remains pre-release acoustic work. The Steinberg VST3 validator
subtest was skipped because no validator path was configured; it remains Phase 7 work and
must not be inferred from the current unit/integration result. See
[room-body.md](room-body.md) for exact measurements and caveats.

## Reuse from Binaural

Reverb is the second real consumer that can justify a shared DSP layer. Extraction is a
behaviour-preserving step, not a redesign mixed into feature work.

The first promoted primitive is the product-neutral `nekospace::dsp::FractionalDelay`.
Binaural retains its `nsb::FractionalDelay` alias, and the frozen six-second baseline WAV
is bit-identical before and after extraction (SHA-256
`D5F48288890CA8094D7B3AC5942EC85D633EFF39B08F2F7ECCFB481A1ED37254`). Smoothers,
biquads, FIR helpers and matrix utilities remain product-local until a second concrete
consumer justifies another behaviour-preserving extraction. Binaural's `HrtfDatabase`,
elevation model, source geometry and full `RoomEngine` remain product-owned.

Reverb development must not silently retune Binaural's released room, parameters, presets
or saved-state meaning. A shared primitive may be improved only through an explicit
Binaural migration/compatibility decision or through a Reverb-specific configuration
that preserves Binaural's validated behaviour.

## Deferred beyond v1

- convolution IR loading and IR-to-algorithm fitting;
- filter-feedback/scattering matrices and internal scattering delays;
- multi-slope or coupled-room decay;
- arbitrary room/source/listener geometry;
- Ambisonics and surround buses;
- head tracking;
- Freeze;
- user-selectable network size, matrix or per-line controls;
- macOS/AU until hardware, signing and host validation are available.

These are not hidden Phase 1 requirements. Each needs its own evidence and product reason.
