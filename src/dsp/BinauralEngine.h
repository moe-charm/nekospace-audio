#pragma once
// NekoSpace Binaural — engine top level. JUCE-free (Contract #4).
// Owns: input routing, per-source binaural renderers, room engine, output stage.
#include <vector>
#include <cmath>
#include <cstring>
#include "Geometry.h"
#include "FractionalDelay.h"
#include "HrtfDatabase.h"
#include "FirConvolver.h"
#include "RoomEngine.h"

namespace nsb
{
struct EngineParams
{
    float azimuthDeg   = 0.0f;    // -180..180, + = right
    float elevationDeg = 0.0f;    // -90..90
    float distanceM    = 1.0f;    // 0.05..20
    float widthDeg     = 60.0f;   // linked stereo spread
    int   sourceMode   = 0;       // 0 = Mono Object, 1 = Linked Stereo
    float nearField    = 0.75f;   // 0..1
    float headRadiusM  = 0.0875f;
    float roomAmount   = 0.15f;   // 0..1 (0 => exact direct)
    float roomSize     = 0.35f;
    float roomDamping  = 0.5f;
    float roomEarlyLate= 0.35f;
    int   qualityMode  = 1;       // 0 = Economy(64), 1 = Standard(128)
    float outputGainDb = 0.0f;
    bool  bypassRoom   = false;
};

// One mono source -> binaural stereo (adds into output buses).
class SourceRenderer
{
public:
    void prepare (float sampleRate, int maxBlock, const HrtfDatabase* db)
    {
        sr = sampleRate; hrtf = db; maxBlockSize = maxBlock;
        baseDelay = (int) (0.002f * sr + 0.5f); // reported to the host as plugin latency
        const int maxDelay = (int) (0.02f * sr) + maxBlock + 8; // base 2ms + wrap margin
        for (int e = 0; e < 2; ++e)
        {
            earDelay[e].prepare (maxDelay + (int) (0.01f * sr));
            fir[e].prepare (HrtfDatabase::kMaxTaps, maxBlock, (int) (0.010f * sr));
            airLP[e].prepare (sr);
            gainSm[e].prepare (sr, 0.020f);
            gainSm[e].snap (0.0f);
            delaySm[e].prepare (sr, 0.020f);
            delaySm[e].snap (baseDelaySamples());
        }
        scratch.assign ((size_t) maxBlock, 0.0f);
        coefL.assign (HrtfDatabase::kMaxTaps, 0.0f);
        coefR.assign (HrtfDatabase::kMaxTaps, 0.0f);
        primed = false;
    }

    void reset()
    {
        for (int e = 0; e < 2; ++e) { earDelay[e].reset(); fir[e].reset(); airLP[e].reset(); }
        primed = false;
    }

    int latencySamples() const noexcept { return baseDelay; }

    // Block-rate geometry update. Continuity across position jumps comes from the
    // FIR output crossfade + per-ear delay/gain smoothing (no angle smoothing needed,
    // so the ±180° wrap can never take the long way around). Quality (tap-count)
    // changes ride the same crossfade — never a reset, never a click.
    void update (float azDeg, float elDeg, float dist, float nearAmt, float headR,
                 int taps) noexcept
    {
        // a source can't be inside the head: effective distance floors at the skull
        const float r = clampf (dist, headR + 0.005f, kMaxDistance);
        const Vec3 dir = directionFromAngles (azDeg, elDeg);
        const Vec3 pos = dir * r;
        lastPos = pos;

        for (int e = 0; e < 2; ++e)
        {
            const float sign = e == 0 ? -1.0f : 1.0f;
            const float exact  = earPathLengthExact (pos, r, headR, sign);
            const float farOff = r + earPathOffsetFarField (dir, headR, sign);
            // nearfield.amount blends far-field-equivalent path vs exact per-ear geometry
            const float path = farOff + (exact - farOff) * nearAmt;
            const float gPath = clampf (path, 0.02f, kMaxDistance + 1.0f);

            // 1/r law referenced to 1 m, capped at +32 dB (keeps ear-whisper ILD growing
            // all the way to the skull without ever saturating both steps of an approach)
            float g = 1.0f / gPath;
            if (g > 40.0f) g = 40.0f;

            delaySm[e].setTarget (baseDelaySamples() + (path - r) / kSpeedOfSound * sr);
            gainSm[e].setTarget (g);
            // air absorption with distance
            airLP[e].setCutoff (20000.0f / std::pow (std::max (r, 1.0f), 0.55f));
        }

        // HRTF spectral part (time-aligned) — interpolate & stage with crossfade
        hrtf->interpolate (azDeg, elDeg, coefL.data(), coefR.data());
        if (taps < HrtfDatabase::kMaxTaps)
        {
            // soften the truncation edge so Economy mode has no spectral ripple step
            for (int k = taps - 8; k < taps; ++k)
            {
                const float w = 0.5f + 0.5f * std::cos (kPi * (float) (k - (taps - 8)) / 8.0f);
                coefL[(size_t) k] *= w;
                coefR[(size_t) k] *= w;
            }
        }
        if (!primed)
        {
            fir[0].setCoefficientsImmediate (coefL.data(), taps);
            fir[1].setCoefficientsImmediate (coefR.data(), taps);
            primed = true;
        }
        else
        {
            fir[0].setCoefficients (coefL.data(), taps);
            fir[1].setCoefficients (coefR.data(), taps);
        }
    }

