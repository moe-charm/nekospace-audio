// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/ReverbCore.h"

namespace nsr::pid
{
inline constexpr auto bypass = "reverb.bypass";
inline constexpr auto space = "reverb.space";
inline constexpr auto decay = "reverb.decay";
inline constexpr auto bassTail = "reverb.bassTail";
inline constexpr auto airTail = "reverb.airTail";
inline constexpr auto mix = "reverb.mix";
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
    double getTailLengthSeconds() const override { return tailSeconds.load(); }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Phase 3.5 developer comparison. It is deliberately not a parameter or saved state.
    void setAuditionNetworkLines (int lines) noexcept
    {
        auditionTarget.store (lines == 8 ? 0.0f : 1.0f, std::memory_order_relaxed);
    }
    int getAuditionNetworkLines() const noexcept
    {
        return auditionTarget.load (std::memory_order_relaxed) < 0.5f ? 8 : 16;
    }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    nsr::ReverbSettings readSettings() const noexcept;

    nsr::ReverbCore8 core8;
    nsr::ReverbCore core16;
    juce::AudioBuffer<float> dry, wet8, wet16;
    nsr::ReverbSettings lastSettings;
    int preparedBlockSize = 1;
    double preparedSampleRate = 48000.0;
    float auditionMix = 1.0f;
    std::atomic<float> auditionTarget { 1.0f };
    std::atomic<double> tailSeconds { 1.9 };

    std::atomic<float>* pBypass = nullptr;
    std::atomic<float>* pSpace = nullptr;
    std::atomic<float>* pDecay = nullptr;
    std::atomic<float>* pBassTail = nullptr;
    std::atomic<float>* pAirTail = nullptr;
    std::atomic<float>* pMix = nullptr;
    juce::AudioProcessorParameter* bypassParameter = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NekoSpaceReverbProcessor)
};
