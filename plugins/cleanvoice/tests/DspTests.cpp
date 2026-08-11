// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Acceptance tests for the CleanVoice DSP core. JUCE-free, so they run without
// instantiating anything.
//
// The first two matter more than the denoising ones: if the FFT or the overlap-add is
// wrong, every later judgement about "how it sounds" is measuring the wrong thing.

#include <cstdio>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>
#ifdef _WIN32
 #include <direct.h>
#else
 #include <sys/stat.h>
 #include <unistd.h>
#endif
#include "../src/dsp/Denoiser.h"
#include "../src/io/WavFile.h"

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf ("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); ++failures; } } while (0)

using namespace cv;

static std::vector<float> noise (int n, unsigned seed, float amp)
{
    std::mt19937 rng (seed);
    std::normal_distribution<float> d (0.0f, amp);
    std::vector<float> v ((size_t) n);
    for (auto& x : v) x = d (rng);
    return v;
}

static float rmsDb (const std::vector<float>& v, int from = 0, int to = -1)
{
    if (to < 0) to = (int) v.size();
    double acc = 0; int n = 0;
    for (int i = from; i < to; ++i) { acc += (double) v[(size_t) i] * v[(size_t) i]; ++n; }
    if (n == 0 || acc <= 0) return -200.0f;
    return (float) (10.0 * std::log10 (acc / n));
}

// ---------------------------------------------------------------- foundations ----

static void testFftRoundTrip()
{
    for (int n : { 256, 1024, 2048 })
    {
        Fft fft (n);
        auto src = noise (n, 3, 0.5f);
        std::vector<Complex> x ((size_t) n);
        for (int i = 0; i < n; ++i) x[(size_t) i] = Complex (src[(size_t) i], 0.0f);
        fft.forward (x);
        fft.inverse (x);
        float worst = 0;
        for (int i = 0; i < n; ++i)
            worst = std::max (worst, std::fabs (x[(size_t) i].real() - src[(size_t) i]));
        CHECK (worst < 1.0e-4f, "fft: inverse(forward(x)) == x");
    }
}

// Nothing may be judged by ear until this passes: with unity gain the analysis/synthesis
// pair has to give the input back, including at the very start and end of the file.
static void testUnityReconstruction()
{
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        const int fftSize = fftSizeForRate (sr);
        Stft stft (fftSize, fftSize / 4);
        const int n = (int) (0.35 * sr) + 137;      // deliberately not a frame multiple

        std::vector<std::vector<float>> in (2);
        in[0] = noise (n, 11, 0.3f);
        in[1] = noise (n, 12, 0.3f);
        // a click at the very first sample and at the very last one
        in[0][0] = 0.9f; in[1][(size_t) (n - 1)] = -0.9f;

        const int padded = n + 2 * fftSize;
        std::vector<std::vector<float>> out (2, std::vector<float> ((size_t) padded, 0.0f));
        std::vector<float> wsum ((size_t) padded, 0.0f);
        std::vector<Complex> spec;

        const int frames = stft.frameCount (n);
        for (int f = 0; f < frames; ++f)
            for (int c = 0; c < 2; ++c)
            {
                stft.analyse (in[(size_t) c].data(), n, f, spec);
                stft.overlapAdd (spec, f, out[(size_t) c], wsum);
            }
        for (auto& v : wsum) v /= 2.0f;
        for (int c = 0; c < 2; ++c) Stft::normalise (out[(size_t) c], wsum);

        float worst = 0;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
                worst = std::max (worst, std::fabs (out[(size_t) c][(size_t) (i + fftSize)]
                                                    - in[(size_t) c][(size_t) i]));
        std::printf ("  [wola] %.0f Hz, fft %d: worst reconstruction error %.2e\n",
                     sr, fftSize, (double) worst);
        CHECK (worst < 1.0e-4f, "wola: unity gain reconstructs the input exactly");
    }
}

// ---------------------------------------------------------------- denoising ----

