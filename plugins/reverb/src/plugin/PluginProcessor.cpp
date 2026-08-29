// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

namespace
{
const Identifier formalStateType { "NekoSpaceReverbState" };
const Identifier legacyPrototypeStateType { "NekoSpaceReverbPrototypeState" };
constexpr int currentStateSchema = 1;

// JUCE's AudioParameterBool intentionally retains arbitrary host-normalised values even
// though its semantic value is boolean. Its NormalisableRange snaps the APVTS state to
// 0/1, so an editor-open save/restore can leave the raw host value unrestored. Keeping the
// parameter canonical at its boundary makes automation, state and pluginval agree.
class CanonicalBoolParameter final : public RangedAudioParameter
{
public:
    CanonicalBoolParameter (const ParameterID& id, const String& name, bool defaultValue)
        : RangedAudioParameter (id, name, AudioProcessorParameterWithIDAttributes {}),
          value (defaultValue ? 1.0f : 0.0f),
          defaultNormalised (defaultValue ? 1.0f : 0.0f)
    {
    }

    const NormalisableRange<float>& getNormalisableRange() const override { return range; }
    float getValue() const override { return value.load (std::memory_order_relaxed); }
    void setValue (float next) override
    {
        value.store (next >= 0.5f ? 1.0f : 0.0f, std::memory_order_relaxed);
    }
    float getDefaultValue() const override { return defaultNormalised; }
    int getNumSteps() const override { return 2; }
    bool isDiscrete() const override { return true; }
    bool isBoolean() const override { return true; }
    String getText (float normalised, int) const override
    {
        return normalised >= 0.5f ? "On" : "Off";
    }
    float getValueForText (const String& text) const override
    {
        const auto valueText = text.trim().toLowerCase();
        return valueText == "on" || valueText == "yes" || valueText == "true"
                   || valueText.getIntValue() != 0
               ? 1.0f : 0.0f;
    }

private:
    const NormalisableRange<float> range { 0.0f, 1.0f, 1.0f };
    std::atomic<float> value;
    const float defaultNormalised;
};
}

AudioProcessorValueTreeState::ParameterLayout NekoSpaceReverbProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;
    using Float = AudioParameterFloat;
    const auto percent = NormalisableRange<float> (0.0f, 100.0f, 0.1f);
    layout.add (std::make_unique<CanonicalBoolParameter> (
        ParameterID { nsr::pid::bypass, 1 }, "Bypass", false));
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
    layout.add (std::make_unique<CanonicalBoolParameter> (
        ParameterID { nsr::pid::wetMonoInput, 3 }, "Wet Mono Input", false));
    return layout;
}

