// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

namespace nekospace::dsp
{
class FractionalDelay
{
public:
    void prepare (int maxDelaySamples)
    {
        std::size_t n = 16;
        while (n < static_cast<std::size_t> (maxDelaySamples) + 8) n <<= 1;
        buf.assign (n, 0.0f);
        mask = static_cast<int> (n) - 1;
        writePos = 0;
    }

    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); }

    void push (float x) noexcept
    {
        writePos = (writePos + 1) & mask;
        buf[static_cast<std::size_t> (writePos)] = x;
    }

    float read (float delaySamples) const noexcept
    {
        if (delaySamples < 1.0f) delaySamples = 1.0f;
        const float maxD = static_cast<float> (mask - 4);
        if (delaySamples > maxD) delaySamples = maxD;

        const int di = static_cast<int> (delaySamples);
        const float frac = delaySamples - static_cast<float> (di);
        const int i0 = (writePos - di + 1 + mask + 1) & mask;
        const int i1 = (i0 - 1) & mask;
        const int i2 = (i1 - 1) & mask;
        const int i3 = (i2 - 1) & mask;

        const float xm1 = buf[static_cast<std::size_t> (i0)];
        const float x0  = buf[static_cast<std::size_t> (i1)];
        const float x1  = buf[static_cast<std::size_t> (i2)];
        const float x2  = buf[static_cast<std::size_t> (i3)];
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
} // namespace nekospace::dsp
