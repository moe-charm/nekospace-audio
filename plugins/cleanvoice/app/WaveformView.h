// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// Waveform with zoom, a drag-selected range and a playhead.
//
// Two things here are not conveniences.
//
// ZOOM. A 22-minute take drawn across 1500 pixels puts nearly a second in every pixel, so
// marking a one-second noise region is not a matter of care - it is below the resolution of
// the control. Without zoom the tool works on test files and not on real ones.
//
// THE SELECTION LABEL. The one thing a user can get wrong about this tool is thinking the
// selection is the part being cleaned. It is the part being LEARNED FROM, and the whole
// file is processed. Saying so on the selection itself is cheaper than explaining it later.
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <algorithm>
#include <cmath>

namespace cvapp
{
namespace col
{
const juce::Colour bg      { 0xff14171c };
const juce::Colour panel   { 0xff1b1f26 };
const juce::Colour line    { 0xff2a3038 };
const juce::Colour wave    { 0xff8fa3b8 };
const juce::Colour accent  { 0xffe8963c };
const juce::Colour select  { 0xff3ea8c8 };
const juce::Colour text    { 0xffd8dee6 };
const juce::Colour textDim { 0xff8892a0 };
}

class WaveformView : public juce::Component
{
public:
    // Declared explicitly: the non-copyable macro below declares a copy constructor, which
    // suppresses the implicit default one.
    WaveformView() = default;

    std::function<void()> onSelectionChanged;
    std::function<void (int)> onPlayheadMoved;
    // Fired whenever the visible range moves, so a spectrogram underneath can follow it.
    std::function<void()> onViewChanged;

    int viewStartSample() const noexcept { return viewStart; }
    int viewLengthSamples() const noexcept { return viewLen; }

    void setAudio (const std::vector<std::vector<float>>* channels, double sampleRate)
    {
        src = channels;
        sr = sampleRate;
        total = (src != nullptr && ! src->empty()) ? (int) (*src)[0].size() : 0;
        selStart = selEnd = 0;
        zoomToFit();
    }

    // Swaps which buffer is drawn without disturbing the selection or the zoom: switching
    // between Original / Clean / Removed must not throw away what you were looking at.
    void setAudioKeepSelection (const std::vector<std::vector<float>>* channels)
    {
        src = channels;
        rebuildPeaks();
        repaint();
    }

    void setPlayhead (int samplePos) { playhead = samplePos; repaint(); }

    bool hasSelection() const noexcept { return selEnd > selStart; }
    int selectionStart() const noexcept { return selStart; }
    int selectionEnd() const noexcept { return selEnd; }
    double selectionSeconds() const noexcept
    { return sr > 0 ? (selEnd - selStart) / sr : 0.0; }

    void zoomToFit()
    {
        viewStart = 0;
        viewLen = juce::jmax (1, total);
        rebuildPeaks();
        repaint();
        if (onViewChanged) onViewChanged();
    }

    void zoomToSelection()
    {
        if (! hasSelection()) return;
        const int pad = juce::jmax (1, (selEnd - selStart) / 8);
        setView (selStart - pad, (selEnd - selStart) + 2 * pad);
    }

    // Keyboard zoom works about the playhead when there is one, so the thing you are
    // listening to stays put - the same rule the wheel follows about the pointer.
    void zoomBy (double factor)
    {
        if (total <= 0) return;
        const int anchor = (playhead >= 0 && playhead < total)
                             ? playhead : viewStart + viewLen / 2;
        const int newLen = juce::jlimit (256, juce::jmax (256, total),
                                         juce::roundToInt (viewLen * factor));
        const double frac = juce::jlimit (0.0, 1.0,
            (double) (anchor - viewStart) / juce::jmax (1, viewLen));
        setView (anchor - juce::roundToInt (frac * newLen), newLen);
    }

    void scrollBy (double screensFraction)
    {
        setView (viewStart + juce::roundToInt (screensFraction * viewLen), viewLen);
    }

