#pragma once
// Time-domain FIR with coefficient crossfading (Architecture Contract #8).
// Two coefficient sets share one input history; on update the outputs are
// crossfaded over ~10 ms so moving sources never click. Zero latency. JUCE-free.
#include <vector>
#include <cmath>
#include <cstring>

namespace nsb
{
class CrossfadeFir
{
public:
    void prepare (int maxTaps_, int maxBlock_, int fadeSamples_)
    {
        maxTaps = maxTaps_;
        fadeLen = fadeSamples_ < 8 ? 8 : fadeSamples_;
        coeffA.assign ((size_t) maxTaps, 0.0f);
        coeffB.assign ((size_t) maxTaps, 0.0f);
        hist.assign ((size_t) (maxTaps + maxBlock_), 0.0f);
        coeffA[0] = 1.0f;
        activeTaps = maxTaps;
        fadePos = fadeLen; // not fading
    }

    void reset()
    {
        std::fill (hist.begin(), hist.end(), 0.0f);
        fadePos = fadeLen;
    }

    void setNumTaps (int n) noexcept
    {
        if (n < 8) n = 8;
        if (n > maxTaps) n = maxTaps;
        activeTaps = n;
    }

    // Stage new coefficients; starts (or restarts) a crossfade. Called at block rate.
    void setCoefficients (const float* c) noexcept
    {
        if (fadePos < fadeLen)
        {
            // fade in progress: fold current mix into A so a new fade can start cleanly
            const float w = (float) fadePos / (float) fadeLen;
            for (int i = 0; i < activeTaps; ++i)
                coeffA[(size_t) i] = coeffA[(size_t) i] * (1.0f - w) + coeffB[(size_t) i] * w;
        }
        else
        {
            std::memcpy (coeffA.data(), coeffB.data(), sizeof (float) * (size_t) activeTaps);
        }
        std::memcpy (coeffB.data(), c, sizeof (float) * (size_t) activeTaps);
        fadePos = 0;
    }

    // First call after prepare/reset: set both banks without fading.
    void setCoefficientsImmediate (const float* c) noexcept
    {
        std::memcpy (coeffA.data(), c, sizeof (float) * (size_t) activeTaps);
        std::memcpy (coeffB.data(), c, sizeof (float) * (size_t) activeTaps);
        fadePos = fadeLen;
    }

    // in/out may not alias. Accumulates nothing — plain replace.
    void process (const float* in, float* out, int n) noexcept
    {
        const int T = activeTaps;
        // build contiguous [history | new block]
        float* h = hist.data();
        std::memcpy (h + T, in, sizeof (float) * (size_t) n);

        const float* ca = coeffA.data();
        const float* cb = coeffB.data();

        for (int i = 0; i < n; ++i)
        {
            const float* x = h + T + i; // x[0]=current, x[-k]=k samples ago
            float accA = 0.0f;
            for (int k = 0; k < T; ++k)
                accA += ca[k] * x[-k];

            if (fadePos < fadeLen)
            {
                float accB = 0.0f;
                for (int k = 0; k < T; ++k)
                    accB += cb[k] * x[-k];
                const float w = (float) ++fadePos / (float) fadeLen;
                out[i] = accA * (1.0f - w) + accB * w;
                if (fadePos >= fadeLen)
                    std::memcpy (coeffA.data(), coeffB.data(), sizeof (float) * (size_t) T);
            }
            else
            {
                out[i] = accA;
            }
        }

        // keep last T samples as history for next block
        std::memmove (h, h + n, sizeof (float) * (size_t) T);
    }

private:
    std::vector<float> coeffA, coeffB, hist;
    int maxTaps = 128, activeTaps = 128;
    int fadeLen = 480, fadePos = 480;
};
} // namespace nsb
