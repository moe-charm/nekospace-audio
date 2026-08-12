# NekoSpace CleanVoice

Part of **NekoSpace Audio** — https://github.com/moe-charm/nekospace-audio

Noise removal for whispered and breathy voice. **v1 prototype: a small desktop app, plus a command-line tool.**

The design and its sources are in [reference-denoise.md](../../docs/reference-denoise.md);
this file is how to run it.

- **Format:** standalone app (`NekoSpace CleanVoice`) and a CLI (`cleanvoice`). No plugin,
  no realtime, yet.
- **Stack:** C++17. The **DSP links no JUCE at all** — WAV I/O and the FFT are in-tree, and
  JUCE appears only in the app's GUI layer. A VST3 later is a wrapper, not a rewrite.
- **Input:** WAV, PCM 16/24/32-bit or 32-bit float, any channel count, any sample rate.
- **Output:** 32-bit float WAV at the input's sample rate.

## The app

Open a take, drag over a stretch with no voice in it, **alt-drag** 10–30 s to preview,
press **Learn + Preview Range**, then switch between **Original / Clean / Removed Noise**
while it plays.

### Two ranges, and they are not interchangeable

| | |
| --- | --- |
| **Cyan — drag** | the noise range. Learned FROM, never removed |
| **Orange — alt+drag** | the preview range. The only part that gets processed |

Deciding a setting by rendering twenty minutes is absurd when the decision is made on ten
seconds of it. Mark a span with whispers, breaths and `s`/`sh`/`f`/`h` in it, judge there,
and press **Process Whole File** only once it is right. Whole-file rendering is a separate
button and a separate key — never something you reach by pressing the same one twice.

**A preview is what you will actually get.** The span is rendered with the audio that
precedes it fed in first and then discarded, because the decision-directed SNR estimate
starts each run cold and takes a few frames to settle — a span rendered without that
history is softer at the start than the finished file will be. With it, the preview and the
full render agree **sample for sample** inside the range, which is a test, not a hope.
Outside the range the output is the input exactly, so the removed signal is silent there
and the spectrogram shows at a glance what was and was not touched.

The three buttons share one playhead, because the difference between two renders is only
audible if you hear the same moment in each. The waveform follows whichever one you are
monitoring, so speech left in the removed signal is visible as well as audible.

**Zoom** — wheel zooms about the pointer, shift-wheel scrolls, right-drag pans, and *Fit*
and *Zoom to Selection* jump. This is not a convenience: a 22-minute take drawn across
1500 pixels puts nearly a second in every pixel, so marking a one-second region is below
the resolution of the control without it.

**Space loops the marked range** when there is one, the way an audio editor does. This is
how you check the range before learning from it — if you can hear any voice or breath in
the loop, move it, because whatever is in there is about to be treated as noise and removed
from the entire file. Clicking the waveform moves the playhead without disturbing the
selection; double-clicking clears it.

**The learned noise profile outlives the selection.** Once you have processed once, clear
the selection and keep pressing *Process* at different Reduction settings — that is the
loop this tool is used in, and making it re-select every time would tax the most common
action. The button says which it will do.

Drop a `.wav` on the window, or pass one on the command line, to skip the file dialog.

### Keys

| | |
| --- | --- |
| **Space** | play / stop — **loops the selection when there is one** |
| **Shift+Space** | play the whole take, ignoring the selection |
| **1 / 2 / 3** | Original / Clean / Removed Noise |
| **Enter** | process the preview range |
| **Shift+Enter** | process the whole file |
| **alt+drag** | mark the preview range |
| **Esc** | cancel processing |
| **F** | fit the whole file |
| **Z** | zoom to selection |
| **+ / −**, up/down | zoom about the playhead |
| **← / →** | scroll |
| **Home** | playhead to the selection, or to the start |
| **G** | show / hide the spectrogram |
| **N** | show / hide the noise floor |
| **double-click** | clear the selection |
| **Ctrl+O / Ctrl+S** | open / export |

`1` `2` `3` are the ones that matter. Judging a denoiser means hearing the same moment as
Original, then Clean, then Removed, one after another — and hunting for a button with the
mouse between each is long enough to lose what you were comparing. No control takes
keyboard focus, so Space never presses whichever button you last clicked.

**Monitor Gain** boosts playback only — it never touches the exported file. It is not a
convenience: a studio noise floor sits near −100 dBFS per bin, and what gets removed from
it is quieter still, so at unity the Removed bus is inaudible and the tool looks broken
when it is working perfectly. Push it to +30 dB or so to audition Removed.

**Spectrogram** (`G`) draws whatever you are monitoring, over the waveform's view range,
and it is the display that makes this tool judgeable. Hiss is a flat haze; a fan is a
horizontal line; a fricative is a vertical brush stroke near the top; vowels are stacked
horizontal bands. Switch the monitor to **Removed Noise** and the question "am I taking the
performance out?" becomes a picture — broadband haze with nothing in it is right, and
vertical strokes or horizontal formant bands mean consonants and voice are being eaten.

It computes one FFT per pixel column rather than one per hop, so a 22-minute take costs the
same to draw as a two-second one.

**Noise Floor** (`N`) draws the learned profile as level against log frequency. It answers
what listening at unity cannot: whether there is a hiss at all and what shape it is,
whether there is a tonal spike — a fan, a whine, mains hum — rather than broadband noise,
and whether something that is obviously not noise got into the selection.

**Reduction** and **Smoothing** are on the front. **Preserve Breath** and
**Oversubtraction** are behind *Advanced*, and Preserve Breath starts at 0 on purpose: the
first listen has to be the unprotected behaviour, or there is no way to know what the
protection is for.

Long processing runs on its own thread with a progress bar and a Cancel button; the window
stays responsive.

