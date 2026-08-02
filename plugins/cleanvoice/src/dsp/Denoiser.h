// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// Fixed-profile spectral suppression. JUCE-free (Architecture Contract #4).
//
// The gain rule is a Wiener gain over a decision-directed a-priori SNR estimate. The
// decision-directed part is what stops isolated spectral peaks appearing and disappearing
// frame to frame, which is what musical noise is.
//
// Two things here exist because of the material rather than the algorithm:
//
// * ONE REAL GAIN IS APPLIED TO BOTH CHANNELS. With independent gains,
//   Z_L/Z_R = (G_L/G_R)(Y_L/Y_R), so any difference between the two gains IS a change in
//   interaural level difference. With a single real gain the per-bin ILD and IPD are
//   preserved exactly. On a dummy-head recording that is the difference between cleaning
//   the noise and dissolving the room. The two candidate gains are combined with max(),
//   not min(): a whisper close to one ear is buried in the other, and min() would let the
//   far ear's low SNR erase the near ear's evidence.
//
// * ATTENUATION IS CAPPED AND THE CAP IS SHALLOW BY DEFAULT. The product is not "remove
//   the most noise", it is "remove only what is provably noise". 20-40 dB of reduction on
//   whispered material removes the performance along with the hiss.
//
// See docs/reference-denoise.md for the reasoning and the sources.
#include <vector>
#include <cmath>
#include <algorithm>
#include <functional>
#include "Stft.h"
#include "NoiseProfile.h"

