// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

#include "BaselineRenderer.h"
#include "Fft.h"

namespace nsr::analysis
{
struct DecayEstimate
{
    bool valid = false;
    double t60Seconds = 0.0;
    double rSquared = 0.0;
    double startDb = 0.0;
    double endDb = 0.0;
};

struct BandDecay
{
    double centerHz = 0.0;
    DecayEstimate t20;
    DecayEstimate t30;
};

struct NedPoint
{
    double timeSeconds = 0.0;
    double normalizedDensity = 0.0;
};

struct SpectrumPoint
{
    double centerHz = 0.0;
    double relativeDb = 0.0;
};

struct CorrelationPoint
{
    double lagSeconds = 0.0;
    double normalizedCorrelation = 0.0;
};

struct EdrData
{
    std::vector<double> bandCentersHz;
    std::vector<double> timesSeconds;
    std::vector<std::vector<double>> db; // [frame][band], normalized at time zero
};

struct AnalysisResult
{
    bool allFinite = true;
    double peak = 0.0;
    double rms = 0.0;
    double nedT90Seconds = -1.0;
    double maxAutocorrelation = 0.0;
    std::vector<BandDecay> decayBands;
    std::vector<NedPoint> ned;
    std::vector<SpectrumPoint> spectrum;
    std::vector<CorrelationPoint> autocorrelation;
    EdrData edr;
};

class Biquad
{
public:
    void setLowPass (double sampleRate, double cutoff, double q)
    {
        configure (sampleRate, cutoff, q, false);
    }

    void setHighPass (double sampleRate, double cutoff, double q)
    {
        configure (sampleRate, cutoff, q, true);
    }

