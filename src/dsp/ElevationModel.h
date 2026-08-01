// SPDX-FileCopyrightText: 2026 TextureVoice
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// Tunable elevation model: three anchors (below / level / above) that are adjusted by
// ear rather than derived from a formula, and interpolated in between.
//
// The point of the anchor form is that up and down are NOT tied to one symmetric
// expression. Analytic B derives everything from sin/cos of the elevation, so any
// change to "above" drags "below" with it; here the two are independent, and the
// setting that reads as "lifted out of the head" can be found separately from the one
// that reads as "sunk toward the floor".
//
// JUCE-free (Architecture Contract #4).
#include <cmath>
#include "Geometry.h"

namespace nsb
{
struct ElevationAnchor
{
    float notchHz   = 6950.0f;   // first pinna notch centre
    float notchDb   = -12.0f;    // its depth (negative = cut)
    float notchQ    = 3.5f;      // its width
    float peakRatio = 0.62f;     // companion peak centre, as a fraction of notchHz
    float peakDb    = 4.0f;      // its height
    float shelfDb   = 0.0f;      // high shelf at 8 kHz: air above, torso shadow below
    float torsoMs   = 0.78f;     // shoulder reflection delay
    float torsoAmt  = 0.427f;    // shoulder reflection strength
};

// Four plain-language controls that generate the whole anchor set. This is what you
// actually turn while listening; the 24 raw values exist underneath for when a curve
// needs finishing by hand.
struct ElevationMacros
{
    float up    = 1.0f;   // how far "above" departs from level  (0 = none, 2 = double)
    float down  = 1.0f;   // how far "below" departs from level
    float body  = 1.0f;   // torso/shoulder reflection strength (the low-frequency cue)
    float focus = 1.0f;   // notch width: low = broad tonal shift, high = sharp colouring

    bool operator== (const ElevationMacros& o) const noexcept
    { return up == o.up && down == o.down && body == o.body && focus == o.focus; }
    bool operator!= (const ElevationMacros& o) const noexcept { return ! (*this == o); }
};

struct ElevationModel
{
    // anchors at -60, 0 and +60 degrees
    ElevationAnchor below, level, above;

    static ElevationModel fromMacros (const ElevationMacros& m) noexcept;

    // Defaults reproduce Analytic B closely, so a tuning session starts from the current
    // sound rather than from silence. Extrapolating the log-frequency line to +/-90
    // lands on 4.2 kHz and 11.5 kHz, which are B's endpoints.
    static ElevationModel analyticBDefaults() noexcept
    {
        ElevationModel m;
        m.below = { 4967.0f,  -8.40f, 3.5f, 0.62f, 2.40f, -3.90f, 1.144f, 0.224f };
        m.level = { 6950.0f, -12.00f, 3.5f, 0.62f, 4.00f,  0.00f, 0.780f, 0.427f };
        m.above = { 9726.0f, -13.20f, 3.5f, 0.62f, 4.80f,  3.90f, 0.416f, 0.405f };
        return m;
    }

    bool operator== (const ElevationModel& o) const noexcept
    {
        auto same = [] (const ElevationAnchor& a, const ElevationAnchor& b)
        {
            return a.notchHz == b.notchHz && a.notchDb == b.notchDb && a.notchQ == b.notchQ
                && a.peakRatio == b.peakRatio && a.peakDb == b.peakDb
                && a.shelfDb == b.shelfDb && a.torsoMs == b.torsoMs
                && a.torsoAmt == b.torsoAmt;
        };
        return same (below, o.below) && same (level, o.level) && same (above, o.above);
    }
    bool operator!= (const ElevationModel& o) const noexcept { return ! (*this == o); }

