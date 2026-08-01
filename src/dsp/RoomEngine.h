// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// Room engine: 6 first-order shoebox reflections + 8-line FDN late reverb.
// room.amount == 0 must reduce to exact direct rendering (Contract #13) — the
// engine simply never calls this when amount is 0. JUCE-free.
#include <vector>
#include <cmath>
#include <algorithm>
#include "Geometry.h"
#include "FractionalDelay.h"
#include "HrtfDatabase.h"
#include "FirConvolver.h"

namespace nsb
{
struct RoomParams
{
    float size    = 0.35f;  // 0..1
    float damping = 0.5f;   // 0..1
    float earlyLate = 0.35f;// 0 = all early, 1 = all late
};

// Six first-order shoebox reflections, each rendered through the HRTF at its own image
// direction rather than being panned. That is what makes a source above the listener
// produce a floor bounce that genuinely arrives from below — an elevation cue that works
// regardless of how well the listener's pinnae match the dataset, which the direct-path
// spectral cue does not.
class EarlyReflections
{
public:
    static constexpr int kNumRefl = 6;

    void prepare (float sampleRate, int maxBlock, const HrtfDatabase* db)
    {
        sr = sampleRate;
        // A quarter of the direct path's length: enough for the pinna notch (a Q=3.5
        // notch at 4 kHz settles inside ~13 samples at 48 kHz) at a fraction of the cost.
        reflTaps = std::max (16, db->numTaps() / 4);
        line.prepare ((int) (0.25f * sr) + 8); // up to 250 ms reflection paths
        for (int k = 0; k < kNumRefl; ++k)
        {
            for (int e = 0; e < 2; ++e)
            {
                dampLP[k][e].prepare (sr);
                fir[k][e].prepare (reflTaps, maxBlock, (int) (0.015f * sr));
                gainSm[k][e].prepare (sr, 0.05f);  gainSm[k][e].snap (0.0f);
                delaySm[k][e].prepare (sr, 0.05f); delaySm[k][e].snap (100.0f);
                scratch[k][e].assign ((size_t) maxBlock, 0.0f);
            }
        }
        coefL.assign ((size_t) db->numTaps(), 0.0f);
        coefR.assign ((size_t) db->numTaps(), 0.0f);
        primed = false;
        lastValid = false;
    }

    void reset()
    {
        line.reset();
        for (int k = 0; k < kNumRefl; ++k)
            for (int e = 0; e < 2; ++e) { dampLP[k][e].reset(); fir[k][e].reset(); }
        primed = false;
    }