    juce::String viewDescription() const
    {
        if (total <= 0 || sr <= 0) return {};
        auto t = [this] (int s) { return juce::String (s / sr, 2) + " s"; };
        return t (viewStart) + " - " + t (viewStart + viewLen)
                 + "   (" + juce::String (100.0 * viewLen / juce::jmax (1, total), 1) + "% shown)";
    }

    void resized() override { rebuildPeaks(); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds();
        g.setColour (col::panel);
        g.fillRoundedRectangle (b.toFloat(), 4.0f);

        if (total == 0)
        {
            g.setColour (col::textDim);
            g.setFont (juce::Font (juce::FontOptions (14.0f)));
            g.drawText ("Open a WAV file", b, juce::Justification::centred);
            return;
        }

        const float midY = (float) b.getCentreY();
        const float halfH = (float) b.getHeight() * 0.45f;

        if (hasSelection())
        {
            const int x0 = sampleToX (selStart), x1 = sampleToX (selEnd);
            g.setColour (col::select.withAlpha (0.22f));
            g.fillRect (juce::jmax (b.getX(), x0), b.getY(),
                        juce::jlimit (1, b.getWidth(), x1 - x0), b.getHeight());
            g.setColour (col::select);
            if (x0 >= b.getX() && x0 <= b.getRight())
                g.drawVerticalLine (x0, (float) b.getY(), (float) b.getBottom());
            if (x1 >= b.getX() && x1 <= b.getRight())
                g.drawVerticalLine (x1, (float) b.getY(), (float) b.getBottom());
        }

        g.setColour (col::wave);
        for (size_t i = 0; i < peakMin.size(); ++i)
        {
            const float lo = peakMin[i], hi = peakMax[i];
            const float y0 = midY - hi * halfH, y1 = midY - lo * halfH;
            g.drawVerticalLine ((int) i + b.getX(), juce::jmin (y0, y1), juce::jmax (y0, y1) + 1.0f);
        }

        g.setColour (col::line);
        g.drawHorizontalLine ((int) midY, (float) b.getX(), (float) b.getRight());

        if (playhead >= 0 && playhead < total)
        {
            const int x = sampleToX (playhead);
            if (x >= b.getX() && x <= b.getRight())
            {
                g.setColour (col::accent);
                g.drawVerticalLine (x, (float) b.getY(), (float) b.getBottom());
            }
        }

        if (hasSelection())
        {
            const int x0 = juce::jmax (b.getX() + 2, sampleToX (selStart));
            auto tag = juce::Rectangle<int> (x0, b.getY() + 4, 260, 34);
            g.setColour (col::select);
            g.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
            g.drawText ("NOISE LEARN RANGE", tag.removeFromTop (16),
                        juce::Justification::centredLeft);
            g.setColour (col::textDim);
            g.setFont (juce::Font (juce::FontOptions (10.5f)));
            g.drawText (juce::String (selectionSeconds(), 2)
                          + " s  -  learned from, not removed",
                        tag, juce::Justification::centredLeft);
        }
    }

    // ---------------------------------------------------------------- mouse ----

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragAnchor = xToSample (e.x);
        panning = e.mods.isRightButtonDown() || e.mods.isMiddleButtonDown();
        panAnchorView = viewStart;
        if (! panning) dragged = false;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (panning)
        {
            const double perPixel = (double) viewLen / juce::jmax (1, getWidth());
            setView (panAnchorView - juce::roundToInt (e.getDistanceFromDragStartX() * perPixel),
                     viewLen);
            return;
        }
        if (std::abs (e.getDistanceFromDragStartX()) > 2) dragged = true;
        if (! dragged) return;
        const int s = xToSample (e.x);
        selStart = juce::jmin (dragAnchor, s);
        selEnd   = juce::jmax (dragAnchor, s);
        repaint();
    }

