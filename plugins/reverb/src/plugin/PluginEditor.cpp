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
    subtitle.setText ("ROOM BODY PROTOTYPE  |  OWNER AUDITION", dontSendNotification);
    subtitle.setColour (Label::textColourId, Colour (0xffd69562));
    notice.setText ("Six first-order reflections are active. HRTF output and higher orders come later.",
                    dontSendNotification);
    notice.setColour (Label::textColourId, Colour (0xff9f9992));
    for (auto* label : { &title, &subtitle, &notice }) addAndMakeVisible (*label);

    addKnob (space, spaceLabel, "SPACE", nsr::pid::space, spaceAttachment);
    addKnob (distance, distanceLabel, "DISTANCE", nsr::pid::distance, distanceAttachment);
    addKnob (definition, definitionLabel, "DEFINITION", nsr::pid::definition,
             definitionAttachment);
    addKnob (preDelay, preDelayLabel, "PRE-DELAY", nsr::pid::preDelay, preDelayAttachment);
    addKnob (decay, decayLabel, "DECAY", nsr::pid::decay, decayAttachment);
    addKnob (bass, bassLabel, "BASS TAIL", nsr::pid::bassTail, bassAttachment);
    addKnob (air, airLabel, "AIR TAIL", nsr::pid::airTail, airAttachment);
    addKnob (mix, mixLabel, "MIX", nsr::pid::mix, mixAttachment);

    bypass.setButtonText ("BYPASS");
    addAndMakeVisible (bypass);
    bypassAttachment = std::make_unique<ButtonAttachment> (p.apvts, nsr::pid::bypass, bypass);

    for (auto* button : { &tailOnly, &roomBody })
    {
        button->setClickingTogglesState (false);
        button->setWantsKeyboardFocus (false);
        addAndMakeVisible (*button);
    }
    tailOnly.onClick = [this] { processor.setRoomBodyEnabled (false); updateRoomBodyButtons(); };
    roomBody.onClick = [this] { processor.setRoomBodyEnabled (true); updateRoomBodyButtons(); };
    updateRoomBodyButtons();
    setSize (900, 520);
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

void NekoSpaceReverbEditor::updateRoomBodyButtons()
{
    const bool enabled = processor.isRoomBodyEnabled();
    tailOnly.setColour (TextButton::buttonColourId,
                        ! enabled ? Colour (0xffb9663e) : Colour (0xff393431));
    roomBody.setColour (TextButton::buttonColourId,
                       enabled ? Colour (0xffb9663e) : Colour (0xff393431));
    notice.setText (enabled
                        ? "Six first-order reflections are active. HRTF output and higher orders come later."
                        : "Tail Only mutes the six reflections; the accepted 16-line late tail keeps running.",
                    dontSendNotification);
}

void NekoSpaceReverbEditor::paint (Graphics& g)
{
    g.fillAll (Colour (0xff171615));
    g.setColour (Colour (0xff2a2725));
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (18.0f).withTrimmedTop (73.0f), 10.0f);
    g.setColour (Colour (0xffd69562));
    g.setFont (Font (FontOptions (12.0f, Font::bold)));
    g.drawText ("ROOM BODY", 35, 104, 180, 20, Justification::centredLeft);
    g.drawText ("TAIL & BLEND", 35, 278, 180, 20, Justification::centredLeft);
    g.setColour (Colour (0xff7b716a));
    g.setFont (12.0f);
    g.drawText ("AUDITION MODE | NOT SAVED OR AUTOMATABLE", 325, 467, 535, 20,
                Justification::centred);
}

void NekoSpaceReverbEditor::resized()
{
    title.setBounds (24, 15, 430, 36);
    subtitle.setBounds (26, 49, 500, 22);
    bypass.setBounds (770, 25, 100, 30);
    notice.setBounds (31, 79, 835, 25);
    constexpr int startX = 35, width = 175, gap = 35;
    Slider* topSliders[] = { &space, &distance, &definition, &preDelay };
    Label* topLabels[] = { &spaceLabel, &distanceLabel, &definitionLabel, &preDelayLabel };
    Slider* bottomSliders[] = { &decay, &bass, &air, &mix };
    Label* bottomLabels[] = { &decayLabel, &bassLabel, &airLabel, &mixLabel };
    for (int i = 0; i < 4; ++i)
    {
        const int x = startX + i * (width + gap);
        topLabels[i]->setBounds (x, 122, width, 23);
        topSliders[i]->setBounds (x, 142, width, 126);
        bottomLabels[i]->setBounds (x, 296, width, 23);
        bottomSliders[i]->setBounds (x, 316, width, 126);
    }
    tailOnly.setBounds (40, 461, 120, 32);
    roomBody.setBounds (168, 461, 130, 32);
}
