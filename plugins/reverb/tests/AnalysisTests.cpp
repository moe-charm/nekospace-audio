// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "analysis/Analyzer.h"
#include "analysis/FloatWavWriter.h"
#include "analysis/ReportWriter.h"

namespace
{
int failures = 0;

#define CHECK(condition, message) do { if (! (condition)) { \
    std::fprintf (stderr, "FAIL: %s\n", message); ++failures; } } while (false)

void testFftRoundTrip()
{
    nsr::analysis::Fft fft (64);
    std::vector<nsr::analysis::Complex> values (64);
    for (std::size_t i = 0; i < values.size(); ++i)
        values[i] = { std::sin (0.17 * static_cast<double> (i)),
                      std::cos (0.31 * static_cast<double> (i)) };
    const auto original = values;
    fft.forward (values);
    fft.inverse (values);
    double maximum = 0.0;
    for (std::size_t i = 0; i < values.size(); ++i)
        maximum = std::max (maximum, std::abs (values[i] - original[i]));
    CHECK (maximum < 1.0e-12, "FFT forward/inverse reconstructs its input");
}

void testBaselineIsDeterministicAndBlockInvariant()
{
    nsr::analysis::BaselineSettings settings;
    settings.durationSeconds = 0.75;
    settings.decaySeconds = 0.5f;
    settings.blockSize = 1;
    const auto one = nsr::analysis::renderBaselineImpulse (settings);
    settings.blockSize = 127;
    const auto odd = nsr::analysis::renderBaselineImpulse (settings);
    settings.blockSize = 512;
    const auto large = nsr::analysis::renderBaselineImpulse (settings);

    CHECK (one.left.size() == odd.left.size() && odd.left.size() == large.left.size(),
           "all block sequences render the same length");
    double maximumDifference = 0.0;
    double peak = 0.0;
    bool finite = true;
    for (std::size_t i = 0; i < one.left.size(); ++i)
    {
        maximumDifference = std::max ({ maximumDifference,
            std::abs (static_cast<double> (one.left[i] - odd.left[i])),
            std::abs (static_cast<double> (one.right[i] - odd.right[i])),
            std::abs (static_cast<double> (one.left[i] - large.left[i])),
            std::abs (static_cast<double> (one.right[i] - large.right[i])) });
        peak = std::max ({ peak, std::abs (static_cast<double> (one.left[i])),
                          std::abs (static_cast<double> (one.right[i])) });
        finite = finite && std::isfinite (one.left[i]) && std::isfinite (one.right[i]);
    }
    CHECK (maximumDifference < 1.0e-7, "baseline IR is block-size invariant");
    CHECK (peak > 1.0e-5, "baseline impulse produces a non-empty tail");
    CHECK (finite, "baseline impulse contains no NaN or Inf");
}

void testSupportedSampleRatesStayFinite()
{
    for (double sampleRate : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        nsr::analysis::BaselineSettings settings;
        settings.sampleRate = sampleRate;
        settings.durationSeconds = 0.15;
        settings.blockSize = 127;
        settings.decaySeconds = 0.25f;
        const auto ir = nsr::analysis::renderBaselineImpulse (settings);
        bool finite = true;
        double peak = 0.0;
        for (std::size_t i = 0; i < ir.left.size(); ++i)
        {
            finite = finite && std::isfinite (ir.left[i]) && std::isfinite (ir.right[i]);
            peak = std::max ({ peak, std::abs (static_cast<double> (ir.left[i])),
                              std::abs (static_cast<double> (ir.right[i])) });
        }
        CHECK (finite, "baseline is finite at every supported sample rate");
        CHECK (peak > 1.0e-7, "baseline produces output at every supported sample rate");
    }
}

nsr::analysis::StereoIr makeExponentialNoise (double sampleRate, double duration,
                                               double t60Seconds)
{
    nsr::analysis::StereoIr ir;
    ir.sampleRate = sampleRate;
    const std::size_t samples = static_cast<std::size_t> (sampleRate * duration);
    ir.left.resize (samples);
    ir.right.resize (samples);
    std::uint32_t stateL = 0x12345678u, stateR = 0x9abcdef0u;
    auto noise = [] (std::uint32_t& state)
    {
        state = state * 1664525u + 1013904223u;
        return static_cast<float> ((static_cast<double> (state) / 4294967295.0) * 2.0 - 1.0);
    };
    for (std::size_t i = 0; i < samples; ++i)
    {
        const double time = static_cast<double> (i) / sampleRate;
        const float envelope = static_cast<float> (std::pow (10.0, -3.0 * time / t60Seconds));
        ir.left[i] = noise (stateL) * envelope;
        ir.right[i] = noise (stateR) * envelope;
    }
    return ir;
}

