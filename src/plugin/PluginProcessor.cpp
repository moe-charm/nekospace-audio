// SPDX-FileCopyrightText: 2026 TextureVoice
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

static NormalisableRange<float> logDistanceRange()
{
    NormalisableRange<float> r (nsb::kMinDistance, nsb::kMaxDistance);
    r.setSkewForCentre (1.0f); // 1 m at the knob center
    return r;
}

AudioProcessorValueTreeState::ParameterLayout NekoSpaceProcessor::createLayout()
{
    using P = AudioParameterFloat;
    AudioProcessorValueTreeState::ParameterLayout lo;

    auto pct = NormalisableRange<float> (0.0f, 100.0f, 0.1f);

    lo.add (std::make_unique<P> (ParameterID { nsb::pid::azimuth, 1 }, "Azimuth",
            NormalisableRange<float> (-180.0f, 180.0f, 0.1f), 0.0f,
            AudioParameterFloatAttributes().withLabel ("deg")));
    lo.add (std::make_unique<P> (ParameterID { nsb::pid::elevation, 1 }, "Elevation",
            NormalisableRange<float> (-90.0f, 90.0f, 0.1f), 0.0f,
            AudioParameterFloatAttributes().withLabel ("deg")));
    // The log distance range has no step interval, so without an explicit formatter both
    // the GUI text box and the host's automation readout show 7 decimal places.
    lo.add (std::make_unique<P> (ParameterID { nsb::pid::distance, 1 }, "Distance",
            logDistanceRange(), 1.0f,
            AudioParameterFloatAttributes()
                .withStringFromValueFunction ([] (float v, int)
                {
                    return v < 1.0f ? String (v * 100.0f, 1) + " cm"
                                    : String (v, 2) + " m";
                })
                .withValueFromStringFunction ([] (const String& s)
                {
                    const float v = s.getFloatValue();
                    return s.containsIgnoreCase ("cm") ? v * 0.01f : v;
                })));
    lo.add (std::make_unique<P> (ParameterID { nsb::pid::width, 1 }, "Width",
            NormalisableRange<float> (0.0f, 180.0f, 0.5f), 60.0f,
            AudioParameterFloatAttributes().withLabel ("deg")));
    lo.add (std::make_unique<AudioParameterChoice> (ParameterID { nsb::pid::mode, 1 },
            "Source Mode", StringArray { "Mono Object", "Linked Stereo" }, 0));
    lo.add (std::make_unique<P> (ParameterID { nsb::pid::nearfield, 1 }, "Near Field", pct, 75.0f,
            AudioParameterFloatAttributes().withLabel ("%")));
    lo.add (std::make_unique<P> (ParameterID { nsb::pid::headRadius, 1 }, "Head Size",
            NormalisableRange<float> (0.075f, 0.100f, 0.0005f), 0.0875f,
            AudioParameterFloatAttributes()
                .withStringFromValueFunction ([] (float v, int)
                                              { return String (v * 100.0f, 2) + " cm"; })
                .withValueFromStringFunction ([] (const String& s)
                                              { return s.getFloatValue() * 0.01f; })));
    lo.add (std::make_unique<P> (ParameterID { nsb::pid::roomAmount, 1 }, "Room Amount", pct, 15.0f,
            AudioParameterFloatAttributes().withLabel ("%")));
    lo.add (std::make_unique<P> (ParameterID { nsb::pid::roomSize, 1 }, "Room Size", pct, 35.0f,
            AudioParameterFloatAttributes().withLabel ("%")));
    lo.add (std::make_unique<P> (ParameterID { nsb::pid::roomDamping, 1 }, "Damping", pct, 50.0f,
            AudioParameterFloatAttributes().withLabel ("%")));
    lo.add (std::make_unique<P> (ParameterID { nsb::pid::earlyLate, 1 }, "Early/Late", pct, 35.0f,
            AudioParameterFloatAttributes().withLabel ("%")));
    lo.add (std::make_unique<AudioParameterChoice> (ParameterID { nsb::pid::hrtfProfile, 1 },
            "HRTF Profile", StringArray { "Analytic A" }, 0));
    lo.add (std::make_unique<AudioParameterChoice> (ParameterID { nsb::pid::quality, 1 },
            "Quality", StringArray { "Economy", "Standard" }, 1));
    lo.add (std::make_unique<P> (ParameterID { nsb::pid::outputGain, 1 }, "Output Gain",
            NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
            AudioParameterFloatAttributes().withLabel ("dB")));
    lo.add (std::make_unique<AudioParameterBool> (ParameterID { nsb::pid::bypassRoom, 1 },
            "Room Bypass", false));
    return lo;
}

