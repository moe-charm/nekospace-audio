// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "dsp/RoomEngine.h"

namespace nsr::analysis
{
struct BaselineSettings
{
    double sampleRate = 48000.0;
    double durationSeconds = 6.0;
    int blockSize = 256;
    float size = 0.35f;
    float decaySeconds = 1.4f;
    float damping = 0.0f;
};

struct StereoIr
{
    std::vector<float> left;
    std::vector<float> right;
    double sampleRate = 48000.0;
};

// Renders the exact 8-line FDN currently shipped inside NekoSpace Binaural. The class is
// an adapter, not a fork: Phase 0 exists to freeze this behaviour before any extraction.
inline StereoIr renderBaselineImpulse (const BaselineSettings& settings)
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

    nsb::FdnReverb fdn;
    fdn.prepare (static_cast<float> (settings.sampleRate), settings.blockSize);
    nsb::RoomParams room;
    room.size = std::clamp (settings.size, 0.0f, 1.0f);
    room.decaySeconds = std::clamp (settings.decaySeconds, 0.15f, 4.0f);
    room.damping = std::clamp (settings.damping, 0.0f, 1.0f);
    fdn.setRoom (room);

    std::vector<float> input (static_cast<std::size_t> (settings.blockSize), 0.0f);
    std::vector<float> left (static_cast<std::size_t> (settings.blockSize), 0.0f);
    std::vector<float> right (static_cast<std::size_t> (settings.blockSize), 0.0f);

    bool impulsePending = true;
    for (int position = 0; position < total; position += settings.blockSize)
    {
        const int count = std::min (settings.blockSize, total - position);
        std::fill (input.begin(), input.end(), 0.0f);
        std::fill (left.begin(), left.end(), 0.0f);
        std::fill (right.begin(), right.end(), 0.0f);
        if (impulsePending)
        {
            input[0] = 1.0f;
            impulsePending = false;
        }
        fdn.process (input.data(), left.data(), right.data(), count);
        std::copy_n (left.begin(), count, result.left.begin() + position);
        std::copy_n (right.begin(), count, result.right.begin() + position);
    }
    return result;
}
} // namespace nsr::analysis
