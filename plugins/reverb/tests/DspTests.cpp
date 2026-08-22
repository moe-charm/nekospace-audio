// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "dsp/ReverbCore.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

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

    if (failures == 0) std::cout << "Reverb DSP tests passed\n";
    return failures == 0 ? 0 : 1;
}
