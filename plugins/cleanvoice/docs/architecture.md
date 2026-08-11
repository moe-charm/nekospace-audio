# CleanVoice Architecture

CleanVoice is an offline fixed-profile denoiser with two front ends over the same
JUCE-independent DSP: a command-line renderer and a JUCE standalone auditioning app.

## Signal path

```text
noise-only selection -> STFT -> trimmed-mean noise profile
                                          |
whole input ----------> STFT -> Wiener gain + smoothing -> inverse STFT -> Clean
             |                                                   |
             +---------------------------------------------------+-> Removed = Input - Clean
```

`Clean + Removed` reconstructs the input. Stereo and binaural inputs receive one shared
real gain per time-frequency bin, preserving their interaural level and phase differences.

## Ownership and threads

| Thread | Owns |
| --- | --- |
| Message thread | controls, waveform/spectrogram views, current file and render handles |
| Audio callback | one immutable playback snapshot acquired at block start |
| Worker | its own input snapshot, parameters and working noise profile |

Audio sample vectors are never mutated after publication. Replacing the current file or a
completed render swaps a ref-counted snapshot; a callback already using the old snapshot
keeps it alive until that block ends.

Each worker has a monotonically increasing job ID and a safe component reference. A queued
completion is accepted only when both the component and that job ID are still current.
Closing the app, cancelling, or opening a newer job cannot publish stale results.

Noise-profile learning is cancellable and happens into the worker's private profile. A
cancelled or failed learn never damages the last valid profile. Learn selections are capped
at 30 seconds; 1–3 seconds remains the intended range.

Worker progress is atomic. The message-thread timer copies it into the `double` consumed by
JUCE's `ProgressBar`, avoiding a C++ data race.

## Monitoring

Original, Clean and Removed share one playhead. Monitoring gain is playback-only and never
changes exported samples. If the audio device cannot run at the file's sample rate, preview
playback uses interpolated resampling and the mismatch remains visible in the status text.

The spectrogram combines channels as mean spectral power. It deliberately does not sum L/R
in the time domain: binaural phase differences could cancel and hide residue present in one
ear.

## Private material

Real recordings are inputs, not repository assets. `*.wav` is ignored at the repository
root and no production recording may be force-added. Any future generated test fixture
requires one exact `.gitignore` exception and a provenance review.
