// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

AudioProcessorValueTreeState::ParameterLayout NekoSpaceReverbProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;
    using Float = AudioParameterFloat;
    const auto percent = NormalisableRange<float> (0.0f, 100.0f, 0.1f);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { nsr::pid::bypass, 1 },
                                                       "Bypass", false));
    layout.add (std::make_unique<Float> (ParameterID { nsr::pid::space, 1 }, "Space",
                                          percent, 35.0f,
                                          AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<Float> (ParameterID { nsr::pid::decay, 1 }, "Decay",
                                          NormalisableRange<float> (0.15f, 4.0f, 0.01f, 0.45f),
                                          1.4f,
                                          AudioParameterFloatAttributes().withLabel ("s")));
    layout.add (std::make_unique<Float> (ParameterID { nsr::pid::bassTail, 1 }, "Bass Tail",
                                          NormalisableRange<float> (25.0f, 200.0f, 0.1f), 100.0f,
                                          AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<Float> (ParameterID { nsr::pid::airTail, 1 }, "Air Tail",
                                          NormalisableRange<float> (25.0f, 200.0f, 0.1f), 70.0f,
                                          AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<Float> (ParameterID { nsr::pid::mix, 1 }, "Mix",
                                          percent, 35.0f,
                                          AudioParameterFloatAttributes().withLabel ("%")));
    return layout;
}

NekoSpaceReverbProcessor::NekoSpaceReverbProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NekoSpaceReverbPrototypeState", createLayout())
{
    apvts.state.setProperty ("schemaVersion", 0, nullptr);
    auto raw = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pBypass = raw (nsr::pid::bypass); pSpace = raw (nsr::pid::space);
    pDecay = raw (nsr::pid::decay); pBassTail = raw (nsr::pid::bassTail);
    pAirTail = raw (nsr::pid::airTail); pMix = raw (nsr::pid::mix);
    bypassParameter = apvts.getParameter (nsr::pid::bypass);
}

bool NekoSpaceReverbProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::stereo()) return false;
    const auto input = layouts.getMainInputChannelSet();
    return input == AudioChannelSet::mono() || input == AudioChannelSet::stereo();
}

nsr::ReverbSettings NekoSpaceReverbProcessor::readSettings() const noexcept
{
    return { pSpace->load() * 0.01f, pDecay->load(), pBassTail->load() * 0.01f,
             pAirTail->load() * 0.01f, pMix->load() * 0.01f };
}

void NekoSpaceReverbProcessor::prepareToPlay (double sampleRate, int maximumBlockSize)
{
    preparedSampleRate = sampleRate;
    preparedBlockSize = jmax (1, maximumBlockSize);
    dry.setSize (2, preparedBlockSize, false, true, false);
    wet8.setSize (2, preparedBlockSize, false, true, false);
    wet16.setSize (2, preparedBlockSize, false, true, false);
    lastSettings = readSettings();
    core8.prepare (sampleRate, preparedBlockSize, lastSettings);
    core16.prepare (sampleRate, preparedBlockSize, lastSettings);
    core8.reset(); core16.reset();
    auditionMix = auditionTarget.load (std::memory_order_relaxed);
    tailSeconds.store (lastSettings.decaySeconds
                       * jmax (lastSettings.bassTailRatio, lastSettings.airTailRatio) + 0.5);
}

void NekoSpaceReverbProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int sampleCount = buffer.getNumSamples();
    if (sampleCount == 0) return;
    const int inputChannels = getTotalNumInputChannels();
    if (inputChannels == 1 && buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, sampleCount);

    const auto settings = readSettings();
    if (settings.space != lastSettings.space || settings.decaySeconds != lastSettings.decaySeconds
        || settings.bassTailRatio != lastSettings.bassTailRatio
        || settings.airTailRatio != lastSettings.airTailRatio || settings.mix != lastSettings.mix)
    {
        lastSettings = settings;
        core8.setSettings (settings); core16.setSettings (settings);
        tailSeconds.store (settings.decaySeconds
                           * jmax (settings.bassTailRatio, settings.airTailRatio) + 0.5);
    }

    const float target = auditionTarget.load (std::memory_order_relaxed);
    const float step = static_cast<float> (1.0 / jmax (1.0, preparedSampleRate * 0.05));
    for (int offset = 0; offset < sampleCount; offset += preparedBlockSize)
    {
        const int count = jmin (preparedBlockSize, sampleCount - offset);
        const float* left = buffer.getReadPointer (0, offset);
        const float* right = buffer.getReadPointer (jmin (1, buffer.getNumChannels() - 1), offset);
        dry.copyFrom (0, 0, left, count);
        dry.copyFrom (1, 0, right, count);
        core8.process (left, right, wet8.getWritePointer (0), wet8.getWritePointer (1), count);
        core16.process (left, right, wet16.getWritePointer (0), wet16.getWritePointer (1), count);
        for (int i = 0; i < count; ++i)
        {
            if (auditionMix < target) auditionMix = jmin (target, auditionMix + step);
            else if (auditionMix > target) auditionMix = jmax (target, auditionMix - step);
            for (int channel = 0; channel < jmin (2, buffer.getNumChannels()); ++channel)
            {
                auto* output = buffer.getWritePointer (channel, offset);
                const auto* a = wet8.getReadPointer (channel);
                const auto* b = wet16.getReadPointer (channel);
                output[i] = pBypass->load() > 0.5f ? dry.getSample (channel, i)
                                                   : a[i] + (b[i] - a[i]) * auditionMix;
            }
        }
    }
}

void NekoSpaceReverbProcessor::getStateInformation (MemoryBlock& destination)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, destination);
}

void NekoSpaceReverbProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessorEditor* NekoSpaceReverbProcessor::createEditor()
{
    return new NekoSpaceReverbEditor (*this);
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NekoSpaceReverbProcessor(); }
