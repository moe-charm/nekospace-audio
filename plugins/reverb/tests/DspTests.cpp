// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "dsp/ReverbCore.h"
#include "dsp/RoomBodyCore.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <vector>

namespace
{
std::atomic<std::size_t> allocations { 0 };
}

void* operator new (std::size_t size)
{
    ++allocations;
    if (void* memory = std::malloc (size)) return memory;
    throw std::bad_alloc();
}
void* operator new[] (std::size_t size) { return ::operator new (size); }
void operator delete (void* memory) noexcept { std::free (memory); }
void operator delete[] (void* memory) noexcept { std::free (memory); }
void operator delete (void* memory, std::size_t) noexcept { std::free (memory); }
void operator delete[] (void* memory, std::size_t) noexcept { std::free (memory); }

namespace
{
int failures = 0;

void expect (bool condition, const char* message)
{
    if (! condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

struct Render
{
    std::vector<float> left, right;
};

Render render (const std::vector<float>& left, const std::vector<float>& right,
               float mix, int blockSize)
{
    nsr::ReverbCore core;
    core.prepare (48000.0, 512);
    nsr::ReverbSettings settings;
    settings.mix = mix;
    core.setSettings (settings);

    Render result { std::vector<float> (left.size()), std::vector<float> (right.size()) };
    for (std::size_t position = 0; position < left.size(); position += static_cast<std::size_t> (blockSize))
    {
        const int count = static_cast<int> (std::min (static_cast<std::size_t> (blockSize),
                                                      left.size() - position));
        core.process (left.data() + position, right.data() + position,
                      result.left.data() + position, result.right.data() + position, count);
    }
    return result;
}

Render renderRoomBody (const std::vector<float>& left, const std::vector<float>& right,
                       const nsr::RoomBodySettings& settings, int blockSize,
                       bool roomBodyEnabled = true)
{
    nsr::RoomBodyCore core;
    core.setRoomBodyEnabled (roomBodyEnabled);
    core.prepare (48000.0, 512, settings);

    Render result { std::vector<float> (left.size()), std::vector<float> (right.size()) };
    for (std::size_t position = 0; position < left.size();
         position += static_cast<std::size_t> (blockSize))
    {
        const int count = static_cast<int> (std::min (static_cast<std::size_t> (blockSize),
                                                      left.size() - position));
        core.process (left.data() + position, right.data() + position,
                      result.left.data() + position, result.right.data() + position, count);
    }
    return result;
}

float energy (const std::vector<float>& values)
{
    float sum = 0.0f;
    for (float value : values) sum += value * value;
    return sum;
}

void testRoomBodyTargets()
{
    constexpr float sampleRate = 48000.0f;
    nsr::RoomBodySettings settings;
    settings.preDelayMs = 0.0f;
    nsr::RoomBodyCore core;
    core.prepare (sampleRate, 127, settings);

    bool valid = true;
    for (int reflection = 0; reflection < nsr::RoomBodyCore::reflectionCount; ++reflection)
        for (int ear = 0; ear < 2; ++ear)
        {
            const float delay = core.targetReflectionDelaySamples (reflection, ear);
            valid = valid && std::isfinite (delay) && delay > 0.0f
                    && delay < sampleRate * 0.40f;
        }
    expect (valid, "all six room-body reflections have finite prepared delays");

    const auto delay = [&core] (int reflection, int ear)
    {
        return core.targetReflectionDelaySamples (reflection, ear);
    };
    constexpr float targetTolerance = 0.01f;
    expect (std::abs (delay (nsr::RoomBodyCore::leftWall, 0)
                      - delay (nsr::RoomBodyCore::rightWall, 1)) < targetTolerance
            && std::abs (delay (nsr::RoomBodyCore::leftWall, 1)
                         - delay (nsr::RoomBodyCore::rightWall, 0)) < targetTolerance,
            "left/right wall target delays mirror across the two ears");
    bool centredImagesAreSymmetric = true;
    for (int reflection : { nsr::RoomBodyCore::frontWall, nsr::RoomBodyCore::backWall,
                            nsr::RoomBodyCore::floor, nsr::RoomBodyCore::ceiling })
        centredImagesAreSymmetric = centredImagesAreSymmetric
            && std::abs (delay (reflection, 0) - delay (reflection, 1)) < targetTolerance;
    expect (centredImagesAreSymmetric,
            "centred wall, floor and ceiling images have mirrored ear delays");

    float withoutPreDelay[nsr::RoomBodyCore::reflectionCount][2] = {};
    for (int reflection = 0; reflection < nsr::RoomBodyCore::reflectionCount; ++reflection)
        for (int ear = 0; ear < 2; ++ear)
            withoutPreDelay[reflection][ear] = delay (reflection, ear);
    const float lateWithoutPreDelay = core.targetLateExcitationDelaySamples();
    settings.preDelayMs = 120.0f;
    core.setSettings (settings);
    bool preDelayShiftedAllWetTargets = true;
    constexpr float expectedPreDelayShift = 0.120f * sampleRate;
    for (int reflection = 0; reflection < nsr::RoomBodyCore::reflectionCount; ++reflection)
        for (int ear = 0; ear < 2; ++ear)
            preDelayShiftedAllWetTargets = preDelayShiftedAllWetTargets
                && std::abs ((delay (reflection, ear) - withoutPreDelay[reflection][ear])
                             - expectedPreDelayShift) < targetTolerance;
    preDelayShiftedAllWetTargets = preDelayShiftedAllWetTargets
        && std::abs ((core.targetLateExcitationDelaySamples() - lateWithoutPreDelay)
                     - expectedPreDelayShift) < targetTolerance;
    expect (preDelayShiftedAllWetTargets,
            "120 ms pre-delay shifts every early and late target by exactly 5760 samples");

    settings.preDelayMs = 0.0f;
    settings.distance = 0.0f;
    core.setSettings (settings);
    const float nearFront = delay (nsr::RoomBodyCore::frontWall, 0);
    const float nearBack = delay (nsr::RoomBodyCore::backWall, 0);
    settings.distance = 1.0f;
    core.setSettings (settings);
    expect (delay (nsr::RoomBodyCore::frontWall, 0) < nearFront
            && delay (nsr::RoomBodyCore::backWall, 0) > nearBack,
            "Distance moves the hidden wet source toward front and away from back images");

    settings.definition = 0.0f;
    core.setSettings (settings);
    const float blendedLateTarget = core.targetLateExcitationDelaySamples();
    settings.definition = 1.0f;
    core.setSettings (settings);
    constexpr float expectedDefinitionShift = 0.016f * sampleRate;
    expect (std::abs ((core.targetLateExcitationDelaySamples() - blendedLateTarget)
                      - expectedDefinitionShift) < targetTolerance,
            "Definition moves the late excitation target across its documented 16 ms range");
}

void testRoomBodyActualEarlyArrival()
{
    constexpr float sampleRate = 48000.0f;
    nsr::RoomBodySettings settings;
    settings.preDelayMs = 0.0f;
    settings.mix = 1.0f;

    nsr::RoomBodyCore withBody, tailOnly;
    tailOnly.setRoomBodyEnabled (false);
    withBody.prepare (sampleRate, 127, settings);
    tailOnly.prepare (sampleRate, 127, settings);

    float minimumTarget = sampleRate;
    for (int reflection = 0; reflection < nsr::RoomBodyCore::reflectionCount; ++reflection)
        for (int ear = 0; ear < 2; ++ear)
            minimumTarget = std::min (
                minimumTarget, withBody.targetReflectionDelaySamples (reflection, ear));

    const int renderSamples = static_cast<int> (std::ceil (minimumTarget)) + 32;
    std::vector<float> inputLeft (static_cast<std::size_t> (renderSamples), 0.0f);
    std::vector<float> inputRight (static_cast<std::size_t> (renderSamples), 0.0f);
    std::vector<float> bodyLeft (static_cast<std::size_t> (renderSamples));
    std::vector<float> bodyRight (static_cast<std::size_t> (renderSamples));
    std::vector<float> tailLeft (static_cast<std::size_t> (renderSamples));
    std::vector<float> tailRight (static_cast<std::size_t> (renderSamples));
    inputLeft[0] = inputRight[0] = 1.0f;

    withBody.process (inputLeft.data(), inputRight.data(), bodyLeft.data(), bodyRight.data(),
                      renderSamples);
    tailOnly.process (inputLeft.data(), inputRight.data(), tailLeft.data(), tailRight.data(),
                      renderSamples);

    int firstArrival = -1;
    constexpr float detectionFloor = 1.0e-9f;
    for (int sample = 0; sample < renderSamples; ++sample)
    {
        const auto index = static_cast<std::size_t> (sample);
        const float isolatedEarly = std::max (std::abs (bodyLeft[index] - tailLeft[index]),
                                              std::abs (bodyRight[index] - tailRight[index]));
        if (isolatedEarly > detectionFloor)
        {
            firstArrival = sample;
            break;
        }
    }

    // Four-point Hermite interpolation may expose one pre-ring sample before floor(delay).
    constexpr float fractionalDelayTolerance = 2.0f;
    expect (firstArrival >= 0
            && std::abs (static_cast<float> (firstArrival) - minimumTarget)
                   <= fractionalDelayTolerance,
            "isolated early-field first arrival follows the minimum physical delay target");
    std::cout << "RoomBody isolated ER first arrival: target=" << minimumTarget
              << " samples, measured=" << firstArrival << " samples\n";
}

void testRoomBodyLargeDelayAutomation()
{
    constexpr float sampleRate = 48000.0f;
    constexpr int blockSize = 127;
    constexpr int blocks = 700;
    constexpr int changeBlock = 200;
    constexpr float inputAmplitude = 0.1f;
    constexpr float frequencyHz = 997.0f;
    constexpr float twoPi = 6.28318530717958647692f;

    nsr::RoomBodySettings settings;
    settings.space = 0.0f;
    settings.distance = 0.0f;
    settings.definition = 0.0f;
    settings.preDelayMs = 0.0f;
    settings.mix = 1.0f;
    nsr::RoomBodyCore core;
    core.prepare (sampleRate, blockSize, settings);

    float inputLeft[blockSize] = {}, inputRight[blockSize] = {};
    float outputLeft[blockSize] = {}, outputRight[blockSize] = {};
    float previousLeft = 0.0f, previousRight = 0.0f;
    float maximumAutomationStep = 0.0f, maximumPeak = 0.0f;
    bool finite = true;
    int absoluteSample = 0;

    for (int block = 0; block < blocks; ++block)
    {
        if (block == changeBlock)
        {
            settings.space = 1.0f;
            settings.decaySeconds = 4.0f;
            settings.bassTailRatio = 2.0f;
            settings.airTailRatio = 0.25f;
            settings.distance = 1.0f;
            settings.definition = 1.0f;
            settings.preDelayMs = 120.0f;
            core.setSettings (settings);
        }

        for (int sample = 0; sample < blockSize; ++sample, ++absoluteSample)
        {
            const float input = inputAmplitude * std::sin (
                twoPi * frequencyHz * static_cast<float> (absoluteSample) / sampleRate);
            inputLeft[sample] = inputRight[sample] = input;
        }
        core.process (inputLeft, inputRight, outputLeft, outputRight, blockSize);

        for (int sample = 0; sample < blockSize; ++sample)
        {
            finite = finite && std::isfinite (outputLeft[sample])
                            && std::isfinite (outputRight[sample]);
            maximumPeak = std::max ({ maximumPeak, std::abs (outputLeft[sample]),
                                      std::abs (outputRight[sample]) });
            if (block >= changeBlock)
                maximumAutomationStep = std::max ({
                    maximumAutomationStep,
                    std::abs (outputLeft[sample] - previousLeft),
                    std::abs (outputRight[sample] - previousRight) });
            previousLeft = outputLeft[sample];
            previousRight = outputRight[sample];
        }
    }

    // A 997 Hz, 0.1-peak input steps by at most 0.0131. The 0.06 ceiling leaves over
    // 4.5x margin for the summed room while still rejecting a click-sized discontinuity.
    constexpr float maximumAllowedStep = 0.06f;
    expect (finite, "large RoomBody delay automation remains finite on a sustained sine");
    expect (maximumAutomationStep < maximumAllowedStep,
            "large RoomBody delay automation stays below the conservative step ceiling");
    std::cout << "RoomBody large-delay automation: max step=" << maximumAutomationStep
              << ", peak=" << maximumPeak << ", limit=" << maximumAllowedStep << '\n';
}

void testRoomBodyExtremePeak()
{
    constexpr float sampleRate = 48000.0f;
    constexpr int blockSize = 127;
    constexpr int renderSamples = static_cast<int> (sampleRate * 4.0f);
    nsr::RoomBodySettings settings;
    settings.space = 0.0f;
    settings.decaySeconds = 4.0f;
    settings.bassTailRatio = 2.0f;
    settings.airTailRatio = 2.0f;
    settings.distance = 0.0f;
    settings.definition = 1.0f;
    settings.preDelayMs = 0.0f;
    settings.mix = 1.0f;

    nsr::RoomBodyCore core;
    core.prepare (sampleRate, blockSize, settings);
    float inputLeft[blockSize] = {}, inputRight[blockSize] = {};
    float outputLeft[blockSize] = {}, outputRight[blockSize] = {};
    inputLeft[0] = inputRight[0] = 1.0f;
    float maximumPeak = 0.0f;
    bool finite = true;

    for (int position = 0; position < renderSamples; position += blockSize)
    {
        const int count = std::min (blockSize, renderSamples - position);
        core.process (inputLeft, inputRight, outputLeft, outputRight, count);
        inputLeft[0] = inputRight[0] = 0.0f;
        for (int sample = 0; sample < count; ++sample)
        {
            finite = finite && std::isfinite (outputLeft[sample])
                            && std::isfinite (outputRight[sample]);
            maximumPeak = std::max ({ maximumPeak, std::abs (outputLeft[sample]),
                                      std::abs (outputRight[sample]) });
        }
    }

    // This is an engineering runaway/headroom guard, not a limiter. The deliberately
    // worst-gain unit impulse should retain substantial margin below full-scale peak.
    constexpr float maximumAllowedPeak = 1.0f;
    expect (finite && maximumPeak > 0.0f,
            "extreme RoomBody plus tail produces a finite non-zero impulse response");
    expect (maximumPeak < maximumAllowedPeak,
            "extreme RoomBody plus tail remains below the conservative peak ceiling");
    std::cout << "RoomBody extreme ER+tail impulse peak=" << maximumPeak
              << ", limit=" << maximumAllowedPeak << '\n';
}

void testRoomBodySignalInvariants()
{
    constexpr std::size_t samples = 12000;
    nsr::RoomBodySettings settings;
    settings.mix = 1.0f;

    std::vector<float> monoLeft (samples, 0.0f), monoRight (samples, 0.0f);
    monoLeft[0] = monoRight[0] = 1.0f;
    const auto mono = renderRoomBody (monoLeft, monoRight, settings, 127);
    expect (mono.left == mono.right,
            "full room-body core keeps duplicated mono exactly symmetric");
    expect (energy (mono.left) > 0.0f, "full room-body mono impulse produces wet output");

    std::vector<float> sideLeft (samples, 0.0f), sideRight (samples, 0.0f);
    sideLeft[0] = 1.0f;
    sideRight[0] = -1.0f;
    const auto side = renderRoomBody (sideLeft, sideRight, settings, 127);
    bool antiSymmetric = energy (side.left) > 0.0f;
    for (std::size_t i = 0; i < samples; ++i)
        antiSymmetric = antiSymmetric && side.left[i] == -side.right[i];
    expect (antiSymmetric, "full room-body core preserves a non-zero pure Side field");

    std::vector<float> hardLeft (samples, 0.0f), silentRight (samples, 0.0f);
    hardLeft[0] = 1.0f;
    const auto panned = renderRoomBody (hardLeft, silentRight, settings, 127);
    expect (energy (panned.right) > 1.0e-8f,
            "a hard-left source reaches the opposite ear through room reflections");

    const auto block1 = renderRoomBody (monoLeft, monoRight, settings, 1);
    const auto block512 = renderRoomBody (monoLeft, monoRight, settings, 512);
    expect (block1.left == block512.left && block1.right == block512.right,
            "static room-body processing is block-size invariant");

    nsr::RoomBodySettings hiddenSettings = settings;
    hiddenSettings.mix = 0.0f;
    nsr::RoomBodyCore hidden, audible;
    hidden.prepare (48000.0, 127, hiddenSettings);
    audible.prepare (48000.0, 127, settings);
    constexpr int chargeSamples = 8192;
    std::vector<float> chargeLeft (chargeSamples, 0.0f), chargeRight (chargeSamples, 0.0f);
    std::vector<float> hiddenLeft (chargeSamples), hiddenRight (chargeSamples);
    std::vector<float> audibleLeft (chargeSamples), audibleRight (chargeSamples);
    chargeLeft[0] = chargeRight[0] = 1.0f;
    hidden.process (chargeLeft.data(), chargeRight.data(), hiddenLeft.data(), hiddenRight.data(),
                    chargeSamples);
    audible.process (chargeLeft.data(), chargeRight.data(), audibleLeft.data(), audibleRight.data(),
                     chargeSamples);
    expect (hiddenLeft == chargeLeft && hiddenRight == chargeRight,
            "RoomBody Mix zero is an exact dry identity");

    hidden.setSettings (settings);
    constexpr int releaseSamples = 4096;
    std::vector<float> silence (releaseSamples, 0.0f);
    std::vector<float> hiddenReleaseLeft (releaseSamples), hiddenReleaseRight (releaseSamples);
    std::vector<float> audibleReleaseLeft (releaseSamples), audibleReleaseRight (releaseSamples);
    hidden.process (silence.data(), silence.data(), hiddenReleaseLeft.data(),
                    hiddenReleaseRight.data(), releaseSamples);
    audible.process (silence.data(), silence.data(), audibleReleaseLeft.data(),
                     audibleReleaseRight.data(), releaseSamples);
    expect (energy (hiddenReleaseLeft) > 0.0f,
            "Mix zero advances the room and reveals an active charged tail when restored");
    bool settledStateMatches = true;
    constexpr int mixRampSamples = 2400; // 50 ms at 48 kHz.
    for (int i = mixRampSamples; i < releaseSamples; ++i)
        settledStateMatches = settledStateMatches
            && hiddenReleaseLeft[static_cast<std::size_t> (i)]
                   == audibleReleaseLeft[static_cast<std::size_t> (i)]
            && hiddenReleaseRight[static_cast<std::size_t> (i)]
                   == audibleReleaseRight[static_cast<std::size_t> (i)];
    expect (settledStateMatches,
            "after the Mix ramp, hidden and audible processing reveal the same room state");
}

void testRoomBodyRealtimeSafety()
{
    for (double sampleRate : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        nsr::RoomBodySettings settings;
        settings.space = 1.0f;
        settings.decaySeconds = 4.0f;
        settings.bassTailRatio = 2.0f;
        settings.airTailRatio = 0.25f;
        settings.distance = 1.0f;
        settings.definition = 1.0f;
        settings.preDelayMs = 120.0f;
        settings.mix = 1.0f;
        nsr::RoomBodyCore core;
        core.prepare (sampleRate, 127, settings);
        float inputLeft[127] = {}, inputRight[127] = {};
        float outputLeft[127] = {}, outputRight[127] = {};
        inputLeft[0] = inputRight[0] = 1.0f;
        const auto before = allocations.load();
        bool finite = true;
        const int blocks = static_cast<int> (std::ceil (sampleRate * 0.25 / 127.0));
        for (int block = 0; block < blocks; ++block)
        {
            if (block == 8)
            {
                settings.space = 0.0f;
                settings.decaySeconds = 0.15f;
                settings.bassTailRatio = 0.25f;
                settings.airTailRatio = 2.0f;
                settings.distance = 0.0f;
                settings.definition = 0.0f;
                settings.preDelayMs = 0.0f;
                core.setSettings (settings);
            }
            if (block == 16) core.setRoomBodyEnabled (false);
            if (block == 28) core.setRoomBodyEnabled (true);
            core.process (inputLeft, inputRight, outputLeft, outputRight, 127);
            inputLeft[0] = inputRight[0] = 0.0f;
            for (int i = 0; i < 127; ++i)
                finite = finite && std::isfinite (outputLeft[i]) && std::isfinite (outputRight[i]);
        }
        expect (allocations.load() == before,
                "RoomBody settings, audition switch and processing must not allocate");
        expect (finite, "RoomBody remains finite at every supported sample rate");
    }
}

void testRoomBodyAuditionTransition()
{
    nsr::RoomBodySettings settings;
    settings.mix = 1.0f;
    nsr::RoomBodyCore switched, reference;
    switched.prepare (48000.0, 127, settings);
    reference.prepare (48000.0, 127, settings);

    float input[127], switchedLeft[127], switchedRight[127], referenceLeft[127], referenceRight[127];
    std::fill (std::begin (input), std::end (input), 0.01f);
    for (int block = 0; block < 120; ++block)
    {
        switched.process (input, input, switchedLeft, switchedRight, 127);
        reference.process (input, input, referenceLeft, referenceRight, 127);
    }

    switched.setRoomBodyEnabled (false);
    constexpr int transitionSamples = 3000;
    std::vector<float> transitionInput (transitionSamples, 0.01f);
    std::vector<float> switchedTransitionLeft (transitionSamples), switchedTransitionRight (transitionSamples);
    std::vector<float> referenceTransitionLeft (transitionSamples), referenceTransitionRight (transitionSamples);
    switched.process (transitionInput.data(), transitionInput.data(), switchedTransitionLeft.data(),
                      switchedTransitionRight.data(), transitionSamples);
    reference.process (transitionInput.data(), transitionInput.data(), referenceTransitionLeft.data(),
                       referenceTransitionRight.data(), transitionSamples);

    bool finite = true;
    float maximumDifference = 0.0f;
    float maximumDifferenceStep = 0.0f;
    float previousDifference = 0.0f;
    for (int sample = 0; sample < transitionSamples; ++sample)
    {
        const float difference = switchedTransitionLeft[static_cast<std::size_t> (sample)]
                               - referenceTransitionLeft[static_cast<std::size_t> (sample)];
        finite = finite && std::isfinite (switchedTransitionLeft[static_cast<std::size_t> (sample)])
                        && std::isfinite (switchedTransitionRight[static_cast<std::size_t> (sample)]);
        maximumDifference = std::max (maximumDifference, std::abs (difference));
        maximumDifferenceStep = std::max (maximumDifferenceStep,
                                          std::abs (difference - previousDifference));
        previousDifference = difference;
    }
    const float firstDifference = std::abs (switchedTransitionLeft[0]
                                            - referenceTransitionLeft[0]);
    expect (finite && maximumDifference > 1.0e-6f,
            "RoomBody audition transition stays finite and removes a real early field");
    expect (firstDifference < maximumDifference * 0.02f + 1.0e-7f
            && maximumDifferenceStep < maximumDifference * 0.02f + 1.0e-7f,
            "RoomBody audition change is bounded by the 50 ms smoother rather than hard-switched");
}
} // namespace

int main()
{
    testRoomBodyTargets();
    testRoomBodyActualEarlyArrival();
    testRoomBodyLargeDelayAutomation();
    testRoomBodyExtremePeak();
    testRoomBodySignalInvariants();
    testRoomBodyRealtimeSafety();
    testRoomBodyAuditionTransition();

    constexpr std::size_t samples = 48000;
    std::vector<float> monoLeft (samples, 0.0f), monoRight (samples, 0.0f);
    monoLeft[0] = monoRight[0] = 1.0f;
    const auto mono = render (monoLeft, monoRight, 1.0f, 127);
    expect (mono.left == mono.right, "mono wet output must be exactly left/right symmetric");
    expect (energy (mono.left) > 0.0f, "mono excitation must produce a tail");

    std::vector<float> sideLeft (samples, 0.0f), sideRight (samples, 0.0f);
    sideLeft[0] = 1.0f;
    sideRight[0] = -1.0f;
    const auto side = render (sideLeft, sideRight, 1.0f, 127);
    expect (energy (side.left) > 0.0f, "pure Side excitation must not collapse");
    bool antiSymmetric = true;
    for (std::size_t i = 0; i < samples; ++i)
        antiSymmetric = antiSymmetric && side.left[i] == -side.right[i];
    expect (antiSymmetric, "pure Side wet output must remain antisymmetric");

    std::vector<float> arbitraryLeft (4099), arbitraryRight (4099);
    for (std::size_t i = 0; i < arbitraryLeft.size(); ++i)
    {
        arbitraryLeft[i] = std::sin (static_cast<float> (i) * 0.017f) * 0.37f;
        arbitraryRight[i] = std::cos (static_cast<float> (i) * 0.013f) * 0.29f;
    }
    const auto dry = render (arbitraryLeft, arbitraryRight, 0.0f, 113);
    expect (dry.left == arbitraryLeft && dry.right == arbitraryRight,
            "zero mix must be an exact dry identity");

    const auto block1 = render (monoLeft, monoRight, 1.0f, 1);
    const auto block512 = render (monoLeft, monoRight, 1.0f, 512);
    expect (block1.left == block512.left && block1.right == block512.right,
            "render must be deterministic and block-size invariant");

    for (double sampleRate : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        nsr::ReverbSettings extreme;
        extreme.space = 1.0f;
        extreme.decaySeconds = 4.0f;
        extreme.bassTailRatio = 2.0f;
        extreme.airTailRatio = 0.25f;
        nsr::ReverbCore core;
        core.prepare (sampleRate, 127, extreme);
        float inputLeft[127] = {}, inputRight[127] = {}, outputLeft[127] = {}, outputRight[127] = {};
        inputLeft[0] = inputRight[0] = 1.0f;
        const auto before = allocations.load();
        bool finite = true;
        const int blocks = static_cast<int> (std::ceil (sampleRate * 5.0 / 127.0));
        for (int block = 0; block < blocks; ++block)
        {
            core.process (inputLeft, inputRight, outputLeft, outputRight, 127);
            inputLeft[0] = inputRight[0] = 0.0f;
            for (int i = 0; i < 127; ++i)
                finite = finite && std::isfinite (outputLeft[i]) && std::isfinite (outputRight[i]);
            if (block == 50)
            {
                extreme.space = 0.0f;
                extreme.decaySeconds = 0.15f;
                extreme.bassTailRatio = 0.25f;
                extreme.airTailRatio = 2.0f;
                core.setSettings (extreme);
            }
        }
        expect (allocations.load() == before, "processing and coefficient updates must not allocate");
        expect (finite, "extreme settings and automation remain finite at every sample rate");
    }

    {
        nsr::ReverbCore core;
        nsr::ReverbSettings initial;
        initial.mix = 1.0f;
        core.prepare (48000.0, 127, initial);
        float inputLeft[127] = {}, inputRight[127] = {}, outputLeft[127] = {}, outputRight[127] = {};
        float previousLeft = 0.0f, previousRight = 0.0f, maximumStep = 0.0f;
        int sample = 0;
        for (int block = 0; block < 48000 / 127 + 1; ++block)
        {
            for (int i = 0; i < 127; ++i, ++sample)
                inputLeft[i] = inputRight[i] = 0.1f * std::sin (0.031f * static_cast<float> (sample));
            if (block == 190)
            {
                auto changed = initial;
                changed.space = 1.0f;
                changed.decaySeconds = 4.0f;
                changed.bassTailRatio = 2.0f;
                changed.airTailRatio = 0.25f;
                core.setSettings (changed);
            }
            core.process (inputLeft, inputRight, outputLeft, outputRight, 127);
            for (int i = 0; i < 127; ++i)
            {
                maximumStep = std::max ({ maximumStep, std::abs (outputLeft[i] - previousLeft),
                                          std::abs (outputRight[i] - previousRight) });
                previousLeft = outputLeft[i];
                previousRight = outputRight[i];
            }
        }
        expect (maximumStep < 0.05f, "extreme decay update is smoothed without a click-sized step");
    }

    if (failures == 0) std::cout << "Reverb DSP tests passed\n";
    return failures == 0 ? 0 : 1;
}
