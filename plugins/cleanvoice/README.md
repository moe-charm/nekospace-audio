# NekoSpace CleanVoice

Part of **NekoSpace Audio** — https://github.com/moe-charm/nekospace-audio

Noise removal for whispered and breathy voice. **v1 prototype: command line only.**

The design and its sources are in [reference-denoise.md](../../docs/reference-denoise.md);
this file is how to run it.

- **Format:** command-line tool. No GUI, no plugin, no realtime.
- **Stack:** C++17, no dependencies — **not even JUCE**. WAV I/O and the FFT are in-tree.
- **Input:** WAV, PCM 16/24/32-bit or 32-bit float, any channel count, any sample rate.
- **Output:** 32-bit float WAV at the input's sample rate.

## Why there is no window

The only question v1 has to answer is whether the hiss can go without thinning the
whisper. A GUI does not help answer it, and building one first is how a prototype stops
being a prototype. Two files come out, and you listen.

## Use

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
- It must be **only noise**. A half-swallowed word or a chair creak in the selection teaches
  the tool that those are noise, and it will remove them everywhere.
- Same take, same gain. A region from another file describes another noise floor.

## Build

Built by the top-level configure along with everything else:

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release
```

Binary at `build/plugins/cleanvoice/Release/cleanvoice.exe`.

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

Adaptive noise tracking, machine learning, automatic silence detection, a GUI, VST3,
de-click, de-reverb, batch processing. Each has a reason recorded in
[reference-denoise.md](../../docs/reference-denoise.md); the short version is that adaptive
tracking absorbs sustained breath into the noise floor, and everything else is scope.

## License

AGPLv3-or-later, like the rest of the repository. No third-party code is vendored here.
