#pragma once
// HRTF grid: time-aligned (ITD-free) FIRs on an az/el grid + bilinear interpolation.
// v1 ships the procedural "Analytic A" profile; measured data (KEMAR/KU100) will fill
// the same grid via the .bhrtf pipeline later (docs/hrtf-format.md). JUCE-free.
#include <vector>
#include <cmath>
#include "Geometry.h"

namespace nsb
{
struct BiquadCoeffs { float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

inline BiquadCoeffs makeNotch (float sr, float fc, float q, float gainDb)
{
    // RBJ peaking EQ used as a cut — gentler than a pure notch, controllable depth
    BiquadCoeffs c;
    const float A = std::pow (10.0f, gainDb / 40.0f);
    const float w0 = 2.0f * kPi * clampf (fc, 40.0f, sr * 0.45f) / sr;
    const float alpha = std::sin (w0) / (2.0f * q);
    const float cw = std::cos (w0);
    const float a0 = 1.0f + alpha / A;
    c.b0 = (1.0f + alpha * A) / a0;
    c.b1 = (-2.0f * cw) / a0;
    c.b2 = (1.0f - alpha * A) / a0;
    c.a1 = (-2.0f * cw) / a0;
    c.a2 = (1.0f - alpha / A) / a0;
    return c;
}

class HrtfDatabase
{
public:
    static constexpr int kNumAz = 72;             // 0..355 step 5
    static constexpr int kNumEl = 13;             // -90..+90 step 15 (poles included:
    static constexpr float kElMin = -90.0f, kElStep = 15.0f, kAzStep = 5.0f;
    // at el = ±90 the direction vector is azimuth-independent, so every az slot holds
    // the same FIR and interpolation stays lateral-bias-free straight up/down)
    static constexpr int kMaxTaps = 256;

    void generateAnalytic (float sampleRate, float headRadius)
    {
        sr = sampleRate;
        // keep the HRIR window a constant ~2.7 ms so Economy/Standard timbre does not
        // depend on the session sample rate (128 taps @48k). Capped at 256: the analytic
        // profile's filters have fully decayed within ~1.3 ms, so 192 kHz loses nothing
        // audible while time-domain convolution cost stays bounded.
        taps = (int) clampf (128.0f * sampleRate / 48000.0f + 0.5f, 64.0f, (float) kMaxTaps);
        firs.assign ((size_t) kNumAz * kNumEl * 2 * (size_t) taps, 0.0f);

        for (int ei = 0; ei < kNumEl; ++ei)
        {
            const float el = kElMin + kElStep * (float) ei;
            for (int ai = 0; ai < kNumAz; ++ai)
            {
                const float az = kAzStep * (float) ai;
                const Vec3 dir = directionFromAngles (az, el);
                buildDirection (dir, az, el, headRadius, firAt (ei, ai, 0), firAt (ei, ai, 1));
            }
        }
    }

    // Bilinear interpolation over the grid. outL/outR must hold numTaps floats.
    void interpolate (float azDeg, float elDeg, float* outL, float* outR) const noexcept
    {
        azDeg = azDeg - std::floor (azDeg / 360.0f) * 360.0f;      // -> [0, 360)
        const float elC = clampf (elDeg, kElMin, kElMin + kElStep * (float) (kNumEl - 1));

        const float af = azDeg / kAzStep;
        int a0 = (int) af % kNumAz;
        const int a1 = (a0 + 1) % kNumAz;
        const float aw = af - std::floor (af);

        const float ef = (elC - kElMin) / kElStep;
        int e0 = (int) ef;
        if (e0 > kNumEl - 2) e0 = kNumEl - 2;
        const int e1 = e0 + 1;
        const float ew = clampf (ef - (float) e0, 0.0f, 1.0f);

        const float w00 = (1 - aw) * (1 - ew), w10 = aw * (1 - ew);
        const float w01 = (1 - aw) * ew,       w11 = aw * ew;

        for (int ear = 0; ear < 2; ++ear)
        {
            const float* f00 = firAtConst (e0, a0, ear);
            const float* f10 = firAtConst (e0, a1, ear);
            const float* f01 = firAtConst (e1, a0, ear);
            const float* f11 = firAtConst (e1, a1, ear);
            float* out = ear == 0 ? outL : outR;
            for (int t = 0; t < taps; ++t)
                out[t] = w00 * f00[t] + w10 * f10[t] + w01 * f01[t] + w11 * f11[t];
        }
    }

