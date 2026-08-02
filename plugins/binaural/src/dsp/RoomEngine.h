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
    // How long the tail rings, in seconds, INDEPENDENT of size. These used to be one
    // control (t60 derived from size), which made the most useful room in audio drama
    // impossible to build: a tiled bathroom is small and rings for the best part of a
    // second, and deriving decay from size forces you to choose one or the other.
    float decaySeconds = 0.8f;
};

// The size-linked decay curve used before decay became its own control. Kept as the
// migration path for projects saved under state schema 2, so they reload sounding the
// same rather than picking up the new default; see docs/state-format.md rule 8.
inline float legacyDecayForSize (float size) noexcept
{
    return 0.25f + 2.4f * size * size;   // 0.25 - 2.65 s
}

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
            lfoInc[i] = kTwoPi * lfoHz[i] / sr;
            lfoPhase[i] = (float) i * 0.7853981f;   // spread the starts around the circle
        }
        setRoom ({});
        for (int i = 0; i < kLines; ++i) { lenSm[i].snap (len[i]); fbSm[i].snap (fb[i]); }
    }

    void reset() { for (int i = 0; i < kLines; ++i) { lines[i].reset(); damp[i].reset(); } }

    void setRoom (const RoomParams& rp) noexcept
    {
        // Size sets the delay lengths — the room's dimensions and modal density.
        // Decay sets how long it rings. Keeping them apart is what makes "small and
        // long" (bathroom, tiled corridor, stairwell) reachable at all.
        const float scale = (0.35f + 0.85f * rp.size) * (sr / 48000.0f);
        const float t60 = clampf (rp.decaySeconds, 0.15f, 4.0f);
        tail = t60;
        const float dampFc = 13000.0f - 10500.0f * rp.damping;
        for (int i = 0; i < kLines; ++i)
        {
            len[i] = clampf ((float) baseLen[i] * scale, 32.0f, (float) (maxLen[i] - 8));
            fb[i]  = std::pow (10.0f, -3.0f * len[i] / (t60 * sr));
            lenSm[i].setTarget (len[i]);   // smoothed per sample: size automation
            fbSm[i].setTarget (fb[i]);     // never steps the tail discontinuously
            damp[i].setCutoff (dampFc);
            // Depth is a fraction of the line, so it scales with size and sample rate
            // on its own. Small: eight fixed-length lines ring metallically, and a few
            // samples of wander is enough to break that up. Large enough to hear as
            // pitch movement on a sustained vowel is far too much for a voice reverb.
            modDepth[i] = clampf (len[i] * 0.0016f, 0.8f, 5.0f);
        }
    }

    float tailSeconds() const noexcept { return tail + 0.3f; }

    void process (const float* monoIn, float* outL, float* outR, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            float d[kLines];
            for (int k = 0; k < kLines; ++k)
            {
                // Slow, mutually incommensurate wander on each line length. Without it
                // eight fixed delays beat against each other and the tail rings.
                const float m = modDepth[k] * std::sin (lfoPhase[k]);
                lfoPhase[k] += lfoInc[k];
                if (lfoPhase[k] >= kTwoPi) lfoPhase[k] -= kTwoPi;

                const float rd = clampf (lenSm[k].next() + m, 2.0f, (float) (maxLen[k] - 2));
                d[k] = damp[k].process (lines[k].read (rd) * fbSm[k].next());
            }

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
    static constexpr float kTwoPi = 2.0f * kPi;
    static constexpr int baseLen[kLines] = { 1123, 1327, 1523, 1723, 1931, 2129, 2333, 2539 };
    static constexpr float inGain[kLines] = { 0.5f, -0.4f, 0.45f, -0.35f, 0.4f, -0.45f, 0.35f, -0.5f };
    // Deliberately not harmonically related: shared factors would put the lines back in
    // step periodically, which is the flutter the modulation exists to remove.
    static constexpr float lfoHz[kLines] = { 0.31f, 0.43f, 0.57f, 0.67f,
                                             0.79f, 0.91f, 1.03f, 1.17f };
    FractionalDelay lines[kLines];
    OnePoleLP damp[kLines];
    LinearSmoother lenSm[kLines], fbSm[kLines];
    int maxLen[kLines] = {};
    float len[kLines] = {}, fb[kLines] = {};
    float modDepth[kLines] = {}, lfoPhase[kLines] = {}, lfoInc[kLines] = {};
    float sr = 48000.0f, tail = 1.0f;
};
} // namespace nsb
