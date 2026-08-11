# Reference: noise reduction for whisper and breath

Design notes and sources for **NekoSpace CleanVoice**. Written before the first line of it
existed, so that the implementation started from decisions rather than a blank file, and
kept up to date since — v0 and v1 of the roadmap below are built; see
[the product readme](../plugins/cleanvoice/README.md) for how to run them.

Compiled 2026-08-03 from a literature and product survey. Where the survey's sources are
primary (papers, official product docs, licence files) the claim is treated as established;
where it is inference, that is said so.

## The problem, stated precisely

The target material is whispered and breathy voice recorded on a Neumann KU 100 dummy head
for audio drama and ASMR. The noise to remove is stationary: air conditioning, PC fans,
microphone hiss.

The difficulty is not incidental, it is the whole problem:

- Whispered speech has **no periodic vocal-fold excitation, no stable F0, no harmonic
  series**. Unvoiced fricatives (`s`, `ʃ`, `f`, `h`) and breath are **broadband noise**.
- So the usual discriminator — "harmonic structure means voice" — is unavailable, and
  conventional VADs are known to degrade on whispered speech.
- What does survive: **vocal-tract resonance shaping the spectral envelope, and temporal
  modulation of band energy**. Articulation and breathing move; a fan does not.

Everything below follows from that last line.

## The decision

> Build the noise profile from **verified pure-noise regions** and treat it as the source
> of truth. Apply a conservative **OM-LSA** with decision-directed a-priori SNR. Use
> whisper-specific features **only to protect, never to decide what to remove**. Apply
> **one real-valued gain to both channels**. Keep adaptive estimation out of v1.

Shallow, smooth, and the same on both ears. The product is not "remove the most noise"; it
is "remove only what is provably noise, and prove the breath survived".

## What answers the make-or-break question

The question that decides whether this product can exist: **how do you tell breath from
hiss without harmonics?** There is real literature, and it does not rely on pitch.

| Feature | What it measures | Why it separates breath from a fan |
| --- | --- | --- |
| **LTLEV** (long-term log-energy variation) | spread of per-band log energy over 100–500 ms | articulation moves band energy; hiss barely moves. Proposed specifically for whisper activity detection |
| **Modulation spectrum** | energy in the band envelope at ~2–20 Hz vs near DC | stationary noise concentrates at DC; speech and breathing modulate |
| **Long-term spectral divergence** | distance between the running spectrum and the fixed noise profile | uses the profile we already trust as the reference |
| **Positive spectral flux** | Σ max(\|Y(k,m)\|−\|Y(k,m−1)\|, 0) | catches fricative and inhale onsets, which is where a smoothed estimator lags |

Two implementation notes that matter more than the feature list:

- **The features gate protection, not removal.** A high score raises the gain floor and
  freezes the noise estimate. A low score never *causes* deeper attenuation. Getting this
  backwards turns the product into a breath remover.
- ASMR breathing is slower than speech. The 2–20 Hz modulation band is a starting value,
  not an optimum; a slow band (~0.2–2 Hz) should be tracked separately or long exhales
  read as stationary.

**Known limit, and it is a hard one.** Where a sustained breath and microphone hiss occupy
the same time-frequency cell with the same statistics and the same direction, a single
recording cannot separate them. Long-term features improve the odds; they do not make a
decider. This is why the design rule is *do not attenuate deeply unless confident it is
noise*, rather than *keep it if confident it is voice*.

## Binaural material: one gain, both ears

This is the constraint most easily got wrong, and the maths is unambiguous.

Processing channels independently gives `Z_L = G_L·Y_L`, `Z_R = G_R·Y_R`, so

```
Z_L / Z_R  =  (G_L / G_R) · (Y_L / Y_R)
```

Any time- or frequency-varying difference between `G_L` and `G_R` **is** a change in
interaural level difference. Applying one real gain `G` to both:

```
|Z_L| / |Z_R|      = |Y_L| / |Y_R|          (ILD preserved exactly)
arg Z_L − arg Z_R  = arg Y_L − arg Y_R      (IPD preserved exactly)
```

per time-frequency bin. Not approximately — exactly. Broadband localisation and interaural
coherence can still shift if attenuation is uneven across frequency, so this is "far safer",
not "perceptually identical".

