// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

#include "ReverbCore.h"

namespace nsr
{
namespace detail
{
// Keep the reconstruction in one tested helper. Any generated Side cancels under a
// mono fold-down because the two output channels are formed as the same Mid +/- Side.
inline void reconstructMidSide (float mid, float side, float& left, float& right) noexcept
{
    left = mid + side;
    right = mid - side;
}
} // namespace detail

enum class RoomBodyAuditionMode : int
{
    tailOnly = 0,
    roomBody,
    earlyOnly
};

struct RoomBodySettings
{
    float space = 0.35f;
    float decaySeconds = 1.4f;
    float bassTailRatio = 1.0f;
    float airTailRatio = 0.7f;
    float distance = 0.25f;
    float definition = 0.65f;
    float preDelayMs = 12.0f;
    float mix = 1.0f;
    float wetMonoInput = 0.0f;
};

// A fixed-capacity first-order shoebox wrapped around the selected 16-line late tail.
// The virtual source/listener geometry affects the wet field only; the dry input is never
// delayed, panned or otherwise spatialised. All storage is allocated during prepare().
class RoomBodyCore
{
public:
    static constexpr int reflectionCount = 6;

    enum Reflection : int
    {
        leftWall = 0,
        rightWall,
        frontWall,
        backWall,
        floor,
        ceiling
    };

    void prepare (double sampleRate, int maximumBlockSize,
                  const RoomBodySettings& initialSettings = {})
    {
        sr = sampleRate > 1000.0 ? static_cast<float> (sampleRate) : 48000.0f;
        preparedBlockSize = std::max (1, maximumBlockSize);

        // 120 ms maximum user pre-delay + the longest supported first-order image path
        // and Definition-controlled late onset fit inside this fixed prepared history.
        maxHistoryDelaySamples = static_cast<int> (std::ceil (sr * 0.40f)) + 16;
        midHistory.prepare (maxHistoryDelaySamples);
        sideHistory.prepare (maxHistoryDelaySamples);

        for (auto& reflection : reflections)
        {
            reflection.filterCoefficient.prepare (sr, smoothingSeconds);
            for (int ear = 0; ear < 2; ++ear)
            {
                reflection.delay[ear].prepare (sr, smoothingSeconds);
                reflection.gain[ear].prepare (sr, smoothingSeconds);
            }
        }
        lateExcitationDelay.prepare (sr, smoothingSeconds);
        earlyAuditionGain.prepare (sr, smoothingSeconds);
        lateAuditionGain.prepare (sr, smoothingSeconds);
        wetStereoAmount.prepare (sr, smoothingSeconds);
        mixSmoother.prepare (sr, smoothingSeconds);

        earlySpreadA.prepare (sr, 257, 0.55f);
        earlySpreadB.prepare (sr, 379, 0.55f);
        earlySpreadHighpassCoefficient = onePoleCoefficient (250.0f);
        earlySpreadLowpassCoefficient = onePoleCoefficient (8000.0f);

        dryLeft.assign (static_cast<std::size_t> (preparedBlockSize), 0.0f);
        dryRight.assign (static_cast<std::size_t> (preparedBlockSize), 0.0f);
        earlyLeft.assign (static_cast<std::size_t> (preparedBlockSize), 0.0f);
        earlyRight.assign (static_cast<std::size_t> (preparedBlockSize), 0.0f);
        lateInputLeft.assign (static_cast<std::size_t> (preparedBlockSize), 0.0f);
        lateInputRight.assign (static_cast<std::size_t> (preparedBlockSize), 0.0f);
        lateOutputLeft.assign (static_cast<std::size_t> (preparedBlockSize), 0.0f);
        lateOutputRight.assign (static_cast<std::size_t> (preparedBlockSize), 0.0f);

        settings = normalise (initialSettings);
        ReverbSettings lateSettings;
        copyLateSettings (lateSettings);
        lateCore.prepare (sr, preparedBlockSize, lateSettings);

        prepared = true;
        updateTargets (true);
        snapAuditionGains();
        wetStereoAmount.snap (1.0f - settings.wetMonoInput);
        mixSmoother.snap (settings.mix);
        clearSignalState();
    }

