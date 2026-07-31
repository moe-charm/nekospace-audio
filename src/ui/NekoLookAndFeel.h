#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace nsbui
{
namespace col
{
    const juce::Colour bg        { 0xff0f1115 };
    const juce::Colour bg2       { 0xff14171c };
    const juce::Colour panel     { 0xff191d24 };
    const juce::Colour panelLine { 0xff272c36 };
    const juce::Colour text      { 0xffdde1e8 };
    const juce::Colour textDim   { 0xff8b93a2 };
    const juce::Colour accent    { 0xffffb454 };   // amber
    const juce::Colour accentDim { 0xff8a6a3c };
    const juce::Colour node      { 0xffffc36e };
    const juce::Colour grid      { 0xff2a3040 };
    const juce::Colour meterLo   { 0xff59c97e };
    const juce::Colour meterHi   { 0xffe86a4a };
}

class NekoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NekoLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, col::bg);
        setColour (juce::Label::textColourId, col::text);
        setColour (juce::Slider::textBoxTextColourId, col::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::trackColourId, col::accent);
        setColour (juce::Slider::backgroundColourId, col::grid);
        setColour (juce::Slider::thumbColourId, col::accent);
        setColour (juce::Slider::rotarySliderFillColourId, col::accent);
        setColour (juce::Slider::rotarySliderOutlineColourId, col::grid);
        setColour (juce::ComboBox::backgroundColourId, col::panel);
        setColour (juce::ComboBox::textColourId, col::text);
        setColour (juce::ComboBox::outlineColourId, col::panelLine);
        setColour (juce::ComboBox::arrowColourId, col::textDim);
        setColour (juce::PopupMenu::backgroundColourId, col::panel);
        setColour (juce::PopupMenu::textColourId, col::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accentDim);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::TextButton::buttonColourId, col::panel);
        setColour (juce::TextButton::buttonOnColourId, col::accentDim);
        setColour (juce::TextButton::textColourOffId, col::text);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::ToggleButton::textColourId, col::text);
        setColour (juce::ToggleButton::tickColourId, col::accent);
        setColour (juce::TooltipWindow::backgroundColourId, col::panel);
        setColour (juce::TooltipWindow::textColourId, col::text);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle,
                           juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h)
                          .reduced (4.0f);
        const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
        auto square = bounds.withSizeKeepingCentre (size, size);
        const float radius = size * 0.5f - 2.0f;
        const auto centre = square.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);
        const float lw = juce::jmax (2.0f, radius * 0.16f);

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                             startAngle, endAngle, true);
        g.setColour (col::grid);
        g.strokePath (track, juce::PathStrokeType (lw, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        juce::Path val;
        val.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                           startAngle, angle, true);
        g.setColour (col::accent);
        g.strokePath (val, juce::PathStrokeType (lw, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
        // pointer
        const float pr = radius - lw * 1.4f;
        juce::Point<float> tip (centre.x + pr * std::sin (angle),
                                centre.y - pr * std::cos (angle));
        g.setColour (col::text);
        g.drawLine ({ centre.toFloat(), tip }, 2.0f);
        g.setColour (col::accent.withAlpha (0.25f));
        g.fillEllipse (juce::Rectangle<float> (5, 5).withCentre (centre));
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                           float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        if (style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearVertical)
        {
            const bool horiz = style == juce::Slider::LinearHorizontal;
            auto track = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
            const float t = 4.0f;
            juce::Rectangle<float> groove = horiz
                ? track.withSizeKeepingCentre ((float) w, t)
                : track.withSizeKeepingCentre (t, (float) h);
            g.setColour (col::grid);
            g.fillRoundedRectangle (groove, t * 0.5f);

            juce::Rectangle<float> fill = groove;
            if (horiz) fill = fill.withWidth (sliderPos - (float) x);
            else       fill = fill.withTop (sliderPos);
            g.setColour (col::accent.withAlpha (0.85f));
            g.fillRoundedRectangle (fill, t * 0.5f);

            const float r = 7.0f;
            juce::Point<float> c = horiz
                ? juce::Point<float> (sliderPos, track.getCentreY())
                : juce::Point<float> (track.getCentreX(), sliderPos);
            g.setColour (col::bg);
            g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (c));
            g.setColour (s.isMouseOverOrDragging() ? col::node : col::accent);
            g.fillEllipse (juce::Rectangle<float> (r * 2 - 5, r * 2 - 5).withCentre (c));
        }
        else
        {
            juce::LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos,
                                                    minPos, maxPos, style, s);
        }
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b,
                               const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        auto base = b.getToggleState() ? col::accentDim : col::panel;
        if (down) base = base.brighter (0.15f);
        else if (over) base = base.brighter (0.08f);
        g.setColour (base);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (b.getToggleState() ? col::accent : col::panelLine);
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override
    {
        return juce::Font (juce::FontOptions ((float) juce::jmin (13, h - 6)));
    }
    juce::Font getComboBoxFont (juce::ComboBox&) override
    {
        return juce::Font (juce::FontOptions (13.0f));
    }
    juce::Font getLabelFont (juce::Label& l) override
    {
        return l.getFont();
    }
};
} // namespace nsbui
