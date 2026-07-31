// SPDX-FileCopyrightText: 2026 TextureVoice
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "../ui/NekoLookAndFeel.h"
#include "../ui/SpatialPad.h"

// ---------------------------------------------------------------- meters ----
class StereoMeter : public juce::Component, private juce::Timer
{
public:
    explicit StereoMeter (NekoSpaceProcessor& p) : proc (p) { startTimerHz (30); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        auto labels = b.removeFromBottom (12.0f);
        const float w = (b.getWidth() - 8.0f) / 3.0f;

        auto lCol = b.removeFromLeft (w); b.removeFromLeft (4.0f);
        auto rCol = b.removeFromLeft (w); b.removeFromLeft (4.0f);
        drawBar (g, lCol, dispL);
        drawBar (g, rCol, dispR);
        drawGr (g, b); // limiter gain reduction, fills top-down

        g.setColour (nsbui::col::textDim);
        g.setFont (juce::Font (juce::FontOptions (9.0f)));
        g.drawText ("L", labels.removeFromLeft (w), juce::Justification::centred);
        labels.removeFromLeft (4.0f);
        g.drawText ("R", labels.removeFromLeft (w), juce::Justification::centred);
        labels.removeFromLeft (4.0f);
        g.setColour (dispGr > 0.1f ? nsbui::col::meterHi : nsbui::col::textDim);
        g.drawText ("GR", labels, juce::Justification::centred);
    }

private:
    void drawGr (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (nsbui::col::bg);
        g.fillRoundedRectangle (r, 3.0f);
        if (dispGr > 0.001f)
        {
            const float t = juce::jlimit (0.0f, 1.0f, dispGr / 12.0f); // 12 dB full scale
            g.setColour (nsbui::col::meterHi);
            g.fillRoundedRectangle (r.withHeight (r.getHeight() * t), 3.0f);
        }
        g.setColour (nsbui::col::panelLine);
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }

    void drawBar (juce::Graphics& g, juce::Rectangle<float> r, float v)
    {
        g.setColour (nsbui::col::bg);
        g.fillRoundedRectangle (r, 3.0f);
        const float db = juce::Decibels::gainToDecibels (v, -60.0f);
        const float t = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
        auto fill = r.withTop (r.getBottom() - r.getHeight() * t);
        g.setGradientFill (juce::ColourGradient (nsbui::col::meterLo, r.getX(), r.getBottom(),
                                                 nsbui::col::meterHi, r.getX(), r.getY(), false));
        g.fillRoundedRectangle (fill, 3.0f);
        g.setColour (nsbui::col::panelLine);
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }

    void timerCallback() override
    {
        const float l = proc.meterL.load (std::memory_order_relaxed);
        const float r = proc.meterR.load (std::memory_order_relaxed);
        const float grDb = -juce::Decibels::gainToDecibels (
            proc.meterGR.load (std::memory_order_relaxed), -60.0f);
        dispL = l > dispL ? l : dispL * 0.88f;
        dispR = r > dispR ? r : dispR * 0.88f;
        dispGr = grDb > dispGr ? grDb : dispGr * 0.90f;
        repaint();
    }

    NekoSpaceProcessor& proc;
    float dispL = 0, dispR = 0, dispGr = 0;
};

// ---------------------------------------------------------------- editor ----
class NekoSpaceEditor : public juce::AudioProcessorEditor
{
public:
    explicit NekoSpaceEditor (NekoSpaceProcessor& p)
        : AudioProcessorEditor (p), proc (p), meter (p),
          pad (*p.apvts.getParameter (nsb::pid::azimuth),
               *p.apvts.getParameter (nsb::pid::elevation),
               *p.apvts.getParameter (nsb::pid::distance))
    {
        setLookAndFeel (&lnf);

        addAndMakeVisible (pad);
        addAndMakeVisible (meter);

        setupCombo (modeBox, nsb::pid::mode, modeAtt);
        setupCombo (qualityBox, nsb::pid::quality, qualityAtt);

        // right-panel horizontal sliders
        addRow (azSlider, azLabel, "AZIMUTH", nsb::pid::azimuth, azSAtt);
        addRow (elSlider, elLabel, "ELEVATION", nsb::pid::elevation, elSAtt);
        addRow (distSlider, distLabel, "DISTANCE", nsb::pid::distance, distSAtt);
        addRow (widthSlider, widthLabel, "WIDTH", nsb::pid::width, widthSAtt);
        addRow (nearSlider, nearLabel, "NEAR FIELD", nsb::pid::nearfield, nearSAtt);
        addRow (headSlider, headLabel, "HEAD SIZE", nsb::pid::headRadius, headSAtt);

        // elevation big vertical slider next to pad
        elevBig.setSliderStyle (juce::Slider::LinearVertical);
        elevBig.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        elevBig.setTooltip ("Elevation");
        elevBig.setTitle ("Elevation");
        pad.setTitle ("Spatial Pad");
        modeBox.setTitle ("Source Mode");
        qualityBox.setTitle ("Quality");
        presetBox.setTitle ("Presets");
        addAndMakeVisible (elevBig);
        elevBigAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            proc.apvts, nsb::pid::elevation, elevBig);