    void reset() noexcept
    {
        if (! prepared) return;
        clearSignalState();
        lateCore.reset();
        for (int reflection = 0; reflection < reflectionCount; ++reflection)
        {
            reflections[reflection].filterCoefficient.snap (
                targetFilterCoefficient[reflection]);
            for (int ear = 0; ear < 2; ++ear)
            {
                reflections[reflection].delay[ear].snap (
                    targetReflectionDelay[reflection][ear]);
                reflections[reflection].gain[ear].snap (
                    targetReflectionGain[reflection][ear]);
            }
        }
        lateExcitationDelay.snap (targetLateExcitationDelay);
        snapAuditionGains();
        wetStereoAmount.snap (1.0f - settings.wetMonoInput);
        mixSmoother.snap (settings.mix);
    }

    void setSettings (const RoomBodySettings& next) noexcept
    {
        settings = normalise (next);
        if (! prepared) return;

        mixSmoother.setTarget (settings.mix);
        wetStereoAmount.setTarget (1.0f - settings.wetMonoInput);
        ReverbSettings lateSettings;
        copyLateSettings (lateSettings);
        lateCore.setSettings (lateSettings);
        updateTargets (false);
    }

    void setAuditionMode (RoomBodyAuditionMode mode) noexcept
    {
        auditionMode.store (static_cast<int> (mode), std::memory_order_relaxed);
    }

    RoomBodyAuditionMode getAuditionMode() const noexcept
    {
        const int value = auditionMode.load (std::memory_order_relaxed);
        if (value == static_cast<int> (RoomBodyAuditionMode::tailOnly))
            return RoomBodyAuditionMode::tailOnly;
        if (value == static_cast<int> (RoomBodyAuditionMode::earlyOnly))
            return RoomBodyAuditionMode::earlyOnly;
        return RoomBodyAuditionMode::roomBody;
    }

    // Fixed default-reference correction, calibrated offline after the bounded v2 retune.
    // It belongs only to the unsaved Room Body comparison; it is not live AGC or a product
    // Wet Trim parameter.
    static constexpr float roomBodyAuditionTrim = 0.9913f;

    // Includes common wet pre-delay and the physical image-to-ear travel time.
    // reflectionIndex follows Reflection; earIndex is 0 = left, 1 = right.
    float targetReflectionDelaySamples (int reflectionIndex, int earIndex) const noexcept
    {
        if (reflectionIndex < 0 || reflectionIndex >= reflectionCount
            || earIndex < 0 || earIndex > 1)
            return 0.0f;
        return targetReflectionDelay[reflectionIndex][earIndex];
    }

    // Delay applied before the existing diffuser/FDN. It includes common wet pre-delay
    // and the continuously Definition-controlled late-onset offset.
    float targetLateExcitationDelaySamples() const noexcept
    {
        return targetLateExcitationDelay;
    }

    void process (const float* inputLeft, const float* inputRight,
                  float* outputLeft, float* outputRight, int count) noexcept
    {
        if (count <= 0) return;
        if (! prepared)
        {
            for (int i = 0; i < count; ++i)
            {
                outputLeft[i] = inputLeft[i];
                outputRight[i] = inputRight[i];
            }
            return;
        }

        for (int offset = 0; offset < count; offset += preparedBlockSize)
        {
            const int chunk = std::min (preparedBlockSize, count - offset);
            processChunk (inputLeft + offset, inputRight + offset,
                          outputLeft + offset, outputRight + offset, chunk);
        }
    }

private:
    static constexpr float smoothingSeconds = 0.05f;
    static constexpr float speedOfSound = 343.0f;
    static constexpr float headRadius = 0.0875f;
    // Keep stereo difference information without turning the early room into two
    // disconnected left/right reverbs. A hard-panned source must still reach both ears.
    static constexpr float earlySideRetention = 0.5f;
    static constexpr float earlySpreadAmount = 0.22f;
    static constexpr float pi = 3.14159265358979323846f;

