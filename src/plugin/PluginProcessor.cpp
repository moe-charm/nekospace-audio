// SPDX-FileCopyrightText: 2026 TextureVoice
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PluginProcessor.h"
#include "PluginEditor.h"

#if NSB_WITH_KU100
 #include "BinaryData.h"
#endif

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

    // An explicit bypass parameter, first in the list. Without one the VST3 wrapper
    // synthesises a hidden bypass parameter, which shifts the host-visible parameter
    // indices and made the first real parameter fail state round-trip under
    // pluginval --repeat --randomise.
    lo.add (std::make_unique<AudioParameterBool> (ParameterID { nsb::pid::bypass, 1 },
            "Bypass", false));
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
    // The choice always lists every profile so the parameter's range never changes with
    // the build configuration (a host project must restore the same index either way).
    // Selecting a profile that is not present falls back to Analytic B in the engine.
    lo.add (std::make_unique<AudioParameterChoice> (ParameterID { nsb::pid::hrtfProfile, 1 },
            "HRTF Profile",
            StringArray { "Analytic A (legacy)", "Analytic B", "KU100 48k (experimental)",
                          "Custom (Elevation Lab)" }, 1));
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
    apvts.state.setProperty ("schemaVersion", nsb::kStateSchemaVersion, nullptr);

    auto raw = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pAz = raw (nsb::pid::azimuth);       pEl = raw (nsb::pid::elevation);
    pDist = raw (nsb::pid::distance);    pWidth = raw (nsb::pid::width);
    pMode = raw (nsb::pid::mode);        pNear = raw (nsb::pid::nearfield);
    pHead = raw (nsb::pid::headRadius);  pRoomAmt = raw (nsb::pid::roomAmount);
    pRoomSize = raw (nsb::pid::roomSize);pRoomDamp = raw (nsb::pid::roomDamping);
    pEarlyLate = raw (nsb::pid::earlyLate);
    pQuality = raw (nsb::pid::quality);  pOutGain = raw (nsb::pid::outputGain);
    pBypassRoom = raw (nsb::pid::bypassRoom);
    pBypass = raw (nsb::pid::bypass);
    pProfile = raw (nsb::pid::hrtfProfile);

#if NSB_WITH_KU100
    engine.setMeasuredPack (BinaryData::ku100_48k_bhrtf, (size_t) BinaryData::ku100_48k_bhrtfSize);
#endif
    bypassParam = apvts.getParameter (nsb::pid::bypass);
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

static void readAnchor (const juce::ValueTree& t, const char* prefix, nsb::ElevationAnchor& a)
{
    const juce::String p (prefix);
    auto get = [&] (const char* suffix, float fallback)
    {
        const auto v = t.getProperty (p + suffix);
        return v.isVoid() ? fallback : (float) v;
    };
    a.notchHz   = get ("NotchHz",   a.notchHz);
    a.notchDb   = get ("NotchDb",   a.notchDb);
    a.notchQ    = get ("NotchQ",    a.notchQ);
    a.peakRatio = get ("PeakRatio", a.peakRatio);
    a.peakDb    = get ("PeakDb",    a.peakDb);
    a.shelfDb   = get ("ShelfDb",   a.shelfDb);
    a.torsoMs   = get ("TorsoMs",   a.torsoMs);
    a.torsoAmt  = get ("TorsoAmt",  a.torsoAmt);
}

void NekoSpaceProcessor::setElevationModel (const nsb::ElevationModel& m)
{
    if (m == elevModel) return;
    elevModel = m;
    engine.rebuildCustom (elevModel);   // message thread; publishes with an atomic swap
}

void NekoSpaceProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare ((float) sampleRate, samplesPerBlock);
    engine.rebuildCustom (elevModel);
    // The renderer runs every voice through a fixed 2 ms base delay (headroom for the
    // near-field per-ear geometry). Report it so FL Studio PDC stays phase-accurate
    // when NekoSpace runs in parallel with dry paths (Contract #17).
    setLatencySamples (engine.latencySamples());
    bypassDelay.prepare (engine.latencySamples(), samplesPerBlock);
    bypassDelay.reset();
}

void NekoSpaceProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0) return;

    if (pBypass->load() > 0.5f)
    {
        // Host bypass: pass through, but keep the reported latency honest by delaying
        // the dry signal by the same amount the active path adds.
        bypassDelay.process (buffer, n, getMainBusNumInputChannels());
        meterL.store (0.0f, std::memory_order_relaxed);
        meterR.store (0.0f, std::memory_order_relaxed);
        meterGR.store (1.0f, std::memory_order_relaxed);
        return;
    }

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
    p.hrtfProfile   = (int) pProfile->load();
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

// ---------------------------------------------------------------- state ----
// Layout and the rules that keep it loadable as the plugin grows: docs/state-format.md.

