// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "dsp/ReverbCore.h"

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

float energy (const std::vector<float>& values)
{
    float sum = 0.0f;
    for (float value : values) sum += value * value;
    return sum;
}
} // namespace

int main()
{
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
