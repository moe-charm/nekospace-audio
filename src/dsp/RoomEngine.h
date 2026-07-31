#pragma once
// Room engine: 6 first-order shoebox reflections + 8-line FDN late reverb.
// room.amount == 0 must reduce to exact direct rendering (Contract #13) — the
// engine simply never calls this when amount is 0. JUCE-free.
#include <vector>
#include <cmath>
#include "Geometry.h"
#include "FractionalDelay.h"

namespace nsb
{
struct RoomParams
{
    float size    = 0.35f;  // 0..1
    float damping = 0.5f;   // 0..1
    float earlyLate = 0.35f;// 0 = all early, 1 = all late
};

class EarlyReflections
{
public:
    static constexpr int kNumRefl = 6;

    void prepare (float sampleRate, int /*maxBlock*/)
    {
        sr = sampleRate;
        line.prepare ((int) (0.25f * sr) + 8); // up to 250 ms reflection paths
        for (auto& lp : dampLP) lp.prepare (sr);
        for (auto& g : gainSm) { g.prepare (sr, 0.05f); g.snap (0.0f); }
        for (auto& d : delaySm) { d.prepare (sr, 0.05f); d.snap (100.0f); }
        for (auto& p : panSm) { p.prepare (sr, 0.05f); p.snap (0.0f); }
    }

    void reset()
    {
        line.reset();
        for (auto& lp : dampLP) lp.reset();
    }

    // Recompute image sources; call at block rate (cheap).
    void update (const Vec3& srcPos, const RoomParams& rp) noexcept
    {
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

        for (int i = 0; i < kNumRefl; ++i)
        {
            const float dist = std::max (images[i].length(), 0.4f);
            delaySm[i].setTarget (clampf (dist / kSpeedOfSound * sr, 8.0f, 0.24f * sr));
            gainSm[i].setTarget (reflCoef / dist);
            // pan from image azimuth (x/z plane), equal-power
            panSm[i].setTarget (std::atan2 (images[i].x, images[i].z)); // -pi..pi
            dampLP[i].setCutoff (dampFc + earY);
        }
    }

    // monoIn: room feed. Adds into outL/outR.
    void process (const float* monoIn, float* outL, float* outR, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            line.push (monoIn[i]);
            float l = 0, r = 0;
            for (int k = 0; k < kNumRefl; ++k)
            {
                const float d = delaySm[k].next();
                const float g = gainSm[k].next();
                const float az = panSm[k].next();
                float v = dampLP[k].process (line.read (d) * g);
                // equal-power pan; small widen from |az| toward rear
                const float p = clampf (std::sin (az), -1.0f, 1.0f) * 0.5f + 0.5f;
                l += v * std::cos (p * kPi * 0.5f);
                r += v * std::sin (p * kPi * 0.5f);
            }
            outL[i] += l;
            outR[i] += r;
        }
    }

private:
    FractionalDelay line;
    OnePoleLP dampLP[kNumRefl];
    LinearSmoother gainSm[kNumRefl], delaySm[kNumRefl], panSm[kNumRefl];
    float sr = 48000.0f;
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
