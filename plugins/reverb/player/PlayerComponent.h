// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "../src/plugin/PluginProcessor.h"

class ReverbPlayerComponent final : public juce::AudioAppComponent,
                                    public juce::FileDragAndDropTarget,
                                    private juce::Timer
{
public:
    ReverbPlayerComponent()
    {
        formats.registerBasicFormats();
        open.setButtonText ("Open Audio...");
        play.setButtonText ("Play");
        loop.setButtonText ("Loop");
        device.setButtonText ("Audio Device...");
        loop.setClickingTogglesState (true);
        loop.setToggleState (true, juce::dontSendNotification);
        addAndMakeVisible (open); addAndMakeVisible (play);
        addAndMakeVisible (loop); addAndMakeVisible (device);
        open.onClick = [this] { openFile(); };
        play.onClick = [this] { togglePlayback(); };
        loop.onClick = [this]
        {
            if (reader != nullptr) reader->setLooping (loop.getToggleState());
        };
        device.onClick = [this] { showDeviceSettings(); };

        position.setSliderStyle (juce::Slider::LinearHorizontal);
        position.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        position.setRange (0.0, 1.0, 0.0001);
        position.onDragStart = [this] { scrubbing = true; };
        position.onDragEnd = [this]
        {
            scrubbing = false;
            if (transport.getLengthInSeconds() > 0.0)
                transport.setPosition (position.getValue() * transport.getLengthInSeconds());
        };
        addAndMakeVisible (position);
        status.setText ("Drop a WAV, AIFF or FLAC file here", juce::dontSendNotification);
        status.setColour (juce::Label::textColourId, juce::Colour (0xffaaa19a));
        status.setJustificationType (juce::Justification::centredLeft);
        time.setColour (juce::Label::textColourId, juce::Colour (0xffaaa19a));
        time.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (status); addAndMakeVisible (time);

        editor.reset (processor.createEditorAndMakeActive());
        addAndMakeVisible (editor.get());
        readThread.startThread();
        setSize (juce::jmax (900, editor->getWidth()), editor->getHeight() + transportHeight);
        setAudioChannels (0, 2);
        startTimerHz (25);
    }

    ~ReverbPlayerComponent() override
    {
        shutdownAudio();
        transport.setSource (nullptr);
        readThread.stopThread (2000);
        editor = nullptr;
        processor.editorBeingDeleted (nullptr);
    }

    void prepareToPlay (int blockSize, double sampleRate) override
    {
        transport.prepareToPlay (blockSize, sampleRate);
        processor.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        processor.prepareToPlay (sampleRate, blockSize);
        preparedBlock = juce::jmax (blockSize, 8192);
        scratch.setSize (2, preparedBlock, false, false, true);
    }

    void releaseResources() override
    {
        transport.releaseResources(); processor.releaseResources();
    }

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override
    {
        info.clearActiveBufferRegion();
        if (reader == nullptr) return;
        transport.getNextAudioBlock (info);
        if (info.numSamples > preparedBlock) return; // never resize on the device callback
        for (int channel = 0; channel < 2; ++channel)
            scratch.copyFrom (channel, 0, *info.buffer,
                              juce::jmin (channel, info.buffer->getNumChannels() - 1),
                              info.startSample, info.numSamples);
        juce::AudioBuffer<float> block (scratch.getArrayOfWritePointers(), 2, 0, info.numSamples);
        juce::MidiBuffer midi;
        processor.processBlock (block, midi);
        for (int channel = 0; channel < info.buffer->getNumChannels(); ++channel)
            info.buffer->copyFrom (channel, info.startSample, scratch,
                                   juce::jmin (channel, 1), 0, info.numSamples);
    }

    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff171615)); }
    void resized() override
    {
        auto bounds = getLocalBounds();
        auto bar = bounds.removeFromTop (transportHeight).reduced (12, 8);
        auto row = bar.removeFromTop (28);
        open.setBounds (row.removeFromLeft (125)); row.removeFromLeft (8);
        play.setBounds (row.removeFromLeft (78)); row.removeFromLeft (6);
        loop.setBounds (row.removeFromLeft (68)); row.removeFromLeft (10);
        device.setBounds (row.removeFromRight (140));
        time.setBounds (row.removeFromRight (130));
        status.setBounds (row);
        bar.removeFromTop (4); position.setBounds (bar.removeFromTop (20));
        if (editor != nullptr) editor->setBounds (bounds);
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key.getKeyCode() == juce::KeyPress::spaceKey) { togglePlayback(); return true; }
        return false;
    }
    void loadFromCommandLine (const juce::File& file) { load (file); }
    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        return files.size() == 1 && formats.findFormatForFileExtension (
            juce::File (files[0]).getFileExtension()) != nullptr;
    }
    void filesDropped (const juce::StringArray& files, int, int) override
    {
        if (files.size() == 1) load (juce::File (files[0]));
    }