    // monoIn -> adds into outL/outR. n <= maxBlock guaranteed by engine chunking.
    void process (const float* monoIn, float* outL, float* outR, int n) noexcept
    {
        for (int e = 0; e < 2; ++e)
        {
            float* s = scratch.data();
            for (int i = 0; i < n; ++i)
            {
                earDelay[e].push (monoIn[i]);
                float v = earDelay[e].read (delaySm[e].next());
                v *= gainSm[e].next();
                s[i] = airLP[e].process (v);
            }
            float* out = e == 0 ? outL : outR;
            fir[e].process (s, s, n); // in-place is safe: input is copied to history first
            for (int i = 0; i < n; ++i)
                out[i] += s[i];
        }
    }

    Vec3 position() const noexcept { return lastPos; }

private:
    float baseDelaySamples() const noexcept { return (float) baseDelay; }

    FractionalDelay earDelay[2];
    CrossfadeFir fir[2];
    OnePoleLP airLP[2];
    LinearSmoother gainSm[2], delaySm[2];
    std::vector<float> scratch, coefL, coefR;
    const HrtfDatabase* hrtf = nullptr;
    float sr = 48000.0f;
    int maxBlockSize = 0, baseDelay = 96;
    bool primed = false;
    Vec3 lastPos;
};

class BinauralEngine
{
public:
    static constexpr int kChunk = 512; // internal processing granularity

    void prepare (float sampleRate, int /*hostMaxBlock*/)
    {
        sr = sampleRate;
        hrtf.generateAnalytic (sr, 0.0875f);
        for (auto& s : sources) s.prepare (sr, kChunk, &hrtf);
        early.prepare (sr, kChunk);
        fdn.prepare (sr, kChunk);
        outGainSm.prepare (sr, 0.02f); outGainSm.snap (1.0f);
        roomAmtSm.prepare (sr, 0.05f); roomAmtSm.snap (0.0f); // room fades in; amount 0 stays bit-exact direct
        earlyLateSm.prepare (sr, 0.05f); earlyLateSm.snap (params.roomEarlyLate);
        modeFade.prepare (sr, 0.008f); modeFade.snap (1.0f);
        limiterRelease = std::exp (-1.0f / (0.120f * sr));
        monoBuf.assign (kChunk, 0.0f);
        roomFeed.assign (kChunk, 0.0f);
        erL.assign (kChunk, 0.0f); erR.assign (kChunk, 0.0f);
        fdnL.assign (kChunk, 0.0f); fdnR.assign (kChunk, 0.0f);
        activeMode = params.sourceMode;
        reset();
    }

    void reset()
    {
        for (auto& s : sources) s.reset();
        early.reset(); fdn.reset();
        peakL = peakR = 0.0f;
        limiterEnv = 0.0f;
    }

    int latencySamples() const noexcept { return sources[0].latencySamples(); }

    void setParams (const EngineParams& p) noexcept { params = p; }

    float tailSeconds() const noexcept
    {
        return (params.roomAmount > 0.0f && !params.bypassRoom) ? fdn.tailSeconds() : 0.0f;
    }

    // inL/inR -> outL/outR (may alias input). Any n; chunked internally.
    void process (const float* inL, const float* inR, float* outL, float* outR, int n) noexcept
    {
        int done = 0;
        while (done < n)
        {
            const int m = (n - done) < kChunk ? (n - done) : kChunk;
            processChunk (inL + done, inR + done, outL + done, outR + done, m);
            done += m;
        }
    }

