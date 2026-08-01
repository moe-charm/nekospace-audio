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

struct ElevationModel
{
    // anchors at -60, 0 and +60 degrees
    ElevationAnchor below, level, above;

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
} // namespace nsb
