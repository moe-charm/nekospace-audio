// SPDX-FileCopyrightText: 2026 TextureVoice
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// Top-down spatial pad: head fixed at center (with neko ears), draggable source node.
// Drag = azimuth + distance, wheel = distance, Shift = fine, double-click = front 1 m.
#include <juce_audio_processors/juce_audio_processors.h>
#include "NekoLookAndFeel.h"
#include "../dsp/Geometry.h"

namespace nsbui
{
class SpatialPad : public juce::Component, private juce::Timer
{
public:
    SpatialPad (juce::RangedAudioParameter& azP, juce::RangedAudioParameter& elP,
                juce::RangedAudioParameter& distP)
        : azAtt (azP, [this] (float v) { az = v; repaint(); }),
          elAtt (elP, [this] (float v) { el = v; repaint(); }),
          distAtt (distP, [this] (float v) { dist = v; repaint(); })
    {
        azAtt.sendInitialUpdate();
        elAtt.sendInitialUpdate();
        distAtt.sendInitialUpdate();
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        const auto c = b.getCentre();
        const float R = padRadius();

        g.setColour (col::bg2);
        g.fillRoundedRectangle (b, 10.0f);
        g.setColour (col::panelLine);
        g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.0f);

        // distance rings (log scale)
        g.setFont (juce::Font (juce::FontOptions (10.0f)));
        for (float d : { 0.1f, 0.25f, 0.5f, 1.0f, 2.5f, 5.0f, 10.0f, 20.0f })
        {
            const float r = radiusFor (d);
            const bool major = (d == 1.0f || d == 10.0f);
            g.setColour (col::grid.withAlpha (major ? 0.9f : 0.45f));
            g.drawEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (c), 1.0f);
            if (major || d == 0.1f)
            {
                g.setColour (col::textDim.withAlpha (0.7f));
                g.drawText (d < 1.0f ? juce::String (d, 1) + " m" : juce::String ((int) d) + " m",
                            (int) (c.x + r * 0.7071f) + 3, (int) (c.y - r * 0.7071f) - 12,
                            44, 12, juce::Justification::left);
            }
        }

        // cross hairs
        g.setColour (col::grid.withAlpha (0.5f));
        g.drawLine (c.x - R, c.y, c.x + R, c.y, 1.0f);
        g.drawLine (c.x, c.y - R, c.x, c.y + R, 1.0f);

        // compass labels
        g.setColour (col::textDim);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText ("FRONT", juce::Rectangle<int> ((int) c.x - 30, (int) (c.y - R) + 4, 60, 14),
                    juce::Justification::centred);
        g.drawText ("BACK", juce::Rectangle<int> ((int) c.x - 30, (int) (c.y + R) - 18, 60, 14),
                    juce::Justification::centred);
        g.drawText ("L", juce::Rectangle<int> ((int) (c.x - R) + 6, (int) c.y - 7, 14, 14),
                    juce::Justification::centred);
        g.drawText ("R", juce::Rectangle<int> ((int) (c.x + R) - 20, (int) c.y - 7, 14, 14),
                    juce::Justification::centred);

        drawHead (g, c);
        drawSource (g, c);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isLeftButtonDown())
        {
            azAtt.beginGesture();
            distAtt.beginGesture();
            dragging = true;
            applyMouse (e);
        }
    }
    void mouseDrag (const juce::MouseEvent& e) override { if (dragging) applyMouse (e); }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragging) { azAtt.endGesture(); distAtt.endGesture(); dragging = false; }
    }
    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        azAtt.setValueAsCompleteGesture (0.0f);
        distAtt.setValueAsCompleteGesture (1.0f);
        elAtt.setValueAsCompleteGesture (0.0f);
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const float fine = e.mods.isShiftDown() ? 0.25f : 1.0f;
        const float factor = std::pow (1.18f, -w.deltaY * 3.0f * fine);
        distAtt.setValueAsCompleteGesture (
            nsb::clampf (dist * factor, nsb::kMinDistance, nsb::kMaxDistance));
    }

