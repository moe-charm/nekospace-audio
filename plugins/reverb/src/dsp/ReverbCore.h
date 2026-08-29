// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <algorithm>
#include <cmath>
#include <complex>

#include <nekospace/dsp/FractionalDelay.h>

namespace nsr
{
struct ReverbSettings
{
    float space = 0.35f;
    float decaySeconds = 1.4f;
    float bassTailRatio = 1.0f;
    float airTailRatio = 0.7f;
    float mix = 1.0f;
};

struct ReverbConfiguration
{
    int inputDiffuserStages = 4;
};

namespace detail
{
inline float clamp (float value, float lo, float hi) noexcept
{
    return value < lo ? lo : (value > hi ? hi : value);
}

class LinearSmoother
{
public:
    void prepare (float sampleRate, float seconds) noexcept
    {
        total = std::max (1, static_cast<int> (sampleRate * seconds));
    }
    void snap (float value) noexcept { current = target = value; step = 0.0f; left = 0; }
    void setTarget (float value) noexcept
    {
        if (value == target) return;
        target = value;
        step = (target - current) / static_cast<float> (total);
        left = total;
    }
    float next() noexcept
    {
        if (left > 0 && --left == 0) current = target;
        else if (left > 0) current += step;
        return current;
    }

private:
    float current = 0.0f, target = 0.0f, step = 0.0f;
    int total = 1, left = 0;
};

// Complementary three-band split inside the feedback loop. With both ratios at one the
// algebra reduces exactly to the input, so a neutral decay curve has no hidden damping.
class DecayFilter
{
public:
    void prepare (float sampleRate) noexcept
    {
        lowCoefficient = coefficient (sampleRate, 500.0f);
        highCoefficient = coefficient (sampleRate, 4000.0f);
        reset();
    }
    float process (float input, float lowRatio, float highRatio) noexcept
    {
        lowState += lowCoefficient * (input - lowState);
        highState += highCoefficient * (input - highState);
        const float low = lowState;
        const float highLowpass = highState;
        const float mid = highLowpass - low;
        const float high = input - highLowpass;
        return lowRatio * low + mid + highRatio * high;
    }
    void reset() noexcept { lowState = highState = 0.0f; }

private:
    static float coefficient (float sampleRate, float hz) noexcept
    {
        return 1.0f - std::exp (-6.28318530717958647692f * hz / sampleRate);
    }
    float lowCoefficient = 0.0f, highCoefficient = 0.0f;
    float lowState = 0.0f, highState = 0.0f;
};

class AllpassDiffuser
{
public:
    void prepare (float sampleRate, int lengthAt48k, float coefficient)
    {
        const int samples = std::max (1, static_cast<int> (std::lround (
            lengthAt48k * sampleRate / 48000.0f)));
        delay.prepare (samples + 8);
        length = static_cast<float> (samples);
        gain = coefficient;
    }
    void reset() noexcept { delay.reset(); }
    float process (float input) noexcept
    {
        const float delayed = delay.read (length);
        const float output = delayed - gain * input;
        delay.push (input + gain * output);
        return output;
    }

private:
    nekospace::dsp::FractionalDelay delay;
    float length = 1.0f, gain = 0.0f;
};

class InputDiffuser
{
public:
    void prepare (float sampleRate)
    {
        stages[0].prepare (sampleRate, 149, 0.63f);
        stages[1].prepare (sampleRate, 211, -0.57f);
        stages[2].prepare (sampleRate, 263, 0.51f);
        stages[3].prepare (sampleRate, 293, -0.47f);
    }
    void reset() noexcept { for (auto& stage : stages) stage.reset(); }
    float process (float input, int stageCount) noexcept
    {
        for (int stage = 0; stage < stageCount; ++stage) input = stages[stage].process (input);
        return input;
    }

private:
    AllpassDiffuser stages[4];
};

template<int LineCount>
struct NetworkTraits;

template<>
struct NetworkTraits<8>
{
    static constexpr int baseLength[8] = { 1123, 1327, 1523, 1723,
                                            1931, 2129, 2333, 2539 };
    static constexpr float inputGain[8] = { 0.5f, -0.4f, 0.45f, -0.35f,
                                             0.4f, -0.45f, 0.35f, -0.5f };
    static constexpr float outputSign[8] = { 1, -1, 1, -1, 1, -1, 1, -1 };
    static constexpr float outputScale = 0.30f;
};

template<>
struct NetworkTraits<16>
{
    static constexpr int baseLength[16] = { 557, 613, 677, 743, 811, 883, 953, 1021,
                                             1091, 1163, 1237, 1307, 1381, 1453, 1523, 1597 };
    static constexpr float inputGain[16] = {
        0.31f, -0.27f, 0.29f, -0.33f, 0.25f, -0.30f, 0.28f, -0.32f,
        0.26f, -0.29f, 0.34f, -0.24f, 0.30f, -0.28f, 0.27f, -0.31f
    };
    static constexpr float outputSign[16] = {
        1, -1, 1, -1, 1, -1, 1, -1, -1, 1, -1, 1, -1, 1, -1, 1
    };
    static constexpr float outputScale = 0.3042f;
};

template<int LineCount>
class LateNetwork
{
public:
    static constexpr int lines = LineCount;