    int numTaps() const noexcept { return taps; }

private:
    float* firAt (int ei, int ai, int ear) noexcept
    {
        return firs.data() + (((size_t) ei * kNumAz + (size_t) ai) * 2 + (size_t) ear) * (size_t) taps;
    }
    const float* firAtConst (int ei, int ai, int ear) const noexcept
    {
        return firs.data() + (((size_t) ei * kNumAz + (size_t) ai) * 2 + (size_t) ear) * (size_t) taps;
    }

    // Analytic A: Brown–Duda spherical head shadow + pinna elevation cues.
    // ITD is intentionally NOT encoded here (time-aligned grid; geometry supplies ITD).
    void buildDirection (const Vec3& dir, float azDeg, float elDeg, float headRadius,
                         float* outL, float* outR)
    {
        for (int ear = 0; ear < 2; ++ear)
        {
            const float sign = ear == 0 ? -1.0f : 1.0f;
            const Vec3 earDir = earDirection (sign);
            const float cosTheta = clampf (dir.dot (earDir), -1.0f, 1.0f);

            // 1) head shadow: H(s) = (1 + a*s/2w0)/(1 + s/2w0), bilinear transform
            const float alpha = 1.0f + cosTheta;                    // 2 facing ear, 0 opposite
            const float w0 = kSpeedOfSound / headRadius;            // rad/s
            const float K = 2.0f * sr / (2.0f * w0);                // = sr / w0
            const float sb0 = (1.0f + alpha * K) / (1.0f + K);
            const float sb1 = (1.0f - alpha * K) / (1.0f + K);
            const float sa1 = (1.0f - K) / (1.0f + K);

            // 2) pinna notch: frontal & low-elevation emphasis, center tracks elevation
            const float frontness = 0.5f + 0.5f * std::cos (deg2rad (azDeg));
            const float notchFc = 7000.0f * std::pow (2.0f, elDeg / 110.0f);
            const float notchDb = -8.0f * frontness * std::cos (deg2rad (clampf (elDeg, -90.0f, 90.0f)));
            const BiquadCoeffs nc = makeNotch (sr, notchFc, 4.0f, notchDb);

            // 3) small HF lift for sources above
            const float upDb = 3.5f * std::max (0.0f, std::sin (deg2rad (elDeg)));
            const BiquadCoeffs up = makeNotch (sr, 11000.0f, 1.2f, upDb);

            float* out = ear == 0 ? outL : outR;
            float sz = 0;                                    // shadow filter state
            float nz1 = 0, nz2 = 0, uz1 = 0, uz2 = 0;        // biquad states (DF2T)
            for (int t = 0; t < taps; ++t)
            {
                const float x = t == 0 ? 1.0f : 0.0f;
                // first-order shadow (direct form 2 transposed)
                float y = sb0 * x + sz;
                sz = sb1 * x - sa1 * y;
                // pinna notch
                float y2 = nc.b0 * y + nz1;
                nz1 = nc.b1 * y - nc.a1 * y2 + nz2;
                nz2 = nc.b2 * y - nc.a2 * y2;
                // elevation lift
                float y3 = up.b0 * y2 + uz1;
                uz1 = up.b1 * y2 - up.a1 * y3 + uz2;
                uz2 = up.b2 * y2 - up.a2 * y3;
                out[t] = y3;
            }
        }
    }

    std::vector<float> firs;
    float sr = 48000.0f;
    int taps = kMaxTaps;
};
} // namespace nsb
