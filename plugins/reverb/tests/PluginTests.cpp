// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "../src/plugin/PluginProcessor.h"
#include <algorithm>
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

    check (processor.getNumPrograms() == static_cast<int> (nsr::factoryPresets.size()),
           "host program list exposes every factory preset");
    for (int i = 0; i < processor.getNumPrograms(); ++i)
        check (processor.getProgramName (i).isNotEmpty(),
               "every host-visible factory program has a name");
    processor.setCurrentProgram (1);
    check (processor.getCurrentProgram() == 1,
           "host program selection applies and reports the selected preset");
    processor.setCurrentProgram (0);
    processor.setPlayConfigDetails (2, 2, 48000.0, 127);
    processor.prepareToPlay (48000.0, 127);

    check (std::abs (processor.apvts.getRawParameterValue (nsr::pid::distance)->load() - 25.0f)
           < 0.001f, "Distance has the documented prototype default");
    check (std::abs (processor.apvts.getRawParameterValue (nsr::pid::definition)->load() - 65.0f)
           < 0.001f, "Definition has the documented prototype default");
    check (std::abs (processor.apvts.getRawParameterValue (nsr::pid::preDelay)->load() - 12.0f)
           < 0.001f, "Pre-delay has the documented prototype default");
    check (std::abs (processor.apvts.getRawParameterValue (nsr::pid::wetMonoInput)->load())
           < 0.001f, "Wet Mono Input defaults Off");
    check (processor.getAuditionMode() == nsr::RoomBodyAuditionMode::roomBody,
           "Room Body is the fresh-instance audition default");
    check (processor.getMatchingFactoryPreset() == 0,
           "fresh parameter tuple matches the Default factory preset");

    {
        setPlain (processor, nsr::pid::space, 99.0f);
        setPlain (processor, nsr::pid::mix, 1.0f);
        setPlain (processor, nsr::pid::wetMonoInput, 1.0f);
        setPlain (processor, nsr::pid::bypass, 1.0f);
        processor.setAuditionMode (nsr::RoomBodyAuditionMode::earlyOnly);
        processor.applyFactoryPreset (2);
        const auto& expected = nsr::factoryPresets[2];
        check (processor.getMatchingFactoryPreset() == 2,
               "factory preset applies a complete deterministic tuple");
        check (std::abs (processor.apvts.getRawParameterValue (nsr::pid::decay)->load()
                        - expected.decaySeconds) < 0.001f,
               "factory preset applies its Decay value");
        check (processor.apvts.getRawParameterValue (nsr::pid::bypass)->load() < 0.5f,
               "factory preset clears Bypass");
        check (processor.apvts.getRawParameterValue (nsr::pid::wetMonoInput)->load() < 0.5f,
               "factory preset clears Wet Mono Input");
        check (processor.getAuditionMode() == nsr::RoomBodyAuditionMode::roomBody,
               "factory preset returns the diagnostic bus to Room Body");
        setPlain (processor, nsr::pid::space, expected.space + 1.0f);
        check (processor.getMatchingFactoryPreset() == -1,
               "editing a factory tuple is reported as Custom");
        processor.applyFactoryPreset (0);
    }

    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        auto* parameter = processor.apvts.getParameter (nsr::pid::wetMonoInput);
        check (parameter != nullptr, "Wet Mono Input host parameter exists");
        if (parameter != nullptr)
        {
            constexpr float savedNormalised = 0.852673f;
            parameter->setValueNotifyingHost (savedNormalised);
            const float canonicalSaved = parameter->getValue();
            check (canonicalSaved == 1.0f,
                   "Wet Mono Input canonicalises arbitrary host values at the parameter boundary");
            juce::MemoryBlock state;
            processor.getStateInformation (state);
            parameter->setValueNotifyingHost (0.123137f);
            processor.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
            const float restored = parameter->getValue();
            check (restored == canonicalSaved,
                   "Wet Mono Input restores its canonical host value with its editor open");
        }
        setPlain (processor, nsr::pid::wetMonoInput, 0.0f);
    }

    {
        setPlain (processor, nsr::pid::mix, 0.0f);
        setPlain (processor, nsr::pid::wetMonoInput, 1.0f);
        juce::AudioBuffer<float> settle (2, 2400);
        settle.clear();
        process (processor, settle);
        auto input = signal (511);
        auto output = input;
        process (processor, output);
        bool exact = true;
        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < output.getNumSamples(); ++i)
                exact = exact && output.getSample (channel, i) == input.getSample (channel, i);
        check (exact, "Mix zero is exact dry after its bounded transition");
        setPlain (processor, nsr::pid::wetMonoInput, 0.0f);
    }

    {
        setPlain (processor, nsr::pid::mix, 100.0f);
        setPlain (processor, nsr::pid::wetMonoInput, 1.0f);
        setPlain (processor, nsr::pid::bypass, 1.0f);
        juce::AudioBuffer<float> settle (2, 2400);
        settle.clear();
        process (processor, settle);
        auto input = signal (511);
        auto output = input;
        process (processor, output);
        bool exact = true;
        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < output.getNumSamples(); ++i)
                exact = exact && output.getSample (channel, i) == input.getSample (channel, i);
        check (exact, "Bypass is exact original stereo dry with Wet Mono Input On");
        setPlain (processor, nsr::pid::bypass, 0.0f);
        setPlain (processor, nsr::pid::wetMonoInput, 0.0f);
    }

    {
        NekoSpaceReverbProcessor cooldown;
        setPlain (cooldown, nsr::pid::mix, 100.0f);
        cooldown.setPlayConfigDetails (2, 2, 48000.0, 127);
        cooldown.prepareToPlay (48000.0, 127);
        juce::AudioBuffer<float> initialSilence (2, 127);
        initialSilence.clear();
        process (cooldown, initialSilence);
        setPlain (cooldown, nsr::pid::bypass, 1.0f);
        auto bypassedInput = signal (4096);
        process (cooldown, bypassedInput);
        setPlain (cooldown, nsr::pid::bypass, 0.0f);
        juce::AudioBuffer<float> release (2, 4096);
        release.clear();
        process (cooldown, release);
        bool silent = true;
        for (int channel = 0; channel < release.getNumChannels(); ++channel)
            for (int i = 0; i < release.getNumSamples(); ++i)
                silent = silent && release.getSample (channel, i) == 0.0f;
        check (silent, "audio received while bypassed cannot charge a later room tail");
    }

    {
        NekoSpaceReverbProcessor mono;
        mono.setPlayConfigDetails (1, 2, 48000.0, 127);
        mono.prepareToPlay (48000.0, 127);
        setPlain (mono, nsr::pid::mix, 0.0f);
        juce::AudioBuffer<float> settle (2, 2400);
        settle.clear();
        process (mono, settle);
        juce::AudioBuffer<float> monoInput (2, 511);
        for (int i = 0; i < monoInput.getNumSamples(); ++i)
        {
            monoInput.setSample (0, i, 0.37f * std::sin (0.021f * i));
            monoInput.setSample (1, i, -0.9f); // output-only channel must be overwritten.
        }
        process (mono, monoInput);
        bool duplicated = true;
        for (int i = 0; i < monoInput.getNumSamples(); ++i)
            duplicated = duplicated
                      && monoInput.getSample (1, i) == monoInput.getSample (0, i);
        check (duplicated, "mono host input is duplicated exactly to stereo output");
    }

    {
        setPlain (processor, nsr::pid::space, 73.0f);
        setPlain (processor, nsr::pid::distance, 83.0f);
        setPlain (processor, nsr::pid::definition, 17.0f);
        setPlain (processor, nsr::pid::preDelay, 97.0f);
        setPlain (processor, nsr::pid::wetMonoInput, 1.0f);
        processor.setAuditionMode (nsr::RoomBodyAuditionMode::tailOnly);
        juce::MemoryBlock state;
        processor.getStateInformation (state);
        setPlain (processor, nsr::pid::space, 12.0f);
        setPlain (processor, nsr::pid::distance, 4.0f);
        setPlain (processor, nsr::pid::definition, 92.0f);
        setPlain (processor, nsr::pid::preDelay, 3.0f);
        setPlain (processor, nsr::pid::wetMonoInput, 0.0f);
        processor.setAuditionMode (nsr::RoomBodyAuditionMode::earlyOnly);
        processor.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
        check (std::abs (processor.apvts.getRawParameterValue (nsr::pid::space)->load() - 73.0f)
               < 0.001f, "state restores provisional controls");
        check (std::abs (processor.apvts.getRawParameterValue (nsr::pid::distance)->load() - 83.0f)
               < 0.001f, "state restores Distance");
        check (std::abs (processor.apvts.getRawParameterValue (nsr::pid::definition)->load() - 17.0f)
               < 0.001f, "state restores Definition");
        check (std::abs (processor.apvts.getRawParameterValue (nsr::pid::preDelay)->load() - 97.0f)
               < 0.001f, "state restores Pre-delay");
        check (std::abs (processor.apvts.getRawParameterValue (nsr::pid::wetMonoInput)->load()
                        - 1.0f) < 0.001f,
               "state restores Wet Mono Input");
        check (processor.getAuditionMode() == nsr::RoomBodyAuditionMode::earlyOnly,
               "Room Body audition mode is not serialized");
    }

    {
        juce::MemoryBlock completeState;
        processor.getStateInformation (completeState);
        auto xml = juce::AudioProcessor::getXmlFromBinary (
            completeState.getData(), static_cast<int> (completeState.getSize()));
        check (xml != nullptr, "prototype state XML can be decoded");
        auto oldTree = xml != nullptr ? juce::ValueTree::fromXml (*xml) : juce::ValueTree();
        for (int child = oldTree.getNumChildren(); --child >= 0;)
        {
            const auto id = oldTree.getChild (child).getProperty ("id").toString();
            if (id == nsr::pid::distance || id == nsr::pid::definition
                || id == nsr::pid::preDelay || id == nsr::pid::wetMonoInput)
                oldTree.removeChild (child, nullptr);
        }
        juce::MemoryBlock oldState;
        if (auto oldXml = oldTree.createXml())
            juce::AudioProcessor::copyXmlToBinary (*oldXml, oldState);
        setPlain (processor, nsr::pid::distance, 91.0f);
        setPlain (processor, nsr::pid::definition, 9.0f);
        setPlain (processor, nsr::pid::preDelay, 119.0f);
        setPlain (processor, nsr::pid::wetMonoInput, 1.0f);
        processor.setStateInformation (oldState.getData(), static_cast<int> (oldState.getSize()));
        check (std::abs (processor.apvts.getRawParameterValue (nsr::pid::distance)->load() - 25.0f)
               < 0.001f, "old state supplies the Distance default");
        check (std::abs (processor.apvts.getRawParameterValue (nsr::pid::definition)->load() - 65.0f)
               < 0.001f, "old state supplies the Definition default");
        check (std::abs (processor.apvts.getRawParameterValue (nsr::pid::preDelay)->load() - 12.0f)
               < 0.001f, "old state supplies the Pre-delay default");
        check (std::abs (processor.apvts.getRawParameterValue (nsr::pid::wetMonoInput)->load())
               < 0.001f, "old state supplies the Wet Mono Input Off default");
    }

    {
        setPlain (processor, nsr::pid::mix, 100.0f);
        setPlain (processor, nsr::pid::preDelay, 12.0f);
        float phase = 0.0f;
        float previousLeft = 0.0f;
        float maximumStep = 0.0f;
        for (int block = 0; block < 80; ++block)
        {
            if (block == 20)
                processor.setAuditionMode (nsr::RoomBodyAuditionMode::tailOnly);
            if (block == 35)
                processor.setAuditionMode (nsr::RoomBodyAuditionMode::earlyOnly);
            if (block == 50)
                processor.setAuditionMode (nsr::RoomBodyAuditionMode::roomBody);
            juce::AudioBuffer<float> output (2, 127);
            for (int i = 0; i < output.getNumSamples(); ++i)
            {
                const float sample = 0.05f * std::sin (phase);
                phase += 2.0f * juce::MathConstants<float>::pi * 100.0f / 48000.0f;
                output.setSample (0, i, sample);
                output.setSample (1, i, sample);
            }
            process (processor, output);
            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < output.getNumSamples(); ++i)
                    check (std::isfinite (output.getSample (channel, i)),
                           "Room Body crossfade remains finite");
            for (int i = 0; i < output.getNumSamples(); ++i)
            {
                maximumStep = std::max (maximumStep,
                    std::abs (output.getSample (0, i) - previousLeft));
                previousLeft = output.getSample (0, i);
            }
        }
        check (maximumStep < 0.06f, "Room Body crossfade has no click-sized output step");
    }

    {
        setPlain (processor, nsr::pid::preDelay, 0.0f);
        const double withoutPreDelay = processor.getTailLengthSeconds();
        setPlain (processor, nsr::pid::preDelay, 120.0f);
        check (processor.getTailLengthSeconds() >= withoutPreDelay + 0.119,
               "host tail report immediately includes the common wet pre-delay");

        setPlain (processor, nsr::pid::decay, 4.0f);
        setPlain (processor, nsr::pid::bassTail, 25.0f);
        setPlain (processor, nsr::pid::airTail, 25.0f);
        check (processor.getTailLengthSeconds() >= 4.49,
               "host tail report never shortens the active mid-band T60");
    }

    if (failures == 0) std::cout << "All Reverb plugin tests passed\n";
    return failures == 0 ? 0 : 1;
}