How to combine the two candidate gains, for v1: **`G = max(G_L, G_R)`**. A whisper close to
one ear is loud on that side and buried on the other; taking the minimum lets the far ear's
low SNR erase the near ear's evidence. The cost is that noise loud in only one ear is also
under-removed. That trade is correct for this material.

M/S is not an escape: the same gain on M and S is algebraically identical to the same gain
on L/R, and *different* gains on M and S change image width and correlation, which is
exactly what a dummy-head recording exists to preserve.

Audible failure modes to listen for when this is wrong: the whisper wandering left and
right, residual noise pulsing separately in each ear, the image collapsing inside the head,
width breathing in time with the speaker.

Worth knowing: **Adobe's Enhance Speech downmixes stereo to mono** per its own published
requirements. Whatever its quality, it cannot be used on KU 100 material.

## Correcting an earlier recommendation

Earlier in this project I argued that **Adaptive** estimation should be preferred over a
learned fixed profile for whisper, reasoning that whisper material has no clean silence and
a learn pass would capture breath as noise.

**That was wrong, and the correction changes the v1 design.** The asymmetry I missed: with
a fixed profile *you* choose the region and can verify it — listen to it, check no breath is
in it — and offline processing makes that free. An adaptive estimator makes that choice
continuously and unsupervised, and a long sustained breath looks exactly like a noise floor
that has risen. The failure is also insidious: the output sounds right at first and the
breath erodes over the following seconds as the estimator learns it.

So: **fixed profile is primary; adaptation is a restricted later feature**, permitted only
when both ears agree there is no signal, protection scores are low, spectral flux is low,
the state has held for several hundred milliseconds, and the result stays within a few dB of
the fixed profile. Rise should track slowly (tens of seconds); falling — which weakens
suppression — can be faster.

Also note: taking a low percentile of the whole file as the noise floor has the same defect
as adaptation, for the same reason.

## Method choice

Spectral subtraction is the baseline to compare against, not a candidate — small errors in
the noise PSD leave isolated residual peaks, which is textbook musical noise. Wiener is
smooth but dulls at low SNR; it is the right first thing to implement as a *test
instrument*, to prove the STFT, the PSD estimate and the evaluation harness are correct
before the real estimator arrives.

**OM-LSA** is the recommendation, and the reason is structural rather than acoustic: it
gives one gain expression that carries a soft speech-presence probability, an explicit
minimum gain, and a place to inject our own protection score. It does **not** understand
whispers — its stock presence probability is built for ordinary speech and would happily
call breath "absent". It is chosen because it is the easiest to bias toward protection.

Musical-noise countermeasures, in order of how much they matter here:

1. **Do not over-estimate the noise PSD.** No amount of gain smoothing recovers from breath
   in the noise profile; it just removes the breath smoothly.
2. **Decision-directed a-priori SNR**, with `α ≈ 0.95–0.99` — but lowered on detected
   onsets. A permanently high `α` is the direct cause of clipped `s` and swallowed inhales.
3. **Shallow maximum attenuation.** Start at 6–12 dB, not 20–40. Raise the floor further
   where the protection score is high.
4. **Asymmetric smoothing of log gain** — slow to attenuate, fast to release, instant
   release on onset; 1–3 bins of frequency smoothing to begin with.
5. Psychoacoustic masking: **not in v1**. Breath and air are broadband and quiet, so an
   ordinary masking model concludes they are inaudible and therefore removable, which is
   precisely the wrong answer.

## STFT parameters

Hold the window in **milliseconds**, not samples, across sample rates.

| | 48 kHz | 96 kHz |
| --- | --- | --- |
| FFT length | 1024 | 2048 |
| Window | 21.33 ms | 21.33 ms |
| Hop | 256 (5.33 ms) | 512 (5.33 ms) |
| Overlap | 75 % | 75 % |
| Bin spacing | 46.875 Hz | 46.875 Hz |

sqrt-Hann analysis and synthesis, hop N/4, and **normalise by the measured window-product
sum** rather than trusting COLA to hold through floating point and the file's edges.