void testKnownDecayAndDensity()
{
    constexpr double expectedT60 = 0.8;
    const auto ir = makeExponentialNoise (48000.0, 3.0, expectedT60);
    const auto result = nsr::analysis::analyze (ir);
    CHECK (result.allFinite, "synthetic analysis remains finite");
    CHECK (result.nedT90Seconds >= 0.0 && result.nedT90Seconds < 0.05,
           "dense noise reaches normalized echo density promptly");
    CHECK (! result.edr.timesSeconds.empty() && result.edr.bandCentersHz.size() == 7,
           "EDR contains frames and all reference bands");
    CHECK (result.spectrum.size() > 80, "spectrum contains 1/12-octave points");
    CHECK (! result.autocorrelation.empty(), "autocorrelation report is populated");

    int checked = 0;
    for (const auto& band : result.decayBands)
        if (band.centerHz >= 500.0 && band.centerHz <= 4000.0 && band.t20.valid)
        {
            CHECK (std::abs (band.t20.t60Seconds - expectedT60) < expectedT60 * 0.20,
                   "T20 recovers the known exponential decay");
            CHECK (band.t20.rSquared > 0.95, "known exponential decay is linear");
            ++checked;
        }
    CHECK (checked >= 4, "known decay is measurable in the mid bands");
}

void testReportArtifacts()
{
    nsr::analysis::BaselineSettings settings;
    settings.durationSeconds = 0.4;
    settings.decaySeconds = 0.25f;
    const auto ir = nsr::analysis::renderBaselineImpulse (settings);
    const auto result = nsr::analysis::analyze (ir);

    const std::filesystem::path directory = "nsr-analysis-test-output";
    std::filesystem::remove_all (directory);
    std::filesystem::create_directories (directory);
    std::string error;
    CHECK (nsr::analysis::writeFloatStereoWav (directory / "baseline-ir.wav", ir, error),
           "float WAV writes");
    CHECK (nsr::analysis::writeSummaryJson (directory / "baseline-report.json", settings,
                                             result, error), "summary JSON writes");
    CHECK (nsr::analysis::writeNedCsv (directory / "baseline-ned.csv", result.ned, error),
           "NED CSV writes");
    CHECK (nsr::analysis::writeEdrCsv (directory / "baseline-edr.csv", result.edr, error),
           "EDR CSV writes");
    CHECK (nsr::analysis::writeSpectrumCsv (directory / "baseline-spectrum.csv",
                                             result.spectrum, error), "spectrum CSV writes");
    CHECK (nsr::analysis::writeAutocorrelationCsv (
               directory / "baseline-autocorrelation.csv", result.autocorrelation, error),
           "autocorrelation CSV writes");

    std::ifstream wav (directory / "baseline-ir.wav", std::ios::binary);
    char header[12] {};
    wav.read (header, sizeof (header));
    CHECK (wav.gcount() == static_cast<std::streamsize> (sizeof (header))
           && std::string (header, header + 4) == "RIFF"
           && std::string (header + 8, header + 12) == "WAVE",
           "WAV has a valid RIFF/WAVE header");
    wav.close();

    std::ifstream json (directory / "baseline-report.json");
    const std::string text ((std::istreambuf_iterator<char> (json)),
                            std::istreambuf_iterator<char>());
    CHECK (text.find ("\"schema_version\": 1") != std::string::npos,
           "report schema is explicit");
    CHECK (text.find ("binaural-room-fdn-8line-v1") != std::string::npos,
           "report identifies the exact baseline");
    json.close();
    std::filesystem::remove_all (directory);
}
} // namespace

int main (int argc, char** argv)
{
    const int only = argc > 1 ? std::atoi (argv[1]) : 0;
    if (only == 0 || only == 1)
    {
        std::puts ("[1/4] FFT round trip"); std::fflush (stdout);
        testFftRoundTrip();
    }
    if (only == 0 || only == 2)
    {
        std::puts ("[2/4] baseline determinism"); std::fflush (stdout);
        testBaselineIsDeterministicAndBlockInvariant();
        testSupportedSampleRatesStayFinite();
    }
    if (only == 0 || only == 3)
    {
        std::puts ("[3/4] known decay and density"); std::fflush (stdout);
        testKnownDecayAndDensity();
    }
    if (only == 0 || only == 4)
    {
        std::puts ("[4/4] report artifacts"); std::fflush (stdout);
        testReportArtifacts();
    }
    if (failures == 0)
    {
        std::puts ("NekoSpace Reverb analysis tests passed");
        return 0;
    }
    std::fprintf (stderr, "%d Reverb analysis test(s) failed\n", failures);
    return 1;
}
