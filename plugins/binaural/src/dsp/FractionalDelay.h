// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <nekospace/dsp/FractionalDelay.h>

namespace nsb
{
using FractionalDelay = nekospace::dsp::FractionalDelay;

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
