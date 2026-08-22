// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <locale>
#include <stdexcept>
#include <string>

#include "analysis/Analyzer.h"
#include "analysis/FloatWavWriter.h"
#include "analysis/ReportWriter.h"

namespace
{
using nsr::analysis::BaselineSettings;

void printUsage()
{
    std::cout
        << "NekoSpace Reverb Phase 0 baseline analyzer\n\n"
        << "Usage: nekospace_reverb_analyze [options]\n\n"
        << "  --output PATH       report directory (default: reverb-analysis-output)\n"
        << "  --sample-rate HZ    8000..384000 (default: 48000)\n"
        << "  --duration SEC      0..60 (default: 6)\n"
        << "  --block-size N      1..16384 (default: 256)\n"
        << "  --size VALUE        0..1 (default: 0.35)\n"
        << "  --decay SEC         0.15..4 (default: 1.4)\n"
        << "  --damping VALUE     0..1 (default: 0)\n"
        << "  --help              show this text\n";
}

double parseDouble (const char* text, const char* option)
{
    char* end = nullptr;
    const double value = std::strtod (text, &end);
    if (end == text || *end != '\0' || ! std::isfinite (value))
        throw std::invalid_argument (std::string ("invalid value for ") + option);
    return value;
}

int parseInt (const char* text, const char* option)
{
    char* end = nullptr;
    const long value = std::strtol (text, &end, 10);
    if (end == text || *end != '\0' || value < 1 || value > 16384)
        throw std::invalid_argument (std::string ("invalid value for ") + option);
    return static_cast<int> (value);
}
} // namespace

int main (int argc, char** argv)
{
    try
    {
        std::locale::global (std::locale::classic());
        BaselineSettings settings;
        std::filesystem::path output = "reverb-analysis-output";

        for (int i = 1; i < argc; ++i)
        {
            const std::string option = argv[i];
            if (option == "--help") { printUsage(); return 0; }
            if (i + 1 >= argc) throw std::invalid_argument ("missing value for " + option);
            const char* value = argv[++i];
            if (option == "--output") output = std::filesystem::u8path (value);
            else if (option == "--sample-rate") settings.sampleRate = parseDouble (value, "--sample-rate");
            else if (option == "--duration") settings.durationSeconds = parseDouble (value, "--duration");
            else if (option == "--block-size") settings.blockSize = parseInt (value, "--block-size");
            else if (option == "--size") settings.size = static_cast<float> (parseDouble (value, "--size"));
            else if (option == "--decay") settings.decaySeconds = static_cast<float> (parseDouble (value, "--decay"));
            else if (option == "--damping") settings.damping = static_cast<float> (parseDouble (value, "--damping"));
            else throw std::invalid_argument ("unknown option: " + option);
        }

        if (settings.size < 0.0f || settings.size > 1.0f) throw std::invalid_argument ("--size must be 0..1");
        if (settings.decaySeconds < 0.15f || settings.decaySeconds > 4.0f) throw std::invalid_argument ("--decay must be 0.15..4");
        if (settings.damping < 0.0f || settings.damping > 1.0f) throw std::invalid_argument ("--damping must be 0..1");

        std::filesystem::create_directories (output);
        const auto ir = nsr::analysis::renderBaselineImpulse (settings);
        const auto result = nsr::analysis::analyze (ir);

        std::string error;
        auto require = [&error] (bool ok)
        {
            if (! ok) throw std::runtime_error (error);
        };
        require (nsr::analysis::writeFloatStereoWav (output / "baseline-ir.wav", ir, error));
        require (nsr::analysis::writeSummaryJson (output / "baseline-report.json", settings,
                                                   result, error));
        require (nsr::analysis::writeNedCsv (output / "baseline-ned.csv", result.ned, error));
        require (nsr::analysis::writeEdrCsv (output / "baseline-edr.csv", result.edr, error));
        require (nsr::analysis::writeSpectrumCsv (output / "baseline-spectrum.csv",
                                                   result.spectrum, error));
        require (nsr::analysis::writeAutocorrelationCsv (
            output / "baseline-autocorrelation.csv", result.autocorrelation, error));

        std::cout << "NekoSpace Reverb Phase 0 baseline\n"
                  << "  output: " << output.string() << '\n'
                  << "  samples: " << ir.left.size() << " @ " << ir.sampleRate << " Hz\n"
                  << "  peak: " << result.peak << ", RMS: " << result.rms << '\n'
                  << "  NED t90: ";
        if (result.nedT90Seconds >= 0.0) std::cout << result.nedT90Seconds << " s\n";
        else std::cout << "not reached\n";
        std::cout << "  max autocorrelation (10-100 ms): "
                  << result.maxAutocorrelation << '\n';
        for (const auto& band : result.decayBands)
        {
            std::cout << "  " << band.centerHz << " Hz: T20 ";
            if (band.t20.valid) std::cout << band.t20.t60Seconds << " s (R2 "
                                          << band.t20.rSquared << ')';
            else std::cout << "n/a";
            std::cout << ", T30 ";
            if (band.t30.valid) std::cout << band.t30.t60Seconds << " s (R2 "
                                          << band.t30.rSquared << ')';
            else std::cout << "n/a";
            std::cout << '\n';
        }
        return result.allFinite ? 0 : 2;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "error: " << exception.what() << '\n';
        return 1;
    }
}
