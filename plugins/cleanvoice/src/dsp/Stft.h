// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// Weighted overlap-add STFT. JUCE-free (Architecture Contract #4).
//
// Two decisions worth stating, both from docs/reference-denoise.md:
//
// 1. The window is held at ~21 ms in MILLISECONDS, not in samples, so 96 kHz gets the same
//    time and frequency resolution as 48 kHz rather than half the window.
// 2. Reconstruction divides by the window-product sum that was actually accumulated,
//    rather than trusting the COLA identity. sqrt-Hann at 75 % overlap does satisfy COLA,
//    but the file's first and last frames do not, and measuring costs one buffer.
#include <vector>
#include <cmath>
#include "Fft.h"

namespace cv
{
// ~21.33 ms rounded up to a power of two: 1024 at 48 kHz, 2048 at 96 kHz.
inline int fftSizeForRate (double sampleRate) noexcept
{
    const double want = 0.02133 * sampleRate;
    int n = 256;
    while ((double) n < want) n <<= 1;
    return n;
}

class Stft
{
public:
    Stft (int fftSize, int hopSize)
        : n (fftSize), hop (hopSize), fft (fftSize)
    {
        win.resize ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            // periodic Hann, then square-rooted so analysis * synthesis == Hann
            const float h = 0.5f * (1.0f - std::cos (2.0f * kPi * (float) i / (float) n));
            win[(size_t) i] = std::sqrt (h);
        }
    }

    int fftSize() const noexcept { return n; }
    int hopSize() const noexcept { return hop; }
    int numBins() const noexcept { return n / 2 + 1; }

    // Frames start at -n so the first real sample is fully covered by overlapping frames.
    int frameCount (int numSamples) const noexcept
    {
        return (numSamples + 2 * n) / hop + 1;
    }
    int frameStart (int frameIndex) const noexcept { return frameIndex * hop - n; }

    // Reads one frame out of x (zero outside), windows it and transforms it.
    void analyse (const float* x, int numSamples, int frameIndex,
                  std::vector<Complex>& spec) const
    {
        spec.assign ((size_t) n, Complex {});
        const int start = frameStart (frameIndex);
        for (int i = 0; i < n; ++i)
        {
            const int s = start + i;
            const float v = (s >= 0 && s < numSamples) ? x[s] : 0.0f;
            spec[(size_t) i] = Complex (v * win[(size_t) i], 0.0f);
        }
        fft.forward (spec);
    }

    // Inverse-transforms, windows again, and accumulates into out / wsum.
    //
    // The output buffer is written in PADDED coordinates: frame f lands at f*hop, so
    // out[j] holds input sample j - fftSize. Analysis runs in input coordinates and starts
    // at -fftSize, which is the same thing shifted; keeping the two apart is the whole
    // reason this is spelled out rather than left implicit. Getting it wrong reconstructs
    // a plausible-sounding but completely misaligned signal.
    void overlapAdd (std::vector<Complex>& spec, int frameIndex,
                     std::vector<float>& out, std::vector<float>& wsum) const
    {
        // rebuild the negative-frequency half from the positive one before inverting
        for (int k = 1; k < n / 2; ++k)
            spec[(size_t) (n - k)] = std::conj (spec[(size_t) k]);
        spec[0] = Complex (spec[0].real(), 0.0f);
        spec[(size_t) (n / 2)] = Complex (spec[(size_t) (n / 2)].real(), 0.0f);

        fft.inverse (spec);

        const int start = frameIndex * hop;          // padded coordinates
        for (int i = 0; i < n; ++i)
        {
            const int s = start + i;
            if (s < 0 || s >= (int) out.size()) continue;
            const float w = win[(size_t) i];
            out[(size_t) s]  += spec[(size_t) i].real() * w;
            wsum[(size_t) s] += w * w;
        }
    }

    // Divides out the accumulated window energy. Anything the frames never reached keeps
    // a zero denominator, so it is left silent rather than amplified by 1/eps.
    static void normalise (std::vector<float>& out, const std::vector<float>& wsum)
    {
        for (size_t i = 0; i < out.size(); ++i)
            out[i] = wsum[i] > 1.0e-6f ? out[i] / wsum[i] : 0.0f;
    }

private:
    int n, hop;
    Fft fft;
    std::vector<float> win;
};
} // namespace cv
