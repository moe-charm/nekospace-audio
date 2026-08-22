// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "../src/plugin/PluginProcessor.h"
#include <cmath>
#include <iostream>

namespace
{
int failures = 0;
void check (bool condition, const char* message)
{
    if (! condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
void setPlain (NekoSpaceReverbProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.apvts.getParameter (id);
    check (parameter != nullptr, "parameter exists");
    if (parameter != nullptr)
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}
juce::AudioBuffer<float> signal (int samples)
{
    juce::AudioBuffer<float> buffer (2, samples);
    for (int i = 0; i < samples; ++i)
    {
        buffer.setSample (0, i, 0.31f * std::sin (0.017f * i) + (i == 0 ? 0.4f : 0.0f));
        buffer.setSample (1, i, 0.23f * std::cos (0.013f * i));
    }
    return buffer;
}
void process (NekoSpaceReverbProcessor& processor, juce::AudioBuffer<float>& buffer)
{
    juce::MidiBuffer midi;
    processor.processBlock (buffer, midi);
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    NekoSpaceReverbProcessor processor;
    processor.setPlayConfigDetails (2, 2, 48000.0, 127);
    processor.prepareToPlay (48000.0, 127);

    {
        auto input = signal (511);
        auto output = input;
        setPlain (processor, nsr::pid::mix, 0.0f);
        process (processor, output);
        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < output.getNumSamples(); ++i)
                check (output.getSample (channel, i) == input.getSample (channel, i),
                       "Mix zero is exact dry");
    }

    {
        auto input = signal (511);
        auto output = input;
        setPlain (processor, nsr::pid::mix, 100.0f);
        setPlain (processor, nsr::pid::bypass, 1.0f);
        process (processor, output);
        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < output.getNumSamples(); ++i)
                check (output.getSample (channel, i) == input.getSample (channel, i),
                       "Bypass is exact dry");
        setPlain (processor, nsr::pid::bypass, 0.0f);
    }

    {
        setPlain (processor, nsr::pid::space, 73.0f);
        processor.setAuditionNetworkLines (8);
        juce::MemoryBlock state;
        processor.getStateInformation (state);
        setPlain (processor, nsr::pid::space, 12.0f);
        processor.setAuditionNetworkLines (16);
        processor.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
        check (std::abs (processor.apvts.getRawParameterValue (nsr::pid::space)->load() - 73.0f)
               < 0.001f, "state restores provisional controls");
        check (processor.getAuditionNetworkLines() == 16, "network audition is not serialized");
    }

    {
        setPlain (processor, nsr::pid::mix, 100.0f);
        for (int block = 0; block < 80; ++block)
        {
            if (block == 20) processor.setAuditionNetworkLines (8);
            if (block == 50) processor.setAuditionNetworkLines (16);
            auto output = signal (127);
            process (processor, output);
            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < output.getNumSamples(); ++i)
                    check (std::isfinite (output.getSample (channel, i)),
                           "network crossfade remains finite");
        }
    }

    if (failures == 0) std::cout << "All Reverb plugin tests passed\n";
    return failures == 0 ? 0 : 1;
}

