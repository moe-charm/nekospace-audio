// SPDX-FileCopyrightText: 2026 TextureVoice
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// Elevation Lab — a tuning bench, not a product control.
//
// Three anchors (below / level / above) are adjusted by ear while real material plays.
// Up and down are deliberately independent: the setting that reads as "lifted out of the
// head" is found separately from the one that reads as "sunk toward the floor", which a
// symmetric formula cannot express. Once a curve works, "Copy as C++" prints it so it can
// be frozen into ElevationModel::analyticBDefaults.
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "NekoLookAndFeel.h"
#include "../dsp/ElevationModel.h"

namespace nsbui
{
class ElevationLab : public juce::Component
{
public:
    // Called whenever a value changes; the host rebuilds the profile off the audio thread.
    std::function<void (const nsb::ElevationModel&)> onChange;

    explicit ElevationLab (const nsb::ElevationModel& initial, const nsb::ElevationMacros& mac)
        : model (initial), macros (mac)
    {
        struct MacroDef { const char* name; const char* hint; double lo, hi; float* target; };
        const MacroDef macroDefs[kNumMacros] = {
            { "UP",    "how far a raised source departs from ear level",   0.0, 2.0, &macros.up },
            { "DOWN",  "how far a lowered source departs from ear level",  0.0, 2.0, &macros.down },
            { "BODY",  "shoulder reflection - the low-frequency height cue", 0.0, 2.0, &macros.body },
            { "FOCUS", "narrow colouring vs broad tonal shift",            0.4, 2.0, &macros.focus },
        };
        for (int i = 0; i < kNumMacros; ++i)
        {
            auto* s = macroSliders.add (new juce::Slider());
            s->setSliderStyle (juce::Slider::LinearHorizontal);
            s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
            s->setRange (macroDefs[i].lo, macroDefs[i].hi, 0.01);
            s->setValue (*macroDefs[i].target, juce::dontSendNotification);
            s->setTitle (macroDefs[i].name);
            float* t = macroDefs[i].target;
            s->onValueChange = [this, t, s] { *t = (float) s->getValue(); applyMacros(); };
            addAndMakeVisible (s);

            auto* n = macroNames.add (new juce::Label ({}, macroDefs[i].name));
            n->setFont (juce::Font (juce::FontOptions (14.0f)).boldened());
            n->setColour (juce::Label::textColourId, col::accent);
            n->setJustificationType (juce::Justification::centredRight);
            addAndMakeVisible (n);

            auto* h = macroHints.add (new juce::Label ({}, macroDefs[i].hint));
            h->setFont (juce::Font (juce::FontOptions (10.5f)));
            h->setColour (juce::Label::textColourId, col::textDim);
            addAndMakeVisible (h);
        }

        advancedButton.setButtonText ("Advanced...");
        advancedButton.setClickingTogglesState (true);
        advancedButton.onClick = [this] { setAdvancedVisible (advancedButton.getToggleState()); };
        addAndMakeVisible (advancedButton);

        buildAdvanced();
        setAdvancedVisible (false);
    }

    void buildAdvanced()
    {
        static const char* names[kNumRows] = {
            "Notch Hz", "Notch dB", "Notch Q", "Peak ratio",
            "Peak dB", "Shelf dB", "Torso ms", "Torso amt"
        };
        static const double lo[kNumRows] = { 2000.0, -30.0, 0.5, 0.2,  -12.0, -18.0, 0.05, 0.0 };
        static const double hi[kNumRows] = { 16000.0,  0.0, 12.0, 1.5,  15.0,  18.0, 2.50, 0.9 };
        static const double step[kNumRows] = { 10.0, 0.1, 0.05, 0.005, 0.1, 0.1, 0.005, 0.005 };

        for (int col = 0; col < kNumCols; ++col)
        {
            auto* header = headers.add (new juce::Label ({}, columnName (col)));
            header->setJustificationType (juce::Justification::centred);
            header->setFont (juce::Font (juce::FontOptions (13.0f)).boldened());
            header->setColour (juce::Label::textColourId, col::accent);
            addChildComponent (header);

            for (int row = 0; row < kNumRows; ++row)
            {
                auto* s = sliders.add (new juce::Slider());
                s->setSliderStyle (juce::Slider::LinearHorizontal);
                s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 72, 18);
                s->setRange (lo[row], hi[row], step[row]);
                s->setValue (get (col, row), juce::dontSendNotification);
                s->setTitle (juce::String (columnName (col)) + " " + names[row]);
                s->onValueChange = [this, col, row, s] { set (col, row, (float) s->getValue()); };
                addChildComponent (s);   // shown only in Advanced
            }
        }