    void prepare (float sampleRate, const ReverbSettings& initialSettings)
    {
        sr = sampleRate;
        prepareShelfBasis();
        for (int i = 0; i < lines; ++i)
        {
            maxLength[i] = static_cast<int> (NetworkTraits<lines>::baseLength[i]
                                             * (sr / 48000.0f) * 2.2f) + 8;
            delay[i].prepare (maxLength[i]);
            decayFilter[i].prepare (sr);
            length[i].prepare (sr, 0.05f);
            midGain[i].prepare (sr, 0.05f);
            lowRatio[i].prepare (sr, 0.05f);
            highRatio[i].prepare (sr, 0.05f);
        }
        setSettings (initialSettings);
        for (int i = 0; i < lines; ++i)
        {
            length[i].snap (targetLength[i]);
            midGain[i].snap (targetMidGain[i]);
            lowRatio[i].snap (targetLowRatio[i]);
            highRatio[i].snap (targetHighRatio[i]);
        }
        reset();
    }

    void reset() noexcept
    {
        for (int i = 0; i < lines; ++i)
        {
            delay[i].reset();
            decayFilter[i].reset();
        }
    }

    void setSettings (const ReverbSettings& settings) noexcept
    {
        const float scale = (0.35f + 0.85f * clamp (settings.space, 0.0f, 1.0f))
                            * (sr / 48000.0f);
        const float midT60 = clamp (settings.decaySeconds, 0.15f, 4.0f);
        const float lowT60 = clamp (midT60 * clamp (settings.bassTailRatio, 0.25f, 2.0f),
                                    0.15f, 8.0f);
        const float highT60 = clamp (midT60 * clamp (settings.airTailRatio, 0.25f, 2.0f),
                                     0.15f, 8.0f);
        for (int i = 0; i < lines; ++i)
        {
            const float scaledLength = clamp (
                static_cast<float> (NetworkTraits<lines>::baseLength[i]) * scale,
                                              32.0f, static_cast<float> (maxLength[i] - 8));
            // A static fractional tap adds interpolation loss that varies with its
            // fractional part, making high-band T60 drift when Space changes. Static
            // targets are integral; the smoother still crosses fractional positions
            // during automation without a delay-length step.
            targetLength[i] = std::floor (scaledLength + 0.5f);
            const float nominalMidGain = loopGain (targetLength[i], midT60);
            const float lowGain = loopGain (targetLength[i], lowT60);
            const float highGain = loopGain (targetLength[i], highT60);
            const auto fitted = fitDecayGains (lowGain, nominalMidGain, highGain);
            targetMidGain[i] = fitted.mid;
            targetLowRatio[i] = fitted.lowRatio;
            targetHighRatio[i] = fitted.highRatio;
            length[i].setTarget (targetLength[i]);
            midGain[i].setTarget (targetMidGain[i]);
            lowRatio[i].setTarget (targetLowRatio[i]);
            highRatio[i].setTarget (targetHighRatio[i]);
        }
    }

