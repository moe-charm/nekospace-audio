// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// NekoSpace Binaural — engine top level. JUCE-free (Contract #4).
// Owns: input routing, per-source binaural renderers, room engine, output stage.
#include <vector>
#include <cmath>
#include <cstring>
#include <atomic>
#include "Geometry.h"
#include "FractionalDelay.h"
#include "HrtfDatabase.h"
#include "FirConvolver.h"
#include "RoomEngine.h"
#include "VoiceDuck.h"

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
    float duckAmount   = 0.5f;    // 0..1, how far the late bus is held down while speaking
    float duckRelease  = 0.45f;   // seconds for the room to reappear after a phrase
    int   qualityMode  = 1;       // 0 = Economy(half taps), 1 = Standard(full)
    int   hrtfProfile  = 1;       // 0 = Analytic A (legacy), 1 = Analytic B, 2 = measured
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
            fir[e].prepare (db->numTaps(), maxBlock, (int) (0.010f * sr));
            airLP[e].prepare (sr);
            gainSm[e].prepare (sr, 0.020f);
            gainSm[e].snap (0.0f);
            delaySm[e].prepare (sr, 0.020f);
            delaySm[e].snap (baseDelaySamples());
        }
        scratch.assign ((size_t) maxBlock, 0.0f);
        coefL.assign ((size_t) db->numTaps(), 0.0f);
        coefR.assign ((size_t) db->numTaps(), 0.0f);
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
    void update (const HrtfDatabase* db, float azDeg, float elDeg, float dist,
                 float nearAmt, float headR, int taps) noexcept
    {
        hrtf = db;
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

            // Timing is always per-ear: ITD is the primary localisation cue at every
            // distance, so it is never switched off. nearfield.amount only moves it from
            // the Woodworth far-field approximation to the exact sphere path, which
            // matters once the source is within a few head radii.
            const float timingPath = farOff + (exact - farOff) * nearAmt;
            delaySm[e].setTarget (baseDelaySamples()
                                  + (timingPath - r) / kSpeedOfSound * sr);

            // Level is what the control really spans. At 0 % both ears are attenuated by
            // the same 1/r, exactly as a conventional panner would, and the whole level
            // difference comes from the head-shadow filter in the HRTF. At 100 % each ear
            // gets its own distance, so a source at the left ear is dramatically louder
            // there — the ear-whisper effect. Anything in between is a blend.
            const float levelPath = r + (exact - r) * nearAmt;
            const float gPath = clampf (levelPath, 0.02f, kMaxDistance + 1.0f);

            // 1/r law referenced to 1 m, capped at +32 dB (keeps ear-whisper ILD growing
            // all the way to the skull without ever saturating both steps of an approach)
            float g = 1.0f / gPath;
            if (g > 40.0f) g = 40.0f;
            gainSm[e].setTarget (g);
            // air absorption with distance
            airLP[e].setCutoff (20000.0f / std::pow (std::max (r, 1.0f), 0.55f));
        }

        // HRTF spectral part (time-aligned) — interpolate & stage with crossfade.
        // Skipped entirely while the direction is static so no crossfade restarts and
        // the steady-state convolver runs single-bank (no redundant CPU).
        const bool coeffsDirty = !primed || azDeg != lastAz || elDeg != lastEl
                                 || taps != lastTaps || db != lastDb;
        if (coeffsDirty)
        {
            hrtf->interpolate (azDeg, elDeg, coefL.data(), coefR.data());
            if (taps < hrtf->numTaps())
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
            lastAz = azDeg; lastEl = elDeg; lastTaps = taps; lastDb = db;
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
    float lastAz = 1e9f, lastEl = 1e9f;
    int lastTaps = -1;
    const HrtfDatabase* lastDb = nullptr;
    bool primed = false;
    Vec3 lastPos;
};

class BinauralEngine
{
public:
    static constexpr int kChunk = 512; // internal processing granularity

    // Optional measured pack (see tools/hrtf-pack). Set before prepare(); ownership
    // stays with the caller and the data is only read during prepare().
    void setMeasuredPack (const void* data, size_t bytes) noexcept
    {
        packData = data; packBytes = bytes;
    }

    bool measuredAvailable() const noexcept { return hrtf[HrtfDatabase::Measured].isValid(); }

    // Rebuild the hand-tuned profile. NOT real-time safe — call from the message thread.
    // Double-buffered: the grid is built into whichever copy the audio thread is not
    // reading, then published with a single atomic store. The renderer notices the
    // pointer changed and crossfades, exactly as it does for any other profile switch.
    void rebuildCustom (const ElevationModel& model)
    {
        const HrtfDatabase* live = customActive.load (std::memory_order_acquire);
        const int target = (live == &custom[0]) ? 1 : 0;
        custom[target].generateCustom (sr, 0.0875f, model);

        const float ref = hrtf[HrtfDatabase::AnalyticB].frontalRms();
        const float own = custom[target].frontalRms();
        if (own > 1e-9f && ref > 1e-9f)
            custom[target].applyGain (ref / own);   // level-matched like every other profile

        customActive.store (&custom[target], std::memory_order_release);
    }