        for (int row = 0; row < kNumRows; ++row)
        {
            auto* l = rowLabels.add (new juce::Label ({}, names[row]));
            l->setFont (juce::Font (juce::FontOptions (11.0f)));
            l->setColour (juce::Label::textColourId, col::textDim);
            l->setJustificationType (juce::Justification::centredRight);
            addChildComponent (l);
        }

        resetButton.setButtonText ("Reset");
        resetButton.onClick = [this]
        {
            macros = nsb::ElevationMacros{};
            model = nsb::ElevationModel::analyticBDefaults();
            refreshSliders();
            notify();
        };
        addAndMakeVisible (resetButton);

        copyButton.setButtonText ("Copy as C++");
        copyButton.onClick = [this] { juce::SystemClipboard::copyTextToClipboard (asCode()); };
        addAndMakeVisible (copyButton);

        hint.setText ("Room OFF, real voice. Get UP reading as out-of-the-head and above - "
                      "not merely brighter - then find DOWN on its own.",
                      juce::dontSendNotification);
        hint.setFont (juce::Font (juce::FontOptions (11.0f)));
        hint.setColour (juce::Label::textColourId, col::textDim);
        hint.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (hint);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (col::bg);
        if (! advancedVisible) return;
        g.setColour (col::panelLine);
        g.drawLine (10.0f, (float) advancedTop - 8.0f,
                    (float) getWidth() - 10.0f, (float) advancedTop - 8.0f, 1.0f);
        for (int c = 1; c < kNumCols; ++c)
        {
            const float x = (float) (10 + labelW + c * colW());
            g.drawLine (x - 4.0f, (float) advancedTop + 18.0f,
                        x - 4.0f, (float) getHeight() - 66.0f, 1.0f);
        }
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (10);

        auto foot = b.removeFromBottom (48);
        auto row = foot.removeFromTop (26);
        resetButton.setBounds (row.removeFromLeft (90).reduced (2));
        copyButton.setBounds (row.removeFromLeft (130).reduced (2));
        row.removeFromLeft (10);
        advancedButton.setBounds (row.removeFromLeft (130).reduced (2));
        hint.setBounds (foot);

        for (int i = 0; i < kNumMacros; ++i)
        {
            auto r = b.removeFromTop (46);
            macroNames[i]->setBounds (r.removeFromLeft (74).withTrimmedRight (8)
                                        .withTrimmedBottom (14));
            auto hintArea = r.removeFromBottom (14);
            macroSliders[i]->setBounds (r.reduced (2, 1));
            macroHints[i]->setBounds (hintArea.withTrimmedLeft (4));
        }

        if (! advancedVisible) return;

        b.removeFromTop (10);
        advancedTop = b.getY();
        auto head = b.removeFromTop (20);
        head.removeFromLeft (labelW);
        for (int c = 0; c < kNumCols; ++c)
            headers[c]->setBounds (head.removeFromLeft (colW()));

        const int rowH = juce::jmax (20, b.getHeight() / kNumRows);
        for (int rw = 0; rw < kNumRows; ++rw)
        {
            auto r = b.removeFromTop (rowH);
            rowLabels[rw]->setBounds (r.removeFromLeft (labelW).withTrimmedRight (6));
            for (int c = 0; c < kNumCols; ++c)
                sliders[c * kNumRows + rw]->setBounds (r.removeFromLeft (colW()).reduced (3, 1));
        }
    }

