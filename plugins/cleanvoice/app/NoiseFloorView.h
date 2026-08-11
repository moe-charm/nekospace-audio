// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// The learned noise floor, drawn as level against log frequency.
//
// This exists because "is it working?" turned out to be unanswerable by ear alone. The
// noise floor on a studio take sits around -77 dBFS; the signal removed from it is quieter
// still, so at normal monitoring level nothing is audible either way and the tool looks
// broken when it is not. Seeing the curve answers three questions at a glance:
//
//   * is there actually a hiss, and what shape is it
//   * is there a tonal spike (a fan, a whine, mains hum) rather than broadband noise
//   * did the selection contain something that is obviously not noise
//
// dBFS is sine-referenced: a full-scale sine lands at (A/2)*windowSum in its bin.
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <cmath>
#include "WaveformView.h"      // for cvapp::col

namespace cvapp
{
class NoiseFloorView : public juce::Component
{
public:
    NoiseFloorView() = default;

    // psd[channel][bin], raw bin power as the profile stores it.
    void setProfile (std::vector<std::vector<float>> psd, double sampleRate,
                     float windowSum)
    {
        curves = std::move (psd);
        sr = sampleRate;
        refDb = 20.0f * std::log10 (juce::jmax (1.0e-9f, windowSum * 0.5f));
        repaint();
    }

    void clear() { curves.clear(); repaint(); }
    bool hasProfile() const noexcept { return ! curves.empty(); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds();
        g.setColour (col::panel);
        g.fillRoundedRectangle (b.toFloat(), 4.0f);

        if (curves.empty() || curves[0].size() < 4)
        {
            g.setColour (col::textDim);
            g.setFont (juce::Font (juce::FontOptions (12.0f)));
            g.drawText ("Learn a noise range to see its floor", b,
                        juce::Justification::centred);
            return;
        }

        auto plot = b.reduced (44, 12).withTrimmedBottom (14);

        // grid: decades of frequency, 20 dB steps of level
        g.setFont (juce::Font (juce::FontOptions (9.5f)));
        for (double f : { 100.0, 1000.0, 10000.0 })
        {
            const int x = freqToX (f, plot);
            g.setColour (col::line);
            g.drawVerticalLine (x, (float) plot.getY(), (float) plot.getBottom());
            g.setColour (col::textDim);
            g.drawText (f >= 1000.0 ? juce::String (f / 1000.0, 0) + "k"
                                    : juce::String (f, 0),
                        x - 16, plot.getBottom(), 32, 12, juce::Justification::centred);
        }
        for (float db = kTopDb; db >= kBotDb; db -= 20.0f)
        {
            const int y = dbToY (db, plot);
            g.setColour (col::line);
            g.drawHorizontalLine (y, (float) plot.getX(), (float) plot.getRight());
            g.setColour (col::textDim);
            g.drawText (juce::String ((int) db), 4, y - 7, 38, 14,
                        juce::Justification::centredRight);
        }

        static const juce::Colour chCol[] = { col::accent, col::select };
        for (size_t c = 0; c < curves.size() && c < 2; ++c)
        {
            juce::Path p;
            const auto& v = curves[c];
            const int bins = (int) v.size();
            bool started = false;
            for (int k = 1; k < bins; ++k)      // skip DC
            {
                const double f = (double) k * sr / (2.0 * (bins - 1));
                if (f < kMinHz || f > sr * 0.5) continue;
                const float db = 10.0f * std::log10 (juce::jmax (v[(size_t) k], 1.0e-20f))
                                   - refDb;
                const float x = (float) freqToX (f, plot);
                const float y = (float) dbToY (db, plot);
                if (! started) { p.startNewSubPath (x, y); started = true; }
                else p.lineTo (x, y);
            }
            g.setColour (chCol[c].withAlpha (0.9f));
            g.strokePath (p, juce::PathStrokeType (1.2f));
        }

        g.setColour (col::textDim);
        g.setFont (juce::Font (juce::FontOptions (10.5f)));
        g.drawText (curves.size() > 1 ? "LEARNED NOISE FLOOR   L / R" : "LEARNED NOISE FLOOR",
                    plot.getX() + 4, plot.getY() + 2, 240, 14,
                    juce::Justification::centredLeft);
    }

private:
    static constexpr float kTopDb = -20.0f, kBotDb = -140.0f, kMinHz = 20.0f;

    int freqToX (double f, juce::Rectangle<int> r) const
    {
        const double lo = std::log10 (kMinHz), hi = std::log10 (juce::jmax (1000.0, sr * 0.5));
        const double t = (std::log10 (juce::jmax (f, (double) kMinHz)) - lo) / (hi - lo);
        return r.getX() + juce::roundToInt (t * r.getWidth());
    }
    static int dbToY (float db, juce::Rectangle<int> r)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (kTopDb - db) / (kTopDb - kBotDb));
        return r.getY() + juce::roundToInt (t * r.getHeight());
    }

    std::vector<std::vector<float>> curves;
    double sr = 48000.0;
    float refDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoiseFloorView)
};
} // namespace cvapp
