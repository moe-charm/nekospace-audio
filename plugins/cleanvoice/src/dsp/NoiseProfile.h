// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// Fixed noise profile, learned from a region the user says contains only noise.
// JUCE-free (Architecture Contract #4).
//
// Fixed rather than adaptive, deliberately. An adaptive estimator decides for itself what
// is noise, and a long sustained breath looks exactly like a noise floor that has risen -
// it sounds right for the first few seconds and then erodes the breath. Here a human picks
// the region and can listen to it first. See docs/reference-denoise.md.
//
// Per channel, because the two ears of a dummy head do not hear the same room.
#include <vector>
#include <algorithm>
#include <cmath>
#include "Stft.h"

namespace cv
{
class NoiseProfile
{
public:
    // Learns per-bin noise power from the frames wholly inside [startSample, endSample).
    // Uses a trimmed mean of per-frame power: a stray click or a half-swallowed word in
    // the selection shifts a plain mean upwards, and an inflated noise floor is exactly
    // what removes breath.
    bool learn (const Stft& stft, const std::vector<std::vector<float>>& channels,
                int numSamples, int startSample, int endSample)
    {
        const int nCh = (int) channels.size();
        const int bins = stft.numBins();
        psd.assign ((size_t) nCh, std::vector<float> ((size_t) bins, 0.0f));
        framesUsed = 0;
        if (nCh == 0 || endSample <= startSample) return false;

        // collect the power of every frame that lies entirely within the selection
        std::vector<std::vector<std::vector<float>>> obs (
            (size_t) nCh, std::vector<std::vector<float>> ((size_t) bins));

        std::vector<Complex> spec;
        const int total = stft.frameCount (numSamples);
        for (int f = 0; f < total; ++f)
        {
            const int s0 = stft.frameStart (f);
            const int s1 = s0 + stft.fftSize();
            if (s0 < startSample || s1 > endSample) continue;
            ++framesUsed;
            for (int c = 0; c < nCh; ++c)
            {
                stft.analyse (channels[(size_t) c].data(), numSamples, f, spec);
                for (int k = 0; k < bins; ++k)
                {
                    const float re = spec[(size_t) k].real(), im = spec[(size_t) k].imag();
                    obs[(size_t) c][(size_t) k].push_back (re * re + im * im);
                }
            }
        }
        if (framesUsed < 4) return false;   // too short to be a profile

        for (int c = 0; c < nCh; ++c)
            for (int k = 0; k < bins; ++k)
                psd[(size_t) c][(size_t) k] = trimmedMean (obs[(size_t) c][(size_t) k]);
        return true;
    }

    int frames() const noexcept { return framesUsed; }
    int channels() const noexcept { return (int) psd.size(); }
    float power (int ch, int bin) const noexcept { return psd[(size_t) ch][(size_t) bin]; }

private:
    // Drops the top and bottom 20 % before averaging.
    static float trimmedMean (std::vector<float>& v)
    {
        if (v.empty()) return 1.0e-12f;
        std::sort (v.begin(), v.end());
        const size_t lo = v.size() / 5;
        const size_t hi = v.size() - lo;
        double acc = 0; size_t cnt = 0;
        for (size_t i = lo; i < hi; ++i) { acc += v[i]; ++cnt; }
        if (cnt == 0) { acc = v[v.size() / 2]; cnt = 1; }
        return std::max ((float) (acc / (double) cnt), 1.0e-12f);
    }

    std::vector<std::vector<float>> psd;
    int framesUsed = 0;
};
} // namespace cv