    const nsb::ElevationModel& current() const noexcept { return model; }
    const nsb::ElevationMacros& currentMacros() const noexcept { return macros; }

private:
    static constexpr int kNumCols = 3;   // below / level / above
    static constexpr int kNumRows = 8;
    static constexpr int kNumMacros = 4;
    static constexpr int labelW = 78;

    int colW() const { return (getLocalBounds().reduced (10).getWidth() - labelW) / kNumCols; }

    void setAdvancedVisible (bool shouldShow)
    {
        advancedVisible = shouldShow;
        for (auto* s : sliders)   s->setVisible (shouldShow);
        for (auto* l : headers)   l->setVisible (shouldShow);
        for (auto* l : rowLabels) l->setVisible (shouldShow);
        // the owning DocumentWindow was given resizeToFitWhenContentChangesSize, so it
        // follows this
        setSize (640, shouldShow ? 640 : 300);
        repaint();
    }

    // The macros rebuild the whole anchor set, so they intentionally discard any
    // hand-edits made in Advanced — one direction of authority, no silent conflict.
    void applyMacros()
    {
        model = nsb::ElevationModel::fromMacros (macros);
        refreshSliders();
        notify();
    }

    void notify() { if (onChange) onChange (model); }

    static const char* columnName (int col)
    {
        return col == 0 ? "Below -60" : (col == 1 ? "Level 0" : "Above +60");
    }

    nsb::ElevationAnchor& anchor (int col) noexcept
    {
        return col == 0 ? model.below : (col == 1 ? model.level : model.above);
    }

    float get (int col, int row) noexcept
    {
        const auto& a = anchor (col);
        switch (row)
        {
            case 0: return a.notchHz;   case 1: return a.notchDb;
            case 2: return a.notchQ;    case 3: return a.peakRatio;
            case 4: return a.peakDb;    case 5: return a.shelfDb;
            case 6: return a.torsoMs;   default: return a.torsoAmt;
        }
    }

    void set (int col, int row, float v)
    {
        auto& a = anchor (col);
        switch (row)
        {
            case 0: a.notchHz = v; break;   case 1: a.notchDb = v; break;
            case 2: a.notchQ = v; break;    case 3: a.peakRatio = v; break;
            case 4: a.peakDb = v; break;    case 5: a.shelfDb = v; break;
            case 6: a.torsoMs = v; break;   default: a.torsoAmt = v; break;
        }
        if (onChange) onChange (model);
    }

    void refreshSliders()
    {
        for (int c = 0; c < kNumCols; ++c)
            for (int rw = 0; rw < kNumRows; ++rw)
                sliders[c * kNumRows + rw]->setValue (get (c, rw), juce::dontSendNotification);

        const float* vals[kNumMacros] = { &macros.up, &macros.down, &macros.body, &macros.focus };
        for (int i = 0; i < kNumMacros; ++i)
            macroSliders[i]->setValue (*vals[i], juce::dontSendNotification);
    }

    juce::String asCode()
    {
        auto line = [this] (const char* name, int c)
        {
            const auto& a = anchor (c);
            return juce::String ("        m.") + name + " = { "
                 + juce::String (a.notchHz, 1) + "f, " + juce::String (a.notchDb, 2) + "f, "
                 + juce::String (a.notchQ, 2) + "f, " + juce::String (a.peakRatio, 3) + "f, "
                 + juce::String (a.peakDb, 2) + "f, " + juce::String (a.shelfDb, 2) + "f, "
                 + juce::String (a.torsoMs, 3) + "f, " + juce::String (a.torsoAmt, 3) + "f };\n";
        };
        return juce::String ("        // notchHz, notchDb, notchQ, peakRatio, peakDb, "
                             "shelfDb, torsoMs, torsoAmt\n")
             + line ("below", 0) + line ("level", 1) + line ("above", 2);
    }

    nsb::ElevationModel model;
    nsb::ElevationMacros macros;
    juce::OwnedArray<juce::Slider> sliders, macroSliders;
    juce::OwnedArray<juce::Label> headers, rowLabels, macroNames, macroHints;
    juce::TextButton resetButton, copyButton, advancedButton;
    juce::Label hint;
    bool advancedVisible = false;
    int advancedTop = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ElevationLab)
};
} // namespace nsbui