private:
    void timerCallback() override { glowPhase += 0.05f; repaint(); }

    float padRadius() const
    {
        return juce::jmin (getWidth(), getHeight()) * 0.5f - 22.0f;
    }
    float headPx() const { return juce::jmax (padRadius() * 0.075f, 10.0f); }

    float radiusFor (float d) const
    {
        const float t = std::log (d / nsb::kMinDistance)
                        / std::log (nsb::kMaxDistance / nsb::kMinDistance);
        return headPx() + 4.0f + t * (padRadius() - headPx() - 4.0f);
    }
    float distanceFor (float rPx) const
    {
        const float t = nsb::clampf ((rPx - headPx() - 4.0f)
                                     / (padRadius() - headPx() - 4.0f), 0.0f, 1.0f);
        return nsb::kMinDistance
               * std::pow (nsb::kMaxDistance / nsb::kMinDistance, t);
    }

    void applyMouse (const juce::MouseEvent& e)
    {
        const auto c = getLocalBounds().toFloat().getCentre();
        const float dx = (float) e.position.x - c.x;
        const float dy = (float) e.position.y - c.y;
        const float newAz = juce::radiansToDegrees (std::atan2 (dx, -dy)); // up = front
        const float newDist = distanceFor (std::sqrt (dx * dx + dy * dy));
        azAtt.setValueAsPartOfGesture (newAz);
        distAtt.setValueAsPartOfGesture (newDist);
    }

    void drawHead (juce::Graphics& g, juce::Point<float> c)
    {
        const float hr = headPx();
        // neko ears (our own branding — subtle triangles)
        juce::Path ear;
        auto addEar = [&] (float sign)
        {
            juce::Path p;
            p.addTriangle (0.0f, -hr * 1.55f, -hr * 0.42f, -hr * 0.72f, hr * 0.42f, -hr * 0.78f);
            p.applyTransform (juce::AffineTransform::rotation (sign * 0.62f).translated (c.x, c.y));
            ear.addPath (p);
        };
        addEar (-1.0f); addEar (1.0f);
        g.setColour (col::textDim.withAlpha (0.9f));
        g.fillPath (ear);

        g.setColour (col::panel.brighter (0.35f));
        g.fillEllipse (juce::Rectangle<float> (hr * 2, hr * 2).withCentre (c));
        g.setColour (col::textDim);
        g.drawEllipse (juce::Rectangle<float> (hr * 2, hr * 2).withCentre (c), 1.2f);
        // nose (facing front/up)
        juce::Path nose;
        nose.addTriangle (c.x, c.y - hr - 3.0f, c.x - 3.5f, c.y - hr + 2.0f,
                          c.x + 3.5f, c.y - hr + 2.0f);
        g.setColour (col::textDim);
        g.fillPath (nose);
    }

    void drawSource (juce::Graphics& g, juce::Point<float> c)
    {
        const float r = radiusFor (nsb::clampf (dist, nsb::kMinDistance, nsb::kMaxDistance));
        const float a = juce::degreesToRadians (az);
        const juce::Point<float> pos (c.x + r * std::sin (a), c.y - r * std::cos (a));

        // elevation stem
        const float stem = -el / 90.0f * 26.0f;
        if (std::fabs (stem) > 2.0f)
        {
            g.setColour (col::accent.withAlpha (0.7f));
            g.drawLine (pos.x, pos.y, pos.x, pos.y + stem, 2.0f);
            juce::Path tipP;
            const float dir = stem < 0 ? -1.0f : 1.0f;
            tipP.addTriangle (pos.x, pos.y + stem + dir * 4.0f,
                              pos.x - 4.0f, pos.y + stem - dir * 2.0f,
                              pos.x + 4.0f, pos.y + stem - dir * 2.0f);
            g.fillPath (tipP);
        }

        // glow + node
        const float pulse = 3.0f + 1.5f * std::sin (glowPhase);
        g.setColour (col::node.withAlpha (0.18f));
        g.fillEllipse (juce::Rectangle<float> (26 + pulse * 2, 26 + pulse * 2).withCentre (pos));
        g.setColour (col::node.withAlpha (0.45f));
        g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (pos));
        g.setColour (dragging ? juce::Colours::white : col::node);
        g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (pos));

        // readout near node
        g.setColour (col::text);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        juce::String txt = juce::String (az, 0) + juce::String::fromUTF8 ("\xc2\xb0  ")
                         + (dist < 1.0f ? juce::String (dist * 100.0f, 0) + " cm"
                                        : juce::String (dist, 2) + " m");
        g.drawText (txt, (int) pos.x - 60, (int) pos.y - 30, 120, 14,
                    juce::Justification::centred);
    }

    juce::ParameterAttachment azAtt, elAtt, distAtt;
    float az = 0, el = 0, dist = 1.0f;
    float glowPhase = 0;
    bool dragging = false;
};
} // namespace nsbui
