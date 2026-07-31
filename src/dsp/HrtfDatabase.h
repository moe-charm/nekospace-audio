// SPDX-FileCopyrightText: 2026 TextureVoice
// SPDX-License-Identifier: AGPL-3.0-or-later

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

inline BiquadCoeffs makeHighShelf (float sr, float fc, float gainDb)
{
    BiquadCoeffs c;
    const float A = std::pow (10.0f, gainDb / 40.0f);
    const float w0 = 2.0f * kPi * clampf (fc, 40.0f, sr * 0.45f) / sr;
    const float cw = std::cos (w0), sw = std::sin (w0);
    const float alpha = sw * 0.5f * std::sqrt (2.0f);
    const float tsa = 2.0f * std::sqrt (A) * alpha;
    const float a0 = (A + 1.0f) - (A - 1.0f) * cw + tsa;
    c.b0 =  A * ((A + 1.0f) + (A - 1.0f) * cw + tsa) / a0;
    c.b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cw) / a0;
    c.b2 =  A * ((A + 1.0f) + (A - 1.0f) * cw - tsa) / a0;
    c.a1 =  2.0f * ((A - 1.0f) - (A + 1.0f) * cw) / a0;
    c.a2 = ((A + 1.0f) - (A - 1.0f) * cw - tsa) / a0;
    return c;
}

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

    // A: original model — elevation cue fades to nothing at the poles and behind.
    // B: elevation cue survives everywhere (see buildDirectionB).
    enum Profile { AnalyticA = 0, AnalyticB = 1, kNumProfiles = 2 };

    void generateAnalytic (float sampleRate, float headRadius, Profile p = AnalyticB)
    {
        sr = sampleRate;
        profile = p;
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

    // Both profiles: Brown–Duda spherical head shadow + pinna cues.
    // ITD is intentionally NOT encoded here (time-aligned grid; geometry supplies ITD).
    void buildDirection (const Vec3& dir, float azDeg, float elDeg, float headRadius,
                         float* outL, float* outR)
    {
        for (int ear = 0; ear < 2; ++ear)
        {
            const float sign = ear == 0 ? -1.0f : 1.0f;
            const Vec3 earDir = earDirection (sign);
            const float cosTheta = clampf (dir.dot (earDir), -1.0f, 1.0f);

            // head shadow: H(s) = (1 + a*s/2w0)/(1 + s/2w0), bilinear transform
            const float alpha = 1.0f + cosTheta;                    // 2 facing ear, 0 opposite
            const float w0 = kSpeedOfSound / headRadius;            // rad/s
            const float K = 2.0f * sr / (2.0f * w0);                // = sr / w0
            const float sb0 = (1.0f + alpha * K) / (1.0f + K);
            const float sb1 = (1.0f - alpha * K) / (1.0f + K);
            const float sa1 = (1.0f - K) / (1.0f + K);

            BiquadCoeffs s1, s2, s3;
            if (profile == AnalyticA)
            {
                // Legacy. The notch depth is scaled by cos(elevation) and by frontness,
                // so the elevation cue vanishes at the poles and behind the listener —
                // which is exactly why "above"/"below" were hard to hear. Kept only for
                // A/B comparison.
                const float frontness = 0.5f + 0.5f * std::cos (deg2rad (azDeg));
                const float notchFc = 7000.0f * std::pow (2.0f, elDeg / 110.0f);
                const float notchDb = -8.0f * frontness
                                      * std::cos (deg2rad (clampf (elDeg, -90.0f, 90.0f)));
                s1 = makeNotch (sr, notchFc, 4.0f, notchDb);
                s2 = makeNotch (sr, 11000.0f, 1.2f,
                                3.5f * std::max (0.0f, std::sin (deg2rad (elDeg))));
                s3 = BiquadCoeffs{};                                // unused
            }
            else
            {
                // Analytic B — elevation cues that survive at the poles and behind.
                //
                // Real pinna cues are a moving notch/peak pair, not a notch that fades:
                // the first pinna notch sweeps upward in frequency as a source rises
                // (Hebrank & Wright; Langendijk & Bronkhorst), while its depth stays
                // roughly constant. Sources below additionally lose HF to torso shadow.
                const float elNorm = clampf (elDeg, -90.0f, 90.0f) / 90.0f;   // -1 .. +1
                const float horiz  = std::fabs (std::cos (deg2rad (elDeg)));  // 1 horizon, 0 pole

                // Front/back weighting, floored so the rear keeps most of the cue, and
                // blended toward its mean at the poles (straight up/down has no azimuth).
                constexpr float wMean = 0.8f;
                const float wAz = 0.45f + 0.55f * (0.5f + 0.5f * std::cos (deg2rad (azDeg)));
                const float w = wMean + (wAz - wMean) * horiz;

                // N1: geometric sweep 4.2 kHz (below) -> 11.5 kHz (above), monotonic in
                // elevation. Depth never scales to zero. Measured HRIRs show 10-20 dB
                // here, so a shallow notch simply is not audible as height.
                const float fN = 4200.0f * std::pow (11500.0f / 4200.0f,
                                                     (elNorm + 1.0f) * 0.5f);
                const float notchDb = -(12.0f + 4.0f * elNorm) * w;     // -8 .. -16 dB
                // P1: companion peak below the notch — the pair is what reads as height
                const float peakDb = (4.0f + 2.0f * elNorm) * w;        // +2 .. +6 dB
                // HF balance: up gains air, down loses it to torso shadow
                const float shelfDb = 6.5f * elNorm * w;

                s1 = makeNotch (sr, fN, 3.5f, notchDb);
                s2 = makeNotch (sr, fN * 0.62f, 1.2f, peakDb);
                s3 = makeHighShelf (sr, 8000.0f, shelfDb);
            }

            float* out = ear == 0 ? outL : outR;
            float sz = 0;                                    // shadow filter state
            float az1 = 0, az2 = 0, bz1 = 0, bz2 = 0, cz1 = 0, cz2 = 0;  // biquads (DF2T)
            for (int t = 0; t < taps; ++t)
            {
                const float x = t == 0 ? 1.0f : 0.0f;
                // first-order shadow (direct form 2 transposed)
                float y = sb0 * x + sz;
                sz = sb1 * x - sa1 * y;
                // notch
                float y2 = s1.b0 * y + az1;
                az1 = s1.b1 * y - s1.a1 * y2 + az2;
                az2 = s1.b2 * y - s1.a2 * y2;
                // companion peak / legacy lift
                float y3 = s2.b0 * y2 + bz1;
                bz1 = s2.b1 * y2 - s2.a1 * y3 + bz2;
                bz2 = s2.b2 * y2 - s2.a2 * y3;
                // elevation shelf (profile B only; identity for A)
                float y4 = s3.b0 * y3 + cz1;
                cz1 = s3.b1 * y3 - s3.a1 * y4 + cz2;
                cz2 = s3.b2 * y3 - s3.a2 * y4;
                out[t] = y4;
            }
        }
    }

    std::vector<float> firs;
    float sr = 48000.0f;
    int taps = kMaxTaps;
    Profile profile = AnalyticB;
};
} // namespace nsb