    struct Vec3
    {
        float x, y, z;

        float distanceTo (float earX) const noexcept
        {
            const float dx = x - earX;
            return std::sqrt (dx * dx + y * y + z * z);
        }

        float length() const noexcept
        {
            return std::sqrt (x * x + y * y + z * z);
        }
    };

    // A time smoother alone can move a delay tap by more than one sample per sample on a
    // full Pre-delay jump, which makes the read head run backwards. Keep the transition
    // at least 50 ms while also bounding its speed. Large gestures take longer rather
    // than reversing or skipping the stored waveform.
    class DelaySmoother
    {
    public:
        void prepare (float sampleRate, float seconds) noexcept
        {
            minimumSamples = std::max (1, static_cast<int> (sampleRate * seconds));
        }

        void snap (float value) noexcept
        {
            current = target = value;
            step = 0.0f;
            samplesLeft = 0;
        }

        void setTarget (float value) noexcept
        {
            if (value == target) return;
            target = value;
            const float difference = target - current;
            const int slewSamples = static_cast<int> (
                std::ceil (std::abs (difference) / maximumSamplesPerSample));
            samplesLeft = std::max (minimumSamples, slewSamples);
            step = difference / static_cast<float> (samplesLeft);
        }

        float next() noexcept
        {
            if (samplesLeft > 0)
            {
                current += step;
                if (--samplesLeft == 0) current = target;
            }
            return current;
        }

    private:
        static constexpr float maximumSamplesPerSample = 0.5f;
        float current = 0.0f, target = 0.0f, step = 0.0f;
        int minimumSamples = 1, samplesLeft = 0;
    };

    struct ReflectionState
    {
        DelaySmoother delay[2];
        detail::LinearSmoother gain[2];
        detail::LinearSmoother filterCoefficient;
        float filterState[2][2] = {}; // [ear][Mid, Side]
    };

    static RoomBodySettings normalise (RoomBodySettings value) noexcept
    {
        value.space = detail::clamp (value.space, 0.0f, 1.0f);
        value.decaySeconds = detail::clamp (value.decaySeconds, 0.15f, 4.0f);
        value.bassTailRatio = detail::clamp (value.bassTailRatio, 0.25f, 2.0f);
        value.airTailRatio = detail::clamp (value.airTailRatio, 0.25f, 2.0f);
        value.distance = detail::clamp (value.distance, 0.0f, 1.0f);
        value.definition = detail::clamp (value.definition, 0.0f, 1.0f);
        value.preDelayMs = detail::clamp (value.preDelayMs, 0.0f, 120.0f);
        value.mix = detail::clamp (value.mix, 0.0f, 1.0f);
        value.wetMonoInput = value.wetMonoInput >= 0.5f ? 1.0f : 0.0f;
        return value;
    }

    float onePoleCoefficient (float cutoffHz) const noexcept
    {
        const float cutoff = detail::clamp (cutoffHz, 40.0f, 0.45f * sr);
        return 1.0f - std::exp (-2.0f * pi * cutoff / sr);
    }

    void auditionTargets (float& early, float& late) const noexcept
    {
        switch (getAuditionMode())
        {
            case RoomBodyAuditionMode::tailOnly:
                early = 0.0f;
                late = 1.0f;
                break;
            case RoomBodyAuditionMode::earlyOnly:
                early = 1.0f;
                late = 0.0f;
                break;
            case RoomBodyAuditionMode::roomBody:
            default:
                early = roomBodyAuditionTrim;
                late = roomBodyAuditionTrim;
                break;
        }
    }

    void snapAuditionGains() noexcept
    {
        float early = 1.0f, late = 1.0f;
        auditionTargets (early, late);
        earlyAuditionGain.snap (early);
        lateAuditionGain.snap (late);
    }

