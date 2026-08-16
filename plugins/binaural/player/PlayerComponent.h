// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// NekoSpace Binaural Player — a file player wrapped around the real plugin.
//
// The plugin's own Standalone build takes live input, which is the wrong shape for
// auditioning a take or filming a demonstration. This is the other shape: a file goes in,
// the plugin processes it, headphones get the result.
//
// IT HOSTS THE ACTUAL PROCESSOR AND THE ACTUAL EDITOR. No control is reimplemented here.
// Rebuilding the pad, the presets, the Elevation Lab and the help would mean maintaining
// two of everything and watching them drift; worse, a demonstration filmed against a
// rebuilt interface would be showing something nobody can download. What is on screen is
// the plugin, by construction. This component owns only the transport strip above it.
//
// Reading and decoding happen off the audio thread - AudioTransportSource keeps its own
// background reader - so processBlock still sees nothing but audio, exactly as it does in
// a host.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "../src/plugin/PluginProcessor.h"
#include "../src/ui/NekoLookAndFeel.h"

namespace nsbplayer
{
class PlayerComponent : public juce::AudioAppComponent,
                        public juce::FileDragAndDropTarget,
                        private juce::Timer
{
public:
    PlayerComponent()
    {
        setLookAndFeel (&lnf);
        formats.registerBasicFormats();          // WAV, AIFF, FLAC, Ogg

        auto button = [this] (juce::TextButton& b, const juce::String& text,
                              std::function<void()> fn)
        {
            b.setButtonText (text);
            b.onClick = std::move (fn);
            b.setWantsKeyboardFocus (false);
            addAndMakeVisible (b);
        };
        button (openBtn, "Open Audio...", [this] { openFile(); });
        button (playBtn, "Play", [this] { togglePlay(); });
        button (deviceBtn, "Audio Device...", [this] { showDeviceSettings(); });

        loopBtn.setButtonText ("Loop");
        loopBtn.setClickingTogglesState (true);
        loopBtn.setToggleState (true, juce::dontSendNotification);
        loopBtn.setWantsKeyboardFocus (false);
        loopBtn.onClick = [this]
        {
            if (readerSource != nullptr) readerSource->setLooping (loopBtn.getToggleState());
        };
        addAndMakeVisible (loopBtn);

        position.setSliderStyle (juce::Slider::LinearHorizontal);
        position.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        position.setRange (0.0, 1.0, 0.0001);
        position.setWantsKeyboardFocus (false);
        position.onDragStart = [this] { scrubbing = true; };
        position.onDragEnd = [this]
        {
            scrubbing = false;
            if (transport.getLengthInSeconds() > 0.0)
                transport.setPosition (position.getValue() * transport.getLengthInSeconds());
        };
        addAndMakeVisible (position);

        status.setJustificationType (juce::Justification::centredLeft);
        status.setColour (juce::Label::textColourId, nsbui::col::textDim);
        status.setFont (juce::Font (juce::FontOptions (12.0f)));
        status.setText ("Open an audio file, or drop one on the window",
                        juce::dontSendNotification);
        addAndMakeVisible (status);

        time.setJustificationType (juce::Justification::centredRight);
        time.setColour (juce::Label::textColourId, nsbui::col::textDim);
        time.setFont (juce::Font (juce::FontOptions (12.0f)));
        addAndMakeVisible (time);

        // The plugin, and its own editor. Not a copy of it.
        editor.reset (processor.createEditorIfNeeded());
        addAndMakeVisible (editor.get());

        // AudioTransportSource reads ahead on this thread. Without it running, the
        // buffering source waits for data that will never arrive and takes the audio
        // callback down with it - the window stops responding and it looks like a hang in
        // the plugin rather than a missing startThread().
        readThread.startThread();

        setWantsKeyboardFocus (true);
        setSize (juce::jmax (1000, editor->getWidth()), editor->getHeight() + kTransportH);
        setAudioChannels (0, 2);
        startTimerHz (25);
    }

    ~PlayerComponent() override
    {
        shutdownAudio();
        transport.setSource (nullptr);
        readThread.stopThread (2000);
        editor = nullptr;
        processor.editorBeingDeleted (nullptr);
        setLookAndFeel (nullptr);
    }

    // ---------------------------------------------------------------- audio ----

    void prepareToPlay (int blockSize, double sampleRate) override
    {
        transport.prepareToPlay (blockSize, sampleRate);
        processor.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        processor.prepareToPlay (sampleRate, blockSize);
        scratch.setSize (2, blockSize, false, false, true);
    }

    void releaseResources() override
    {
        transport.releaseResources();
        processor.releaseResources();
    }

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override
    {
        info.clearActiveBufferRegion();
        if (readerSource == nullptr) return;

        transport.getNextAudioBlock (info);

        // Hand the plugin a plain stereo block starting at sample 0. Doing it in place on
        // the device buffer would give processBlock a non-zero start offset, which is not
        // what a host presents and not what the processor is written against.
        const int n = info.numSamples;
        if (scratch.getNumSamples() < n) scratch.setSize (2, n, false, false, true);
        for (int c = 0; c < 2; ++c)
        {
            const int src = juce::jmin (c, info.buffer->getNumChannels() - 1);
            scratch.copyFrom (c, 0, *info.buffer, src, info.startSample, n);
        }

        juce::AudioBuffer<float> block (scratch.getArrayOfWritePointers(), 2, 0, n);
        juce::MidiBuffer midi;
        processor.processBlock (block, midi);

        for (int c = 0; c < info.buffer->getNumChannels(); ++c)
            info.buffer->copyFrom (c, info.startSample, scratch, juce::jmin (c, 1), 0, n);
    }

    // ---------------------------------------------------------------- ui ----

    void paint (juce::Graphics& g) override { g.fillAll (nsbui::col::bg); }

    void resized() override
    {
        auto b = getLocalBounds();
        auto bar = b.removeFromTop (kTransportH).reduced (10, 8);

        auto row = bar.removeFromTop (28);
        openBtn.setBounds (row.removeFromLeft (130));
        row.removeFromLeft (8);
        playBtn.setBounds (row.removeFromLeft (86));
        row.removeFromLeft (6);
        loopBtn.setBounds (row.removeFromLeft (70));
        row.removeFromLeft (12);
        deviceBtn.setBounds (row.removeFromRight (140));
        row.removeFromRight (10);
        time.setBounds (row.removeFromRight (150));
        row.removeFromRight (10);
        status.setBounds (row);

        bar.removeFromTop (4);
        position.setBounds (bar.removeFromTop (20));

        if (editor != nullptr) editor->setBounds (b);
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        // Only Space, and only when the plugin's own editor did not want it. The editor is
        // the thing being demonstrated; the transport must not steal its keys.
        if (k.getKeyCode() == juce::KeyPress::spaceKey) { togglePlay(); return true; }
        return false;
    }

    void loadFromCommandLine (const juce::File& f) { load (f); }

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
    static constexpr int kTransportH = 64;

    void openFile()
    {
        chooser = std::make_unique<juce::FileChooser> (
            "Open an audio file", juce::File{}, formats.getWildcardForAllFormats());
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc) { load (fc.getResult()); });
    }

    void load (const juce::File& f)
    {
        if (f == juce::File{}) return;

        // Everything to do with opening and decoding happens here, on the message thread.
        transport.stop();
        transport.setSource (nullptr);
        readerSource.reset();

        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (f));
        if (reader == nullptr)
        {
            status.setText ("Could not read " + f.getFileName(), juce::dontSendNotification);
            return;
        }

        const double fileRate = reader->sampleRate;
        auto src = std::make_unique<juce::AudioFormatReaderSource> (reader.release(), true);
        src->setLooping (loopBtn.getToggleState());
        // The final argument makes the transport resample the file to the device rate, so
        // a 96 kHz take plays at the right speed on a 48 kHz device.
        transport.setSource (src.get(), 32768, &readThread, fileRate);
        readerSource = std::move (src);

        status.setText (f.getFileName() + "   " + juce::String (fileRate / 1000.0, 1)
                          + " kHz   " + (transport.getLengthInSeconds() < 1.0
                                           ? juce::String ("short")
                                           : juce::String (transport.getLengthInSeconds(), 1) + " s"),
                        juce::dontSendNotification);
        transport.setPosition (0.0);
        grabKeyboardFocus();
    }

    void togglePlay()
    {
        if (readerSource == nullptr) return;
        if (transport.isPlaying()) transport.stop();
        else
        {
            if (transport.getCurrentPosition() >= transport.getLengthInSeconds() - 0.01)
                transport.setPosition (0.0);
            transport.start();
        }
        playBtn.setButtonText (transport.isPlaying() ? "Stop" : "Play");
    }

    void showDeviceSettings()
    {
        auto panel = std::make_unique<juce::AudioDeviceSelectorComponent> (
            deviceManager, 0, 0, 2, 2, false, false, true, false);
        panel->setSize (460, 300);
        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned (panel.release());
        o.dialogTitle = "Audio Device";
        o.dialogBackgroundColour = nsbui::col::bg;
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = true;
        o.resizable = false;
        o.launchAsync();
    }

    void timerCallback() override
    {
        const double len = transport.getLengthInSeconds();
        if (len > 0.0)
        {
            if (! scrubbing)
                position.setValue (transport.getCurrentPosition() / len,
                                   juce::dontSendNotification);
            auto fmt = [] (double s)
            {
                return juce::String ((int) s / 60) + ":"
                         + juce::String ((int) s % 60).paddedLeft ('0', 2);
            };
            time.setText (fmt (transport.getCurrentPosition()) + " / " + fmt (len),
                          juce::dontSendNotification);
        }
        if (! transport.isPlaying() && playBtn.getButtonText() == "Stop")
            playBtn.setButtonText ("Play");
    }

    nsbui::NekoLookAndFeel lnf;
    NekoSpaceProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor;

    juce::AudioFormatManager formats;
    juce::TimeSliceThread readThread { "player read" };
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transport;
    juce::AudioBuffer<float> scratch;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::TextButton openBtn, playBtn, deviceBtn;
    juce::ToggleButton loopBtn;
    juce::Slider position;
    juce::Label status, time;
    bool scrubbing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayerComponent)
};
} // namespace nsbplayer