    // Double-click clears the selection, the way an audio editor does. The learned noise
    // profile deliberately survives this - see MainComponent - so clearing the range to
    // get out of the loop does not cost you the thing you selected it for.
    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        selStart = selEnd = 0;
        if (onSelectionChanged) onSelectionChanged();
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (panning) { panning = false; return; }
        // A click without a drag places the playhead and leaves the selection alone, so
        // you can audition around a marked region without losing it.
        if (! dragged)
        {
            playhead = dragAnchor;
            if (onPlayheadMoved) onPlayheadMoved (playhead);
            repaint();
            return;
        }
        if (onSelectionChanged) onSelectionChanged();
    }

    // Wheel zooms about the pointer, so the thing you are looking at stays under it.
    // Shift-wheel scrolls instead.
    void mouseWheelMove (const juce::MouseEvent& e,
                         const juce::MouseWheelDetails& d) override
    {
        if (total <= 0) return;
        if (e.mods.isShiftDown())
        {
            setView (viewStart - juce::roundToInt (d.deltaY * viewLen * 0.5), viewLen);
            return;
        }
        const int anchor = xToSample (e.x);
        const double factor = d.deltaY > 0 ? 1.0 / 1.25 : 1.25;
        const int newLen = juce::jlimit (256, juce::jmax (256, total),
                                         juce::roundToInt (viewLen * factor));
        // keep the anchor sample under the same pixel
        const double frac = juce::jlimit (0.0, 1.0,
            (double) (anchor - viewStart) / juce::jmax (1, viewLen));
        setView (anchor - juce::roundToInt (frac * newLen), newLen);
    }

private:
    void setView (int start, int len)
    {
        viewLen = juce::jlimit (256, juce::jmax (256, total), len);
        viewStart = juce::jlimit (0, juce::jmax (0, total - viewLen), start);
        rebuildPeaks();
        repaint();
        if (onViewChanged) onViewChanged();
    }

    int sampleToX (int s) const
    {
        if (viewLen <= 0) return 0;
        // paint() and mouse events use component-local coordinates. getX() is the
        // component's position in its parent and would shift every overlay to the right.
        return juce::roundToInt ((double) (s - viewStart) / viewLen * getWidth());
    }
    int xToSample (int x) const
    {
        if (getWidth() <= 0) return 0;
        return juce::jlimit (0, total,
                             viewStart + juce::roundToInt ((double) x / getWidth() * viewLen));
    }

    // One min/max pair per pixel column, over every channel, so a transient on one ear is
    // never averaged out of view.
    void rebuildPeaks()
    {
        peakMin.clear(); peakMax.clear();
        const int w = getWidth();
        if (src == nullptr || total <= 0 || w <= 0 || viewLen <= 0) return;
        peakMin.assign ((size_t) w, 0.0f);
        peakMax.assign ((size_t) w, 0.0f);

        // Fully zoomed out on a 22-minute file there are ~44 000 samples per pixel, and
        // scanning all of them on every redraw makes zooming feel broken. Above the
        // threshold the column is sampled instead: peaks can read very slightly low, which
        // costs nothing at a zoom level where one pixel is a second of audio - and the
        // reason to trust a region is auditioning it, not squinting at it.
        const int perPixel = juce::jmax (1, viewLen / w);
        const int stride = perPixel > 4096 ? perPixel / 4096 : 1;

        for (int x = 0; x < w; ++x)
        {
            const int a = viewStart + (int) ((double) x / w * viewLen);
            const int b = juce::jmin (total,
                            juce::jmax (a + 1, viewStart + (int) ((double) (x + 1) / w * viewLen)));
            float lo = 0.0f, hi = 0.0f;
            for (const auto& ch : *src)
                for (int i = a; i < b; i += stride)
                {
                    const float v = ch[(size_t) i];
                    lo = juce::jmin (lo, v); hi = juce::jmax (hi, v);
                }
            peakMin[(size_t) x] = lo;
            peakMax[(size_t) x] = hi;
        }
    }

    const std::vector<std::vector<float>>* src = nullptr;
    double sr = 48000.0;
    int total = 0, selStart = 0, selEnd = 0, dragAnchor = 0, playhead = -1;
    int viewStart = 0, viewLen = 1, panAnchorView = 0;
    bool panning = false, dragged = false;
    std::vector<float> peakMin, peakMax;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformView)
};
} // namespace cvapp
