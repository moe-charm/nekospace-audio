// SPDX-FileCopyrightText: 2026 TextureVoice
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// Fractional delay line, 4-point Hermite interpolation. JUCE-free.
#include <vector>
#include <cmath>
#include <cstring>

namespace nsb
{
class FractionalDelay
{
public:
    void prepare (int maxDelaySamples)
    {
        size_t n = 16;
        while (n < (size_t) maxDelaySamples + 8) n <<= 1;
        buf.assign (n, 0.0f);
        mask = (int) n - 1;
        writePos = 0;
    }

    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); }

    void push (float x) noexcept
    {
        writePos = (writePos + 1) & mask;
        buf[(size_t) writePos] = x;
    }

    // delaySamples >= 1 recommended; reads relative to the most recently pushed sample
    float read (float delaySamples) const noexcept
    {
        if (delaySamples < 1.0f) delaySamples = 1.0f;
        const float maxD = (float) (mask - 4);
        if (delaySamples > maxD) delaySamples = maxD;

        const int   di = (int) delaySamples;
        const float frac = delaySamples - (float) di;

        const int i0 = (writePos - di + 1 + mask + 1) & mask; // newer neighbour
        const int i1 = (i0 - 1) & mask;
        const int i2 = (i1 - 1) & mask;
        const int i3 = (i2 - 1) & mask;

        // 4-point, 3rd-order Hermite (Catmull-Rom), interpolating between i1 and i2
        const float xm1 = buf[(size_t) i0];
        const float x0  = buf[(size_t) i1];
        const float x1  = buf[(size_t) i2];
        const float x2  = buf[(size_t) i3];

        const float c0 = x0;
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

private:
    std::vector<float> buf;
    int mask = 15;
    int writePos = 0;
};

// Plain integer-tap mono delay for reflections
class TapDelay
{
public:
    void prepare (int maxDelaySamples)
    {
        size_t n = 16;
        while (n < (size_t) maxDelaySamples + 4) n <<= 1;
        buf.assign (n, 0.0f);
        mask = (int) n - 1;
        writePos = 0;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); }
    void push (float x) noexcept { writePos = (writePos + 1) & mask; buf[(size_t) writePos] = x; }
    float read (int delaySamples) const noexcept
    {
        if (delaySamples < 0) delaySamples = 0;
        if (delaySamples > mask - 1) delaySamples = mask - 1;
        return buf[(size_t) ((writePos - delaySamples + mask + 1) & mask)];
    }

private:
    std::vector<float> buf;
    int mask = 15;
    int writePos = 0;
};
} // namespace nsb