    // Mid and Side use the same network geometry and decay targets. Calculate the
    // nonlinear fit once, then apply the identical targets to the independent state.
    void setSettingsFrom (const LateNetwork& source) noexcept
    {
        for (int i = 0; i < lines; ++i)
        {
            targetLength[i] = source.targetLength[i];
            targetMidGain[i] = source.targetMidGain[i];
            targetLowRatio[i] = source.targetLowRatio[i];
            targetHighRatio[i] = source.targetHighRatio[i];
            length[i].setTarget (targetLength[i]);
            midGain[i].setTarget (targetMidGain[i]);
            lowRatio[i].setTarget (targetLowRatio[i]);
            highRatio[i].setTarget (targetHighRatio[i]);
        }
    }

    float processSample (float input) noexcept
    {
        float d[lines];
        for (int i = 0; i < lines; ++i)
        {
            const float delayed = delay[i].read (length[i].next());
            d[i] = decayFilter[i].process (delayed, lowRatio[i].next(), highRatio[i].next())
                   * midGain[i].next();
        }

        float h[lines];
        for (int i = 0; i < lines; ++i) h[i] = d[i];
        for (int width = 1; width < lines; width <<= 1)
            for (int start = 0; start < lines; start += width * 2)
                for (int offset = 0; offset < width; ++offset)
                {
                    const float a = h[start + offset];
                    const float b = h[start + offset + width];
                    h[start + offset] = a + b;
                    h[start + offset + width] = a - b;
                }
        const float matrixScale = 1.0f / std::sqrt (static_cast<float> (lines));

        for (int i = 0; i < lines; ++i)
            delay[i].push (h[i] * matrixScale + input * NetworkTraits<lines>::inputGain[i]);

        float output = 0.0f;
        for (int i = 0; i < lines; ++i)
            output += d[i] * NetworkTraits<lines>::outputSign[i];
        return output * NetworkTraits<lines>::outputScale;
    }

private:
    struct FittedGains { float mid, lowRatio, highRatio; };
    using Complex = std::complex<float>;

    struct ShelfBasis
    {
        Complex lowBand;
        Complex highLowpass;
    };

    float loopGain (float delaySamples, float t60Seconds) const noexcept
    {
        return std::pow (10.0f, -3.0f * delaySamples / (t60Seconds * sr));
    }

    void prepareShelfBasis() noexcept
    {
        constexpr float hz[3] = { 125.0f, 1000.0f, 8000.0f };
        const float lowCoefficient = 1.0f - std::exp (-6.28318530717958647692f * 500.0f / sr);
        const float highCoefficient = 1.0f - std::exp (-6.28318530717958647692f * 4000.0f / sr);
        for (int band = 0; band < 3; ++band)
        {
            const float omega = 6.28318530717958647692f * hz[band] / sr;
            const Complex zInverse = std::polar (1.0f, -omega);
            const auto lowpass = [zInverse] (float coefficient)
            {
                return Complex (coefficient, 0.0f)
                       / (Complex (1.0f, 0.0f) - (1.0f - coefficient) * zInverse);
            };
            shelfBasis[band] = { lowpass (lowCoefficient), lowpass (highCoefficient) };
        }
    }

    float shelfMagnitude (int band, float low, float high) const noexcept
    {
        const auto& basis = shelfBasis[band];
        const Complex response = low * basis.lowBand
                               + (basis.highLowpass - basis.lowBand)
                               + high * (1.0f - basis.highLowpass);
        return std::max (std::abs (response), 1.0e-6f);
    }

