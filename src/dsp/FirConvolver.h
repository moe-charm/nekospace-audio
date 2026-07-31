#pragma once
// Time-domain FIR with coefficient crossfading (Architecture Contract #8).
// Two coefficient banks (each with its own tap count) share one input history; on any
// update — including a tap-count / quality change — the two outputs are crossfaded over
// ~10 ms so nothing ever clicks. Zero added latency. JUCE-free.
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
        tapsA = tapsB = maxTaps;
        fadePos = fadeLen; // not fading
    }

    void reset()
    {
        std::fill (hist.begin(), hist.end(), 0.0f);
        fadePos = fadeLen;
    }

    // Stage new coefficients (taps may differ from the current bank — quality switches
    // ride the same crossfade). Called at block rate.
    void setCoefficients (const float* c, int taps) noexcept
    {
        taps = taps < 8 ? 8 : (taps > maxTaps ? maxTaps : taps);
        if (fadePos < fadeLen)
        {
            // fade in progress: fold the current mix into A so a new fade starts cleanly
            const float w = (float) fadePos / (float) fadeLen;
            const int span = tapsA > tapsB ? tapsA : tapsB; // both banks zero-padded
            for (int i = 0; i < span; ++i)
                coeffA[(size_t) i] = coeffA[(size_t) i] * (1.0f - w) + coeffB[(size_t) i] * w;
            tapsA = span;
        }
        else
        {
            std::memcpy (coeffA.data(), coeffB.data(), sizeof (float) * (size_t) maxTaps);
            tapsA = tapsB;
        }
        loadBank (coeffB, c, taps);
        tapsB = taps;
        fadePos = 0;
    }

    // First call after prepare/reset: set both banks without fading.
    void setCoefficientsImmediate (const float* c, int taps) noexcept
    {
        taps = taps < 8 ? 8 : (taps > maxTaps ? maxTaps : taps);
        loadBank (coeffA, c, taps);
        loadBank (coeffB, c, taps);
        tapsA = tapsB = taps;
        fadePos = fadeLen;
    }

    // in/out may alias (input is copied into history before output is written)
    void process (const float* in, float* out, int n) noexcept
    {
        const int M = maxTaps;
        float* h = hist.data();
        std::memcpy (h + M, in, sizeof (float) * (size_t) n);

        const float* ca = coeffA.data();
        const float* cb = coeffB.data();

        for (int i = 0; i < n; ++i)
        {
            const float* x = h + M + i; // x[0]=current, x[-k]=k samples ago

            if (fadePos < fadeLen)
            {
                float accA = 0.0f;
                for (int k = 0; k < tapsA; ++k)
                    accA += ca[k] * x[-k];
                float accB = 0.0f;
                for (int k = 0; k < tapsB; ++k)
                    accB += cb[k] * x[-k];
                const float w = (float) ++fadePos / (float) fadeLen;
                out[i] = accA * (1.0f - w) + accB * w;
                if (fadePos >= fadeLen)
                {
                    std::memcpy (coeffA.data(), coeffB.data(), sizeof (float) * (size_t) maxTaps);
                    tapsA = tapsB;
                }
            }
            else
            {
                float acc = 0.0f;
                for (int k = 0; k < tapsB; ++k)
                    acc += cb[k] * x[-k];
                out[i] = acc;
            }
        }

        std::memmove (h, h + n, sizeof (float) * (size_t) M);
    }

private:
    static void loadBank (std::vector<float>& bank, const float* c, int taps) noexcept
    {
        std::memcpy (bank.data(), c, sizeof (float) * (size_t) taps);
        if ((int) bank.size() > taps)
            std::memset (bank.data() + taps, 0, sizeof (float) * (bank.size() - (size_t) taps));
    }

    std::vector<float> coeffA, coeffB, hist;
    int maxTaps = 128, tapsA = 128, tapsB = 128;
    int fadeLen = 480, fadePos = 480;
};
} // namespace nsb