        // room / output knobs
        addKnob (roomAmtKnob, roomAmtLabel, "ROOM", nsb::pid::roomAmount, roomAmtAtt);
        addKnob (roomSizeKnob, roomSizeLabel, "SIZE", nsb::pid::roomSize, roomSizeAtt);
        addKnob (dampKnob, dampLabel, "DAMPING", nsb::pid::roomDamping, dampAtt);
        addKnob (elKnob, elKnobLabel, "EARLY/LATE", nsb::pid::earlyLate, elKnobAtt);
        addKnob (gainKnob, gainLabel, "OUTPUT", nsb::pid::outputGain, gainAtt);

        bypassRoomBtn.setButtonText ("ROOM BYPASS");
        bypassRoomBtn.setClickingTogglesState (true);
        addAndMakeVisible (bypassRoomBtn);
        bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            proc.apvts, nsb::pid::bypassRoom, bypassRoomBtn);

        setupPresets();
        setupSnaps();

        setResizable (true, true);
        setResizeLimits (760, 520, 1800, 1200);
        const int w = (int) proc.apvts.state.getProperty ("uiWidth", 1000);
        const int h = (int) proc.apvts.state.getProperty ("uiHeight", 640);
        setSize (juce::jlimit (760, 1800, w), juce::jlimit (520, 1200, h));
    }

    ~NekoSpaceEditor() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        g.setGradientFill (juce::ColourGradient (nsbui::col::bg, 0, 0,
                                                 nsbui::col::bg2, 0, (float) getHeight(), false));
        g.fillAll();

        // header title
        auto header = getLocalBounds().removeFromTop (44);
        g.setColour (nsbui::col::accent);
        g.setFont (juce::Font (juce::FontOptions (20.0f)).boldened());
        g.drawText ("NEKOSPACE", header.removeFromLeft (140).withTrimmedLeft (14),
                    juce::Justification::centredLeft);
        g.setColour (nsbui::col::textDim);
        g.setFont (juce::Font (juce::FontOptions (20.0f)));
        g.drawText ("BINAURAL", header.removeFromLeft (110),
                    juce::Justification::centredLeft);
        g.setColour (nsbui::col::panelLine);
        g.drawLine (0, 44, (float) getWidth(), 44, 1.0f);

        // section caption
        g.setColour (nsbui::col::textDim);
        g.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
        if (! spaceCaption.isEmpty())
            g.drawText ("SPACE", spaceCaption, juce::Justification::centredLeft);

        // Info + AGPL Appropriate Legal Notices (LICENSE sections 0 and 5d).
        // ASCII only: keeps rendering identical regardless of host source encoding.
        if (! infoArea.isEmpty())
        {
            g.setColour (nsbui::col::panelLine);
            g.drawLine ((float) infoArea.getX(), (float) infoArea.getY() - 2.0f,
                        (float) infoArea.getRight(), (float) infoArea.getY() - 2.0f, 1.0f);

            const juce::Font f (juce::FontOptions (9.5f));
            g.setFont (f);
            g.setColour (nsbui::col::textDim.withAlpha (0.75f));
            g.drawText ("HRTF: Analytic A (built-in)  |  latency 2 ms",
                        infoArea, juce::Justification::centredLeft);
            g.drawText (fitting (f, infoArea.getWidth() - 280,
                                 { "NekoSpace Binaural  |  Copyright (C) 2026 TextureVoice  |  "
                                   "AGPLv3, NO WARRANTY  |  see LICENSE",
                                   "(C) 2026 TextureVoice  |  AGPLv3, NO WARRANTY",
                                   "(C) 2026 TextureVoice  |  AGPLv3" }),
                        infoArea, juce::Justification::centredRight);
        }
    }

    // Longest variant that fits the available width; never renders an ellipsis.
    static juce::String fitting (const juce::Font& f, int width,
                                 std::initializer_list<const char*> options)
    {
        const char* last = nullptr;
        for (auto* o : options)
        {
            last = o;
            if (juce::GlyphArrangement::getStringWidthInt (f, o) <= width)
                return juce::String (o);
        }
        return juce::String (last != nullptr ? last : "");
    }

    void resized() override
    {
        proc.apvts.state.setProperty ("uiWidth", getWidth(), nullptr);
        proc.apvts.state.setProperty ("uiHeight", getHeight(), nullptr);
        auto b = getLocalBounds();
        auto header = b.removeFromTop (44);
        header.removeFromLeft (260);
        presetBox.setBounds (header.removeFromLeft (190).reduced (4, 9));
        modeBox.setBounds (header.removeFromLeft (150).reduced (4, 9));
        qualityBox.setBounds (header.removeFromLeft (120).reduced (4, 9));

        // full-width footer strip: always has room for the info + licence line
        infoArea = b.removeFromBottom (18).reduced (12, 2);

        auto bottom = b.removeFromBottom (150).reduced (10, 6);
        layoutBottom (bottom);

        auto right = b.removeFromRight (280).reduced (8);
        layoutRight (right);

        auto elevArea = b.removeFromRight (44);
        elevBig.setBounds (elevArea.reduced (4, 24));

        pad.setBounds (b.reduced (10));
    }