    // Piecewise linear through the anchors, extrapolated at the same rate beyond +/-60
    // so the cue keeps developing toward the poles instead of flattening off.
    ElevationAnchor at (float elDeg) const noexcept
    {
        const ElevationAnchor* a;
        const ElevationAnchor* b;
        float t;
        if (elDeg <= 0.0f) { a = &below; b = &level; t = (elDeg + 60.0f) / 60.0f; }
        else               { a = &level; b = &above; t = elDeg / 60.0f; }

        auto lin = [t] (float x, float y) { return x + (y - x) * t; };
        auto logLin = [t] (float x, float y)
        {
            return std::exp (std::log (x) + (std::log (y) - std::log (x)) * t);
        };

        ElevationAnchor r;
        r.notchHz   = clampf (logLin (a->notchHz, b->notchHz), 1500.0f, 18000.0f);
        r.notchDb   = clampf (lin (a->notchDb, b->notchDb), -30.0f, 0.0f);
        r.notchQ    = clampf (lin (a->notchQ, b->notchQ), 0.5f, 12.0f);
        r.peakRatio = clampf (lin (a->peakRatio, b->peakRatio), 0.2f, 1.5f);
        r.peakDb    = clampf (lin (a->peakDb, b->peakDb), -12.0f, 15.0f);
        r.shelfDb   = clampf (lin (a->shelfDb, b->shelfDb), -18.0f, 18.0f);
        r.torsoMs   = clampf (lin (a->torsoMs, b->torsoMs), 0.05f, 2.5f);
        r.torsoAmt  = clampf (lin (a->torsoAmt, b->torsoAmt), 0.0f, 0.9f);
        return r;
    }
};

inline ElevationModel ElevationModel::fromMacros (const ElevationMacros& m) noexcept
{
    const ElevationModel base = analyticBDefaults();
    ElevationModel r = base;

    // Each macro scales how far an anchor departs from Level, so 1.0 reproduces
    // Analytic B exactly and 0 collapses that direction onto the horizontal.
    // The endpoints are returned verbatim so that a macro of exactly 1.00 restores
    // Analytic B bit-for-bit; a log/exp round-trip would otherwise drift by an ulp or two
    // and "reset" would not quite reset.
    auto away = [] (float lv, float anchor, float k)
    {
        if (k == 1.0f) return anchor;
        if (k == 0.0f) return lv;
        return lv + (anchor - lv) * k;
    };
    auto awayLog = [] (float lv, float anchor, float k)
    {
        if (k == 1.0f) return anchor;
        if (k == 0.0f) return lv;
        return std::exp (std::log (lv) + (std::log (anchor) - std::log (lv)) * k);
    };

    auto shape = [&] (ElevationAnchor& dst, const ElevationAnchor& src, float k)
    {
        dst.notchHz  = awayLog (base.level.notchHz,  src.notchHz,  k);
        dst.notchDb  = away    (base.level.notchDb,  src.notchDb,  k);
        dst.peakDb   = away    (base.level.peakDb,   src.peakDb,   k);
        dst.shelfDb  = away    (base.level.shelfDb,  src.shelfDb,  k);
        dst.torsoMs  = away    (base.level.torsoMs,  src.torsoMs,  k);
        dst.torsoAmt = away    (base.level.torsoAmt, src.torsoAmt, k);
    };
    shape (r.above, base.above, m.up);
    shape (r.below, base.below, m.down);

    for (ElevationAnchor* a : { &r.below, &r.level, &r.above })
    {
        a->torsoAmt = clampf (a->torsoAmt * m.body, 0.0f, 0.9f);
        a->notchQ   = clampf (a->notchQ * m.focus, 0.5f, 12.0f);
        a->notchHz  = clampf (a->notchHz, 1500.0f, 18000.0f);
        a->notchDb  = clampf (a->notchDb, -30.0f, 0.0f);
        a->peakDb   = clampf (a->peakDb, -12.0f, 15.0f);
        a->shelfDb  = clampf (a->shelfDb, -18.0f, 18.0f);
        a->torsoMs  = clampf (a->torsoMs, 0.05f, 2.5f);
    }
    return r;
}
} // namespace nsb
