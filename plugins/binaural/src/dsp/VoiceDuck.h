// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// Late-only voice duck: holds the late reverb down while the voice is speaking and lets
// it open at the end of a phrase.
//
// Three decisions make this behave like "the room appears at the end of the line" rather
// than like a compressor on a reverb return:
//
// 1. It ducks the late bus *output*, and the FDN keeps being fed normally. The tail is
//    therefore always charged and merely held down — when the voice stops, what is
//    revealed is reverb that has been building all along. Ducking the FDN *input*
//    instead would starve it, and the tail would have to grow from nothing exactly when
//    it is supposed to be heard.
// 2. Direct sound and early reflections are never touched. Early reflections carry the
//    room's shape and, since they are rendered through the HRTF at their image
//    directions, part of the elevation cue. Late reverb is the part that smears a voice.
// 3. There is no threshold to set. The detector normalises against a slowly decaying
//    reference of recent level, so the same setting works for a whisper and for a shout,
//    and at any input gain.
//
// JUCE-free (Architecture Contract #4).
#include <cmath>
#include "Geometry.h"

namespace nsb
{
class VoiceDuck
{
public:
    void prepare (float sampleRate) noexcept
    {
        sr = sampleRate;
        envAtt = coeff (0.003f);     // 3 ms — follow syllable onsets
        envRel = coeff (0.030f);     // 30 ms — do not chatter between syllables
        refAtt = 0.0f;               // reference rises instantly with the loudest peak
        refRel = coeff (4.0f);       // and forgets it over seconds
        duckAtt = coeff (0.008f);    // 8 ms — get out of the way immediately
        setParams (0.5f, 0.45f);
        reset();
    }

    void reset() noexcept { env = 0.0f; ref = kRefFloor; gain = 1.0f; }

    // depth 0..1, release in seconds (how long the room takes to reappear)
    void setParams (float depth_, float releaseSeconds) noexcept
    {
        depth = clampf (depth_, 0.0f, 1.0f);
        duckRel = coeff (clampf (releaseSeconds, 0.02f, 4.0f));
    }

    bool isActive() const noexcept { return depth > 0.0001f; }

    // Feed the DRY pre-room voice, never the wet output: detecting on the output would
    // let the reverb drive its own ducking and pump. Returns the gain for the late bus.
    float next (float voice) noexcept
    {
        const float x = std::fabs (voice);

        env = x > env ? envAtt * (x - env) + env : envRel * (x - env) + env;
        ref = x > ref ? x : refRel * (x - ref) + ref;
        if (ref < kRefFloor) ref = kRefFloor;

        // Relative activity: how loud the voice is against its own recent level, so the
        // control means the same thing whatever the input gain or the speaking style.
        const float activity = clampf (env / (kActivityFraction * ref), 0.0f, 1.0f);
        const float target = 1.0f - depth * activity;

        // Duck quickly, recover over the release time — the asymmetry is the effect.
        const float k = target < gain ? duckAtt : duckRel;
        gain += k * (target - gain);
        return gain;
    }

    float currentGain() const noexcept { return gain; }

private:
    // One-pole coefficient reaching ~63 % of a step in `seconds`.
    float coeff (float seconds) const noexcept
    {
        return 1.0f - std::exp (-1.0f / (std::max (seconds, 1.0e-5f) * sr));
    }

    // Voice counts as present once it reaches this fraction of its own recent peak.
    static constexpr float kActivityFraction = 0.20f;
    static constexpr float kRefFloor = 1.0e-4f;   // -80 dBFS: silence stays silent

    float sr = 48000.0f;
    float envAtt = 0.0f, envRel = 0.0f, refAtt = 0.0f, refRel = 0.0f;
    float duckAtt = 0.0f, duckRel = 0.0f;
    float depth = 0.5f;
    float env = 0.0f, ref = kRefFloor, gain = 1.0f;
};
} // namespace nsb