private:
    static constexpr int transportHeight = 64;
    void openFile()
    {
        chooser = std::make_unique<juce::FileChooser> (
            "Open an audio file", juce::File(), formats.getWildcardForAllFormats());
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& c) { load (c.getResult()); });
    }
    void load (const juce::File& file)
    {
        if (! file.existsAsFile()) return;
        transport.stop(); transport.setSource (nullptr); reader.reset();
        std::unique_ptr<juce::AudioFormatReader> formatReader (formats.createReaderFor (file));
        if (formatReader == nullptr)
        {
            status.setText ("Could not read " + file.getFileName(), juce::dontSendNotification);
            return;
        }
        const double fileRate = formatReader->sampleRate;
        auto source = std::make_unique<juce::AudioFormatReaderSource> (formatReader.release(), true);
        source->setLooping (loop.getToggleState());
        transport.setSource (source.get(), 32768, &readThread, fileRate);
        reader = std::move (source);
        transport.setPosition (0.0);
        status.setText (file.getFileName() + "  |  " + juce::String (fileRate / 1000.0, 1)
                        + " kHz", juce::dontSendNotification);
    }
    void togglePlayback()
    {
        if (reader == nullptr) return;
        if (transport.isPlaying()) transport.stop();
        else
        {
            if (transport.getCurrentPosition() >= transport.getLengthInSeconds() - 0.01)
                transport.setPosition (0.0);
            transport.start();
        }
        play.setButtonText (transport.isPlaying() ? "Stop" : "Play");
    }
    void showDeviceSettings()
    {
        auto panel = std::make_unique<juce::AudioDeviceSelectorComponent> (
            deviceManager, 0, 0, 2, 2, false, false, true, false);
        panel->setSize (460, 300);
        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned (panel.release());
        options.dialogTitle = "Audio Device";
        options.dialogBackgroundColour = juce::Colour (0xff171615);
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.launchAsync();
    }
    void timerCallback() override
    {
        const double length = transport.getLengthInSeconds();
        if (length > 0.0)
        {
            if (! scrubbing)
                position.setValue (transport.getCurrentPosition() / length,
                                   juce::dontSendNotification);
            auto format = [] (double seconds)
            {
                return juce::String ((int) seconds / 60) + ":"
                       + juce::String ((int) seconds % 60).paddedLeft ('0', 2);
            };
            time.setText (format (transport.getCurrentPosition()) + " / " + format (length),
                          juce::dontSendNotification);
        }
        if (! transport.isPlaying() && play.getButtonText() == "Stop") play.setButtonText ("Play");
    }

    NekoSpaceReverbProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    juce::AudioFormatManager formats;
    juce::TimeSliceThread readThread { "reverb player read" };
    std::unique_ptr<juce::AudioFormatReaderSource> reader;
    juce::AudioTransportSource transport;
    juce::AudioBuffer<float> scratch;
    int preparedBlock = 1;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::TextButton open, play, device;
    juce::ToggleButton loop;
    juce::Slider position;
    juce::Label status, time;
    bool scrubbing = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverbPlayerComponent)
};
