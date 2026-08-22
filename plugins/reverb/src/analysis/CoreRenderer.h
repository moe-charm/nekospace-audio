// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "BaselineRenderer.h"
#include "dsp/ReverbCore.h"

namespace nsr::analysis
{
struct CoreRenderSettings
{
    double sampleRate = 48000.0;
    double durationSeconds = 6.0;
    int blockSize = 256;
    ReverbSettings reverb;
};

inline StereoIr renderCoreImpulse (const CoreRenderSettings& settings)
{
    if (settings.sampleRate < 8000.0 || settings.sampleRate > 384000.0)
        throw std::invalid_argument ("sample rate outside analysis range");
    if (settings.durationSeconds <= 0.0 || settings.durationSeconds > 60.0)
        throw std::invalid_argument ("duration must be in (0, 60] seconds");
    if (settings.blockSize < 1 || settings.blockSize > 16384)
        throw std::invalid_argument ("block size must be in [1, 16384]");

    const int total = static_cast<int> (std::ceil (settings.durationSeconds
                                                   * settings.sampleRate));
    StereoIr result;
    result.sampleRate = settings.sampleRate;
    result.left.assign (static_cast<std::size_t> (total), 0.0f);
    result.right.assign (static_cast<std::size_t> (total), 0.0f);

    auto reverb = settings.reverb;
    reverb.mix = 1.0f;
    ReverbCore core;
    core.prepare (settings.sampleRate, settings.blockSize, reverb);

    std::vector<float> inputLeft (static_cast<std::size_t> (settings.blockSize), 0.0f);
    std::vector<float> inputRight (static_cast<std::size_t> (settings.blockSize), 0.0f);
    bool impulsePending = true;
    for (int position = 0; position < total; position += settings.blockSize)
    {
        const int count = std::min (settings.blockSize, total - position);
        std::fill (inputLeft.begin(), inputLeft.end(), 0.0f);
        std::fill (inputRight.begin(), inputRight.end(), 0.0f);
        if (impulsePending)
        {
            inputLeft[0] = inputRight[0] = 1.0f;
            impulsePending = false;
        }
        core.process (inputLeft.data(), inputRight.data(), result.left.data() + position,
                      result.right.data() + position, count);
    }
    return result;
}
} // namespace nsr::analysis
