// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <algorithm>
#include <cmath>

#include <nekospace/dsp/FractionalDelay.h>

namespace nsr
{
struct ReverbSettings
{
    float space = 0.35f;
    float decaySeconds = 1.4f;
    float damping = 0.0f;
    float mix = 1.0f;
};

namespace detail
{
inline float clamp (float value, float lo, float hi) noexcept
{
    return value < lo ? lo : (value > hi ? hi : value);
}

class OnePoleLowpass
{
public:
    void prepare (float sampleRate) noexcept { sr = sampleRate; reset(); }
    void setCutoff (float hz) noexcept
    {
        hz = clamp (hz, 20.0f, sr * 0.49f);
        b = std::exp (-6.28318530717958647692f * hz / sr);
        a = 1.0f - b;
    }
    float process (float input) noexcept { state = a * input + b * state; return state; }
    void reset() noexcept { state = 0.0f; }

private:
    float sr = 48000.0f;
    float a = 1.0f, b = 0.0f, state = 0.0f;
};

class LateNetwork
{
public:
    static constexpr int lines = 8;

    void prepare (float sampleRate)
    {
        sr = sampleRate;
        for (int i = 0; i < lines; ++i)
        {
            maxLength[i] = static_cast<int> (baseLength[i] * (sr / 48000.0f) * 2.2f) + 8;
            delay[i].prepare (maxLength[i]);
            lowpass[i].prepare (sr);
        }
        setSettings ({});
        reset();
    }

    void reset() noexcept
    {
        for (int i = 0; i < lines; ++i)
        {
            delay[i].reset();
            lowpass[i].reset();
        }
    }

    void setSettings (const ReverbSettings& settings) noexcept
    {
        const float scale = (0.35f + 0.85f * clamp (settings.space, 0.0f, 1.0f))
                            * (sr / 48000.0f);
        const float t60 = clamp (settings.decaySeconds, 0.15f, 4.0f);
        const float cutoff = 13000.0f - 10500.0f * clamp (settings.damping, 0.0f, 1.0f);
        for (int i = 0; i < lines; ++i)
        {
            length[i] = clamp (static_cast<float> (baseLength[i]) * scale,
                               32.0f, static_cast<float> (maxLength[i] - 8));
            feedback[i] = std::pow (10.0f, -3.0f * length[i] / (t60 * sr));
            lowpass[i].setCutoff (cutoff);
        }
    }

    float processSample (float input) noexcept
    {
        float d[lines];
        for (int i = 0; i < lines; ++i)
            d[i] = lowpass[i].process (delay[i].read (length[i]) * feedback[i]);

        float s[lines];
        for (int i = 0; i < 4; ++i)
        {
            s[i] = d[i] + d[i + 4];
            s[i + 4] = d[i] - d[i + 4];
        }
        float t[lines];
        for (int i = 0; i < 2; ++i)
        {
            t[i] = s[i] + s[i + 2];
            t[i + 2] = s[i] - s[i + 2];
            t[i + 4] = s[i + 4] + s[i + 6];
            t[i + 6] = s[i + 4] - s[i + 6];
        }
        float h[lines];
        for (int i = 0; i < 4; ++i)
        {
            h[2 * i] = (t[2 * i] + t[2 * i + 1]) * 0.35355339f;
            h[2 * i + 1] = (t[2 * i] - t[2 * i + 1]) * 0.35355339f;
        }

        for (int i = 0; i < lines; ++i)
            delay[i].push (h[i] + input * inputGain[i]);

        return (d[0] - d[1] + d[2] - d[3] + d[4] - d[5] + d[6] - d[7]) * 0.30f;
    }

private:
    static constexpr int baseLength[lines] = { 1123, 1327, 1523, 1723,
                                                1931, 2129, 2333, 2539 };
    static constexpr float inputGain[lines] = { 0.5f, -0.4f, 0.45f, -0.35f,
                                                0.4f, -0.45f, 0.35f, -0.5f };
    nekospace::dsp::FractionalDelay delay[lines];
    OnePoleLowpass lowpass[lines];
    int maxLength[lines] = {};
    float length[lines] = {}, feedback[lines] = {};
    float sr = 48000.0f;
};
} // namespace detail

// JUCE-free fixed-stereo core. Mid and Side have independent late states so mono remains
// exactly symmetric while stereo difference information cannot collapse at the input.
class ReverbCore
{
public:
    void prepare (double sampleRate, int /*maximumBlockSize*/)
    {
        mid.prepare (static_cast<float> (sampleRate));
        side.prepare (static_cast<float> (sampleRate));
        setSettings ({});
    }

    void reset() noexcept { mid.reset(); side.reset(); }

    void setSettings (const ReverbSettings& next) noexcept
    {
        settings = next;
        settings.mix = detail::clamp (settings.mix, 0.0f, 1.0f);
        mid.setSettings (settings);
        side.setSettings (settings);
    }

    void process (const float* inputLeft, const float* inputRight,
                  float* outputLeft, float* outputRight, int count) noexcept
    {
        if (settings.mix == 0.0f)
        {
            for (int i = 0; i < count; ++i)
            {
                outputLeft[i] = inputLeft[i];
                outputRight[i] = inputRight[i];
            }
            return;
        }

        const float dry = 1.0f - settings.mix;
        for (int i = 0; i < count; ++i)
        {
            const float left = inputLeft[i];
            const float right = inputRight[i];
            const float wetMid = mid.processSample ((left + right) * 0.5f);
            const float wetSide = side.processSample ((left - right) * 0.5f);
            outputLeft[i] = left * dry + (wetMid + wetSide) * settings.mix;
            outputRight[i] = right * dry + (wetMid - wetSide) * settings.mix;
        }
    }

private:
    detail::LateNetwork mid, side;
    ReverbSettings settings;
};
} // namespace nsr