    void copyLateSettings (ReverbSettings& destination) const noexcept
    {
        destination.space = settings.space;
        destination.decaySeconds = settings.decaySeconds;
        destination.bassTailRatio = settings.bassTailRatio;
        destination.airTailRatio = settings.airTailRatio;
        destination.mix = 1.0f; // RoomBodyCore owns the only dry/wet mix operation.
    }

    void updateTargets (bool snap) noexcept
    {
        const float width = 3.0f + 11.0f * settings.space;
        const float depth = 3.5f + 12.0f * settings.space;
        const float height = 2.4f + 3.6f * settings.space;
        constexpr float listenerEarHeight = 1.4f;

        // Distance changes only the hidden wet excitation point. The dry source remains
        // untouched. Keep a 0.5 m clearance from the front wall at every Space setting.
        const float maximumSourceZ = std::max (0.35f, depth * 0.5f - 0.5f);
        const float sourceZ = 0.35f
                            + settings.distance * (maximumSourceZ - 0.35f);

        const std::array<Vec3, reflectionCount> images {{
            { -width, 0.0f, sourceZ },
            {  width, 0.0f, sourceZ },
            { 0.0f, 0.0f, depth - sourceZ },
            { 0.0f, 0.0f, -depth - sourceZ },
            { 0.0f, -2.0f * listenerEarHeight, sourceZ },
            { 0.0f, 2.0f * (height - listenerEarHeight), sourceZ }
        }};

        // Left/right surfaces deliberately match so the physical field remains centred;
        // the later bounded decorrelator adds energy-balanced Side. These are internal
        // listening values, not user parameters.
        constexpr float surfaceGain[reflectionCount] = {
            0.68f, 0.68f, 0.64f, 0.58f, 0.46f, 0.52f
        };
        constexpr float softSurfaceCutoffHz[reflectionCount] = {
            6500.0f, 6500.0f, 6000.0f, 4500.0f, 3200.0f, 4000.0f
        };
        constexpr float hardSurfaceCutoffHz[reflectionCount] = {
            12000.0f, 12000.0f, 11000.0f, 8000.0f, 6000.0f, 8000.0f
        };

        const float preDelaySamples = settings.preDelayMs * 0.001f * sr;
        const float prominence = 0.45f + 0.55f * settings.definition;

        for (int reflection = 0; reflection < reflectionCount; ++reflection)
        {
            const float centreDistance = std::max (images[reflection].length(), 0.5f);
            const float pan = detail::clamp (0.85f * images[reflection].x / centreDistance,
                                             -0.85f, 0.85f);
            const float panGain[2] = {
                std::sqrt (0.5f * (1.0f - pan)),
                std::sqrt (0.5f * (1.0f + pan))
            };

            const float softCutoff = softSurfaceCutoffHz[reflection];
            const float cutoff = detail::clamp (
                softCutoff * std::pow (hardSurfaceCutoffHz[reflection] / softCutoff,
                                       settings.definition),
                40.0f, 0.45f * sr);
            targetFilterCoefficient[reflection] =
                1.0f - std::exp (-2.0f * pi * cutoff / sr);

            for (int ear = 0; ear < 2; ++ear)
            {
                const float earX = ear == 0 ? -headRadius : headRadius;
                const float pathMetres = std::max (images[reflection].distanceTo (earX), 0.5f);
                targetReflectionDelay[reflection][ear] = detail::clamp (
                    preDelaySamples + pathMetres * sr / speedOfSound,
                    1.0f, static_cast<float> (maxHistoryDelaySamples - 8));
                targetReflectionGain[reflection][ear] =
                    prominence * surfaceGain[reflection] * panGain[ear] / pathMetres;

                if (snap)
                {
                    reflections[reflection].delay[ear].snap (
                        targetReflectionDelay[reflection][ear]);
                    reflections[reflection].gain[ear].snap (
                        targetReflectionGain[reflection][ear]);
                }
                else
                {
                    reflections[reflection].delay[ear].setTarget (
                        targetReflectionDelay[reflection][ear]);
                    reflections[reflection].gain[ear].setTarget (
                        targetReflectionGain[reflection][ear]);
                }
            }

            if (snap)
                reflections[reflection].filterCoefficient.snap (
                    targetFilterCoefficient[reflection]);
            else
                reflections[reflection].filterCoefficient.setTarget (
                    targetFilterCoefficient[reflection]);
        }

        // Higher Definition leaves the explicit wall arrivals exposed for longer. The
        // existing diffuser supplies the continuous buildup after this bounded delay.
        const float lateOnsetMs = 6.0f + 18.0f * settings.definition
                                + 4.0f * settings.space;
        targetLateExcitationDelay = detail::clamp (
            preDelaySamples + lateOnsetMs * 0.001f * sr,
            1.0f, static_cast<float> (maxHistoryDelaySamples - 8));
        if (snap) lateExcitationDelay.snap (targetLateExcitationDelay);
        else lateExcitationDelay.setTarget (targetLateExcitationDelay);
    }