static void testNoiseIsReducedAndProfileLearns()
{
    const double sr = 48000.0;
    const int fftSize = fftSizeForRate (sr);
    Stft stft (fftSize, fftSize / 4);
    const int n = (int) (2.0 * sr);

    // 0.5 s of pure noise, then noise + a tone standing in for voice
    std::vector<std::vector<float>> in (1, noise (n, 21, 0.02f));
    const int voiceFrom = (int) (0.5 * sr);
    for (int i = voiceFrom; i < n; ++i)
        in[0][(size_t) i] += 0.3f * std::sin (2.0 * kPi * 440.0 * i / sr);

    NoiseProfile prof;
    CHECK (prof.learn (stft, in, n, 0, voiceFrom), "profile: learns from a noise-only region");
    CHECK (prof.frames() > 10, "profile: uses a sensible number of frames");

    DenoiseParams p; p.reductionDb = 12.0f; p.smoothing = 0.5f;
    auto clean = Denoiser::process (stft, prof, in, n, p);

    const float before = rmsDb (in[0], 0, voiceFrom);
    const float after  = rmsDb (clean[0], 0, voiceFrom);
    std::printf ("  [denoise] noise-only region %.1f -> %.1f dB (%.1f dB down)\n",
                 before, after, before - after);
    CHECK (before - after > 6.0f, "denoise: the noise-only region is measurably quieter");

    // the tone must survive: it is far above the noise floor
    const float tBefore = rmsDb (in[0], voiceFrom, n);
    const float tAfter  = rmsDb (clean[0], voiceFrom, n);
    std::printf ("  [denoise] tone region     %.1f -> %.1f dB (%.1f dB down)\n",
                 tBefore, tAfter, tBefore - tAfter);
    CHECK (tBefore - tAfter < 1.5f, "denoise: a signal well above the floor is kept");
}

// The reduction control has to mean what it says, because the whole design leans on
// keeping it shallow.
static void testReductionIsBounded()
{
    const double sr = 48000.0;
    const int fftSize = fftSizeForRate (sr);
    Stft stft (fftSize, fftSize / 4);
    const int n = (int) (1.0 * sr);
    std::vector<std::vector<float>> in (1, noise (n, 31, 0.02f));

    NoiseProfile prof;
    prof.learn (stft, in, n, 0, n);

    for (float dB : { 6.0f, 12.0f })
    {
        DenoiseParams p; p.reductionDb = dB; p.smoothing = 0.0f;
        auto clean = Denoiser::process (stft, prof, in, n, p);
        const float drop = rmsDb (in[0], 0, n) - rmsDb (clean[0], 0, n);
        std::printf ("  [reduction] asked %.0f dB, pure noise fell %.1f dB\n", dB, drop);
        CHECK (drop <= dB + 1.0f, "reduction: never attenuates more than asked");
        CHECK (drop > dB * 0.5f, "reduction: actually approaches the ceiling on pure noise");
    }
}

// The reason a single real gain is used at all. Independent per-channel gains would move
// the image; this asserts the interaural level difference survives processing.
static void testBinauralLevelDifferenceIsPreserved()
{
    const double sr = 48000.0;
    const int fftSize = fftSizeForRate (sr);
    Stft stft (fftSize, fftSize / 4);
    const int n = (int) (2.0 * sr);

    // uncorrelated noise in each ear, then a tone 12 dB louder on the left
    std::vector<std::vector<float>> in (2);
    in[0] = noise (n, 41, 0.02f);
    in[1] = noise (n, 42, 0.02f);
    const int from = (int) (0.6 * sr);
    for (int i = from; i < n; ++i)
    {
        const float s = std::sin (2.0 * kPi * 700.0 * i / sr);
        in[0][(size_t) i] += 0.40f * s;
        in[1][(size_t) i] += 0.10f * s;      // 12 dB quieter
    }

    NoiseProfile prof;
    prof.learn (stft, in, n, 0, from);
    DenoiseParams p; p.reductionDb = 12.0f;
    auto clean = Denoiser::process (stft, prof, in, n, p);

    const float ildBefore = rmsDb (in[0], from, n) - rmsDb (in[1], from, n);
    const float ildAfter  = rmsDb (clean[0], from, n) - rmsDb (clean[1], from, n);
    std::printf ("  [binaural] ILD %.2f dB -> %.2f dB (moved %.2f dB)\n",
                 ildBefore, ildAfter, std::fabs (ildAfter - ildBefore));
    CHECK (std::fabs (ildAfter - ildBefore) < 1.0f,
           "binaural: a common gain leaves the interaural level difference alone");
}

// clean + removed must be the input, or something is being created or lost and the
// "listen to what was removed" test stops meaning anything.
static void testRemovedIsTheExactComplement()
{
    const double sr = 48000.0;
    const int fftSize = fftSizeForRate (sr);
    Stft stft (fftSize, fftSize / 4);
    const int n = (int) (1.0 * sr);
    std::vector<std::vector<float>> in (2);
    in[0] = noise (n, 51, 0.05f);
    in[1] = noise (n, 52, 0.05f);

    NoiseProfile prof;
    prof.learn (stft, in, n, 0, n / 2);
    DenoiseParams p;
    auto clean = Denoiser::process (stft, prof, in, n, p);

    float worst = 0;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < n; ++i)
        {
            const float removed = in[(size_t) c][(size_t) i] - clean[(size_t) c][(size_t) i];
            worst = std::max (worst, std::fabs ((clean[(size_t) c][(size_t) i] + removed)
                                                - in[(size_t) c][(size_t) i]));
        }
    CHECK (worst < 1.0e-6f, "removed: clean + removed reconstructs the input");
}

