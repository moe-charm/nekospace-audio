// SPDX-FileCopyrightText: 2026 TextureVoice
// SPDX-License-Identifier: AGPL-3.0-or-later

// NekoSpace Binaural — JUCE-free DSP acceptance tests (Contract #12).
#include <cstdio>
#include <cmath>
#include <vector>
#include <random>
#include "../src/dsp/BinauralEngine.h"

using namespace nsb;

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf ("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); ++failures; } } while (0)

static bool allFinite (const std::vector<float>& v)
{
    for (float x : v) if (!std::isfinite (x)) return false;
    return true;
}

static float rms (const std::vector<float>& v)
{
    double acc = 0; for (float x : v) acc += (double) x * x;
    return (float) std::sqrt (acc / (double) v.size());
}

// Render noiseSeconds of pink-ish noise at a fixed position, return L/R buffers.
static void renderAt (BinauralEngine& eng, EngineParams p, float seconds, float fs,
                      std::vector<float>& L, std::vector<float>& R, unsigned seed = 7,
                      float inputScale = 1.0f)
{
    const int n = (int) (seconds * fs);
    std::vector<float> inL (n), inR (n);
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> d (-0.5f, 0.5f);
    float lp = 0;
    for (int i = 0; i < n; ++i)
    {
        lp = 0.9f * lp + 0.1f * d (rng);
        inL[i] = inR[i] = lp * 3.0f * inputScale;
    }
    L.assign (n, 0.0f); R.assign (n, 0.0f);
    eng.setParams (p);
    // process in odd-sized blocks to exercise chunking
    int pos = 0; const int sizes[] = { 128, 64, 256, 96, 512 }; int si = 0;
    while (pos < n)
    {
        int m = sizes[si++ % 5]; if (pos + m > n) m = n - pos;
        eng.process (inL.data() + pos, inR.data() + pos, L.data() + pos, R.data() + pos, m);
        pos += m;
    }
}

static void testMirrorSymmetry()
{
    const float fs = 48000.0f;
    BinauralEngine e1, e2;
    e1.prepare (fs, 512); e2.prepare (fs, 512);
    EngineParams p; p.sourceMode = 0; p.roomAmount = 0; p.distanceM = 1.5f;
    std::vector<float> L1, R1, L2, R2;
    p.azimuthDeg = 45.0f;  renderAt (e1, p, 1.0f, fs, L1, R1);
    p.azimuthDeg = -45.0f; renderAt (e2, p, 1.0f, fs, L2, R2);
    // skip first 0.2s (smoothers settling)
    const int skip = (int) (0.2f * fs);
    std::vector<float> a (L1.begin() + skip, L1.end()), b (R2.begin() + skip, R2.end());
    std::vector<float> c (R1.begin() + skip, R1.end()), d (L2.begin() + skip, L2.end());
    const float e = std::fabs (rms (a) - rms (b)) / (rms (a) + 1e-9f);
    const float f = std::fabs (rms (c) - rms (d)) / (rms (c) + 1e-9f);
    CHECK (e < 0.02f, "mirror: L(+45) == R(-45) energy");
    CHECK (f < 0.02f, "mirror: R(+45) == L(-45) energy");
    // and the near ear must be louder than the far ear
    CHECK (rms (c) > rms (a) * 1.2f, "mirror: near ear louder (az +45)");
}

static void testDistanceMonotonic()
{
    const float fs = 48000.0f;
    EngineParams p; p.sourceMode = 0; p.roomAmount = 0; p.azimuthDeg = 30.0f;
    float prev = 1e9f;
    for (float dist : { 0.3f, 0.7f, 1.5f, 3.0f, 6.0f, 12.0f })
    {
        BinauralEngine e; e.prepare (fs, 512);
        p.distanceM = dist;
        std::vector<float> L, R;
        // low level: keeps the safety limiter out of the picture, isolating the 1/r law
        renderAt (e, p, 0.6f, fs, L, R, 7, 0.1f);
        const int skip = (int) (0.2f * fs);
        std::vector<float> tl (L.begin() + skip, L.end()), tr (R.begin() + skip, R.end());
        const float energy = rms (tl) + rms (tr);
        CHECK (energy < prev * 1.02f, "distance: energy must not grow with distance");
        prev = energy;
    }
}

static void testRoomZeroIsExactDirect()
{
    const float fs = 48000.0f;
    BinauralEngine e1, e2;
    e1.prepare (fs, 512); e2.prepare (fs, 512);
    EngineParams p; p.azimuthDeg = -60.0f; p.distanceM = 0.4f;
    std::vector<float> L1, R1, L2, R2;
    p.roomAmount = 0.0f;               renderAt (e1, p, 0.5f, fs, L1, R1);
    p.roomAmount = 0.8f; p.bypassRoom = true; renderAt (e2, p, 0.5f, fs, L2, R2);
    // bypassRoom with amount smoothing from 0 start => identical output
    float maxDiff = 0;
    for (size_t i = 0; i < L1.size(); ++i)
        maxDiff = std::max (maxDiff, std::fabs (L1[i] - L2[i]) + std::fabs (R1[i] - R2[i]));
    CHECK (maxDiff < 1e-6f, "room 0% / bypass == exact direct render");
}

static void testAutomationSweepNoNanNoClick()
{
    const float fs = 48000.0f;
    BinauralEngine e; e.prepare (fs, 512);
    EngineParams p; p.roomAmount = 0.5f; p.nearField = 1.0f;
    const int n = (int) (4.0f * fs);
    std::vector<float> inL (1024), inR (1024), L (n, 0.0f), R (n, 0.0f);
    int pos = 0; int bi = 0;
    const int sizes[] = { 1, 16, 64, 127, 256, 1024, 512 };
    double ph = 0.0; const double dph = 2.0 * 3.14159265358979 * 330.0 / fs;
    while (pos < n)
    {
        int m = sizes[bi++ % 7]; if (pos + m > n) m = n - pos;
        // wild automation: full circles, elevation wobble, distance zoom, mode switch
        const float t = (float) pos / fs;
        p.azimuthDeg   = wrapDeg (t * 540.0f);
        p.elevationDeg = 80.0f * std::sin (t * 3.0f);
        p.distanceM    = 0.3f + 2.7f * (0.5f + 0.5f * std::sin (t * 1.7f));
        p.widthDeg     = 90.0f * (0.5f + 0.5f * std::sin (t * 0.9f));
        p.sourceMode   = (t > 2.0f) ? 1 : 0; // mode switch mid-flight
        e.setParams (p);
        // smooth 330 Hz sine: any click shows up as an outlier sample-to-sample jump
        for (int i = 0; i < m; ++i) { const float v = 0.25f * (float) std::sin (ph); ph += dph; inL[i] = v; inR[i] = -v * 0.7f; }
        e.process (inL.data(), inR.data(), L.data() + pos, R.data() + pos, m);
        pos += m;
    }
    CHECK (allFinite (L) && allFinite (R), "sweep: no NaN/Inf anywhere");
    // click detection: a smooth 330 Hz sine (max gain ~x4.7 at 0.3 m) changes by
    // ~0.06/sample at most; a real click is an order of magnitude above that
    float maxJump = 0;
    for (int i = 1; i < n; ++i)
        maxJump = std::max (maxJump, std::fabs (L[i] - L[i - 1]));
    CHECK (maxJump < 0.25f, "sweep: no hard discontinuities");
}

static void testBlockSizeInvariance()
{
    const float fs = 48000.0f;
    EngineParams p; p.azimuthDeg = 70.0f; p.elevationDeg = 20.0f;
    p.distanceM = 0.6f; p.roomAmount = 0.0f;
    const int n = 24000;
    std::vector<float> in (n);
    std::mt19937 rng (11);
    std::uniform_real_distribution<float> d (-0.5f, 0.5f);
    for (auto& x : in) x = d (rng);

    auto run = [&] (std::vector<int> blocks, std::vector<float>& L, std::vector<float>& R)
    {
        BinauralEngine e; e.prepare (fs, 1024);
        e.setParams (p);
        L.assign (n, 0.0f); R.assign (n, 0.0f);
        int pos = 0, bi = 0;
        while (pos < n)
        {
            int m = blocks[(size_t) bi++ % blocks.size()]; if (pos + m > n) m = n - pos;
            e.process (in.data() + pos, in.data() + pos, L.data() + pos, R.data() + pos, m);
            pos += m;
        }
    };
    std::vector<float> L1, R1, L2, R2;
    run ({ 512 }, L1, R1);
    run ({ 1, 17, 128, 63, 1024, 300 }, L2, R2);
    float maxDiff = 0;
    for (int i = 0; i < n; ++i)
        maxDiff = std::max (maxDiff, std::fabs (L1[i] - L2[i]));
    CHECK (maxDiff < 2e-3f, "block-size invariance (static position)");
}

static void testNearFieldEarApproach()
{
    // approaching the LEFT ear: left path gain up, ILD grows monotonically
    const float fs = 48000.0f;
    EngineParams p; p.sourceMode = 0; p.roomAmount = 0;
    p.azimuthDeg = -90.0f; p.nearField = 1.0f;
    float prevRatio = 0.0f;
    for (float dist : { 2.0f, 0.8f, 0.3f, 0.12f, 0.06f })
    {
        BinauralEngine e; e.prepare (fs, 512);
        p.distanceM = dist;
        std::vector<float> L, R;
        renderAt (e, p, 0.6f, fs, L, R);
        const int skip = (int) (0.25f * fs);
        std::vector<float> tl (L.begin() + skip, L.end()), tr (R.begin() + skip, R.end());
        const float ratio = rms (tl) / (rms (tr) + 1e-9f);
        CHECK (ratio > 1.0f, "nearfield: left ear louder at az -90");
        CHECK (ratio > prevRatio * 0.98f, "nearfield: ILD grows while approaching");
        prevRatio = ratio;
    }
    CHECK (prevRatio > 3.0f, "nearfield: strong ILD at 6 cm");
}

// Regression: in-place processing (out buffers alias in buffers) must be identical to
// out-of-place — FL Studio calls plugins in-place. Covers the Linked Stereo silence bug.
static void testInPlaceEquivalence()
{
    const float fs = 48000.0f;
    for (int mode : { 0, 1 })
    {
        EngineParams p; p.sourceMode = mode; p.azimuthDeg = 35.0f;
        p.distanceM = 0.5f; p.roomAmount = 0.4f; p.widthDeg = 80.0f;
        const int n = 12000;
        std::vector<float> inL (n), inR (n);
        std::mt19937 rng (21);
        std::uniform_real_distribution<float> d (-0.5f, 0.5f);
        for (int i = 0; i < n; ++i) { inL[i] = d (rng); inR[i] = d (rng); }

        BinauralEngine e1; e1.prepare (fs, 512); e1.setParams (p);
        std::vector<float> oL (n, 0.0f), oR (n, 0.0f);
        for (int pos = 0; pos < n; pos += 512)
            e1.process (inL.data() + pos, inR.data() + pos, oL.data() + pos, oR.data() + pos,
                        std::min (512, n - pos));

        BinauralEngine e2; e2.prepare (fs, 512); e2.setParams (p);
        std::vector<float> aL = inL, aR = inR; // aliased: in == out
        for (int pos = 0; pos < n; pos += 512)
            e2.process (aL.data() + pos, aR.data() + pos, aL.data() + pos, aR.data() + pos,
                        std::min (512, n - pos));

        float maxDiff = 0; double energy = 0;
        for (int i = 0; i < n; ++i)
        {
            maxDiff = std::max (maxDiff, std::fabs (oL[i] - aL[i]) + std::fabs (oR[i] - aR[i]));
            energy += (double) oL[i] * oL[i] + (double) oR[i] * oR[i];
        }
        CHECK (maxDiff == 0.0f, mode == 0 ? "in-place == out-of-place (Mono Object)"
                                          : "in-place == out-of-place (Linked Stereo)");
        CHECK (energy > 1.0, "in-place: output is not silent");
    }
}

// Regression: reported latency must match actual impulse arrival (FL PDC correctness).
static void testLatencyReported()
{
    for (float fs : { 44100.0f, 48000.0f, 192000.0f })
    {
        BinauralEngine e; e.prepare (fs, 512);
        EngineParams p; p.roomAmount = 0; p.nearField = 0; p.distanceM = 1.0f;
        e.setParams (p);
        const int lat = e.latencySamples();
        CHECK (lat == (int) (0.002f * fs + 0.5f), "latency: 2 ms base delay reported");

        const int n = lat + 512;
        std::vector<float> in (n, 0.0f), L (n, 0.0f), R (n, 0.0f);
        in[0] = 1.0f;
        for (int pos = 0; pos < n; pos += 512)
            e.process (in.data() + pos, in.data() + pos, L.data() + pos, R.data() + pos,
                       std::min (512, n - pos));
        int peakIdx = 0; float peak = 0;
        for (int i = 0; i < n; ++i)
            if (std::fabs (L[i]) > peak) { peak = std::fabs (L[i]); peakIdx = i; }
        CHECK (peak > 0.01f, "latency: impulse came through");
        CHECK (std::abs (peakIdx - lat) <= (int) (fs / 48000.0f * 12.0f),
               "latency: impulse arrives at the reported delay");
    }
}

// Regression: quality (tap count) switching mid-stream must ride the crossfade.
static void testQualitySwitchNoClick()
{
    const float fs = 48000.0f;
    BinauralEngine e; e.prepare (fs, 512);
    EngineParams p; p.azimuthDeg = 40.0f; p.distanceM = 1.0f; p.roomAmount = 0;
    const int n = (int) (2.5f * fs);
    std::vector<float> in (512), L (n, 0.0f), R (n, 0.0f);
    double ph = 0.0; const double dph = 2.0 * 3.14159265358979 * 330.0 / fs;
    for (int pos = 0; pos < n; pos += 512)
    {
        const int m = std::min (512, n - pos);
        p.qualityMode = ((pos / (int) (0.5f * fs)) % 2 == 0) ? 1 : 0;
        e.setParams (p);
        for (int i = 0; i < m; ++i) { in[(size_t) i] = 0.25f * (float) std::sin (ph); ph += dph; }
        e.process (in.data(), in.data(), L.data() + pos, R.data() + pos, m);
    }
    float maxJump = 0;
    for (int i = (int) (0.2f * fs); i < n; ++i)
        maxJump = std::max (maxJump, std::fabs (L[i] - L[i - 1]));
    CHECK (maxJump < 0.08f, "quality switch: no click");
}

// Regression: room early/late automation must be smooth.
static void testEarlyLateAutomationNoClick()
{
    const float fs = 48000.0f;
    BinauralEngine e; e.prepare (fs, 512);
    EngineParams p; p.azimuthDeg = -30.0f; p.distanceM = 1.2f;
    p.roomAmount = 0.6f; p.roomSize = 0.4f;
    const int n = (int) (2.5f * fs);
    std::vector<float> in (512), L (n, 0.0f), R (n, 0.0f);
    double ph = 0.0; const double dph = 2.0 * 3.14159265358979 * 220.0 / fs;
    for (int pos = 0; pos < n; pos += 512)
    {
        const int m = std::min (512, n - pos);
        p.roomEarlyLate = ((pos / (int) (0.25f * fs)) % 2 == 0) ? 0.0f : 1.0f;
        p.roomSize      = 0.2f + 0.6f * ((pos / (int) (0.4f * fs)) % 2);
        e.setParams (p);
        for (int i = 0; i < m; ++i) { in[(size_t) i] = 0.25f * (float) std::sin (ph); ph += dph; }
        e.process (in.data(), in.data(), L.data() + pos, R.data() + pos, m);
    }
    CHECK (allFinite (L) && allFinite (R), "early/late: no NaN");
    float maxJump = 0;
    for (int i = (int) (0.3f * fs); i < n; ++i)
        maxJump = std::max (maxJump, std::fabs (L[i] - L[i - 1]));
    CHECK (maxJump < 0.12f, "early/late + size automation: no click");
}

// Regression: straight below must not lean left or right (grid now reaches -90°).
static void testBottomElevationCentered()
{
    const float fs = 48000.0f;
    BinauralEngine e; e.prepare (fs, 512);
    EngineParams p; p.azimuthDeg = 90.0f; p.elevationDeg = -90.0f;
    p.distanceM = 1.0f; p.roomAmount = 0; p.nearField = 1.0f;
    std::vector<float> L, R;
    renderAt (e, p, 0.8f, fs, L, R);
    const int skip = (int) (0.3f * fs);
    std::vector<float> tl (L.begin() + skip, L.end()), tr (R.begin() + skip, R.end());
    const float ratioDb = 20.0f * std::log10 (rms (tl) / (rms (tr) + 1e-9f));
    CHECK (std::fabs (ratioDb) < 1.0f, "elevation -90: interaural level within 1 dB");
}

// Multi-rate smoke: full feature path stays finite at 44.1 / 96 / 192 kHz, and the
// HRIR window stays a constant ~2.7 ms in time (taps scale with SR, capped at 256).
static void testSampleRatesFinite()
{
    for (float fs : { 44100.0f, 96000.0f, 192000.0f })
    {
        BinauralEngine e; e.prepare (fs, 512);
        const int expectedTaps = std::min (256, (int) (128.0f * fs / 48000.0f + 0.5f));
        CHECK (e.hrtfTaps() == expectedTaps, "multi-rate: taps follow sample rate");
        EngineParams p; p.roomAmount = 0.5f; p.nearField = 1.0f; p.distanceM = 0.15f;
        p.azimuthDeg = -95.0f;
        std::vector<float> L, R;
        renderAt (e, p, 0.4f, fs, L, R);
        CHECK (allFinite (L) && allFinite (R), "multi-rate: finite output");
        CHECK (rms (L) > 1e-4f, "multi-rate: not silent");
    }
}

// Regression: room off -> (tail cooldown) -> on again must not burst stale reverb,
// and the decay must happen without any big audio-thread buffer reset.
static void testRoomToggleNoBurst()
{
    const float fs = 48000.0f;
    BinauralEngine e; e.prepare (fs, 512);
    EngineParams p; p.distanceM = 1.0f; p.roomAmount = 0.7f; p.roomSize = 0.5f;
    e.setParams (p);
    std::vector<float> in (512), L (512), R (512);
    std::mt19937 rng (5);
    std::uniform_real_distribution<float> d (-0.4f, 0.4f);

    auto run = [&] (float seconds, bool noise)
    {
        float peak = 0;
        const int blocks = (int) (seconds * fs) / 512;
        for (int bIdx = 0; bIdx < blocks; ++bIdx)
        {
            for (auto& x : in) x = noise ? d (rng) : 0.0f;
            e.process (in.data(), in.data(), L.data(), R.data(), 512);
            for (int i = 0; i < 512; ++i)
                peak = std::max (peak, std::fabs (L[i]) + std::fabs (R[i]));
        }
        return peak;
    };

    run (1.0f, true);                       // room ringing with signal
    p.roomAmount = 0.0f; e.setParams (p);
    run (3.5f, false);                      // off: tail decays through the cooldown
    p.roomAmount = 0.7f; e.setParams (p);
    const float burst = run (0.5f, false);  // re-enable on silence
    CHECK (burst < 1e-3f, "room toggle: no stale tail burst on re-enable");
}

// ---- elevation cue tests (Analytic B) -------------------------------------
// The failure these guard against: profile A scaled the pinna notch by
// cos(elevation) and by frontness, so "above", "below" and anything behind the
// listener lost their height cue entirely.

static float magDb (const float* fir, int taps, float freq, float fs)
{
    double re = 0, im = 0;
    for (int n = 0; n < taps; ++n)
    {
        const double w = -2.0 * 3.14159265358979 * (double) freq * n / (double) fs;
        re += (double) fir[n] * std::cos (w);
        im += (double) fir[n] * std::sin (w);
    }
    return 20.0f * std::log10 ((float) std::sqrt (re * re + im * im) + 1e-12f);
}

// Frequency of the deepest point in [lo, hi] — the first pinna notch.
static float notchFreq (const HrtfDatabase& db, float az, float el, float fs,
                        float lo = 3500.0f, float hi = 14000.0f)
{
    std::vector<float> L ((size_t) db.numTaps()), R ((size_t) db.numTaps());
    db.interpolate (az, el, L.data(), R.data());
    float best = lo, bestDb = 1e9f;
    for (float f = lo; f <= hi; f += 50.0f)
    {
        const float d = magDb (L.data(), db.numTaps(), f, fs);
        if (d < bestDb) { bestDb = d; best = f; }
    }
    return best;
}

// Mean absolute spectral difference between two directions, over a band.
static float spectralDiffDb (const HrtfDatabase& db, float az1, float el1,
                             float az2, float el2, float fs,
                             float lo = 4000.0f, float hi = 14000.0f)
{
    const int taps = db.numTaps();
    std::vector<float> a ((size_t) taps), b ((size_t) taps), t1 ((size_t) taps), t2 ((size_t) taps);
    db.interpolate (az1, el1, a.data(), t1.data());
    db.interpolate (az2, el2, b.data(), t2.data());
    double acc = 0; int count = 0;
    for (float f = lo; f <= hi; f += 100.0f)
    {
        acc += std::fabs (magDb (a.data(), taps, f, fs) - magDb (b.data(), taps, f, fs));
        ++count;
    }
    return (float) (acc / std::max (1, count));
}

static void testElevationNotchMovesMonotonically()
{
    const float fs = 48000.0f;
    HrtfDatabase db;
    db.generateAnalytic (fs, 0.0875f, HrtfDatabase::AnalyticB);
    float prev = 0.0f;
    for (float el : { -90.0f, -60.0f, -30.0f, 0.0f, 30.0f, 60.0f, 90.0f })
    {
        const float f = notchFreq (db, 0.0f, el, fs);
        CHECK (f > prev + 200.0f, "elevation: notch centre rises monotonically with elevation");
        prev = f;
    }
    // and it actually sweeps a useful range, not a token amount
    const float low = notchFreq (db, 0.0f, -90.0f, fs);
    const float high = notchFreq (db, 0.0f, 90.0f, fs);
    CHECK (high > low * 2.0f, "elevation: notch sweeps more than an octave");
}

static void testElevationSpectraDiffer()
{
    const float fs = 48000.0f;
    HrtfDatabase b;
    b.generateAnalytic (fs, 0.0875f, HrtfDatabase::AnalyticB);

    HrtfDatabase a;
    a.generateAnalytic (fs, 0.0875f, HrtfDatabase::AnalyticA);

    const float frontB = spectralDiffDb (b, 0.0f, -60.0f, 0.0f, 60.0f, fs);
    const float polesB = spectralDiffDb (b, 0.0f, -90.0f, 0.0f, 90.0f, fs);
    const float rearB  = spectralDiffDb (b, 180.0f, -60.0f, 180.0f, 60.0f, fs);
    const float polesA = spectralDiffDb (a, 0.0f, -90.0f, 0.0f, 90.0f, fs);
    std::printf ("  [elevation dB spread] front B=%.2f  poles B=%.2f  rear B=%.2f  poles A=%.2f\n",
                 frontB, polesB, rearB, polesA);

    CHECK (frontB > 4.0f, "elevation: -60 vs +60 spectra clearly differ (front)");
    CHECK (polesB > 4.0f, "elevation: straight down vs straight up clearly differ");
    // the cue must survive behind the listener, where profile A had none at all
    CHECK (rearB > 2.5f, "elevation: cue retained behind the listener");
    // regression guard: profile A really is the weak one, so a future edit that
    // silently reverts the model gets caught
    CHECK (polesB > polesA * 2.0f, "elevation: profile B separates the poles far better than A");
}

static void testProfileSwitchIsClean()
{
    const float fs = 48000.0f;
    BinauralEngine e; e.prepare (fs, 512);
    EngineParams p; p.azimuthDeg = 25.0f; p.elevationDeg = 45.0f;
    p.distanceM = 1.0f; p.roomAmount = 0.0f;
    const int n = (int) (2.0f * fs);
    std::vector<float> in (512), L (n, 0.0f), R (n, 0.0f);
    double ph = 0.0; const double dph = 2.0 * 3.14159265358979 * 440.0 / fs;
    for (int pos = 0; pos < n; pos += 512)
    {
        const int m = std::min (512, n - pos);
        p.hrtfProfile = ((pos / (int) (0.4f * fs)) % 2);
        e.setParams (p);
        for (int i = 0; i < m; ++i) { in[(size_t) i] = 0.25f * (float) std::sin (ph); ph += dph; }
        e.process (in.data(), in.data(), L.data() + pos, R.data() + pos, m);
    }
    CHECK (allFinite (L) && allFinite (R), "profile switch: no NaN");
    float maxJump = 0;
    for (int i = (int) (0.2f * fs); i < n; ++i)
        maxJump = std::max (maxJump, std::fabs (L[i] - L[i - 1]));
    CHECK (maxJump < 0.08f, "profile switch: rides the crossfade, no click");
}

int main()
{
    std::printf ("NekoSpace DSP tests\n");
    testMirrorSymmetry();
    testDistanceMonotonic();
    testRoomZeroIsExactDirect();
    testAutomationSweepNoNanNoClick();
    testBlockSizeInvariance();
    testNearFieldEarApproach();
    testInPlaceEquivalence();
    testLatencyReported();
    testQualitySwitchNoClick();
    testEarlyLateAutomationNoClick();
    testBottomElevationCentered();
    testSampleRatesFinite();
    testRoomToggleNoBurst();
    testElevationNotchMovesMonotonically();
    testElevationSpectraDiffer();
    testProfileSwitchIsClean();
    if (failures == 0) { std::printf ("ALL PASS\n"); return 0; }
    std::printf ("%d failure(s)\n", failures);
    return 1;
}
