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

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    nsb::BinauralEngine engine;
    std::atomic<float> tailSeconds { 0.0f };

    // cached raw-value pointers (atomic loads in processBlock)
    std::atomic<float>* pAz, * pEl, * pDist, * pWidth, * pMode, * pNear, * pHead;
    std::atomic<float>* pRoomAmt, * pRoomSize, * pRoomDamp, * pEarlyLate;
    std::atomic<float>* pQuality, * pOutGain, * pBypassRoom;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NekoSpaceProcessor)
};