    FittedGains fitDecayGains (float lowTarget, float midTarget,
                               float highTarget) const noexcept
    {
        float x[3] = { std::log (midTarget), std::log (lowTarget / midTarget),
                       std::log (highTarget / midTarget) };
        const float target[3] = { std::log (lowTarget), std::log (midTarget),
                                  std::log (highTarget) };
        constexpr float epsilon = 1.0e-3f;

        for (int iteration = 0; iteration < 6; ++iteration)
        {
            float residual[3] = {};
            float jacobian[3][3] = {};
            const auto evaluate = [&] (int band, const float* values)
            {
                return values[0] + std::log (shelfMagnitude (
                    band, std::exp (values[1]), std::exp (values[2])));
            };
            for (int band = 0; band < 3; ++band)
            {
                const float current = evaluate (band, x);
                residual[band] = current - target[band];
                for (int variable = 0; variable < 3; ++variable)
                {
                    float perturbed[3] = { x[0], x[1], x[2] };
                    perturbed[variable] += epsilon;
                    jacobian[band][variable] = (evaluate (band, perturbed)
                                                 - current) / epsilon;
                }
            }

            // Gaussian elimination on the fixed 3x3 Newton system J * delta = residual.
            float augmented[3][4] = {};
            for (int row = 0; row < 3; ++row)
                for (int column = 0; column < 3; ++column)
                    augmented[row][column] = jacobian[row][column];
            for (int row = 0; row < 3; ++row) augmented[row][3] = residual[row];
            for (int pivot = 0; pivot < 3; ++pivot)
            {
                int best = pivot;
                for (int row = pivot + 1; row < 3; ++row)
                    if (std::abs (augmented[row][pivot]) > std::abs (augmented[best][pivot]))
                        best = row;
                if (best != pivot)
                    for (int column = pivot; column < 4; ++column)
                        std::swap (augmented[pivot][column], augmented[best][column]);
                const float divisor = augmented[pivot][pivot];
                if (std::abs (divisor) < 1.0e-7f) break;
                for (int column = pivot; column < 4; ++column)
                    augmented[pivot][column] /= divisor;
                for (int row = 0; row < 3; ++row)
                    if (row != pivot)
                    {
                        const float factor = augmented[row][pivot];
                        for (int column = pivot; column < 4; ++column)
                            augmented[row][column] -= factor * augmented[pivot][column];
                    }
            }
            for (int variable = 0; variable < 3; ++variable)
                x[variable] -= augmented[variable][3];
        }
        return { std::exp (x[0]), std::exp (x[1]), std::exp (x[2]) };
    }

    nekospace::dsp::FractionalDelay delay[lines];
    DecayFilter decayFilter[lines];
    LinearSmoother length[lines], midGain[lines], lowRatio[lines], highRatio[lines];
    int maxLength[lines] = {};
    float targetLength[lines] = {}, targetMidGain[lines] = {};
    float targetLowRatio[lines] = {}, targetHighRatio[lines] = {};
    ShelfBasis shelfBasis[3] {};
    float sr = 48000.0f;
};
} // namespace detail

// JUCE-free fixed-stereo core. Mid and Side have independent late states so mono remains
// exactly symmetric while stereo difference information cannot collapse at the input.
template<int LineCount>
class BasicReverbCore
{
public:
    void prepare (double sampleRate, int /*maximumBlockSize*/,
                  const ReverbSettings& initialSettings = {},
                  const ReverbConfiguration& initialConfiguration = {})
    {
        settings = initialSettings;
        configuration = initialConfiguration;
        configuration.inputDiffuserStages = std::max (0, std::min (4,
            configuration.inputDiffuserStages));
        settings.mix = detail::clamp (settings.mix, 0.0f, 1.0f);
        midDiffuser.prepare (static_cast<float> (sampleRate));
        sideDiffuser.prepare (static_cast<float> (sampleRate));
        mid.prepare (static_cast<float> (sampleRate), settings);
        side.prepare (static_cast<float> (sampleRate), settings);
    }

    void reset() noexcept
    {
        midDiffuser.reset(); sideDiffuser.reset();
        mid.reset(); side.reset();
    }

    void setSettings (const ReverbSettings& next) noexcept
    {
        settings = next;
        settings.mix = detail::clamp (settings.mix, 0.0f, 1.0f);
        mid.setSettings (settings);
        side.setSettingsFrom (mid);
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
            float midInput = (left + right) * 0.5f;
            float sideInput = (left - right) * 0.5f;
            if (configuration.inputDiffuserStages > 0)
            {
                midInput = midDiffuser.process (midInput, configuration.inputDiffuserStages);
                sideInput = sideDiffuser.process (sideInput, configuration.inputDiffuserStages);
            }
            const float wetMid = mid.processSample (midInput);
            const float wetSide = side.processSample (sideInput);
            outputLeft[i] = left * dry + (wetMid + wetSide) * settings.mix;
            outputRight[i] = right * dry + (wetMid - wetSide) * settings.mix;
        }
    }

private:
    detail::InputDiffuser midDiffuser, sideDiffuser;
    detail::LateNetwork<LineCount> mid, side;
    ReverbSettings settings;
    ReverbConfiguration configuration;
};

using ReverbCore8 = BasicReverbCore<8>;
using ReverbCore = BasicReverbCore<16>;
} // namespace nsr