    double process (double input) noexcept
    {
        const double output = b0 * input + z1;
        z1 = b1 * input - a1 * output + z2;
        z2 = b2 * input - a2 * output;
        return output;
    }

private:
    void configure (double sampleRate, double cutoff, double q, bool highPass)
    {
        cutoff = std::clamp (cutoff, 1.0, sampleRate * 0.49);
        const double omega = 2.0 * kPi * cutoff / sampleRate;
        const double cosine = std::cos (omega);
        const double sine = std::sin (omega);
        const double alpha = sine / (2.0 * q);
        const double a0 = 1.0 + alpha;

        if (highPass)
        {
            b0 = (1.0 + cosine) * 0.5 / a0;
            b1 = -(1.0 + cosine) / a0;
            b2 = b0;
        }
        else
        {
            b0 = (1.0 - cosine) * 0.5 / a0;
            b1 = (1.0 - cosine) / a0;
            b2 = b0;
        }
        a1 = -2.0 * cosine / a0;
        a2 = (1.0 - alpha) / a0;
        z1 = z2 = 0.0;
    }

    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;
};

inline DecayEstimate regressDecay (const std::vector<double>& edcDb, double sampleRate,
                                    double startDb, double endDb)
{
    DecayEstimate result;
    result.startDb = startDb;
    result.endDb = endDb;

    double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;
    std::size_t count = 0;
    for (std::size_t i = 0; i < edcDb.size(); ++i)
    {
        const double y = edcDb[i];
        if (y <= startDb && y >= endDb && std::isfinite (y))
        {
            const double x = static_cast<double> (i) / sampleRate;
            sx += x; sy += y; sxx += x * x; sxy += x * y;
            ++count;
        }
    }
    if (count < 10) return result;

    const double n = static_cast<double> (count);
    const double denominator = n * sxx - sx * sx;
    if (std::abs (denominator) < 1.0e-20) return result;
    const double slope = (n * sxy - sx * sy) / denominator;
    const double intercept = (sy - slope * sx) / n;
    if (! std::isfinite (slope) || slope >= -1.0e-9) return result;

    const double mean = sy / n;
    double residual = 0.0, total = 0.0;
    for (std::size_t i = 0; i < edcDb.size(); ++i)
    {
        const double y = edcDb[i];
        if (y <= startDb && y >= endDb && std::isfinite (y))
        {
            const double x = static_cast<double> (i) / sampleRate;
            const double error = y - (intercept + slope * x);
            residual += error * error;
            const double centered = y - mean;
            total += centered * centered;
        }
    }

    result.valid = true;
    result.t60Seconds = -60.0 / slope;
    result.rSquared = total > 1.0e-20 ? 1.0 - residual / total : 1.0;
    return result;
}

inline BandDecay measureBandDecay (const StereoIr& ir, double centerHz)
{
    const double lowCut = centerHz / std::sqrt (2.0);
    const double highCut = centerHz * std::sqrt (2.0);
    constexpr double q = 0.7071067811865476;
    Biquad highL, highR, lowL, lowR;
    highL.setHighPass (ir.sampleRate, lowCut, q);
    highR.setHighPass (ir.sampleRate, lowCut, q);
    lowL.setLowPass (ir.sampleRate, highCut, q);
    lowR.setLowPass (ir.sampleRate, highCut, q);

    std::vector<double> energy (ir.left.size(), 0.0);
    for (std::size_t i = 0; i < ir.left.size(); ++i)
    {
        const double left = lowL.process (highL.process (ir.left[i]));
        const double right = lowR.process (highR.process (ir.right[i]));
        energy[i] = 0.5 * (left * left + right * right);
    }

    for (std::size_t i = energy.size(); i-- > 1;)
        energy[i - 1] += energy[i];

    std::vector<double> edcDb (energy.size(), -300.0);
    const double reference = energy.empty() ? 0.0 : energy.front();
    if (reference > 1.0e-30)
        for (std::size_t i = 0; i < energy.size(); ++i)
            edcDb[i] = 10.0 * std::log10 (std::max (energy[i] / reference, 1.0e-30));

    BandDecay result;
    result.centerHz = centerHz;
    result.t20 = regressDecay (edcDb, ir.sampleRate, -5.0, -25.0);
    result.t30 = regressDecay (edcDb, ir.sampleRate, -5.0, -35.0);
    return result;
}

inline std::vector<NedPoint> measureNed (const StereoIr& ir, double& t90Seconds)
{
    const int window = std::max (16, static_cast<int> (std::lround (0.020 * ir.sampleRate)));
    const int hop = std::max (1, static_cast<int> (std::lround (0.005 * ir.sampleRate)));
    std::vector<NedPoint> result;
    constexpr double gaussianOutsideOneSigma = 0.31731050786291415;

    for (int center = window / 2; center + window / 2 < static_cast<int> (ir.left.size());
         center += hop)
    {
        const int begin = center - window / 2;
        const int end = begin + window;
        double meanL = 0.0, meanR = 0.0;
        for (int i = begin; i < end; ++i)
        {
            meanL += ir.left[static_cast<std::size_t> (i)];
            meanR += ir.right[static_cast<std::size_t> (i)];
        }
        meanL /= window; meanR /= window;

        double varianceL = 0.0, varianceR = 0.0;
        for (int i = begin; i < end; ++i)
        {
            const double l = ir.left[static_cast<std::size_t> (i)] - meanL;
            const double r = ir.right[static_cast<std::size_t> (i)] - meanR;
            varianceL += l * l; varianceR += r * r;
        }
        const double sigmaL = std::sqrt (varianceL / window);
        const double sigmaR = std::sqrt (varianceR / window);
        double normalized = 0.0;
        if (sigmaL > 1.0e-15 || sigmaR > 1.0e-15)
        {
            std::size_t outside = 0;
            for (int i = begin; i < end; ++i)
            {
                if (sigmaL > 1.0e-15
                    && std::abs (ir.left[static_cast<std::size_t> (i)] - meanL) > sigmaL)
                    ++outside;
                if (sigmaR > 1.0e-15
                    && std::abs (ir.right[static_cast<std::size_t> (i)] - meanR) > sigmaR)
                    ++outside;
            }
            normalized = (static_cast<double> (outside) / (2.0 * window))
                         / gaussianOutsideOneSigma;
        }
        result.push_back ({ static_cast<double> (center) / ir.sampleRate, normalized });
    }

    t90Seconds = -1.0;
    const int required = std::max (1, static_cast<int> (std::ceil (0.020 * ir.sampleRate / hop)));
    for (std::size_t i = 0; i + static_cast<std::size_t> (required) <= result.size(); ++i)
    {
        bool sustained = true;
        for (int j = 0; j < required; ++j)
            if (result[i + static_cast<std::size_t> (j)].normalizedDensity < 0.9)
            {
                sustained = false;
                break;
            }
        if (sustained)
        {
            t90Seconds = result[i].timeSeconds;
            break;
        }
    }
    return result;
}

inline EdrData measureEdr (const StereoIr& ir)
{
    constexpr std::size_t fftSize = 2048;
    constexpr std::size_t hop = 512;
    const std::array<double, 7> requested { 125, 250, 500, 1000, 2000, 4000, 8000 };

    EdrData result;
    for (double center : requested)
        if (center * std::sqrt (2.0) < ir.sampleRate * 0.5)
            result.bandCentersHz.push_back (center);
    if (ir.left.size() < fftSize || result.bandCentersHz.empty()) return result;

    const std::size_t frames = 1 + (ir.left.size() - fftSize) / hop;
    result.timesSeconds.resize (frames);
    result.db.assign (frames, std::vector<double> (result.bandCentersHz.size(), 0.0));
    std::vector<std::vector<double>> energy (frames,
                                             std::vector<double> (result.bandCentersHz.size(), 0.0));

    Fft fft (fftSize);
    std::vector<Complex> buffer (fftSize);
    std::vector<double> binEnergy (fftSize / 2 + 1, 0.0);
    for (std::size_t frame = 0; frame < frames; ++frame)
    {
        result.timesSeconds[frame] = static_cast<double> (frame * hop) / ir.sampleRate;
        std::fill (binEnergy.begin(), binEnergy.end(), 0.0);
        for (int channel = 0; channel < 2; ++channel)
        {
            const auto& samples = channel == 0 ? ir.left : ir.right;
            for (std::size_t i = 0; i < fftSize; ++i)
            {
                const double window = 0.5 - 0.5 * std::cos (2.0 * kPi * i / (fftSize - 1));
                buffer[i] = Complex (samples[frame * hop + i] * window, 0.0);
            }
            fft.forward (buffer);
            for (std::size_t bin = 0; bin < binEnergy.size(); ++bin)
                binEnergy[bin] += 0.5 * std::norm (buffer[bin]);
        }

        for (std::size_t band = 0; band < result.bandCentersHz.size(); ++band)
        {
            const double low = result.bandCentersHz[band] / std::sqrt (2.0);
            const double high = result.bandCentersHz[band] * std::sqrt (2.0);
            const std::size_t first = static_cast<std::size_t> (std::ceil (low * fftSize
                                                                          / ir.sampleRate));
            const std::size_t last = std::min (binEnergy.size() - 1,
                static_cast<std::size_t> (std::floor (high * fftSize / ir.sampleRate)));
            for (std::size_t bin = first; bin <= last; ++bin)
                energy[frame][band] += binEnergy[bin];
        }
    }

    for (std::size_t band = 0; band < result.bandCentersHz.size(); ++band)
    {
        for (std::size_t frame = frames; frame-- > 1;)
            energy[frame - 1][band] += energy[frame][band];
        const double reference = std::max (energy[0][band], 1.0e-300);
        for (std::size_t frame = 0; frame < frames; ++frame)
            result.db[frame][band] = 10.0 * std::log10 (
                std::max (energy[frame][band] / reference, 1.0e-16));
    }
    return result;
}

inline std::vector<SpectrumPoint> measureSpectrum (const StereoIr& ir)
{
    const std::size_t fftSize = nextPowerOfTwo (ir.left.size());
    Fft fft (fftSize);
    std::vector<Complex> buffer (fftSize, Complex {});
    std::vector<double> power (fftSize / 2 + 1, 0.0);

    for (int channel = 0; channel < 2; ++channel)
    {
        std::fill (buffer.begin(), buffer.end(), Complex {});
        const auto& samples = channel == 0 ? ir.left : ir.right;
        for (std::size_t i = 0; i < samples.size(); ++i) buffer[i] = Complex (samples[i], 0.0);
        fft.forward (buffer);
        for (std::size_t bin = 0; bin < power.size(); ++bin)
            power[bin] += 0.5 * std::norm (buffer[bin]);
    }

    std::vector<SpectrumPoint> result;
    const double maxHz = std::min (20000.0, ir.sampleRate * 0.45);
    const double halfStep = std::pow (2.0, 1.0 / 24.0);
    for (double center = 20.0; center <= maxHz; center *= std::pow (2.0, 1.0 / 12.0))
    {
        const std::size_t first = std::max<std::size_t> (1, static_cast<std::size_t> (
            std::ceil ((center / halfStep) * fftSize / ir.sampleRate)));
        const std::size_t last = std::min (power.size() - 1, static_cast<std::size_t> (
            std::floor ((center * halfStep) * fftSize / ir.sampleRate)));
        double sum = 0.0;
        std::size_t count = 0;
        for (std::size_t bin = first; bin <= last; ++bin) { sum += power[bin]; ++count; }
        result.push_back ({ center, count > 0 ? sum / count : 0.0 });
    }

    double maximum = 0.0;
    for (const auto& point : result) maximum = std::max (maximum, point.relativeDb);
    maximum = std::max (maximum, 1.0e-300);
    for (auto& point : result)
        point.relativeDb = 10.0 * std::log10 (std::max (point.relativeDb / maximum, 1.0e-16));
    return result;
}

inline std::vector<CorrelationPoint> measureAutocorrelation (const StereoIr& ir,
                                                             double startSeconds,
                                                             double& maximumAbsolute)
{
    const std::size_t start = std::min (ir.left.size(), static_cast<std::size_t> (
        std::max (0.0, startSeconds) * ir.sampleRate));
    const std::size_t length = std::min (ir.left.size() - start,
                                         static_cast<std::size_t> (2.0 * ir.sampleRate));
    const int minLag = std::max (1, static_cast<int> (std::lround (0.010 * ir.sampleRate)));
    const int maxLag = static_cast<int> (std::lround (0.100 * ir.sampleRate));
    const int step = std::max (1, static_cast<int> (std::lround (0.001 * ir.sampleRate)));

    std::vector<CorrelationPoint> result;
    maximumAbsolute = 0.0;
    if (length <= static_cast<std::size_t> (maxLag + 1)) return result;

    for (int lag = minLag; lag <= maxLag; lag += step)
    {
        double cross = 0.0, energyA = 0.0, energyB = 0.0;
        for (std::size_t i = static_cast<std::size_t> (lag); i < length; ++i)
        {
            const std::size_t a = start + i;
            const std::size_t b = a - static_cast<std::size_t> (lag);
            for (int channel = 0; channel < 2; ++channel)
            {
                const auto& samples = channel == 0 ? ir.left : ir.right;
                const double va = samples[a], vb = samples[b];
                cross += va * vb; energyA += va * va; energyB += vb * vb;
            }
        }
        const double denominator = std::sqrt (energyA * energyB);
        const double normalized = denominator > 1.0e-30 ? cross / denominator : 0.0;
        maximumAbsolute = std::max (maximumAbsolute, std::abs (normalized));
        result.push_back ({ static_cast<double> (lag) / ir.sampleRate, normalized });
    }
    return result;
}

inline AnalysisResult analyze (const StereoIr& ir)
{
    if (ir.left.empty() || ir.left.size() != ir.right.size())
        throw std::invalid_argument ("analysis requires equal, non-empty stereo channels");

    AnalysisResult result;
    double sumSquares = 0.0;
    for (std::size_t i = 0; i < ir.left.size(); ++i)
    {
        const double left = ir.left[i], right = ir.right[i];
        result.allFinite = result.allFinite && std::isfinite (left) && std::isfinite (right);
        result.peak = std::max ({ result.peak, std::abs (left), std::abs (right) });
        sumSquares += left * left + right * right;
    }
    result.rms = std::sqrt (sumSquares / (2.0 * ir.left.size()));

    for (double center : { 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0 })
        if (center * std::sqrt (2.0) < ir.sampleRate * 0.5)
            result.decayBands.push_back (measureBandDecay (ir, center));

    result.ned = measureNed (ir, result.nedT90Seconds);
    result.edr = measureEdr (ir);
    result.spectrum = measureSpectrum (ir);
    const double correlationStart = result.nedT90Seconds >= 0.0 ? result.nedT90Seconds : 0.1;
    result.autocorrelation = measureAutocorrelation (ir, correlationStart,
                                                     result.maxAutocorrelation);
    return result;
}
} // namespace nsr::analysis