    float lastPeakL() const noexcept { return peakL; }
    float lastPeakR() const noexcept { return peakR; }

private:
    void processChunk (const float* inL, const float* inR, float* outL, float* outR, int n) noexcept
    {
        const EngineParams p = params;

        // source-mode switch: quick fade-down, swap, fade-up (no clicks)
        if (p.sourceMode != activeMode && !fadingMode)
        {
            fadingMode = true;
            modeFade.setTarget (0.0f);
        }
        if (fadingMode && modeFade.value() <= 0.001f)
        {
            activeMode = p.sourceMode;
            for (auto& s : sources) s.reset();
            modeFade.setTarget (1.0f);
        }
        if (fadingMode && !modeFade.isSmoothing() && modeFade.value() >= 0.999f)
            fadingMode = false;

        const int taps = p.qualityMode == 0 ? 64 : 128;

        // ---- geometry updates (block rate) ----
        const float roomTarget = p.bypassRoom ? 0.0f : p.roomAmount;
        roomAmtSm.setTarget (roomTarget);
        const bool roomActive = roomTarget > 0.0001f || roomAmtSm.value() > 0.0001f;
        if (!roomActive && roomWasActive) { early.reset(); fdn.reset(); } // no stale tail burst on re-enable
        roomWasActive = roomActive;

        // ---- input routing ----
        float* mono = monoBuf.data();
        float* feed = roomFeed.data();

        if (activeMode == 0) // Mono Object
        {
            // downmix BEFORE clearing outputs — in-place (out == in) must stay valid
            for (int i = 0; i < n; ++i)
                mono[i] = 0.5f * (inL[i] + inR[i]);

            sources[0].update (p.azimuthDeg, p.elevationDeg, p.distanceM,
                               p.nearField, p.headRadiusM, taps);
            std::memset (outL, 0, sizeof (float) * (size_t) n);
            std::memset (outR, 0, sizeof (float) * (size_t) n);
            sources[0].process (mono, outL, outR, n);

            const float fg = 1.0f / std::max (p.distanceM, 0.25f);
            for (int i = 0; i < n; ++i)
                feed[i] = mono[i] * clampf (fg, 0.05f, 2.0f);
        }
        else // Linked Stereo: L/R inputs become two sources at az ± width/2
        {
            const float half = p.widthDeg * 0.5f;
            sources[0].update (wrapDeg (p.azimuthDeg - half), p.elevationDeg, p.distanceM,
                               p.nearField, p.headRadiusM, taps);
            sources[1].update (wrapDeg (p.azimuthDeg + half), p.elevationDeg, p.distanceM,
                               p.nearField, p.headRadiusM, taps);
            // stash inputs BEFORE clearing outputs — in-place (out == in) must stay valid
            std::memcpy (mono, inL, sizeof (float) * (size_t) n);
            std::memcpy (feed, inR, sizeof (float) * (size_t) n);
            std::memset (outL, 0, sizeof (float) * (size_t) n);
            std::memset (outR, 0, sizeof (float) * (size_t) n);
            sources[0].process (mono, outL, outR, n);
            sources[1].process (feed, outL, outR, n);

            const float fg = clampf (1.0f / std::max (p.distanceM, 0.25f), 0.05f, 2.0f) * 0.5f;
            for (int i = 0; i < n; ++i)
                feed[i] = (mono[i] + feed[i]) * fg;
        }

        // ---- room ----
        if (roomActive)
        {
            RoomParams rp { p.roomSize, p.roomDamping, p.roomEarlyLate };
            early.update (sources[0].position(), rp);
            fdn.setRoom (rp);

            std::memset (erL.data(), 0, sizeof (float) * (size_t) n);
            std::memset (erR.data(), 0, sizeof (float) * (size_t) n);
            early.process (feed, erL.data(), erR.data(), n);

            std::memset (fdnL.data(), 0, sizeof (float) * (size_t) n);
            std::memset (fdnR.data(), 0, sizeof (float) * (size_t) n);
            fdn.process (feed, fdnL.data(), fdnR.data(), n);

            earlyLateSm.setTarget (p.roomEarlyLate);
            for (int i = 0; i < n; ++i)
            {
                const float amt = roomAmtSm.next();
                const float b = earlyLateSm.next();
                outL[i] += amt * (erL[i] * (1.0f - b) + fdnL[i] * b) * 1.6f;
                outR[i] += amt * (erR[i] * (1.0f - b) + fdnR[i] * b) * 1.6f;
            }
        }

        // ---- output stage: trim, safety limiter (ear-whisper gain can reach +32 dB),
        //      then metering of what actually leaves the plugin ----
        outGainSm.setTarget (std::pow (10.0f, p.outputGainDb / 20.0f));
        constexpr float kCeiling = 0.945f; // ~ -0.5 dBFS
        float env = limiterEnv;
        float pl = peakL * 0.85f, pr = peakR * 0.85f; // block decay
        for (int i = 0; i < n; ++i)
        {
            const float g = outGainSm.next() * modeFade.next();
            float l = outL[i] * g, r = outR[i] * g;
            const float m = std::fabs (l) > std::fabs (r) ? std::fabs (l) : std::fabs (r);
            env = m > env ? m : env * limiterRelease; // instant attack, ~120 ms release
            if (env > kCeiling)
            {
                const float lg = kCeiling / env;
                l *= lg; r *= lg;
            }
            outL[i] = l; outR[i] = r;
            if (std::fabs (l) > pl) pl = std::fabs (l);
            if (std::fabs (r) > pr) pr = std::fabs (r);
        }
        limiterEnv = env;
        peakL = pl; peakR = pr;
    }

    EngineParams params;
    HrtfDatabase hrtf;
    SourceRenderer sources[2];
    EarlyReflections early;
    FdnReverb fdn;
    LinearSmoother outGainSm, roomAmtSm, earlyLateSm, modeFade;
    std::vector<float> monoBuf, roomFeed, erL, erR, fdnL, fdnR;
    float sr = 48000.0f;
    float limiterEnv = 0.0f, limiterRelease = 0.9998f;
    float peakL = 0.0f, peakR = 0.0f;
    int activeMode = 0;
    bool fadingMode = false;
    bool roomWasActive = false;
};
} // namespace nsb