Shortening the window to chase consonant onsets is a trap: it costs the frequency
resolution the noise estimate depends on and spreads the gain across bands. Keep ~21 ms and
buy time resolution from the short hop, onset detection and asymmetric release instead.

Before any denoising is judged, the pipeline must pass **unity-gain reconstruction**: with
G = 1 the output differs from the input only by numerical error — including impulses, DC,
near-Nyquist, different sine per channel, file start and end, odd lengths, and both rates.

## Evaluation

**PESQ, STOI and SI-SDR are not the primary metrics.** PESQ targets telephony and its
wideband form evaluates roughly 50–7000 Hz, which excludes the air above 8 kHz that this
material is about; ITU itself cautions about applying it to noise-suppressed signals. STOI
predicts intelligibility, not whether breath survived. SI-SDR is a useful regression tripwire
but two files with the same SI-SDR can differ completely in whether the image moved.

The method to use is the one already proposed in this project: **keep the gain, apply it
separately to the clean signal and to the noise.**

```
noise reduction        NR = 10·log10( Σ|N|²  / Σ|G·N|² )
target attenuation     SA = 10·log10( Σ|S|²  / Σ|G·S|² )
mask-induced distortion SD = 10·log10( Σ|S|² / Σ|(G−1)·S|² )
```

The survey confirms this is sound for multiplicative STFT masks and notes it has **no single
standard name** — "component-wise mask decomposition" is the least ambiguous label. It does
not transfer to generative models, which cannot be reduced to one gain curve.

Report `SA` **per labelled segment** — `/s/`, `/ʃ/`, `/f/`, `/h/`, inhale, exhale, unvoiced
whisper, weak voiced whisper, normal voice, true silence — and per octave band, plus the
20–100 ms immediately after onset. And measure the spatial side: ΔILD, wrapped IPD error,
and magnitude-squared coherence, for target and residual noise separately.

**There is no standard breath-preservation metric.** We define ours, in the same spirit as
the elevation work: state the number before claiming the result.

A first pass/fail worth writing down: **noise down 10 dB while unvoiced fricatives lose no
more than about 1.5 dB.** That threshold is ours, not the literature's.

## What the products actually tell us

Published product documentation is thinner than it looks. RX's Voice De-noise is described
as a psychoacoustically-spaced 64-band filter/gate with separate Adaptive and Learn modes,
Gentle/Surgical characters, and a Reduction ceiling; Learn wants at least a second of pure
noise. CEDAR's DNS continuously updates its background estimate even while the target is
present. Waves NS1 publishes a single fader and no algorithm.

The useful lesson is a design one — **separate learned from adaptive, and let the user cap
the maximum reduction** — and one honest negative: no vendor documents a mechanism for
recognising and preserving artistic breath. Marketing copy is not evidence that one exists.

## Machine learning: not in v1

RNNoise (BSD-3-Clause) is a small RNN predicting critical-band gains plus a pitch filter;
DeepFilterNet (MIT/Apache-2.0) adds ERB bands and deep filtering. Both are strong on ordinary
speech. Three reasons to defer:

- The pitch component that helps them is weakest exactly where our material lives.
- Published training recipes are built on ordinary phonation; if breath sits on the noise
  side of the training mixtures, the model learns to remove it — correctly, by its own
  objective.
- **No study was found comparing classical OM-LSA against learned models on whispered,
  intentionally-breathy, binaural material.** The benchmark advantage does not transfer by
  assumption.

If a model is trained later, it should predict **one shared band gain applied to both ears**,
never per-channel waveforms — otherwise it can invent interaural differences that were never
recorded.

Dataset licensing is the practical blocker, not compute: EARS is CC BY-NC 4.0 (no commercial
training), MUSAN is CC BY 4.0 but 16 kHz, and the DNS Challenge repository's MIT licence
covers its code, not the audio it aggregates. Our own KU 100 recordings are the cleanest
path, given the right agreements with the performers.

## Licensing

This project is **AGPLv3-or-later**.

| Source | Licence | Can we incorporate? |
| --- | --- | --- |
| RNNoise | BSD-3-Clause | Yes, keeping the notice |
| SpeexDSP | BSD-3-Clause | Yes |
| WebRTC NS/APM | BSD-style + PATENTS | Yes; audit the snapshot's third-party files |
| DeepFilterNet | MIT **or** Apache-2.0 | Yes; pick MIT and record the choice |
| GPLv3 code | GPLv3 | Yes — AGPLv3 §13 permits the combination |
| GPLv2-only code | GPLv2-only | No |