private:
    // ---- construction helpers ----
    void setupCombo (juce::ComboBox& box, const char* pid,
                     std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& att)
    {
        auto* param = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (pid));
        box.addItemList (param->choices, 1);
        addAndMakeVisible (box);
        att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            proc.apvts, pid, box);
    }

    void addRow (juce::Slider& s, juce::Label& l, const char* name, const char* pid,
                 std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 66, 18);
        s.setTitle (name); // accessibility name for screen readers
        addAndMakeVisible (s);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            proc.apvts, pid, s);
        l.setText (name, juce::dontSendNotification);
        l.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
        l.setColour (juce::Label::textColourId, nsbui::col::textDim);
        addAndMakeVisible (l);
    }

    void addKnob (juce::Slider& s, juce::Label& l, const char* name, const char* pid,
                  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        s.setTitle (name); // accessibility name for screen readers
        addAndMakeVisible (s);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            proc.apvts, pid, s);
        l.setText (name, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
        l.setColour (juce::Label::textColourId, nsbui::col::textDim);
        addAndMakeVisible (l);
    }

    // ---- presets (factory) ----
    struct Preset { const char* name; std::vector<std::pair<const char*, float>> vals; };

    void setupPresets()
    {
        using namespace nsb;
        presets = {
            { "Front 1 m",          { { pid::azimuth, 0 }, { pid::elevation, 0 }, { pid::distance, 1.0f },
                                      { pid::nearfield, 75 }, { pid::roomAmount, 15 } } },
            { "Front Intimate 20 cm",{ { pid::azimuth, 0 }, { pid::elevation, 0 }, { pid::distance, 0.2f },
                                      { pid::nearfield, 90 }, { pid::roomAmount, 8 } } },
            { "Left Ear 3 cm",      { { pid::azimuth, -95 }, { pid::elevation, 0 }, { pid::distance, 0.12f },
                                      { pid::nearfield, 100 }, { pid::roomAmount, 5 } } },
            { "Left Ear 8 cm",      { { pid::azimuth, -95 }, { pid::elevation, 0 }, { pid::distance, 0.17f },
                                      { pid::nearfield, 100 }, { pid::roomAmount, 6 } } },
            { "Right Ear 3 cm",     { { pid::azimuth, 95 }, { pid::elevation, 0 }, { pid::distance, 0.12f },
                                      { pid::nearfield, 100 }, { pid::roomAmount, 5 } } },
            { "Right Ear 8 cm",     { { pid::azimuth, 95 }, { pid::elevation, 0 }, { pid::distance, 0.17f },
                                      { pid::nearfield, 100 }, { pid::roomAmount, 6 } } },
            { "Behind Shoulder L",  { { pid::azimuth, -150 }, { pid::elevation, -12 }, { pid::distance, 0.35f },
                                      { pid::nearfield, 90 }, { pid::roomAmount, 12 } } },
            { "Behind Shoulder R",  { { pid::azimuth, 150 }, { pid::elevation, -12 }, { pid::distance, 0.35f },
                                      { pid::nearfield, 90 }, { pid::roomAmount, 12 } } },
            { "Above",              { { pid::azimuth, 0 }, { pid::elevation, 75 }, { pid::distance, 0.8f },
                                      { pid::nearfield, 80 }, { pid::roomAmount, 15 } } },
            { "Small Quiet Room",   { { pid::azimuth, 20 }, { pid::elevation, 0 }, { pid::distance, 1.4f },
                                      { pid::nearfield, 60 }, { pid::roomAmount, 35 },
                                      { pid::roomSize, 20 }, { pid::roomDamping, 70 } } },
        };
        presetBox.setTextWhenNothingSelected ("Presets...");
        for (int i = 0; i < (int) presets.size(); ++i)
            presetBox.addItem (presets[(size_t) i].name, i + 1);
        presetBox.onChange = [this]
        {
            const int idx = presetBox.getSelectedId() - 1;
            if (idx < 0 || idx >= (int) presets.size()) return;
            for (auto& [pid, v] : presets[(size_t) idx].vals)
                if (auto* prm = proc.apvts.getParameter (pid))
                {
                    prm->beginChangeGesture();
                    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
                    prm->endChangeGesture();
                }
        };
        addAndMakeVisible (presetBox);
    }

    // ---- position snap buttons ----
    void setupSnaps()
    {
        struct Snap { const char* name; float az, el, dist; };
        static const Snap snaps[] = {
            { "L EAR",  -95,  0, 0.12f }, { "R EAR",  95,  0, 0.12f },
            { "FRONT",    0,  0, 1.0f  }, { "BEHIND", 180,  0, 1.0f },
            { "ABOVE",    0, 75, 0.8f  }, { "CENTER",  0,  0, 0.3f  },
        };
        for (auto& sn : snaps)
        {
            auto* btn = snapButtons.add (new juce::TextButton (sn.name));
            addAndMakeVisible (btn);
            const float az = sn.az, el = sn.el, dist = sn.dist;
            btn->onClick = [this, az, el, dist]
            {
                auto set = [this] (const char* pid, float v)
                {
                    if (auto* prm = proc.apvts.getParameter (pid))
                    {
                        prm->beginChangeGesture();
                        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
                        prm->endChangeGesture();
                    }
                };
                set (nsb::pid::azimuth, az);
                set (nsb::pid::elevation, el);
                set (nsb::pid::distance, dist);
            };
        }
    }

    // ---- layout ----
    void layoutRight (juce::Rectangle<int> r)
    {
        const int rowH = 40;
        auto row = [&] (juce::Label& l, juce::Slider& s)
        {
            auto rr = r.removeFromTop (rowH);
            l.setBounds (rr.removeFromTop (14));
            s.setBounds (rr);
        };
        row (azLabel, azSlider);
        row (elLabel, elSlider);
        row (distLabel, distSlider);
        row (widthLabel, widthSlider);
        row (nearLabel, nearSlider);
        row (headLabel, headSlider);

        r.removeFromTop (8);
        auto grid = r.removeFromTop (66);
        const int bw = grid.getWidth() / 3, bh = 30;
        for (int i = 0; i < snapButtons.size(); ++i)
        {
            const int cx = i % 3, cy = i / 3;
            snapButtons[i]->setBounds (grid.getX() + cx * bw + 2,
                                       grid.getY() + cy * (bh + 4), bw - 4, bh);
        }
    }

    void layoutBottom (juce::Rectangle<int> b)
    {
        spaceCaption = b.removeFromTop (14);
        const int knobW = juce::jmin (110, b.getWidth() / 7);
        auto place = [&] (juce::Slider& s, juce::Label& l)
        {
            auto cell = b.removeFromLeft (knobW);
            l.setBounds (cell.removeFromTop (14));
            s.setBounds (cell.reduced (2));
        };
        // meters pinned to the right edge so the bar never ends in dead space
        meter.setBounds (b.removeFromRight (72).reduced (4, 2));
        b.removeFromRight (12);
        bypassRoomBtn.setBounds (b.removeFromRight (116).withSizeKeepingCentre (110, 30));
        b.removeFromRight (12);

        place (roomAmtKnob, roomAmtLabel);
        place (roomSizeKnob, roomSizeLabel);
        place (dampKnob, dampLabel);
        place (elKnob, elKnobLabel);
        b.removeFromLeft (16);
        place (gainKnob, gainLabel);
    }

    NekoSpaceProcessor& proc;
    nsbui::NekoLookAndFeel lnf;
    StereoMeter meter;
    nsbui::SpatialPad pad;

    juce::ComboBox presetBox, modeBox, qualityBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAtt, qualityAtt;

    juce::Slider azSlider, elSlider, distSlider, widthSlider, nearSlider, headSlider, elevBig;
    juce::Label azLabel, elLabel, distLabel, widthLabel, nearLabel, headLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        azSAtt, elSAtt, distSAtt, widthSAtt, nearSAtt, headSAtt, elevBigAtt;

    juce::Slider roomAmtKnob, roomSizeKnob, dampKnob, elKnob, gainKnob;
    juce::Label roomAmtLabel, roomSizeLabel, dampLabel, elKnobLabel, gainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        roomAmtAtt, roomSizeAtt, dampAtt, elKnobAtt, gainAtt;

    juce::TextButton bypassRoomBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt;

    juce::OwnedArray<juce::TextButton> snapButtons;
    std::vector<Preset> presets;

    juce::Rectangle<int> spaceCaption, infoArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NekoSpaceEditor)
};
