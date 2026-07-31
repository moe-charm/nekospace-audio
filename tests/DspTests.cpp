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
                      std::vector<float>& L, std::vector<float>& R, unsigned seed = 7)
{
    const int n = (int) (seconds * fs);
    std::vector<float> inL (n), inR (n);
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> d (-0.5f, 0.5f);
    float lp = 0;
    for (int i = 0; i < n; ++i) { lp = 0.9f * lp + 0.1f * d (rng); inL[i] = inR[i] = lp * 3.0f; }
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
        renderAt (e, p, 0.6f, fs, L, R);
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

int main()
{
    std::printf ("NekoSpace DSP tests\n");
    testMirrorSymmetry();
    testDistanceMonotonic();
    testRoomZeroIsExactDirect();
    testAutomationSweepNoNanNoClick();
    testBlockSizeInvariance();
    testNearFieldEarApproach();
    if (failures == 0) { std::printf ("ALL PASS\n"); return 0; }
    std::printf ("%d failure(s)\n", failures);
    return 1;
}