    // Recompute image sources; call at block rate (cheap, and skipped when nothing moved).
    void update (const HrtfDatabase* db, const Vec3& srcPos, const RoomParams& rp,
                 float headRadius) noexcept
    {
        if (lastValid && db == lastDb && srcPos.x == lastPos.x && srcPos.y == lastPos.y
            && srcPos.z == lastPos.z && rp.size == lastSize && rp.damping == lastDamping)
            return;
        lastDb = db; lastPos = srcPos; lastSize = rp.size; lastDamping = rp.damping;
        lastValid = true;
        // shoebox scales with size: 3–14 m wide, listener at center
        const float W = 3.0f + 11.0f * rp.size;    // x extent
        const float D = 3.5f + 12.0f * rp.size;    // z extent
        const float H = 2.4f + 3.6f * rp.size;     // y extent
        const float hw = W * 0.5f, hd = D * 0.5f;
        const float earY = 0.0f;                    // listener ear height = origin
        const float floorY = -1.4f, ceilY = H - 1.4f;

        Vec3 s = srcPos;
        s.x = clampf (s.x, -hw + 0.1f, hw - 0.1f);
        s.z = clampf (s.z, -hd + 0.1f, hd - 0.1f);
        s.y = clampf (s.y, floorY + 0.1f, ceilY - 0.1f);

        const Vec3 images[kNumRefl] = {
            { -2.0f * hw - s.x, s.y, s.z },        // left wall
            {  2.0f * hw - s.x, s.y, s.z },        // right wall
            { s.x, s.y,  2.0f * hd - s.z },        // front wall
            { s.x, s.y, -2.0f * hd - s.z },        // back wall
            { s.x, 2.0f * floorY - s.y, s.z },     // floor
            { s.x, 2.0f * ceilY  - s.y, s.z },     // ceiling
        };

        const float reflCoef = 0.72f;
        const float dampFc = 12000.0f - 9500.0f * rp.damping;

        (void) earY;
        for (int i = 0; i < kNumRefl; ++i)
        {
            const float dist = std::max (images[i].length(), 0.4f);
            const Vec3 dir { images[i].x / dist, images[i].y / dist, images[i].z / dist };
            const float azDeg = wrapDeg (juceless_atan2Deg (dir.x, dir.z));
            const float elDeg = clampf (std::asin (clampf (dir.y, -1.0f, 1.0f)) * 180.0f / kPi,
                                        -90.0f, 90.0f);
            dampLP[i][0].setCutoff (dampFc);
            dampLP[i][1].setCutoff (dampFc);

            // per-ear arrival: far-field head geometry is enough for an image several
            // metres away, and it gives the reflection a correct ITD
            for (int e = 0; e < 2; ++e)
            {
                const float sign = e == 0 ? -1.0f : 1.0f;
                const float path = dist + earPathOffsetFarField (dir, headRadius, sign);
                delaySm[i][e].setTarget (clampf (path / kSpeedOfSound * sr, 8.0f, 0.24f * sr));
                gainSm[i][e].setTarget (reflCoef / dist);
            }

            // directional filter for this image
            db->interpolate (azDeg, elDeg, coefL.data(), coefR.data());
            fadeTail (coefL.data());
            fadeTail (coefR.data());
            if (! primed)
            {
                fir[i][0].setCoefficientsImmediate (coefL.data(), reflTaps);
                fir[i][1].setCoefficientsImmediate (coefR.data(), reflTaps);
            }
            else
            {
                fir[i][0].setCoefficients (coefL.data(), reflTaps);
                fir[i][1].setCoefficients (coefR.data(), reflTaps);
            }
        }
        primed = true;
    }

    // monoIn: room feed. Adds into outL/outR. n <= maxBlock.
    void process (const float* monoIn, float* outL, float* outR, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            line.push (monoIn[i]);
            for (int k = 0; k < kNumRefl; ++k)
                for (int e = 0; e < 2; ++e)
                    scratch[k][e][(size_t) i] =
                        dampLP[k][e].process (line.read (delaySm[k][e].next())
                                              * gainSm[k][e].next());
        }

        for (int k = 0; k < kNumRefl; ++k)
            for (int e = 0; e < 2; ++e)
            {
                float* s = scratch[k][e].data();
                fir[k][e].process (s, s, n);
                float* out = e == 0 ? outL : outR;
                for (int i = 0; i < n; ++i) out[i] += s[i];
            }
    }

private:
    // atan2 in degrees, kept local so the DSP core stays dependency-free
    static float juceless_atan2Deg (float y, float x) noexcept
    {
        return std::atan2 (y, x) * 180.0f / kPi;
    }

    // Soften the truncation edge so the shortened HRIR has no spectral ripple step.
    void fadeTail (float* c) const noexcept
    {
        for (int k = reflTaps - 6; k < reflTaps; ++k)
            c[k] *= 0.5f + 0.5f * std::cos (kPi * (float) (k - (reflTaps - 6)) / 6.0f);
    }

    FractionalDelay line;
    OnePoleLP dampLP[kNumRefl][2];              // one state per ear stream
    CrossfadeFir fir[kNumRefl][2];
    LinearSmoother gainSm[kNumRefl][2], delaySm[kNumRefl][2];
    std::vector<float> scratch[kNumRefl][2], coefL, coefR;
    const HrtfDatabase* lastDb = nullptr;
    Vec3 lastPos;
    float lastSize = -1.0f, lastDamping = -1.0f;
    float sr = 48000.0f;
    int reflTaps = 32;
    bool primed = false, lastValid = false;
};