    void prepare (float sampleRate, int /*hostMaxBlock*/)
    {
        sr = sampleRate;
        // Every profile is built up front (~1 MB each) so switching is a pointer swap at
        // a block boundary — no worker thread, no audio-thread rebuild.
        hrtf[HrtfDatabase::AnalyticA].generateAnalytic (sr, 0.0875f, HrtfDatabase::AnalyticA);
        hrtf[HrtfDatabase::AnalyticB].generateAnalytic (sr, 0.0875f, HrtfDatabase::AnalyticB);

        // Every profile is level-matched to Analytic B at the frontal direction, so
        // switching between them is a timbre comparison and never a loudness one — the
        // whole point of the A/B/KU100 listening test.
        {
            const float ref = hrtf[HrtfDatabase::AnalyticB].frontalRms();
            const float own = hrtf[HrtfDatabase::AnalyticA].frontalRms();
            if (own > 1e-9f && ref > 1e-9f)
                hrtf[HrtfDatabase::AnalyticA].applyGain (ref / own);
        }

        // The measured pack is 48 kHz only for now; loadPack refuses any other rate, so
        // at 44.1/96/192 kHz the profile simply stays unavailable and selecting it falls
        // back to Analytic B rather than playing back at the wrong rate.
        auto& measured = hrtf[HrtfDatabase::Measured];
        measured = HrtfDatabase{};
        if (packData != nullptr && measured.loadPack (packData, packBytes, sr))
        {
            const float ref = hrtf[HrtfDatabase::AnalyticB].frontalRms();
            const float own = measured.frontalRms();
            if (own > 1e-9f && ref > 1e-9f)
                measured.applyGain (ref / own);   // level-matched: compare timbre, not loudness
        }

        customActive.store (nullptr, std::memory_order_release);
        rebuildCustom (customModel);

        for (auto& s : sources) s.prepare (sr, kChunk, &hrtf[HrtfDatabase::AnalyticB]);
        early.prepare (sr, kChunk, &hrtf[HrtfDatabase::AnalyticB]);
        fdn.prepare (sr, kChunk);
        outGainSm.prepare (sr, 0.02f); outGainSm.snap (1.0f);
        roomAmtSm.prepare (sr, 0.05f); roomAmtSm.snap (0.0f); // room fades in; amount 0 stays bit-exact direct
        earlyLateSm.prepare (sr, 0.05f); earlyLateSm.snap (params.roomEarlyLate);
        duck.prepare (sr);
        modeFade.prepare (sr, 0.008f); modeFade.snap (1.0f);
        limiterRelease = std::exp (-1.0f / (0.120f * sr));
        monoBuf.assign (kChunk, 0.0f);
        roomFeed.assign (kChunk, 0.0f);
        zeroBuf.assign (kChunk, 0.0f);
        erL.assign (kChunk, 0.0f); erR.assign (kChunk, 0.0f);
        fdnL.assign (kChunk, 0.0f); fdnR.assign (kChunk, 0.0f);
        activeMode = params.sourceMode;
        roomCooldown = 0;
        reset();
    }