The app treats loaded and rendered audio as immutable snapshots. The audio callback keeps
one snapshot alive for its whole block, so opening another file or publishing a completed
render cannot invalidate memory being monitored. A worker also owns the exact input,
settings and noise profile it started with; cancellation never leaves a half-learned
profile behind. See [architecture.md](docs/architecture.md).

## The command line

```bash
cleanvoice take01.wav --noise 12.5 15.0
```

`--noise <start> <end>` is a stretch of seconds containing **only room noise** — the
performer between lines. That region is the lesson, not the target: **the whole file is
processed**, exactly the way iZotope's Learn mode works.

Writes `take01-clean.wav` and `take01-removed.wav`.

| Option | Default | What it does |
| --- | --- | --- |
| `--reduction <dB>` | 10 | ceiling on attenuation. Start at 6–12, not 20 |
| `--smoothing <0..1>` | 0.5 | musical-noise control |
| `--preserve <0..1>` | **0** | onset protection, off by default — hear the raw damage first |
| `--oversub <x>` | 1.0 | scales the learned noise floor. Above 1 removes more and damages more |

## Listen to the removed file first

`take01-removed.wav` is exactly what was taken away — `clean + removed` reconstructs the
input sample for sample, so nothing can hide in the difference. If you hear speech, breath
or consonants in it, the setting is wrong.

This is the test, and it is more reliable than A/B-ing the cleaned file: denoising almost
always sounds *cleaner* in isolation, and the damage only shows up in context.

## Is it worth running at all?

Denoising is never free — it buys quiet with artefact risk, so it is only worth doing when
the noise is, or will become, audible. If the floor is already inaudible at the gain the
material will actually be played at, the right answer is to leave it alone.

The test: raise **Monitor Gain** by roughly what the mix will add — compression and
normalisation on a whisper can easily be +20 to +30 dB — and compare `1` against `2` at
that level. If the hiss bothers you there, the tool is worth running. If it does not, it is
not, and no setting changes that.

## Reading the level numbers

Broadband RMS is a poor measure of what this does, and the CLI's summary line will
understate it. The gain can sit on the reduction ceiling across the great majority of bins
while total RMS barely moves, because the energy of a quiet passage is dominated by the
handful of loud transient bins the algorithm classifies as signal and passes through — so a
process attenuating most of the spectrum by 10 dB can show up as a change of about 1 dB.
Judge by the spectrogram and the noise floor curve, and by ear with Monitor Gain up, not by
the RMS figure.

## What to check, in order

1. Does the hiss go?
2. Does the whisper stay the same thickness?
3. Is the removed file just noise?
4. Is the chirpy, watery artefact tolerable?
5. **Does the image stay put?** — the whisper should not wander between the ears.

Point 5 is why a **single real gain is applied to both channels**. Independent per-channel
gains change the interaural level difference by construction; one shared gain preserves
per-bin ILD and IPD exactly. On dummy-head material that is the difference between removing
the noise and dissolving the room.

## Choosing the noise region

- **At least ~0.5 s**, ideally 1–3 s. Shorter and the tool refuses.
- **30 s maximum.** A longer range does not improve a stationary-noise estimate enough to
  justify its memory and sorting cost; select a representative 1–3 s instead.
- It must be **only noise**. A half-swallowed word or a chair creak in the selection teaches
  the tool that those are noise, and it will remove them everywhere. **Loop it with Play
  Selection and listen** — that is the check, and it takes five seconds.
- Zoom in before marking. At full zoom on a long take you cannot see what you are selecting.
- Same take, same gain. A region from another file describes another noise floor.

## Paths and file size

Paths with Japanese (or any non-ASCII) characters work. This is not free on Windows —
`fopen` reads a narrow path in the process code page, so a UTF-8 path with Japanese in it
simply fails to open — so the file layer converts to UTF-16 and the CLI takes its arguments
as UTF-16 too. There is a test for it, because every recording here lives in a folder named
after the character in the script.

Long takes work but are heavy: a 22-minute stereo 24-bit file is ~380 MB on disk and needs
roughly 600 MB loaded, then about the same again for each of clean and removed. For judging
settings, a one- or two-minute excerpt turns around far faster.

Recordings are private production material by default. The repository-wide `.gitignore`
excludes every `*.wav`; no recording should be force-added. A future generated test fixture
needs a narrow, reviewed exception rather than removing that rule.

## Build

Built by the top-level configure along with everything else:

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release
```

Binaries:

- `build/plugins/cleanvoice/CleanVoiceApp_artefacts/Release/NekoSpace CleanVoice.exe`
- `build/plugins/cleanvoice/Release/cleanvoice.exe`

## Tests

`cv_tests` covers the foundations before the denoising:

- FFT round trip.
- **Unity-gain reconstruction** at 44.1/48/96 kHz, including a click on the first and last
  sample and a length that is not a whole number of frames. Nothing may be judged by ear
  until this passes — it caught a frame-offset bug that reconstructed a plausible-sounding
  but completely misaligned signal.
- Noise falls, a signal well above the floor does not.
- The reduction ceiling is never exceeded.
- **Interaural level difference survives processing.**
- `clean + removed` reconstructs the input.
- Output stays finite at extreme settings, including a digitally silent channel.
- WAV round trip.

## Not in v1

Adaptive noise tracking, machine learning, automatic silence detection, spectral editing,
VST3, de-click, de-reverb, batch processing. Each has a reason recorded in
[reference-denoise.md](../../docs/reference-denoise.md); the short version is that adaptive
tracking absorbs sustained breath into the noise floor, and everything else is scope.

## License

AGPLv3-or-later, like the rest of the repository. No third-party code is vendored here.