namespace cv
{
struct DenoiseParams
{
    float reductionDb = 10.0f;   // maximum attenuation, dB. Shallow on purpose.
    float smoothing   = 0.5f;    // 0..1, musical-noise control
    float preserve    = 0.0f;    // 0..1, onset protection. Default OFF: hear the raw
                                 // behaviour first, then hear what protection changes.
    float overSubtract = 1.0f;   // noise PSD scaling; >1 removes more and damages more
};

class Denoiser
{
public:
    // channels: de-interleaved input. Returns the cleaned signal, same shape.
    // "removed" is computed by the caller as input - clean, so that clean + removed is
    // exactly the original and nothing can hide in the difference.
    // onProgress is called every few frames with 0..1 and returns false to abort, which
    // is how the GUI keeps its thread responsive and offers a Cancel button. Returning an
    // empty result means cancelled. Still no JUCE here - std::function is enough.
    static std::vector<std::vector<float>> process (
        const Stft& stft, const NoiseProfile& profile,
        const std::vector<std::vector<float>>& channels, int numSamples,
        const DenoiseParams& p,
        const std::function<bool (float)>& onProgress = {})
    {
        const int nCh = (int) channels.size();
        const int bins = stft.numBins();
        const int frames = stft.frameCount (numSamples);
        const int padded = numSamples + 2 * stft.fftSize();

        const float gMin = std::pow (10.0f, -std::fabs (p.reductionDb) / 20.0f);

        // decision-directed memory, and the previous frame's magnitude for the flux
        std::vector<std::vector<float>> prevGain (
            (size_t) nCh, std::vector<float> ((size_t) bins, 1.0f));
        std::vector<std::vector<float>> prevPost (
            (size_t) nCh, std::vector<float> ((size_t) bins, 1.0f));
        std::vector<std::vector<float>> prevMag (
            (size_t) nCh, std::vector<float> ((size_t) bins, 0.0f));
        std::vector<float> smoothedGain ((size_t) bins, 1.0f);

        std::vector<std::vector<float>> out (
            (size_t) nCh, std::vector<float> ((size_t) padded, 0.0f));
        std::vector<float> wsum ((size_t) padded, 0.0f);

        std::vector<std::vector<Complex>> spec ((size_t) nCh);
        std::vector<float> gain ((size_t) bins), work ((size_t) bins);

        for (int f = 0; f < frames; ++f)
        {
            if (onProgress && (f & 63) == 0
                && ! onProgress ((float) f / (float) std::max (frames, 1)))
                return {};                       // cancelled

            for (int c = 0; c < nCh; ++c)
                stft.analyse (channels[(size_t) c].data(), numSamples, f, spec[(size_t) c]);

            // --- positive spectral flux, summed over channels: the onset of a fricative
            // or an inhale shows up here before the decision-directed estimate catches up.
            float flux = 0.0f, energy = 1.0e-12f;
            for (int c = 0; c < nCh; ++c)
                for (int k = 0; k < bins; ++k)
                {
                    const float m = std::abs (spec[(size_t) c][(size_t) k]);
                    flux += std::max (m - prevMag[(size_t) c][(size_t) k], 0.0f);
                    energy += m;
                }
            const float onset = clamp01 (flux / energy * 6.0f);

            // Protection raises the floor and shortens the decision-directed memory, so a
            // consonant is not first flattened and then slowly released.
            const float protect = p.preserve * onset;
            const float floorHere = gMin + (1.0f - gMin) * protect;
            const float alpha = 0.98f - 0.5f * protect;

            // --- per-channel candidate gains, then one common gain
            for (int k = 0; k < bins; ++k) gain[(size_t) k] = 0.0f;

            for (int c = 0; c < nCh; ++c)
            {
                const int pc = std::min (c, profile.channels() - 1);
                for (int k = 0; k < bins; ++k)
                {
                    const float re = spec[(size_t) c][(size_t) k].real();
                    const float im = spec[(size_t) c][(size_t) k].imag();
                    const float power = re * re + im * im;
                    const float lambda = std::max (profile.power (pc, k) * p.overSubtract,
                                                   1.0e-12f);

                    const float post = power / lambda;                   // a posteriori SNR
                    const float g0 = prevGain[(size_t) c][(size_t) k];
                    const float prior = std::max (
                        alpha * g0 * g0 * prevPost[(size_t) c][(size_t) k]
                          + (1.0f - alpha) * std::max (post - 1.0f, 0.0f),
                        1.0e-6f);

                    const float g = prior / (1.0f + prior);              // Wiener
                    prevGain[(size_t) c][(size_t) k] = g;
                    prevPost[(size_t) c][(size_t) k] = post;
                    prevMag[(size_t) c][(size_t) k] = std::sqrt (power);

                    // max(): protect the ear that can actually hear the source
                    gain[(size_t) k] = std::max (gain[(size_t) k], g);
                }
            }

            for (int k = 0; k < bins; ++k)
                gain[(size_t) k] = std::max (gain[(size_t) k], floorHere);

            // --- smoothing, on log gain: the ear hears ratios, so smoothing dB rather
            // than power keeps the audible change even across the spectrum.
            if (p.smoothing > 0.0f)
            {
                for (int k = 0; k < bins; ++k)
                {
                    const int a = std::max (0, k - 1), b = std::min (bins - 1, k + 1);
                    const float lg = (std::log (gain[(size_t) a])
                                    + std::log (gain[(size_t) k])
                                    + std::log (gain[(size_t) b])) / 3.0f;
                    work[(size_t) k] = std::exp (lg);
                }
                for (int k = 0; k < bins; ++k)
                    gain[(size_t) k] += p.smoothing * (work[(size_t) k] - gain[(size_t) k]);

                // Asymmetric in time: slow to attenuate further, immediate to release.
                // Releasing fast is what keeps the front of a consonant intact.
                const float att = 0.35f * p.smoothing;
                for (int k = 0; k < bins; ++k)
                {
                    float& s = smoothedGain[(size_t) k];
                    s = gain[(size_t) k] < s ? s + att * (gain[(size_t) k] - s)
                                             : gain[(size_t) k];
                    gain[(size_t) k] = s;
                }
            }

            for (int c = 0; c < nCh; ++c)
            {
                for (int k = 0; k < bins; ++k)
                    spec[(size_t) c][(size_t) k] *= gain[(size_t) k];
                stft.overlapAdd (spec[(size_t) c], f, out[(size_t) c], wsum);
            }
        }

        // wsum was accumulated once per channel per frame; it is identical for every
        // channel, so divide each channel by the per-channel share.
        for (auto& v : wsum) v /= (float) std::max (nCh, 1);
        for (int c = 0; c < nCh; ++c) Stft::normalise (out[(size_t) c], wsum);

        // trim the analysis padding back off
        std::vector<std::vector<float>> clean (
            (size_t) nCh, std::vector<float> ((size_t) numSamples, 0.0f));
        const int pad = stft.fftSize();
        for (int c = 0; c < nCh; ++c)
            for (int i = 0; i < numSamples; ++i)
                clean[(size_t) c][(size_t) i] = out[(size_t) c][(size_t) (i + pad)];
        return clean;
    }

private:
    static float clamp01 (float v) noexcept { return v < 0 ? 0 : (v > 1 ? 1 : v); }
};
} // namespace cv
