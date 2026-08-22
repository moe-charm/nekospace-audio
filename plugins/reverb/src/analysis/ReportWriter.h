// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>

#include "Analyzer.h"

#ifndef NSR_GIT_COMMIT
#define NSR_GIT_COMMIT "unknown"
#endif
#ifndef NSR_GIT_DIRTY
#define NSR_GIT_DIRTY 1
#endif
#ifndef NSR_COMPILER_ID
#define NSR_COMPILER_ID "unknown"
#endif
#ifndef NSR_COMPILER_VERSION
#define NSR_COMPILER_VERSION "unknown"
#endif
#ifndef NSR_BUILD_CONFIG
#define NSR_BUILD_CONFIG "unknown"
#endif

namespace nsr::analysis
{
inline bool writeTextFile (const std::filesystem::path& path, const std::string& text,
                           std::string& error)
{
    std::ofstream stream (path, std::ios::binary);
    if (! stream)
    {
        error = "cannot create " + path.string();
        return false;
    }
    stream.write (text.data(), static_cast<std::streamsize> (text.size()));
    if (! stream)
    {
        error = "failed while writing " + path.string();
        return false;
    }
    return true;
}

inline std::string jsonNumberOrNull (double value, bool valid = true)
{
    if (! valid || ! std::isfinite (value)) return "null";
    std::ostringstream stream;
    stream.imbue (std::locale::classic());
    stream << std::setprecision (10) << value;
    return stream.str();
}

inline bool writeSummaryJson (const std::filesystem::path& path,
                              const BaselineSettings& settings,
                              const AnalysisResult& result,
                              std::string& error)
{
    std::ostringstream out;
    out.imbue (std::locale::classic());
    out << std::setprecision (10);
    out << "{\n"
        << "  \"schema_version\": 1,\n"
        << "  \"product\": \"NekoSpace Reverb\",\n"
        << "  \"phase\": \"0-baseline\",\n"
        << "  \"analysis_version\": 1,\n"
        << "  \"algorithm\": \"binaural-room-fdn-8line-v1\",\n"
        << "  \"git_commit\": \"" << NSR_GIT_COMMIT << "\",\n"
        << "  \"working_tree_dirty\": " << (NSR_GIT_DIRTY != 0 ? "true" : "false") << ",\n"
        << "  \"build\": {\"compiler\": \"" << NSR_COMPILER_ID
        << "\", \"compiler_version\": \"" << NSR_COMPILER_VERSION
        << "\", \"configuration\": \"" << NSR_BUILD_CONFIG << "\"},\n"
        << "  \"deterministic_seed\": 0,\n"
        << "  \"settings\": {\n"
        << "    \"sample_rate\": " << settings.sampleRate << ",\n"
        << "    \"duration_seconds\": " << settings.durationSeconds << ",\n"
        << "    \"block_sequence\": [" << settings.blockSize << "],\n"
        << "    \"room_size\": " << settings.size << ",\n"
        << "    \"decay_seconds\": " << settings.decaySeconds << ",\n"
        << "    \"damping\": " << settings.damping << "\n"
        << "  },\n"
        << "  \"summary\": {\n"
        << "    \"all_finite\": " << (result.allFinite ? "true" : "false") << ",\n"
        << "    \"input_peak\": 1,\n"
        << "    \"input_rms\": "
        << 1.0 / std::sqrt (std::ceil (settings.durationSeconds * settings.sampleRate)) << ",\n"
        << "    \"output_peak\": " << result.peak << ",\n"
        << "    \"output_rms\": " << result.rms << ",\n"
        << "    \"ned_t90_seconds\": "
        << jsonNumberOrNull (result.nedT90Seconds, result.nedT90Seconds >= 0.0) << ",\n"
        << "    \"max_autocorrelation_10_100ms\": " << result.maxAutocorrelation << "\n"
        << "  },\n"
        << "  \"band_decay\": [\n";

    for (std::size_t i = 0; i < result.decayBands.size(); ++i)
    {
        const auto& band = result.decayBands[i];
        out << "    {\"center_hz\": " << band.centerHz
            << ", \"t20_seconds\": "
            << jsonNumberOrNull (band.t20.t60Seconds, band.t20.valid)
            << ", \"t20_r_squared\": "
            << jsonNumberOrNull (band.t20.rSquared, band.t20.valid)
            << ", \"t30_seconds\": "
            << jsonNumberOrNull (band.t30.t60Seconds, band.t30.valid)
            << ", \"t30_r_squared\": "
            << jsonNumberOrNull (band.t30.rSquared, band.t30.valid) << "}";
        if (i + 1 < result.decayBands.size()) out << ',';
        out << '\n';
    }

    out << "  ],\n"
        << "  \"artifacts\": {\n"
        << "    \"impulse_response\": \"baseline-ir.wav\",\n"
        << "    \"normalized_echo_density\": \"baseline-ned.csv\",\n"
        << "    \"energy_decay_relief\": \"baseline-edr.csv\",\n"
        << "    \"spectrum\": \"baseline-spectrum.csv\",\n"
        << "    \"autocorrelation\": \"baseline-autocorrelation.csv\"\n"
        << "  }\n"
        << "}\n";
    return writeTextFile (path, out.str(), error);
}

inline bool writeNedCsv (const std::filesystem::path& path,
                         const std::vector<NedPoint>& points, std::string& error)
{
    std::ostringstream out;
    out.imbue (std::locale::classic());
    out << "time_seconds,normalized_echo_density\n" << std::setprecision (10);
    for (const auto& point : points)
        out << point.timeSeconds << ',' << point.normalizedDensity << '\n';
    return writeTextFile (path, out.str(), error);
}

inline bool writeSpectrumCsv (const std::filesystem::path& path,
                              const std::vector<SpectrumPoint>& points, std::string& error)
{
    std::ostringstream out;
    out.imbue (std::locale::classic());
    out << "center_hz,relative_db\n" << std::setprecision (10);
    for (const auto& point : points)
        out << point.centerHz << ',' << point.relativeDb << '\n';
    return writeTextFile (path, out.str(), error);
}

inline bool writeAutocorrelationCsv (const std::filesystem::path& path,
                                     const std::vector<CorrelationPoint>& points,
                                     std::string& error)
{
    std::ostringstream out;
    out.imbue (std::locale::classic());
    out << "lag_seconds,normalized_autocorrelation\n" << std::setprecision (10);
    for (const auto& point : points)
        out << point.lagSeconds << ',' << point.normalizedCorrelation << '\n';
    return writeTextFile (path, out.str(), error);
}

inline bool writeEdrCsv (const std::filesystem::path& path, const EdrData& edr,
                         std::string& error)
{
    std::ostringstream out;
    out.imbue (std::locale::classic());
    out << "time_seconds";
    for (double center : edr.bandCentersHz) out << ',' << center << "_hz_db";
    out << '\n' << std::setprecision (10);
    for (std::size_t frame = 0; frame < edr.timesSeconds.size(); ++frame)
    {
        out << edr.timesSeconds[frame];
        for (double value : edr.db[frame]) out << ',' << value;
        out << '\n';
    }
    return writeTextFile (path, out.str(), error);
}
} // namespace nsr::analysis
