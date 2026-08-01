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

    explicit ElevationLab (const nsb::ElevationModel& initial) : model (initial)
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
            addAndMakeVisible (header);

            for (int row = 0; row < kNumRows; ++row)
            {
                auto* s = sliders.add (new juce::Slider());
                s->setSliderStyle (juce::Slider::LinearHorizontal);
                s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 72, 18);
                s->setRange (lo[row], hi[row], step[row]);
                s->setValue (get (col, row), juce::dontSendNotification);
                s->setTitle (juce::String (columnName (col)) + " " + names[row]);
                s->onValueChange = [this, col, row, s] { set (col, row, (float) s->getValue()); };
                addAndMakeVisible (s);
            }
        }

        for (int row = 0; row < kNumRows; ++row)
        {
            auto* l = rowLabels.add (new juce::Label ({}, names[row]));
            l->setFont (juce::Font (juce::FontOptions (11.0f)));
            l->setColour (juce::Label::textColourId, col::textDim);
            l->setJustificationType (juce::Justification::centredRight);
            addAndMakeVisible (l);
        }

        resetButton.setButtonText ("Reset to Analytic B");
        resetButton.onClick = [this]
        {
            model = nsb::ElevationModel::analyticBDefaults();
            refreshSliders();
            if (onChange) onChange (model);
        };
        addAndMakeVisible (resetButton);

        copyButton.setButtonText ("Copy as C++");
        copyButton.onClick = [this] { juce::SystemClipboard::copyTextToClipboard (asCode()); };
        addAndMakeVisible (copyButton);

        hint.setText ("Tune with Room OFF and real material. Find \"above\" and \"below\" "
                      "independently - a setting that is merely brighter is not a setting "
                      "that is higher.", juce::dontSendNotification);
        hint.setFont (juce::Font (juce::FontOptions (11.0f)));
        hint.setColour (juce::Label::textColourId, col::textDim);
        hint.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (hint);

        setSize (760, 430);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (col::bg);
        g.setColour (col::panelLine);
        for (int col = 1; col < kNumCols; ++col)
        {
            const float x = (float) (labelW + col * colW());
            g.drawLine (x - 4.0f, 30.0f, x - 4.0f, (float) getHeight() - 74.0f, 1.0f);
        }
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (10);
        auto foot = b.removeFromBottom (54);
        hint.setBounds (foot.removeFromTop (30));
        resetButton.setBounds (foot.removeFromLeft (170).reduced (2));
        copyButton.setBounds (foot.removeFromLeft (140).reduced (2));

        auto head = b.removeFromTop (22);
        head.removeFromLeft (labelW);
        for (int col = 0; col < kNumCols; ++col)
            headers[col]->setBounds (head.removeFromLeft (colW()));

        const int rowH = b.getHeight() / kNumRows;
        for (int row = 0; row < kNumRows; ++row)
        {
            auto r = b.removeFromTop (rowH);
            rowLabels[row]->setBounds (r.removeFromLeft (labelW).withTrimmedRight (6));
            for (int col = 0; col < kNumCols; ++col)
                sliders[col * kNumRows + row]->setBounds (r.removeFromLeft (colW()).reduced (3, 2));
        }
    }

    const nsb::ElevationModel& current() const noexcept { return model; }

private:
    static constexpr int kNumCols = 3;   // below / level / above
    static constexpr int kNumRows = 8;
    static constexpr int labelW = 78;

    int colW() const { return (getLocalBounds().reduced (10).getWidth() - labelW) / kNumCols; }

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
        for (int col = 0; col < kNumCols; ++col)
            for (int row = 0; row < kNumRows; ++row)
                sliders[col * kNumRows + row]->setValue (get (col, row),
                                                         juce::dontSendNotification);
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
    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::Label> headers, rowLabels;
    juce::TextButton resetButton, copyButton;
    juce::Label hint;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ElevationLab)
};
} // namespace nsbui