Same position as [reference-iem.md](reference-iem.md): incorporation is often legally
available, and we still prefer independent implementation to keep the DSP core's licensing
future open. Anything actually incorporated lands in
[third-party-licenses.md](third-party-licenses.md) in the same commit.

Copyright protects expression, not the algorithm or the mathematics — reading papers and
reimplementing is the intended path. Do not transcribe function structure, comments, lookup
tables or constants without carrying the notice, and check model weights and training data
licences **separately** from the code licence.

## Roadmap

| | Contents |
| --- | --- |
| **v0** — built | STFT/iSTFT, fixed profile, common gain, ILD preservation and unity-gain reconstruction tests |
| **v1** — built | Decision-directed prior SNR with a Wiener gain, 6–12 dB ceiling, asymmetric log-gain smoothing, `G = max(G_L, G_R)`, fixed profile only, plus the app: waveform, zoom, selection audition, Original/Clean/Removed, spectrogram, noise floor, monitoring gain. **OM-LSA is not in yet** — the gain rule is still Wiener over the decision-directed estimate, which is the step below it |
| **v2** | Protection: LTLEV, modulation depth, positive spectral flux, long-term spectral divergence, onset-triggered DD reset, protection-linked gain floor |
| **v3** | Restricted adaptation, gated as described above, blind-compared against v2 |
| **v4** | Compare RNNoise / DeepFilterNet / a common-gain model on the same corpus at the same reduction ceiling |

**Examining the removed signal is the test** — if voice, breath or consonants are in it, the
setting is wrong, and no amount of A/B on the cleaned file reveals that as reliably. The GUI
was built earlier than this plan plans for it, because judging real material by ear needed
the transport and the displays rather than two files on disk.

## Judging the result

Three things about this class of processor make it hard to evaluate, and all three cost
time to rediscover.

**The removed signal is inherently far below the programme.** It is a noise floor minus a
few dB of it, so it sits tens of dB under anything else in the file. Monitoring it at unity
is not a test — nothing is audible either way, and the tool looks broken while working
correctly. **Monitoring gain is a requirement of the design, not a convenience.** It must
be playback-only so it can be pushed hard without touching what gets exported.

**Broadband RMS is close to useless as a measure of a spectral suppressor.** The gain can
sit on the reduction ceiling across the great majority of bins while total RMS barely
moves, because the energy of a quiet passage is dominated by the handful of loud transient
bins the estimator classifies as signal and passes through. A summary level figure will
therefore understate the process, sometimes by an order of magnitude. Report and judge
per-bin behaviour, not total energy.

**A spectrogram of the removed signal settles in one glance what listening struggles with.**
Vertical strokes are consonants being eaten; horizontal bands are voice; flat, structureless
haze is the correct result. On a clean recording the ear cannot reliably tell those apart at
any monitoring level, and the picture can.

## When the tool earns its place

Removing noise is never free — it buys quiet with artefact risk. That trade is only worth
making when the noise is, or will become, audible:

- **After gain.** A whisper lifted by compression or normalisation lifts the floor with it.
  The honest test is to raise the monitoring gain by the amount the mix will apply and
  compare the original against the cleaned version at that level.
- **High-gain close-mic material.** A quiet room is not a treated room, and a whisper at the
  ear runs the preamp hard.

On a recording whose floor is already inaudible under the intended playback gain, the right
answer is to leave it alone. The tool should make that conclusion easy to reach — which is
what the spectrogram and the noise-floor curve are for.

## Open questions

1. **Breath versus hiss** cannot be fully separated from a single recording when they share
   statistics and direction. Protection is probabilistic.
2. **Whether OM-LSA is actually the best base for whisper** is untested; MMSE-LSA should be
   compared under the same harness rather than assumed worse.
3. **Time constants for the protection features** vary with speaker, distance, microphone
   and performance. Published values are starting points.
4. **How much ILD/IPD error is perceptually acceptable** has no single published threshold
   for this use.
5. **What existing products do about breath** is not public.