NekoSpaceProcessor::NekoSpaceProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NekoSpaceState", createLayout())
{
    apvts.state.setProperty ("schemaVersion", 1, nullptr);

    auto raw = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pAz = raw (nsb::pid::azimuth);       pEl = raw (nsb::pid::elevation);
    pDist = raw (nsb::pid::distance);    pWidth = raw (nsb::pid::width);
    pMode = raw (nsb::pid::mode);        pNear = raw (nsb::pid::nearfield);
    pHead = raw (nsb::pid::headRadius);  pRoomAmt = raw (nsb::pid::roomAmount);
    pRoomSize = raw (nsb::pid::roomSize);pRoomDamp = raw (nsb::pid::roomDamping);
    pEarlyLate = raw (nsb::pid::earlyLate);
    pQuality = raw (nsb::pid::quality);  pOutGain = raw (nsb::pid::outputGain);
    pBypassRoom = raw (nsb::pid::bypassRoom);
}

bool NekoSpaceProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Contract #3: stereo out always; stereo in canonical (mono in tolerated for
    // non-FL hosts, handled as dual-mono).
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == AudioChannelSet::stereo() || in == AudioChannelSet::mono();
}

void NekoSpaceProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare ((float) sampleRate, samplesPerBlock);
    // The renderer runs every voice through a fixed 2 ms base delay (headroom for the
    // near-field per-ear geometry). Report it so FL Studio PDC stays phase-accurate
    // when NekoSpace runs in parallel with dry paths (Contract #17).
    setLatencySamples (engine.latencySamples());
}

void NekoSpaceProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0) return;

    nsb::EngineParams p;
    p.azimuthDeg    = pAz->load();
    p.elevationDeg  = pEl->load();
    p.distanceM     = pDist->load();
    p.widthDeg      = pWidth->load();
    p.sourceMode    = (int) pMode->load();
    p.nearField     = pNear->load() * 0.01f;
    p.headRadiusM   = pHead->load();
    p.roomAmount    = pRoomAmt->load() * 0.01f;
    p.roomSize      = pRoomSize->load() * 0.01f;
    p.roomDamping   = pRoomDamp->load() * 0.01f;
    p.roomEarlyLate = pEarlyLate->load() * 0.01f;
    p.qualityMode   = (int) pQuality->load();
    p.outputGainDb  = pOutGain->load();
    p.bypassRoom    = pBypassRoom->load() > 0.5f;
    engine.setParams (p);

    // Channel 1 of the buffer is only valid *input* when the input bus is stereo;
    // on a mono-in/stereo-out layout it is output scratch — treat input as dual-mono.
    const int numInputCh = getMainBusNumInputChannels();
    const float* inL = buffer.getReadPointer (0);
    const float* inR = (numInputCh > 1 && buffer.getNumChannels() > 1)
                           ? buffer.getReadPointer (1) : inL;
    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : outL;

    engine.process (inL, inR, outL, outR, n);

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);

    meterL.store (engine.lastPeakL(), std::memory_order_relaxed);
    meterR.store (engine.lastPeakR(), std::memory_order_relaxed);
    meterGR.store (engine.lastGainReduction(), std::memory_order_relaxed);
    tailSeconds.store (engine.tailSeconds(), std::memory_order_relaxed);
}

void NekoSpaceProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("schemaVersion", 1, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void NekoSpaceProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* NekoSpaceProcessor::createEditor()
{
    return new NekoSpaceEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NekoSpaceProcessor();
}
