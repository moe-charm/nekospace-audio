// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CleanVoice v1 prototype. No GUI on purpose: the only question this has to answer is
// whether hiss can be removed without thinning the whisper, and a window does not help
// answer it. Two files come out - the cleaned signal and exactly what was taken away.
//
// LISTEN TO THE REMOVED FILE. It is the test. If speech, breath or consonants are audible
// in it, the setting is wrong, and A/B-ing the cleaned file will not tell you that with
// anything like the same certainty.

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <cmath>
#include "../src/io/WavFile.h"
#include "../src/dsp/Stft.h"
#include "../src/dsp/NoiseProfile.h"
#include "../src/dsp/Denoiser.h"

namespace
{
void usage()
{
    std::printf (
        "cleanvoice - fixed-profile denoiser for whispered and breathy voice\n"
        "\n"
        "  cleanvoice <in.wav> --noise <start> <end> [options]\n"
        "\n"
        "  --noise <start> <end>   seconds of a region containing ONLY noise. The whole\n"
        "                          file is processed; this region is just the lesson.\n"
        "  --reduction <dB>        maximum attenuation      (default 10, try 6-12 first)\n"
        "  --smoothing <0..1>      musical-noise control    (default 0.5)\n"
        "  --preserve <0..1>       onset protection         (default 0 - hear it raw first)\n"
        "  --oversub <x>           noise PSD scaling        (default 1.0)\n"
        "  --out <prefix>          output prefix            (default: input name)\n"
        "\n"
        "Writes <prefix>-clean.wav and <prefix>-removed.wav.\n"
        "removed = original - clean exactly, so nothing can hide in the difference.\n");
}

float dbOf (const std::vector<std::vector<float>>& ch, int from, int to)
{
    double acc = 0; long long n = 0;
    for (const auto& c : ch)
        for (int i = from; i < to && i < (int) c.size(); ++i) { acc += (double) c[(size_t) i] * c[(size_t) i]; ++n; }
    if (n == 0 || acc <= 0) return -200.0f;
    return (float) (10.0 * std::log10 (acc / (double) n));
}
} // namespace

int main (int argc, char** argv)
{
    if (argc < 2) { usage(); return 1; }

    std::string inPath = argv[1], outPrefix;
    double noiseStart = -1, noiseEnd = -1;
    cv::DenoiseParams p;

    for (int i = 2; i < argc; ++i)
    {
        const std::string a = argv[i];
        auto next = [&] (double def) { return i + 1 < argc ? std::atof (argv[++i]) : def; };
        if (a == "--noise" && i + 2 < argc) { noiseStart = std::atof (argv[i + 1]); noiseEnd = std::atof (argv[i + 2]); i += 2; }
        else if (a == "--reduction") p.reductionDb = (float) next (10.0);
        else if (a == "--smoothing") p.smoothing   = (float) next (0.5);
        else if (a == "--preserve")  p.preserve    = (float) next (0.0);
        else if (a == "--oversub")   p.overSubtract= (float) next (1.0);
        else if (a == "--out" && i + 1 < argc) outPrefix = argv[++i];
        else if (a == "-h" || a == "--help") { usage(); return 0; }
        else { std::printf ("unknown option: %s\n", a.c_str()); return 1; }
    }

    if (noiseStart < 0 || noiseEnd <= noiseStart)
    { std::printf ("error: --noise <start> <end> in seconds is required\n\n"); usage(); return 1; }

    cv::AudioFile in;
    std::string err;
    if (! cv::wav::read (inPath, in, err)) { std::printf ("error: %s\n", err.c_str()); return 1; }

    const int n = in.numSamples(), nCh = in.numChannels();
    std::printf ("in       : %s\n", inPath.c_str());
    std::printf ("format   : %d ch, %.0f Hz, %d-bit %s, %.2f s\n",
                 nCh, in.sampleRate, in.bitsPerSample, in.isFloat ? "float" : "int",
                 (double) n / in.sampleRate);

    const int fftSize = cv::fftSizeForRate (in.sampleRate);
    cv::Stft stft (fftSize, fftSize / 4);
    std::printf ("stft     : %d point (%.2f ms), hop %d (%.2f ms), %d bins\n",
                 fftSize, 1000.0 * fftSize / in.sampleRate,
                 stft.hopSize(), 1000.0 * stft.hopSize() / in.sampleRate, stft.numBins());

    const int s0 = (int) (noiseStart * in.sampleRate);
    const int s1 = (int) (noiseEnd * in.sampleRate);
    if (s0 < 0 || s1 > n)
    { std::printf ("error: noise region is outside the file\n"); return 1; }

    cv::NoiseProfile profile;
    if (! profile.learn (stft, in.channels, n, s0, s1))
    { std::printf ("error: noise region too short - needs at least ~%.0f ms\n",
                   1000.0 * (fftSize + 4 * stft.hopSize()) / in.sampleRate); return 1; }

    std::printf ("noise    : %.2f-%.2f s, %d frames learned\n",
                 noiseStart, noiseEnd, profile.frames());
    std::printf ("settings : reduction %.1f dB, smoothing %.2f, preserve %.2f, oversub %.2f\n",
                 p.reductionDb, p.smoothing, p.preserve, p.overSubtract);

    auto clean = cv::Denoiser::process (stft, profile, in.channels, n, p);

    // removed is the exact complement, so clean + removed reconstructs the input
    std::vector<std::vector<float>> removed ((size_t) nCh, std::vector<float> ((size_t) n));
    for (int c = 0; c < nCh; ++c)
        for (int i = 0; i < n; ++i)
            removed[(size_t) c][(size_t) i] =
                in.channels[(size_t) c][(size_t) i] - clean[(size_t) c][(size_t) i];

    // What actually happened, in the noise region and over the whole file. These are not
    // the real evaluation - that needs the clean voice and the noise as separate files -
    // but they catch a setting that is obviously wrong before you put headphones on.
    std::printf ("\nnoise region : %.1f dB -> %.1f dB  (%.1f dB down)\n",
                 dbOf (in.channels, s0, s1), dbOf (clean, s0, s1),
                 dbOf (in.channels, s0, s1) - dbOf (clean, s0, s1));
    std::printf ("whole file   : %.1f dB -> %.1f dB  (%.1f dB down)\n",
                 dbOf (in.channels, 0, n), dbOf (clean, 0, n),
                 dbOf (in.channels, 0, n) - dbOf (clean, 0, n));

    if (outPrefix.empty())
    {
        outPrefix = inPath;
        const size_t dot = outPrefix.find_last_of ('.');
        if (dot != std::string::npos) outPrefix = outPrefix.substr (0, dot);
    }

    cv::AudioFile outFile;
    outFile.sampleRate = in.sampleRate;
    outFile.channels = clean;
    if (! cv::wav::write (outPrefix + "-clean.wav", outFile, err))
    { std::printf ("error: %s\n", err.c_str()); return 1; }
    outFile.channels = removed;
    if (! cv::wav::write (outPrefix + "-removed.wav", outFile, err))
    { std::printf ("error: %s\n", err.c_str()); return 1; }

    std::printf ("\nwrote %s-clean.wav and %s-removed.wav\n",
                 outPrefix.c_str(), outPrefix.c_str());
    std::printf ("Listen to the REMOVED file first. Voice in it means the setting is wrong.\n");
    return 0;
}
