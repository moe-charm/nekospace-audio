// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "analysis/Analyzer.h"
#include "analysis/CoreRenderer.h"

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>

namespace
{
struct BenchResult { double milliseconds = 0.0; double realtimePercent = 0.0; };

void printMidDecay (const nsr::analysis::AnalysisResult& result)
{
    for (const auto& band : result.decayBands)
        if (band.centerHz == 1000.0 && band.t20.valid)
            std::cout << "  1 kHz T20: " << band.t20.t60Seconds
                      << " (R2 " << band.t20.rSquared << ")\n";
}

template<typename Core>
BenchResult benchmark (int diffuserStages)
{
    constexpr int blockSize = 64;
    constexpr int blocks = 7500; // ten seconds at 48 kHz
    nsr::ReverbSettings settings;
    settings.mix = 1.0f;
    nsr::ReverbConfiguration configuration;
    configuration.inputDiffuserStages = diffuserStages;
    Core core;
    core.prepare (48000.0, blockSize, settings, configuration);
    float left[blockSize] = {}, right[blockSize] = {};
    float outLeft[blockSize] = {}, outRight[blockSize] = {};
    for (int i = 0; i < blockSize; ++i)
    {
        left[i] = 0.1f * std::sin (0.17f * static_cast<float> (i));
        right[i] = 0.1f * std::cos (0.13f * static_cast<float> (i));
    }
    const auto start = std::chrono::steady_clock::now();
    for (int block = 0; block < blocks; ++block)
        core.process (left, right, outLeft, outRight, blockSize);
    const auto end = std::chrono::steady_clock::now();
    const double milliseconds = std::chrono::duration<double, std::milli> (end - start).count();
    return { milliseconds, milliseconds / 10000.0 * 100.0 };
}

template<typename Core>
void report (const char* name, int diffuserStages, bool sixteen)
{
    nsr::analysis::CoreRenderSettings settings;
    settings.durationSeconds = 6.0;
    settings.blockSize = 64;
    settings.reverb.decaySeconds = 1.4f;
    settings.reverb.bassTailRatio = 1.0f;
    settings.reverb.airTailRatio = 1.0f;
    settings.configuration.inputDiffuserStages = diffuserStages;
    const auto ir = sixteen ? nsr::analysis::renderCoreImpulse (settings)
                            : nsr::analysis::renderCore8Impulse (settings);
    const auto result = nsr::analysis::analyze (ir);
    const auto cpu = benchmark<Core> (diffuserStages);
    std::cout << name << '\n'
              << "  NED t90 ms: " << result.nedT90Seconds * 1000.0 << '\n'
              << "  autocorrelation: " << result.maxAutocorrelation << '\n'
              << "  peak: " << result.peak << '\n'
              << "  RMS: " << result.rms << '\n'
              << "  CPU 10 s render ms: " << cpu.milliseconds << '\n'
              << "  CPU realtime percent: " << cpu.realtimePercent << '\n';
    printMidDecay (result);
}

} // namespace

int main()
{
    std::cout << std::fixed << std::setprecision (6);
    report<nsr::ReverbCore8> ("8-line / no diffuser", 0, false);
    report<nsr::ReverbCore8> ("8-line / 2-stage diffuser", 2, false);
    report<nsr::ReverbCore8> ("8-line / 4-stage diffuser", 4, false);
    report<nsr::ReverbCore> ("16-line / 4-stage diffuser", 4, true);
}