    void reset()
    {
        for (auto& s : sources) s.reset();
        early.reset(); fdn.reset(); duck.reset();
        lastDuckGain = 1.0f;
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
    float lastGainReduction() const noexcept { return lastGr; } // linear, 1 = none
    float lastDuck() const noexcept { return lastDuckGain; }    // linear, 1 = room open
    int   hrtfTaps() const noexcept { return hrtf[0].numTaps(); }
    const HrtfDatabase& database (int profile) const noexcept
    {
        if (profile == HrtfDatabase::Custom)
        {
            const HrtfDatabase* c = customActive.load (std::memory_order_acquire);
            return c != nullptr ? *c : hrtf[HrtfDatabase::AnalyticB];
        }
        return hrtf[profile >= 0 && profile < HrtfDatabase::Measured + 1 ? profile : 1];
    }

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

        const int fullTaps = hrtf[0].numTaps(); // scales with sample rate (~2.7 ms window)
        const int taps = p.qualityMode == 0 ? fullTaps / 2 : fullTaps;
        int wanted = p.hrtfProfile;
        if (wanted < 0 || wanted >= HrtfDatabase::kNumProfiles) wanted = HrtfDatabase::AnalyticB;
        const HrtfDatabase* db = (wanted == HrtfDatabase::Custom)
                                     ? customActive.load (std::memory_order_acquire)
                                     : &hrtf[wanted];
        if (db == nullptr || ! db->isValid()) db = &hrtf[HrtfDatabase::AnalyticB];

        // ---- geometry updates (block rate) ----
        const float roomTarget = p.bypassRoom ? 0.0f : p.roomAmount;
        roomAmtSm.setTarget (roomTarget);
        const bool roomOn = roomTarget > 0.0001f || roomAmtSm.value() > 0.0001f;

        // ---- input routing ----
        float* mono = monoBuf.data();
        float* feed = roomFeed.data();

        if (activeMode == 0) // Mono Object
        {
            // downmix BEFORE clearing outputs — in-place (out == in) must stay valid
            for (int i = 0; i < n; ++i)
                mono[i] = 0.5f * (inL[i] + inR[i]);

            sources[0].update (db, p.azimuthDeg, p.elevationDeg, p.distanceM,
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
            sources[0].update (db, wrapDeg (p.azimuthDeg - half), p.elevationDeg, p.distanceM,
                               p.nearField, p.headRadiusM, taps);
            sources[1].update (db, wrapDeg (p.azimuthDeg + half), p.elevationDeg, p.distanceM,
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
        if (roomOn)
            roomCooldown = (int) (fdn.tailSeconds() * sr);
        if (roomOn || roomCooldown > 0)
        {
            RoomParams rp { p.roomSize, p.roomDamping, p.roomEarlyLate };
            early.update (db, sources[0].position(), rp, p.headRadiusM);
            fdn.setRoom (rp);

            // After switch-off the room keeps running on silence until its tail has
            // decayed — state empties naturally, so re-enabling never bursts a stale
            // tail and the audio thread never does large buffer resets (Contract #5).
            const float* roomIn = roomOn ? feed : zeroBuf.data();

            std::memset (erL.data(), 0, sizeof (float) * (size_t) n);
            std::memset (erR.data(), 0, sizeof (float) * (size_t) n);
            early.process (roomIn, erL.data(), erR.data(), n);

            std::memset (fdnL.data(), 0, sizeof (float) * (size_t) n);
            std::memset (fdnR.data(), 0, sizeof (float) * (size_t) n);
            fdn.process (roomIn, fdnL.data(), fdnR.data(), n);

            if (roomOn)
            {
                earlyLateSm.setTarget (p.roomEarlyLate);
                duck.setParams (p.duckAmount, p.duckRelease);
                for (int i = 0; i < n; ++i)
                {
                    const float amt = roomAmtSm.next();
                    const float b = earlyLateSm.next();
                    // Detect on the dry room feed, never on the wet output, or the
                    // reverb would drive its own ducking. The FDN above was fed the
                    // full signal: only its output is held down, so the tail stays
                    // charged and is revealed at the end of a phrase.
                    const float d = duck.next (feed[i]);
                    outL[i] += amt * (erL[i] * (1.0f - b) + fdnL[i] * b * d) * 1.6f;
                    outR[i] += amt * (erR[i] * (1.0f - b) + fdnR[i] * b * d) * 1.6f;
                    lastDuckGain = d;
                }
            }
            else
            {
                roomCooldown -= n;
            }
        }

        // ---- output stage: trim, safety limiter (ear-whisper gain can reach +32 dB),
        //      then metering of what actually leaves the plugin ----
        outGainSm.setTarget (std::pow (10.0f, p.outputGainDb / 20.0f));
        constexpr float kCeiling = 0.945f; // ~ -0.5 dBFS
        float env = limiterEnv;
        float minLg = 1.0f; // worst gain reduction this chunk, for the GR meter
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
                if (lg < minLg) minLg = lg;
                l *= lg; r *= lg;
            }
            outL[i] = l; outR[i] = r;
            if (std::fabs (l) > pl) pl = std::fabs (l);
            if (std::fabs (r) > pr) pr = std::fabs (r);
        }
        limiterEnv = env;
        lastGr = minLg;
        peakL = pl; peakR = pr;
    }

    EngineParams params;
    HrtfDatabase hrtf[HrtfDatabase::Measured + 1];      // A, B, measured
    HrtfDatabase custom[2];                             // double-buffered tuned profile
    std::atomic<const HrtfDatabase*> customActive { nullptr };
    ElevationModel customModel = ElevationModel::analyticBDefaults();
    SourceRenderer sources[2];
    EarlyReflections early;
    FdnReverb fdn;
    LinearSmoother outGainSm, roomAmtSm, earlyLateSm, modeFade;
    std::vector<float> monoBuf, roomFeed, zeroBuf, erL, erR, fdnL, fdnR;
    float sr = 48000.0f;
    float limiterEnv = 0.0f, limiterRelease = 0.9998f, lastGr = 1.0f;
    float peakL = 0.0f, peakR = 0.0f;
    int activeMode = 0;
    int roomCooldown = 0;
    VoiceDuck duck;
    float lastDuckGain = 1.0f;
    bool fadingMode = false;
    const void* packData = nullptr;
    size_t packBytes = 0;
};
} // namespace nsb
