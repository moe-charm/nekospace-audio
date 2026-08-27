// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/RoomBodyCore.h"
#include "FactoryPresets.h"

namespace nsr::pid
{
inline constexpr auto bypass = "reverb.bypass";
inline constexpr auto space = "reverb.space";
inline constexpr auto decay = "reverb.decay";
inline constexpr auto bassTail = "reverb.bassTail";
inline constexpr auto airTail = "reverb.airTail";
inline constexpr auto mix = "reverb.mix";
inline constexpr auto distance = "reverb.distance";
inline constexpr auto definition = "reverb.definition";
inline constexpr auto preDelay = "reverb.preDelay";
inline constexpr auto wetMonoInput = "reverb.wetMonoInput";
}

class NekoSpaceReverbProcessor final : public juce::AudioProcessor
{
public:
    NekoSpaceReverbProcessor();

    void prepareToPlay (double sampleRate, int maximumBlockSize) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParameter; }

    const juce::String getName() const override { return "NekoSpace Reverb"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Room Body developer isolation. It is deliberately not a parameter or saved state.
    void setAuditionMode (nsr::RoomBodyAuditionMode mode) noexcept
    { auditionMode.store (static_cast<int> (mode), std::memory_order_relaxed); }
    nsr::RoomBodyAuditionMode getAuditionMode() const noexcept;
    void applyFactoryPreset (int presetIndex);
    int getMatchingFactoryPreset() const noexcept;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    nsr::RoomBodySettings readSettings() const noexcept;

    nsr::RoomBodyCore core;
    juce::AudioBuffer<float> dry, silence;
    nsr::detail::LinearSmoother bypassMix;
    nsr::RoomBodySettings lastSettings;
    int preparedBlockSize = 1;
    std::atomic<int> auditionMode {
        static_cast<int> (nsr::RoomBodyAuditionMode::roomBody)
    };
    std::atomic<unsigned int> presetWriteSequence { 0 };

    std::atomic<float>* pBypass = nullptr;
    std::atomic<float>* pSpace = nullptr;
    std::atomic<float>* pDecay = nullptr;
    std::atomic<float>* pBassTail = nullptr;
    std::atomic<float>* pAirTail = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pDistance = nullptr;
    std::atomic<float>* pDefinition = nullptr;
    std::atomic<float>* pPreDelay = nullptr;
    std::atomic<float>* pWetMonoInput = nullptr;
    juce::AudioProcessorParameter* bypassParameter = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NekoSpaceReverbProcessor)
};