NekoSpaceReverbProcessor::NekoSpaceReverbProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, formalStateType, createLayout())
{
    apvts.state.setProperty ("schemaVersion", currentStateSchema, nullptr);
    auto raw = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pBypass = raw (nsr::pid::bypass); pSpace = raw (nsr::pid::space);
    pDecay = raw (nsr::pid::decay); pBassTail = raw (nsr::pid::bassTail);
    pAirTail = raw (nsr::pid::airTail); pMix = raw (nsr::pid::mix);
    pDistance = raw (nsr::pid::distance); pDefinition = raw (nsr::pid::definition);
    pPreDelay = raw (nsr::pid::preDelay);
    pWetMonoInput = raw (nsr::pid::wetMonoInput);
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

int NekoSpaceReverbProcessor::getNumPrograms()
{
    return static_cast<int> (nsr::factoryPresets.size());
}

int NekoSpaceReverbProcessor::getCurrentProgram()
{
    const auto matchingPreset = getMatchingFactoryPreset();
    return matchingPreset >= 0 ? matchingPreset : 0;
}

void NekoSpaceReverbProcessor::setCurrentProgram (int index)
{
    applyFactoryPreset (index);
}

const String NekoSpaceReverbProcessor::getProgramName (int index)
{
    if (! isPositiveAndBelow (index, getNumPrograms())) return {};
    return nsr::factoryPresets[static_cast<std::size_t> (index)].name;
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
    settings.wetMonoInput = pWetMonoInput->load();
    return settings;
}

nsr::RoomBodyAuditionMode NekoSpaceReverbProcessor::getAuditionMode() const noexcept
{
    const int value = auditionMode.load (std::memory_order_relaxed);
    if (value == static_cast<int> (nsr::RoomBodyAuditionMode::tailOnly))
        return nsr::RoomBodyAuditionMode::tailOnly;
    if (value == static_cast<int> (nsr::RoomBodyAuditionMode::earlyOnly))
        return nsr::RoomBodyAuditionMode::earlyOnly;
    return nsr::RoomBodyAuditionMode::roomBody;
}

void NekoSpaceReverbProcessor::applyFactoryPreset (int presetIndex)
{
    if (! juce::isPositiveAndBelow (presetIndex,
                                    static_cast<int> (nsr::factoryPresets.size())))
        return;

    const auto& preset = nsr::factoryPresets[static_cast<std::size_t> (presetIndex)];
    presetWriteSequence.fetch_add (1, std::memory_order_acq_rel); // odd: transaction open
    auto setPlain = [this] (const char* id, float value)
    {
        if (auto* parameter = apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };
    setPlain (nsr::pid::bypass, 0.0f);
    setPlain (nsr::pid::space, preset.space);
    setPlain (nsr::pid::distance, preset.distance);
    setPlain (nsr::pid::definition, preset.definition);
    setPlain (nsr::pid::preDelay, preset.preDelayMs);
    setPlain (nsr::pid::decay, preset.decaySeconds);
    setPlain (nsr::pid::bassTail, preset.bassTailPercent);
    setPlain (nsr::pid::airTail, preset.airTailPercent);
    setPlain (nsr::pid::mix, preset.mixPercent);
    setPlain (nsr::pid::wetMonoInput, preset.wetMonoInput ? 1.0f : 0.0f);
    setAuditionMode (nsr::RoomBodyAuditionMode::roomBody);
    presetWriteSequence.fetch_add (1, std::memory_order_release); // even: complete tuple visible
}

int NekoSpaceReverbProcessor::getMatchingFactoryPreset() const noexcept
{
    const auto near = [] (float a, float b) { return std::abs (a - b) < 0.0005f; };
    for (std::size_t i = 0; i < nsr::factoryPresets.size(); ++i)
    {
        const auto& p = nsr::factoryPresets[i];
        if (near (pSpace->load(), p.space) && near (pDistance->load(), p.distance)
            && near (pDefinition->load(), p.definition)
            && near (pPreDelay->load(), p.preDelayMs) && near (pDecay->load(), p.decaySeconds)
            && near (pBassTail->load(), p.bassTailPercent)
            && near (pAirTail->load(), p.airTailPercent) && near (pMix->load(), p.mixPercent)
            && (pWetMonoInput->load() > 0.5f) == p.wetMonoInput
            && pBypass->load() < 0.5f)
            return static_cast<int> (i);
    }
    return -1;
}

void NekoSpaceReverbProcessor::prepareToPlay (double sampleRate, int maximumBlockSize)
{
    preparedBlockSize = jmax (1, maximumBlockSize);
    dry.setSize (2, preparedBlockSize, false, true, false);
    silence.setSize (2, preparedBlockSize, false, true, false);
    silence.clear();
    lastSettings = readSettings();
    core.setAuditionMode (getAuditionMode());
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

    auto settings = lastSettings;
    const auto sequenceBefore = presetWriteSequence.load (std::memory_order_acquire);
    if ((sequenceBefore & 1u) == 0u)
    {
        const auto candidate = readSettings();
        const auto sequenceAfter = presetWriteSequence.load (std::memory_order_acquire);
        if (sequenceBefore == sequenceAfter)
            settings = candidate;
    }
    if (settings.space != lastSettings.space || settings.decaySeconds != lastSettings.decaySeconds
        || settings.bassTailRatio != lastSettings.bassTailRatio
        || settings.airTailRatio != lastSettings.airTailRatio
        || settings.distance != lastSettings.distance
        || settings.definition != lastSettings.definition
        || settings.preDelayMs != lastSettings.preDelayMs || settings.mix != lastSettings.mix
        || settings.wetMonoInput != lastSettings.wetMonoInput)
    {
        lastSettings = settings;
        core.setSettings (settings);
    }

    core.setAuditionMode (getAuditionMode());
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
    {
        auto restored = ValueTree::fromXml (*xml);
        if (! restored.isValid()
            || (restored.getType() != formalStateType
                && restored.getType() != legacyPrototypeStateType))
            return;

        ValueTree migrated { formalStateType };
        migrated.copyPropertiesAndChildrenFrom (restored, nullptr);
        const int sourceSchema = static_cast<int> (migrated.getProperty ("schemaVersion", 0));
        if (sourceSchema <= currentStateSchema)
            migrated.setProperty ("schemaVersion", currentStateSchema, nullptr);
        apvts.replaceState (migrated);
    }
}

AudioProcessorEditor* NekoSpaceReverbProcessor::createEditor()
{
    return new NekoSpaceReverbEditor (*this);
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NekoSpaceReverbProcessor(); }
