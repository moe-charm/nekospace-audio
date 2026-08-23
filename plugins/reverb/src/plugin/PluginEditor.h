// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

class NekoSpaceReverbEditor final : public juce::AudioProcessorEditor
{
public:
    explicit NekoSpaceReverbEditor (NekoSpaceReverbProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    void addKnob (juce::Slider&, juce::Label&, const juce::String&, const char*,
                  std::unique_ptr<SliderAttachment>&);
    void updateAuditionButtons();

    NekoSpaceReverbProcessor& processor;
    juce::Label title, subtitle, notice;
    juce::Slider space, distance, definition, preDelay, decay, bass, air, mix;
    juce::Label spaceLabel, distanceLabel, definitionLabel, preDelayLabel,
                decayLabel, bassLabel, airLabel, mixLabel;
    juce::ToggleButton bypass, monoInput;
    juce::TextButton tailOnly { "TAIL ONLY" }, roomBody { "ROOM BODY" }, erSolo { "ER SOLO" };
    std::unique_ptr<SliderAttachment> spaceAttachment, distanceAttachment,
                                      definitionAttachment, preDelayAttachment,
                                      decayAttachment, bassAttachment, airAttachment,
                                      mixAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment, monoInputAttachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NekoSpaceReverbEditor)
};