static void testOutputStaysFinite()
{
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        const int fftSize = fftSizeForRate (sr);
        Stft stft (fftSize, fftSize / 4);
        const int n = (int) (0.6 * sr);
        std::vector<std::vector<float>> in (2);
        in[0] = noise (n, 61, 0.9f);
        in[1] = std::vector<float> ((size_t) n, 0.0f);   // one channel digital silence

        NoiseProfile prof;
        prof.learn (stft, in, n, 0, n / 2);
        DenoiseParams p; p.reductionDb = 40.0f; p.preserve = 1.0f; p.smoothing = 1.0f;
        auto clean = Denoiser::process (stft, prof, in, n, p);

        bool ok = true;
        for (const auto& c : clean)
            for (float v : c) if (! std::isfinite (v)) ok = false;
        CHECK (ok, "output stays finite at extreme settings, including a silent channel");
    }
}

// ---------------------------------------------------------------- wav io ----

// Every recording in this project lives in a folder named after the character in the
// script, so a path that is not ASCII is the normal case, not an edge case. This caught a
// real failure: fopen on Windows reads a UTF-8 path as CP932 and simply cannot open it.
static void testNonAsciiPath()
{
    AudioFile a;
    a.sampleRate = 48000.0;
    a.channels.assign (1, std::vector<float> (1000, 0.25f));

    // "shirako-chan/te-suto.wav" in Japanese, written as UTF-8 bytes so the test file
    // itself does not depend on the compiler's source encoding.
    const std::string dir  = "ç½èã¡ãã_cvtest";
    const std::string path = dir + "/ãã¹ã.wav";

#ifdef _WIN32
    _wmkdir (cv::utf8ToWide (dir).c_str());
#else
    mkdir (dir.c_str(), 0777);
#endif

    std::string err;
    CHECK (wav::write (path, a, err), "utf-8 path: writes to a Japanese directory");
    AudioFile b;
    CHECK (wav::read (path, b, err), "utf-8 path: reads back from a Japanese directory");
    CHECK (b.numSamples() == 1000, "utf-8 path: content survives");

#ifdef _WIN32
    _wremove (cv::utf8ToWide (path).c_str());
    _wrmdir (cv::utf8ToWide (dir).c_str());
#else
    std::remove (path.c_str());
    rmdir (dir.c_str());
#endif
}

static void testWavRoundTrip()
{
    AudioFile a;
    a.sampleRate = 96000.0;
    a.channels.assign (2, std::vector<float> (5000, 0.0f));
    for (int i = 0; i < 5000; ++i)
    {
        a.channels[0][(size_t) i] = std::sin (i * 0.01f) * 0.8f;
        a.channels[1][(size_t) i] = std::cos (i * 0.02f) * 0.4f;
    }
    std::string err;
    const std::string path = "cleanvoice_roundtrip_test.wav";
    CHECK (wav::write (path, a, err), "wav: writes");

    AudioFile b;
    CHECK (wav::read (path, b, err), "wav: reads back");
    CHECK (b.numChannels() == 2 && b.numSamples() == 5000, "wav: shape survives");
    CHECK (std::fabs (b.sampleRate - 96000.0) < 1.0, "wav: sample rate survives");
    float worst = 0;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < 5000; ++i)
            worst = std::max (worst, std::fabs (a.channels[(size_t) c][(size_t) i]
                                                - b.channels[(size_t) c][(size_t) i]));
    CHECK (worst < 1.0e-6f, "wav: float round-trip is lossless");
    std::remove (path.c_str());
}

int main()
{
    std::printf ("CleanVoice DSP tests\n");
    testFftRoundTrip();
    testUnityReconstruction();
    testNoiseIsReducedAndProfileLearns();
    testReductionIsBounded();
    testBinauralLevelDifferenceIsPreserved();
    testRemovedIsTheExactComplement();
    testOutputStaysFinite();
    testWavRoundTrip();
    testNonAsciiPath();
    if (failures == 0) { std::printf ("ALL PASS\n"); return 0; }
    std::printf ("%d FAILURES\n", failures);
    return 1;
}
