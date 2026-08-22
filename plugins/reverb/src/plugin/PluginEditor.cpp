// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PluginEditor.h"

using namespace juce;

NekoSpaceReverbEditor::NekoSpaceReverbEditor (NekoSpaceReverbProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    title.setText ("NEKOSPACE REVERB", dontSendNotification);
    title.setFont (Font (FontOptions (27.0f, Font::bold)));
    title.setColour (Label::textColourId, Colour (0xfff2e9dc));
    subtitle.setText ("LATE TAIL PROTOTYPE  |  OWNER AUDITION", dontSendNotification);
    subtitle.setColour (Label::textColourId, Colour (0xffd69562));
    notice.setText ("Directional early reflections and Binaural output arrive in later phases.",
                    dontSendNotification);
    notice.setColour (Label::textColourId, Colour (0xff9f9992));
    for (auto* label : { &title, &subtitle, &notice }) addAndMakeVisible (*label);

    addKnob (space, spaceLabel, "SPACE", nsr::pid::space, spaceAttachment);
    addKnob (decay, decayLabel, "DECAY", nsr::pid::decay, decayAttachment);
    addKnob (bass, bassLabel, "BASS TAIL", nsr::pid::bassTail, bassAttachment);
    addKnob (air, airLabel, "AIR TAIL", nsr::pid::airTail, airAttachment);
    addKnob (mix, mixLabel, "MIX", nsr::pid::mix, mixAttachment);

    bypass.setButtonText ("BYPASS");
    addAndMakeVisible (bypass);
    bypassAttachment = std::make_unique<ButtonAttachment> (p.apvts, nsr::pid::bypass, bypass);

    for (auto* button : { &network8, &network16 })
    {
        button->setClickingTogglesState (false);
        button->setWantsKeyboardFocus (false);
        addAndMakeVisible (*button);
    }
    network8.onClick = [this] { processor.setAuditionNetworkLines (8); updateNetworkButtons(); };
    network16.onClick = [this] { processor.setAuditionNetworkLines (16); updateNetworkButtons(); };
    updateNetworkButtons();
    setSize (780, 390);
}

void NekoSpaceReverbEditor::addKnob (Slider& slider, Label& label, const String& text,
                                     const char* id, std::unique_ptr<SliderAttachment>& attachment)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, 82, 22);
    slider.setColour (Slider::rotarySliderFillColourId, Colour (0xffd69562));
    slider.setColour (Slider::rotarySliderOutlineColourId, Colour (0xff393431));
    slider.setColour (Slider::textBoxTextColourId, Colour (0xfff2e9dc));
    slider.setColour (Slider::textBoxBackgroundColourId, Colour (0xff211f1e));
    label.setText (text, dontSendNotification);
    label.setJustificationType (Justification::centred);
    label.setColour (Label::textColourId, Colour (0xffcbc2b8));
    addAndMakeVisible (slider); addAndMakeVisible (label);
    attachment = std::make_unique<SliderAttachment> (processor.apvts, id, slider);
}

void NekoSpaceReverbEditor::updateNetworkButtons()
{
    const bool eight = processor.getAuditionNetworkLines() == 8;
    network8.setColour (TextButton::buttonColourId, eight ? Colour (0xffb9663e) : Colour (0xff393431));
    network16.setColour (TextButton::buttonColourId, ! eight ? Colour (0xffb9663e) : Colour (0xff393431));
}

void NekoSpaceReverbEditor::paint (Graphics& g)
{
    g.fillAll (Colour (0xff171615));
    g.setColour (Colour (0xff2a2725));
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (18.0f).withTrimmedTop (73.0f), 10.0f);
    g.setColour (Colour (0xff7b716a));
    g.setFont (12.0f);
    g.drawText ("AUDITION NETWORK | NOT SAVED OR AUTOMATABLE", 435, 307, 303, 20,
                Justification::centred);
}

void NekoSpaceReverbEditor::resized()
{
    title.setBounds (24, 15, 400, 36);
    subtitle.setBounds (26, 49, 430, 22);
    bypass.setBounds (650, 25, 100, 30);
    notice.setBounds (31, 82, 700, 25);
    const int startX = 35, width = 125, gap = 16;
    Slider* sliders[] = { &space, &decay, &bass, &air, &mix };
    Label* labels[] = { &spaceLabel, &decayLabel, &bassLabel, &airLabel, &mixLabel };
    for (int i = 0; i < 5; ++i)
    {
        const int x = startX + i * (width + gap);
        labels[i]->setBounds (x, 121, width, 23);
        sliders[i]->setBounds (x, 143, width, 146);
    }
    network8.setBounds (526, 329, 102, 32);
    network16.setBounds (636, 329, 102, 32);
}