    void clearSignalState() noexcept
    {
        midHistory.reset();
        sideHistory.reset();
        earlySpreadA.reset();
        earlySpreadB.reset();
        earlySpreadHighpassState = 0.0f;
        earlySpreadLowpassState = 0.0f;
        for (auto& reflection : reflections)
            for (auto& ear : reflection.filterState)
                for (float& state : ear)
                    state = 0.0f;
        std::fill (dryLeft.begin(), dryLeft.end(), 0.0f);
        std::fill (dryRight.begin(), dryRight.end(), 0.0f);
        std::fill (earlyLeft.begin(), earlyLeft.end(), 0.0f);
        std::fill (earlyRight.begin(), earlyRight.end(), 0.0f);
        std::fill (lateInputLeft.begin(), lateInputLeft.end(), 0.0f);
        std::fill (lateInputRight.begin(), lateInputRight.end(), 0.0f);
        std::fill (lateOutputLeft.begin(), lateOutputLeft.end(), 0.0f);
        std::fill (lateOutputRight.begin(), lateOutputRight.end(), 0.0f);
    }

    void processChunk (const float* inputLeft, const float* inputRight,
                       float* outputLeft, float* outputRight, int count) noexcept
    {
        float earlyTarget = 1.0f, lateTarget = 1.0f;
        auditionTargets (earlyTarget, lateTarget);
        earlyAuditionGain.setTarget (earlyTarget);
        lateAuditionGain.setTarget (lateTarget);

        for (int sample = 0; sample < count; ++sample)
        {
            const float left = inputLeft[sample];
            const float right = inputRight[sample];
            dryLeft[static_cast<std::size_t> (sample)] = left;
            dryRight[static_cast<std::size_t> (sample)] = right;

            const float mid = 0.5f * (left + right);
            const float side = 0.5f * (left - right) * wetStereoAmount.next();
            midHistory.push (mid);
            sideHistory.push (side);

            const float lateDelay = lateExcitationDelay.next();
            const float delayedMid = midHistory.read (lateDelay);
            const float delayedSide = sideHistory.read (lateDelay);
            lateInputLeft[static_cast<std::size_t> (sample)] = delayedMid + delayedSide;
            lateInputRight[static_cast<std::size_t> (sample)] = delayedMid - delayedSide;

            float erLeft = 0.0f;
            float erRight = 0.0f;
            for (int reflection = 0; reflection < reflectionCount; ++reflection)
            {
                auto& state = reflections[reflection];
                const float coefficient = detail::clamp (
                    state.filterCoefficient.next(), 0.0f, 1.0f);

                for (int ear = 0; ear < 2; ++ear)
                {
                    const float delay = state.delay[ear].next();
                    const float midTap = midHistory.read (delay);
                    const float sideTap = sideHistory.read (delay);
                    float& filteredMid = state.filterState[ear][0];
                    float& filteredSide = state.filterState[ear][1];
                    filteredMid += coefficient * (midTap - filteredMid);
                    filteredSide += coefficient * (sideTap - filteredSide);
                    const float gain = state.gain[ear].next();

                    if (ear == 0)
                        erLeft += gain * (filteredMid + earlySideRetention * filteredSide);
                    else
                        erRight += gain * (filteredMid - earlySideRetention * filteredSide);
                }
            }

            const float earlyMid = 0.5f * (erLeft + erRight);
            float earlySide = 0.5f * (erLeft - erRight);
            const float spreadDifference = 0.5f * (earlySpreadA.process (earlyMid)
                                                   - earlySpreadB.process (earlyMid));
            earlySpreadHighpassState += earlySpreadHighpassCoefficient
                                      * (spreadDifference - earlySpreadHighpassState);
            const float highpassed = spreadDifference - earlySpreadHighpassState;
            earlySpreadLowpassState += earlySpreadLowpassCoefficient
                                     * (highpassed - earlySpreadLowpassState);
            earlySide += earlySpreadAmount * earlySpreadLowpassState;

            detail::reconstructMidSide (
                earlyMid, earlySide,
                earlyLeft[static_cast<std::size_t> (sample)],
                earlyRight[static_cast<std::size_t> (sample)]);
        }

        // The embedded core is permanently 100% wet. It continues processing even when
        // final Mix is zero, so later automation cannot release a stale frozen tail.
        lateCore.process (lateInputLeft.data(), lateInputRight.data(),
                          lateOutputLeft.data(), lateOutputRight.data(), count);

        for (int sample = 0; sample < count; ++sample)
        {
            const auto index = static_cast<std::size_t> (sample);
            const float wet = mixSmoother.next();
            const float earlyGain = earlyAuditionGain.next();
            const float lateGain = lateAuditionGain.next();
            const float selectedLeft = earlyLeft[index] * earlyGain
                                     + lateOutputLeft[index] * lateGain;
            const float selectedRight = earlyRight[index] * earlyGain
                                      + lateOutputRight[index] * lateGain;
            if (wet == 0.0f)
            {
                outputLeft[sample] = dryLeft[index];
                outputRight[sample] = dryRight[index];
            }
            else if (wet == 1.0f)
            {
                outputLeft[sample] = selectedLeft;
                outputRight[sample] = selectedRight;
            }
            else
            {
                const float dry = 1.0f - wet;
                outputLeft[sample] = dryLeft[index] * dry + selectedLeft * wet;
                outputRight[sample] = dryRight[index] * dry + selectedRight * wet;
            }
        }
    }

