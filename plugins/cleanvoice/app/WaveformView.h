// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// Waveform with a drag-selected range and a playhead.
//
// The label over the selection is not decoration. The single most likely misunderstanding
// of this tool is that the selection is the part being cleaned; it is the part being
// LEARNED FROM, and the whole file is processed. Saying so on the selection itself is
// cheaper than explaining it afterwards.
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

    void setAudio (const std::vector<std::vector<float>>* channels, double sampleRate)
    {
        src = channels;
        sr = sampleRate;
        total = (src != nullptr && ! src->empty()) ? (int) (*src)[0].size() : 0;
        selStart = selEnd = 0;
        rebuildPeaks();
        repaint();
    }

    // Swaps which buffer is drawn without disturbing the selection: switching between
    // Original / Clean / Removed must not throw away the range you just marked.
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

        // selection first, so the waveform draws over it
        if (hasSelection())
        {
            const int x0 = sampleToX (selStart), x1 = sampleToX (selEnd);
            g.setColour (col::select.withAlpha (0.22f));
            g.fillRect (x0, b.getY(), juce::jmax (1, x1 - x0), b.getHeight());
            g.setColour (col::select);
            g.drawVerticalLine (x0, (float) b.getY(), (float) b.getBottom());
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

        if (playhead > 0 && playhead < total)
        {
            g.setColour (col::accent);
            g.drawVerticalLine (sampleToX (playhead), (float) b.getY(), (float) b.getBottom());
        }

        // the label that stops the selection being misread
        if (hasSelection())
        {
            const int x0 = sampleToX (selStart), x1 = sampleToX (selEnd);
            auto tag = juce::Rectangle<int> (x0, b.getY() + 4, juce::jmax (140, x1 - x0), 34);
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

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragAnchor = xToSample (e.x);
        selStart = selEnd = dragAnchor;
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const int s = xToSample (e.x);
        selStart = juce::jmin (dragAnchor, s);
        selEnd   = juce::jmax (dragAnchor, s);
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (onSelectionChanged) onSelectionChanged();
    }

private:
    int sampleToX (int s) const
    {
        if (total <= 0) return getX();
        return getX() + juce::roundToInt ((double) s / total * getWidth());
    }
    int xToSample (int x) const
    {
        if (getWidth() <= 0) return 0;
        return juce::jlimit (0, total,
                             juce::roundToInt ((double) (x) / getWidth() * total));
    }

    // One min/max pair per pixel column, over every channel, so a transient on one ear
    // is never averaged out of view.
    void rebuildPeaks()
    {
        peakMin.clear(); peakMax.clear();
        const int w = getWidth();
        if (src == nullptr || total <= 0 || w <= 0) return;
        peakMin.assign ((size_t) w, 0.0f);
        peakMax.assign ((size_t) w, 0.0f);

        for (int x = 0; x < w; ++x)
        {
            const int a = (int) ((double) x / w * total);
            const int b = juce::jmax (a + 1, (int) ((double) (x + 1) / w * total));
            float lo = 0.0f, hi = 0.0f;
            for (const auto& ch : *src)
                for (int i = a; i < b && i < (int) ch.size(); ++i)
                { lo = juce::jmin (lo, ch[(size_t) i]); hi = juce::jmax (hi, ch[(size_t) i]); }
            peakMin[(size_t) x] = lo;
            peakMax[(size_t) x] = hi;
        }
    }

    const std::vector<std::vector<float>>* src = nullptr;
    double sr = 48000.0;
    int total = 0, selStart = 0, selEnd = 0, dragAnchor = 0, playhead = -1;
    std::vector<float> peakMin, peakMax;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformView)
};
} // namespace cvapp
