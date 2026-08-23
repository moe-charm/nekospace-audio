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
    // Prototype parameters are appended so the existing host-facing order stays stable.
    layout.add (std::make_unique<Float> (ParameterID { nsr::pid::distance, 2 }, "Distance",
                                          percent, 25.0f,
                                          AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<Float> (ParameterID { nsr::pid::definition, 2 }, "Definition",
                                          percent, 65.0f,
                                          AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<Float> (ParameterID { nsr::pid::preDelay, 2 }, "Pre-delay",
                                          NormalisableRange<float> (0.0f, 120.0f, 0.1f, 0.55f),
                                          12.0f,
                                          AudioParameterFloatAttributes().withLabel ("ms")));
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
    pDistance = raw (nsr::pid::distance); pDefinition = raw (nsr::pid::definition);
    pPreDelay = raw (nsr::pid::preDelay);
    bypassParameter = apvts.getParameter (nsr::pid::bypass);
}

bool NekoSpaceReverbProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::stereo()) return false;
    const auto input = layouts.getMainInputChannelSet();
    return input == AudioChannelSet::mono() || input == AudioChannelSet::stereo();
}

double NekoSpaceReverbProcessor::getTailLengthSeconds() const
{
    const float longestRatio = jmax (1.0f, jmax (pBassTail->load(), pAirTail->load()) * 0.01f);
    return static_cast<double> (pDecay->load() * longestRatio
                                + pPreDelay->load() * 0.001f + 0.5f);
}

nsr::RoomBodySettings NekoSpaceReverbProcessor::readSettings() const noexcept
{
    nsr::RoomBodySettings settings;
    settings.space = pSpace->load() * 0.01f;
    settings.decaySeconds = pDecay->load();
    settings.bassTailRatio = pBassTail->load() * 0.01f;
    settings.airTailRatio = pAirTail->load() * 0.01f;
    settings.distance = pDistance->load() * 0.01f;
    settings.definition = pDefinition->load() * 0.01f;
    settings.preDelayMs = pPreDelay->load();
    settings.mix = pMix->load() * 0.01f;
    return settings;
}

void NekoSpaceReverbProcessor::prepareToPlay (double sampleRate, int maximumBlockSize)
{
    preparedBlockSize = jmax (1, maximumBlockSize);
    dry.setSize (2, preparedBlockSize, false, true, false);
    silence.setSize (2, preparedBlockSize, false, true, false);
    silence.clear();
    lastSettings = readSettings();
    core.setRoomBodyEnabled (roomBodyEnabled.load (std::memory_order_relaxed));
    core.prepare (sampleRate, preparedBlockSize, lastSettings);
    core.reset();
    bypassMix.prepare (static_cast<float> (sampleRate), 0.05f);
    const bool bypassed = pBypass->load() > 0.5f;
    bypassMix.snap (bypassed ? 1.0f : 0.0f);
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
        || settings.airTailRatio != lastSettings.airTailRatio
        || settings.distance != lastSettings.distance
        || settings.definition != lastSettings.definition
        || settings.preDelayMs != lastSettings.preDelayMs || settings.mix != lastSettings.mix)
    {
        lastSettings = settings;
        core.setSettings (settings);
    }

    core.setRoomBodyEnabled (roomBodyEnabled.load (std::memory_order_relaxed));
    const bool bypassRequested = pBypass->load() > 0.5f;
    bypassMix.setTarget (bypassRequested ? 1.0f : 0.0f);
    for (int offset = 0; offset < sampleCount; offset += preparedBlockSize)
    {
        const int count = jmin (preparedBlockSize, sampleCount - offset);
        const float* left = buffer.getReadPointer (0, offset);
        const float* right = buffer.getReadPointer (jmin (1, buffer.getNumChannels() - 1), offset);
        dry.copyFrom (0, 0, left, count);
        dry.copyFrom (1, 0, right, count);
        // Once bypass is requested, stop charging the room immediately. The output still
        // reaches exact dry through the prepared 50 ms fade while the existing tail drains
        // naturally from silence. Un-bypass resumes excitation immediately and fades it in.
        const auto* coreLeft = bypassRequested ? silence.getReadPointer (0)
                                               : dry.getReadPointer (0);
        const auto* coreRight = bypassRequested ? silence.getReadPointer (1)
                                                : dry.getReadPointer (1);
        core.process (coreLeft, coreRight,
                      buffer.getWritePointer (0, offset), buffer.getWritePointer (1, offset), count);
        float lastBypassAmount = bypassRequested ? 1.0f : 0.0f;
        auto* outputLeft = buffer.getWritePointer (0, offset);
        auto* outputRight = buffer.getWritePointer (1, offset);
        for (int i = 0; i < count; ++i)
        {
            lastBypassAmount = bypassMix.next();
            if (lastBypassAmount == 0.0f) continue;
            const float dryLeft = dry.getSample (0, i);
            const float dryRight = dry.getSample (1, i);
            if (lastBypassAmount == 1.0f)
            {
                outputLeft[i] = dryLeft;
                outputRight[i] = dryRight;
            }
            else
            {
                outputLeft[i] += (dryLeft - outputLeft[i]) * lastBypassAmount;
                outputRight[i] += (dryRight - outputRight[i]) * lastBypassAmount;
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