class FdnReverb
{
public:
    static constexpr int kLines = 8;

    void prepare (float sampleRate, int /*maxBlock*/)
    {
        sr = sampleRate;
        for (int i = 0; i < kLines; ++i)
        {
            maxLen[i] = (int) (baseLen[i] * (sr / 48000.0f) * 2.2f) + 8;
            lines[i].prepare (maxLen[i]);
            damp[i].prepare (sr);
            lenSm[i].prepare (sr, 0.08f);
            fbSm[i].prepare (sr, 0.08f);
        }
        setRoom ({});
        for (int i = 0; i < kLines; ++i) { lenSm[i].snap (len[i]); fbSm[i].snap (fb[i]); }
    }

    void reset() { for (int i = 0; i < kLines; ++i) { lines[i].reset(); damp[i].reset(); } }

    void setRoom (const RoomParams& rp) noexcept
    {
        const float scale = (0.35f + 0.85f * rp.size) * (sr / 48000.0f);
        const float t60 = 0.25f + 2.4f * rp.size * rp.size; // 0.25–2.65 s
        tail = t60;
        const float dampFc = 13000.0f - 10500.0f * rp.damping;
        for (int i = 0; i < kLines; ++i)
        {
            len[i] = clampf ((float) baseLen[i] * scale, 32.0f, (float) (maxLen[i] - 8));
            fb[i]  = std::pow (10.0f, -3.0f * len[i] / (t60 * sr));
            lenSm[i].setTarget (len[i]);   // smoothed per sample: size automation
            fbSm[i].setTarget (fb[i]);     // never steps the tail discontinuously
            damp[i].setCutoff (dampFc);
        }
    }

    float tailSeconds() const noexcept { return tail + 0.3f; }

    void process (const float* monoIn, float* outL, float* outR, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            float d[kLines];
            for (int k = 0; k < kLines; ++k)
                d[k] = damp[k].process (lines[k].read (lenSm[k].next()) * fbSm[k].next());

            // 8x8 Hadamard via butterfly, scaled 1/sqrt(8)
            float s[kLines];
            for (int k = 0; k < 4; ++k) { s[k] = d[k] + d[k + 4]; s[k + 4] = d[k] - d[k + 4]; }
            float t[kLines];
            for (int k = 0; k < 2; ++k)
            {
                t[k]     = s[k] + s[k + 2];     t[k + 2] = s[k] - s[k + 2];
                t[k + 4] = s[k + 4] + s[k + 6]; t[k + 6] = s[k + 4] - s[k + 6];
            }
            float h[kLines];
            for (int k = 0; k < 4; ++k)
            {
                h[2 * k]     = (t[2 * k] + t[2 * k + 1]) * 0.35355339f;
                h[2 * k + 1] = (t[2 * k] - t[2 * k + 1]) * 0.35355339f;
            }

            const float x = monoIn[i];
            for (int k = 0; k < kLines; ++k)
                lines[k].push (h[k] + x * inGain[k]);

            outL[i] += (d[0] - d[1] + d[2] - d[3] + d[4] - d[5] + d[6] - d[7]) * 0.30f;
            outR[i] += (d[0] + d[1] - d[2] - d[3] + d[4] + d[5] - d[6] - d[7]) * 0.30f;
        }
    }

private:
    static constexpr int baseLen[kLines] = { 1123, 1327, 1523, 1723, 1931, 2129, 2333, 2539 };
    static constexpr float inGain[kLines] = { 0.5f, -0.4f, 0.45f, -0.35f, 0.4f, -0.45f, 0.35f, -0.5f };
    FractionalDelay lines[kLines];
    OnePoleLP damp[kLines];
    LinearSmoother lenSm[kLines], fbSm[kLines];
    int maxLen[kLines] = {};
    float len[kLines] = {}, fb[kLines] = {};
    float sr = 48000.0f, tail = 1.0f;
};
} // namespace nsb