static const juce::Identifier kExtraTag   { "NEKOSPACE_EXTRA" };
static const juce::Identifier kChoicesTag { "CHOICES" };
static const juce::Identifier kChoiceTag  { "CHOICE" };
static const juce::Identifier kElevTag    { "ELEVATION" };
static const juce::Identifier kAnchorTag  { "ANCHOR" };
static const juce::Identifier kMacrosTag  { "MACROS" };

// Permanent machine keys for choice values. These are state-format identifiers, not UI
// copy: never rename/reorder/reuse a key after release.
static const char* const kChoiceParamIds[] = {
    nsb::pid::mode, nsb::pid::quality, nsb::pid::hrtfProfile
};

static std::vector<std::string> choiceKeysFor (const juce::String& id)
{
    if (id == nsb::pid::mode)        return { "mono_object", "linked_stereo" };
    if (id == nsb::pid::quality)     return { "economy", "standard" };
    if (id == nsb::pid::hrtfProfile) return { "analytic_a", "analytic_b",
                                               "thk_ku100_48k", "custom_elevation" };
    return {};
}

// Frozen aliases for the short-lived schema-2 development format that saved only the
// display name. Do not derive these from current UI copy: that would break after a rename.
static std::vector<std::string> legacyChoiceNamesFor (const juce::String& id)
{
    if (id == nsb::pid::mode)        return { "Mono Object", "Linked Stereo" };
    if (id == nsb::pid::quality)     return { "Economy", "Standard" };
    if (id == nsb::pid::hrtfProfile) return { "Analytic A (legacy)", "Analytic B",
                                               "KU100 48k (experimental)",
                                               "Custom (Elevation Lab)" };
    return {};
}

static juce::ValueTree parameterNode (const juce::ValueTree& state,
                                      const juce::String& id)
{
    return state.getChildWithProperty ("id", id);
}

static void writeAnchorTree (juce::ValueTree& parent, const char* which,
                             const nsb::ElevationAnchor& a)
{
    juce::ValueTree t (kAnchorTag);
    t.setProperty ("which", which, nullptr);
    t.setProperty ("notchHz", a.notchHz, nullptr);
    t.setProperty ("notchDb", a.notchDb, nullptr);
    t.setProperty ("notchQ", a.notchQ, nullptr);
    t.setProperty ("peakRatio", a.peakRatio, nullptr);
    t.setProperty ("peakDb", a.peakDb, nullptr);
    t.setProperty ("shelfDb", a.shelfDb, nullptr);
    t.setProperty ("torsoMs", a.torsoMs, nullptr);
    t.setProperty ("torsoAmt", a.torsoAmt, nullptr);
    parent.appendChild (t, nullptr);
}

static void readAnchorTree (const juce::ValueTree& elev, const char* which,
                            nsb::ElevationAnchor& a)
{
    for (int i = 0; i < elev.getNumChildren(); ++i)
    {
        const auto t = elev.getChild (i);
        if (! t.hasType (kAnchorTag) || t.getProperty ("which").toString() != which)
            continue;
        // unknown or missing fields keep their default: adding a field later must not
        // invalidate an older project
        auto get = [&] (const char* key, float fallback)
        {
            const auto v = t.getProperty (key);
            return v.isVoid() ? fallback : (float) v;
        };
        a.notchHz   = get ("notchHz", a.notchHz);
        a.notchDb   = get ("notchDb", a.notchDb);
        a.notchQ    = get ("notchQ", a.notchQ);
        a.peakRatio = get ("peakRatio", a.peakRatio);
        a.peakDb    = get ("peakDb", a.peakDb);
        a.shelfDb   = get ("shelfDb", a.shelfDb);
        a.torsoMs   = get ("torsoMs", a.torsoMs);
        a.torsoAmt  = get ("torsoAmt", a.torsoAmt);
        return;
    }
}

void NekoSpaceProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();      // mutating the copy is safe; the live tree is not
    state.setProperty ("schemaVersion", nsb::kStateSchemaVersion, nullptr);

    // a previous restore may have left an EXTRA block on the tree; never write two
    while (state.getChildWithName (kExtraTag).isValid())
        state.removeChild (state.getChildWithName (kExtraTag), nullptr);

    juce::ValueTree extra (kExtraTag);
    extra.setProperty ("version", 1, nullptr);

    // Stable keys protect plugin state from display renames/localisation. The name is a
    // readable snapshot and lets early schema-2 development states keep loading.
    juce::ValueTree choices (kChoicesTag);
    for (const char* id : kChoiceParamIds)
        if (auto* c = dynamic_cast<AudioParameterChoice*> (apvts.getParameter (id)))
        {
            const auto keys = choiceKeysFor (id);
            jassert (keys.size() == (size_t) c->choices.size());
            juce::ValueTree ch (kChoiceTag);
            ch.setProperty ("id", id, nullptr);
            ch.setProperty ("key", juce::String (nsb::keyForChoiceIndex (
                                        keys, c->getIndex())), nullptr);
            ch.setProperty ("name", c->choices[c->getIndex()], nullptr);
            choices.appendChild (ch, nullptr);
        }
    extra.appendChild (choices, nullptr);

    juce::ValueTree elev (kElevTag);
    writeAnchorTree (elev, "below", elevModel.below);
    writeAnchorTree (elev, "level", elevModel.level);
    writeAnchorTree (elev, "above", elevModel.above);
    juce::ValueTree macros (kMacrosTag);
    macros.setProperty ("up", elevMacros.up, nullptr);
    macros.setProperty ("down", elevMacros.down, nullptr);
    macros.setProperty ("body", elevMacros.body, nullptr);
    macros.setProperty ("focus", elevMacros.focus, nullptr);
    elev.appendChild (macros, nullptr);
    extra.appendChild (elev, nullptr);

    state.appendChild (extra, nullptr);

    if (auto xml = state.createXml())
    {
        xml->setAttribute ("uiWidth", uiWidth.load());
        xml->setAttribute ("uiHeight", uiHeight.load());
        copyXmlToBinary (*xml, destData);
    }
}

void NekoSpaceProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    uiWidth.store (xml->getIntAttribute ("uiWidth", 1000));
    uiHeight.store (xml->getIntAttribute ("uiHeight", 640));
    auto restored = juce::ValueTree::fromXml (*xml);
    const int version = (int) restored.getProperty ("schemaVersion", 1);

    nsb::ElevationModel m = nsb::ElevationModel::analyticBDefaults();
    nsb::ElevationMacros mac;

    const auto extra = restored.getChildWithName (kExtraTag);
    const int extraVersion = extra.isValid() ? (int) extra.getProperty ("version", 1) : 0;
    if (extra.isValid() && extraVersion >= 1)
    {
        // Resolve choices before replaceState. Unknown keys retain the pre-restore value;
        // they must not guess from an unrecognised newer state.
        const auto choices = extra.getChildWithName (kChoicesTag);
        for (int i = 0; i < choices.getNumChildren(); ++i)
        {
            const auto ch = choices.getChild (i);
            const auto id = ch.getProperty ("id").toString();
            auto* param = dynamic_cast<AudioParameterChoice*> (
                apvts.getParameter (id));
            if (param == nullptr) continue;      // parameter retired in a later version

            const auto keys = choiceKeysFor (id);
            jassert (keys.size() == (size_t) param->choices.size());
            const auto storedKey = ch.getProperty ("key").toString().toStdString();
            int idx = nsb::indexForChoiceKey (keys, storedKey, param->getIndex());
            if (storedKey.empty())
                idx = nsb::indexForLegacyChoiceName (
                    legacyChoiceNamesFor (id),
                    ch.getProperty ("name").toString().toStdString(), idx);

            // APVTS ValueTrees hold denormalised values (the choice index), unlike host
            // automation which uses 0..1. Writing a normalised value here would corrupt
            // the very state this layer is intended to protect.
            if (auto node = parameterNode (restored, id); node.isValid())
                node.setProperty ("value", idx, nullptr);
        }

        const auto elev = extra.getChildWithName (kElevTag);
        if (elev.isValid())
        {
            readAnchorTree (elev, "below", m.below);
            readAnchorTree (elev, "level", m.level);
            readAnchorTree (elev, "above", m.above);
            const auto mt = elev.getChildWithName (kMacrosTag);
            auto get = [&] (const char* key, float fallback)
            {
                const auto v = mt.getProperty (key);
                return v.isVoid() ? fallback : (float) v;
            };
            mac.up    = get ("up", 1.0f);
            mac.down  = get ("down", 1.0f);
            mac.body  = get ("body", 1.0f);
            mac.focus = get ("focus", 1.0f);
        }
    }
    else if (version <= 1)
    {
        // APVTS already restores schema-1 choices by their stored raw index. Elevation
        // values, however, sat loose on the root and need their legacy reader.
        readAnchor (restored, "elevBelow", m.below);
        readAnchor (restored, "elevLevel", m.level);
        readAnchor (restored, "elevAbove", m.above);
        auto macro = [&] (const char* key, float fallback)
        {
            const auto v = restored.getProperty (key);
            return v.isVoid() ? fallback : (float) v;
        };
        mac.up    = macro ("elevUp", 1.0f);
        mac.down  = macro ("elevDown", 1.0f);
        mac.body  = macro ("elevBody", 1.0f);
        mac.focus = macro ("elevFocus", 1.0f);
    }

    // Apply the corrected tree once. This avoids notifying the host parameter-by-parameter
    // during state restoration and ensures APVTS sees the stable-key result immediately.
    apvts.replaceState (restored);

    elevModel = m;
    elevMacros = mac;
    engine.rebuildCustom (elevModel);
}

juce::AudioProcessorEditor* NekoSpaceProcessor::createEditor()
{
    return new NekoSpaceEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NekoSpaceProcessor();
}
