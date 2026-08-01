// SPDX-FileCopyrightText: 2026 TextureVoice
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "../dsp/BinauralEngine.h"

namespace nsb::pid
{
// Permanent parameter IDs — see docs/parameter-contract.md. Never rename.
inline constexpr const char* azimuth    = "position.azimuth";
inline constexpr const char* elevation  = "position.elevation";
inline constexpr const char* distance   = "position.distance";
inline constexpr const char* width      = "source.width";
inline constexpr const char* mode       = "source.mode";
inline constexpr const char* nearfield  = "nearfield.amount";
inline constexpr const char* headRadius = "head.radius";
inline constexpr const char* roomAmount = "room.amount";
inline constexpr const char* roomSize   = "room.size";
inline constexpr const char* roomDamping= "room.damping";
inline constexpr const char* earlyLate  = "room.early_late";
inline constexpr const char* hrtfProfile= "hrtf.profile";
inline constexpr const char* quality    = "quality.mode";
inline constexpr const char* outputGain = "output.gain";
inline constexpr const char* bypassRoom = "output.bypass_room";
inline constexpr const char* bypass     = "global.bypass";
}

class NekoSpaceProcessor : public juce::AudioProcessor
{
public:
    NekoSpaceProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    // True when the measured pack loaded for the current sample rate (48 kHz only today).
    bool measuredHrtfAvailable() const noexcept { return engine.measuredAvailable(); }

    // Elevation Lab. The model is design data, not an automatable parameter: it is 24
    // numbers that get frozen into code once tuned, so it lives in the state tree
    // instead of bloating the host's parameter list.
    const nsb::ElevationModel& elevationModel() const noexcept { return elevModel; }
    void setElevationModel (const nsb::ElevationModel& m);   // message thread only

    const juce::String getName() const override { return "NekoSpace Binaural"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return tailSeconds.load(); }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // meters for the UI (audio thread -> UI, relaxed atomics)
    std::atomic<float> meterL { 0.0f }, meterR { 0.0f };
    std::atomic<float> meterGR { 1.0f }; // limiter gain reduction, linear (1 = none)

    // Editor size lives here, NOT in the APVTS ValueTree: the host may call
    // get/setStateInformation from a background thread while the editor resizes on the
    // message thread, and ValueTree is not thread-safe. Serialized as XML attributes.
    std::atomic<int> uiWidth { 1000 }, uiHeight { 640 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Keeps bypassed audio aligned with the latency the plugin reports while active.
    struct BypassDelay
    {
        void prepare (int delaySamples, int maxBlock)
        {
            len = delaySamples;
            for (auto& line : lines) line.prepare (len + maxBlock + 8);
        }
        void reset() { for (auto& line : lines) line.reset(); }
        void process (juce::AudioBuffer<float>& buffer, int n, int numInputCh)
        {
            const int ch = juce::jmin (2, juce::jmax (1, numInputCh));
            for (int c = 0; c < ch; ++c)
            {
                auto* d = buffer.getWritePointer (c);
                for (int i = 0; i < n; ++i)
                {
                    lines[(size_t) c].push (d[i]);
                    d[i] = lines[(size_t) c].read ((float) len);
                }
            }
            if (ch == 1 && buffer.getNumChannels() > 1)
                buffer.copyFrom (1, 0, buffer, 0, 0, n);
        }
        std::array<nsb::FractionalDelay, 2> lines;
        int len = 0;
    };

    nsb::ElevationModel elevModel = nsb::ElevationModel::analyticBDefaults();
    nsb::BinauralEngine engine;
    BypassDelay bypassDelay;
    std::atomic<float> tailSeconds { 0.0f };

    // cached raw-value pointers (atomic loads in processBlock)
    std::atomic<float>* pAz, * pEl, * pDist, * pWidth, * pMode, * pNear, * pHead;
    std::atomic<float>* pRoomAmt, * pRoomSize, * pRoomDamp, * pEarlyLate;
    std::atomic<float>* pQuality, * pOutGain, * pBypassRoom, * pBypass, * pProfile;
    juce::AudioProcessorParameter* bypassParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NekoSpaceProcessor)
};