    ReverbCore lateCore;
    nekospace::dsp::FractionalDelay midHistory, sideHistory;
    std::array<ReflectionState, reflectionCount> reflections;
    detail::AllpassDiffuser earlySpreadA, earlySpreadB;
    DelaySmoother lateExcitationDelay;
    detail::LinearSmoother earlyAuditionGain, lateAuditionGain, wetStereoAmount, mixSmoother;

    std::vector<float> dryLeft, dryRight, earlyLeft, earlyRight;
    std::vector<float> lateInputLeft, lateInputRight, lateOutputLeft, lateOutputRight;

    float targetReflectionDelay[reflectionCount][2] = {};
    float targetReflectionGain[reflectionCount][2] = {};
    float targetFilterCoefficient[reflectionCount] = {};
    float targetLateExcitationDelay = 1.0f;
    float earlySpreadHighpassCoefficient = 0.0f;
    float earlySpreadLowpassCoefficient = 0.0f;
    float earlySpreadHighpassState = 0.0f;
    float earlySpreadLowpassState = 0.0f;
    RoomBodySettings settings;
    std::atomic<int> auditionMode { static_cast<int> (RoomBodyAuditionMode::roomBody) };
    float sr = 48000.0f;
    int preparedBlockSize = 1;
    int maxHistoryDelaySamples = 16;
    bool prepared = false;
};
} // namespace nsr
