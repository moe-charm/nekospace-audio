// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// CleanVoice GUI v1.
//
// The point of this window is to let a real recording be judged in seconds, so the two
// things it does well are switching between Original / Clean / Removed WITHOUT losing the
// playback position, and saying plainly what the selection is for.
//
// JUCE lives here and nowhere else. Everything under src/dsp stays framework-free, which
// is what makes a VST3 version later a matter of adding a wrapper rather than untangling
// one.
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <memory>
#include "WaveformView.h"
#include "../src/io/WavFile.h"
#include "../src/dsp/Stft.h"
#include "../src/dsp/NoiseProfile.h"
#include "../src/dsp/Denoiser.h"

namespace cvapp
{
enum class Monitor { original = 1, clean, removed };

class MainComponent : public juce::AudioAppComponent,
                      public juce::FileDragAndDropTarget,
                      private juce::Timer
{
public:
    MainComponent()
    {
        setLookAndFeel (&lnf);
        lnf.setColourScheme (juce::LookAndFeel_V4::getDarkColourScheme());

        addAndMakeVisible (wave);
        wave.onSelectionChanged = [this] { refreshEnablement(); };

        auto button = [this] (juce::TextButton& b, const juce::String& text,
                              std::function<void()> fn)
        {
            b.setButtonText (text);
            b.onClick = std::move (fn);
            addAndMakeVisible (b);
        };
        button (openBtn,  "Open WAV...", [this] { openFile(); });
        button (learnBtn, "Learn Noise + Process", [this] { startProcessing(); });
        button (cancelBtn,"Cancel", [this] { cancelProcessing(); });
        button (playBtn,  "Play", [this] { togglePlay(); });
        button (exportBtn,"Export Clean WAV...", [this] { exportClean(); });
        cancelBtn.setVisible (false);

        for (auto* b : { &origBtn, &cleanBtn, &removedBtn })
        {
            b->setRadioGroupId (7);
            b->setClickingTogglesState (true);
            addAndMakeVisible (b);
        }
        origBtn.setButtonText ("Original");
        cleanBtn.setButtonText ("Clean");
        removedBtn.setButtonText ("Removed Noise");
        origBtn.setToggleState (true, juce::dontSendNotification);
        // Switching source keeps the playhead, which is the entire point: the difference
        // between two renders is only audible if you hear the same moment in each.
        // The waveform follows what you are listening to. Seeing speech in the removed
        // signal is faster than hearing it, and both beat guessing.
        auto pick = [this] (Monitor m)
        {
            monitor = m;
            wave.setAudioKeepSelection (bufferFor (m));
        };
        origBtn.onClick    = [pick] { pick (Monitor::original); };
        cleanBtn.onClick   = [pick] { pick (Monitor::clean); };
        removedBtn.onClick = [pick] { pick (Monitor::removed); };

        auto slider = [this] (juce::Slider& s, juce::Label& l, const juce::String& name,
                              double lo, double hi, double step, double val,
                              const juce::String& suffix)
        {
            s.setSliderStyle (juce::Slider::LinearHorizontal);
            s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 74, 20);
            s.setRange (lo, hi, step);
            s.setValue (val, juce::dontSendNotification);
            s.setTextValueSuffix (suffix);
            addAndMakeVisible (s);
            l.setText (name, juce::dontSendNotification);
            l.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
            l.setColour (juce::Label::textColourId, col::textDim);
            addAndMakeVisible (l);
        };
        slider (reductionS, reductionL, "REDUCTION", 0.0, 24.0, 0.5, 10.0, " dB");
        slider (smoothingS, smoothingL, "SMOOTHING", 0.0, 1.0, 0.01, 0.5, "");
        slider (preserveS,  preserveL,  "PRESERVE BREATH", 0.0, 1.0, 0.01, 0.0, "");
        slider (oversubS,   oversubL,   "OVERSUBTRACTION", 0.5, 3.0, 0.05, 1.0, "x");

        // Preserve Breath and Oversubtraction start hidden: the first listen has to be the
        // unprotected behaviour, or there is no way to know what the protection is for.
        advancedBtn.setButtonText ("Advanced");
        advancedBtn.setClickingTogglesState (true);
        advancedBtn.onClick = [this] { showAdvanced = advancedBtn.getToggleState(); resized(); };
        addAndMakeVisible (advancedBtn);

        status.setJustificationType (juce::Justification::centredLeft);
        status.setColour (juce::Label::textColourId, col::textDim);
        status.setFont (juce::Font (juce::FontOptions (12.0f)));
        addAndMakeVisible (status);

        hint.setJustificationType (juce::Justification::centredLeft);
        hint.setColour (juce::Label::textColourId, col::textDim);
        hint.setFont (juce::Font (juce::FontOptions (11.5f)));
        hint.setText ("Drag over a stretch of room tone with no voice in it. That range is "
                      "what the noise is learned FROM - the whole file gets processed.",
                      juce::dontSendNotification);
        addAndMakeVisible (hint);

        progress.setColour (juce::ProgressBar::backgroundColourId, col::panel);
        addChildComponent (progress);

        refreshEnablement();
        setSize (1040, 620);
        setAudioChannels (0, 2);
        startTimerHz (30);
    }

    ~MainComponent() override
    {
        cancelProcessing();
        shutdownAudio();
        setLookAndFeel (nullptr);
    }

    // ---------------------------------------------------------------- audio ----

    void prepareToPlay (int, double sampleRate) override { deviceRate = sampleRate; }
    void releaseResources() override {}

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override
    {
        info.clearActiveBufferRegion();
        if (! playing.load()) return;

        const auto* src = bufferFor (monitor);
        if (src == nullptr || src->empty()) return;

        const int n = (int) (*src)[0].size();
        const int outCh = info.buffer->getNumChannels();
        const int srcCh = (int) src->size();
        // If the device could not open at the file's rate, step through at the ratio
        // rather than pretending. It is monitoring, and the mismatch is reported on screen.
        const double step = fileRate > 0 && deviceRate > 0 ? fileRate / deviceRate : 1.0;

        double pos = playPos.load();
        for (int i = 0; i < info.numSamples; ++i)
        {
            const int idx = (int) pos;
            if (idx >= n) { playing.store (false); pos = 0; break; }
            for (int c = 0; c < outCh; ++c)
            {
                const auto& chan = (*src)[(size_t) juce::jmin (c, srcCh - 1)];
                info.buffer->setSample (c, info.startSample + i, chan[(size_t) idx]);
            }
            pos += step;
        }
        playPos.store (pos);
    }

    // ---------------------------------------------------------------- ui ----

    void paint (juce::Graphics& g) override { g.fillAll (col::bg); }

    void resized() override
    {
        auto b = getLocalBounds().reduced (14);

        auto top = b.removeFromTop (32);
        openBtn.setBounds (top.removeFromLeft (130));
        top.removeFromLeft (10);
        exportBtn.setBounds (top.removeFromRight (170));
        top.removeFromRight (10);
        status.setBounds (top);

        b.removeFromTop (10);
        hint.setBounds (b.removeFromTop (34));
        b.removeFromTop (6);

        auto bottom = b.removeFromBottom (showAdvanced ? 176 : 116);
        wave.setBounds (b);

        auto row = bottom.removeFromTop (30);
        playBtn.setBounds (row.removeFromLeft (90));
        row.removeFromLeft (16);
        origBtn.setBounds (row.removeFromLeft (110));
        cleanBtn.setBounds (row.removeFromLeft (110));
        removedBtn.setBounds (row.removeFromLeft (140));
        row.removeFromLeft (16);
        advancedBtn.setBounds (row.removeFromRight (110));
        row.removeFromRight (10);
        learnBtn.setBounds (row.removeFromRight (200));
        cancelBtn.setBounds (row.removeFromRight (0).withWidth (0));   // placed below

        bottom.removeFromTop (8);
        auto lay = [&bottom] (juce::Label& l, juce::Slider& s)
        {
            auto r = bottom.removeFromTop (26);
            l.setBounds (r.removeFromLeft (150));
            s.setBounds (r);
        };
        lay (reductionL, reductionS);
        lay (smoothingL, smoothingS);
        if (showAdvanced) { lay (preserveL, preserveS); lay (oversubL, oversubS); }
        preserveL.setVisible (showAdvanced); preserveS.setVisible (showAdvanced);
        oversubL.setVisible (showAdvanced);  oversubS.setVisible (showAdvanced);

        // progress and cancel share the learn button's slot while a job runs
        progress.setBounds (learnBtn.getBounds().withTrimmedRight (90));
        cancelBtn.setBounds (learnBtn.getBounds().removeFromRight (84));
    }

private:
    // ---------------------------------------------------------------- work ----

    // Processing runs here, not on the message thread, so the window keeps repainting and
    // Cancel is answerable.
    class Job : public juce::Thread
    {
    public:
        Job (MainComponent& o) : juce::Thread ("cleanvoice"), owner (o) {}
        void run() override { owner.runJob (*this); }
        MainComponent& owner;
    };

    void startProcessing()
    {
        if (file.numSamples() == 0 || ! wave.hasSelection()) return;
        if (job != nullptr) return;

        params.reductionDb  = (float) reductionS.getValue();
        params.smoothing    = (float) smoothingS.getValue();
        params.preserve     = (float) preserveS.getValue();
        params.overSubtract = (float) oversubS.getValue();

        progressValue = 0.0;
        progress.setVisible (true);
        cancelBtn.setVisible (true);
        learnBtn.setVisible (false);
        job = std::make_unique<Job> (*this);
        job->startThread();
    }

    void cancelProcessing()
    {
        if (job == nullptr) return;
        job->signalThreadShouldExit();
        job->stopThread (4000);
        job.reset();
        progress.setVisible (false);
        cancelBtn.setVisible (false);
        learnBtn.setVisible (true);
    }

    void runJob (juce::Thread& thread)
    {
        const int n = file.numSamples();
        const int fftSize = cv::fftSizeForRate (file.sampleRate);
        cv::Stft stft (fftSize, fftSize / 4);

        cv::NoiseProfile profile;
        const bool learned = profile.learn (stft, file.channels, n,
                                            wave.selectionStart(), wave.selectionEnd());
        if (! learned)
        {
            juce::MessageManager::callAsync ([this, fftSize]
            {
                status.setText ("Selection too short - needs about "
                                  + juce::String (1000.0 * (fftSize * 2) / file.sampleRate, 0)
                                  + " ms of noise", juce::dontSendNotification);
                finishJob (false);
            });
            return;
        }

        auto result = cv::Denoiser::process (
            stft, profile, file.channels, n, params,
            [this, &thread] (float p)
            {
                progressValue = (double) p;
                return ! thread.threadShouldExit();
            });

        if (result.empty() || thread.threadShouldExit()) return;   // cancelled

        std::vector<std::vector<float>> rem ((size_t) file.numChannels(),
                                             std::vector<float> ((size_t) n));
        for (int c = 0; c < file.numChannels(); ++c)
            for (int i = 0; i < n; ++i)
                rem[(size_t) c][(size_t) i] =
                    file.channels[(size_t) c][(size_t) i] - result[(size_t) c][(size_t) i];

        juce::MessageManager::callAsync (
            [this, r = std::move (result), rm = std::move (rem), frames = profile.frames()]() mutable
            {
                clean = std::move (r);
                removed = std::move (rm);
                status.setText (fileName + "  -  processed from " + juce::String (frames)
                                  + " noise frames", juce::dontSendNotification);
                finishJob (true);
            });
    }

    void finishJob (bool ok)
    {
        if (job != nullptr) { job->stopThread (2000); job.reset(); }
        progress.setVisible (false);
        cancelBtn.setVisible (false);
        learnBtn.setVisible (true);
        if (ok)
        {
            cleanBtn.setToggleState (true, juce::sendNotification);
            monitor = Monitor::clean;
        }
        refreshEnablement();
    }

    // ---------------------------------------------------------------- files ----

    void openFile()
    {
        chooser = std::make_unique<juce::FileChooser> ("Open a WAV file", juce::File{}, "*.wav");
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
        {
            loadFile (fc.getResult());
        });
    }

public:
    // Shared by the file chooser, a dropped file and the command line.
    void loadFile (const juce::File& f)
    {
        {
            if (f == juce::File{}) return;

            cv::AudioFile loaded;
            std::string err;
            if (! cv::wav::read (f.getFullPathName().toStdString(), loaded, err))
            {
                status.setText ("Could not read: " + juce::String (err),
                                juce::dontSendNotification);
                return;
            }
            playing.store (false);
            playPos.store (0.0);
            file = std::move (loaded);
            clean.clear(); removed.clear();
            fileName = f.getFileName();
            fileRate = file.sampleRate;
            wave.setAudio (&file.channels, file.sampleRate);
            origBtn.setToggleState (true, juce::sendNotification);
            monitor = Monitor::original;

            juce::String s = fileName + "  -  " + juce::String (file.numChannels()) + " ch, "
                             + juce::String (file.sampleRate, 0) + " Hz, "
                             + juce::String (file.bitsPerSample) + "-bit"
                             + (file.isFloat ? " float" : "") + ", "
                             + juce::String (file.numSamples() / file.sampleRate, 2) + " s";
            if (std::abs (deviceRate - fileRate) > 1.0)
                s += "   [monitoring at " + juce::String (deviceRate, 0)
                       + " Hz - resampled for playback only]";
            status.setText (s, juce::dontSendNotification);
            refreshEnablement();
        }
    }

    // Dropping a take onto the window is how this actually gets used.
    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        return files.size() == 1 && files[0].endsWithIgnoreCase (".wav");
    }
    void filesDropped (const juce::StringArray& files, int, int) override
    {
        if (files.size() == 1) loadFile (juce::File (files[0]));
    }

private:

    void exportClean()
    {
        if (clean.empty()) return;
        chooser = std::make_unique<juce::FileChooser> ("Save cleaned WAV",
                                                       juce::File{}, "*.wav");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode
                                | juce::FileBrowserComponent::canSelectFiles
                                | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f == juce::File{}) return;
            cv::AudioFile out;
            out.sampleRate = file.sampleRate;
            out.channels = clean;
            std::string err;
            const bool ok = cv::wav::write (f.getFullPathName().toStdString(), out, err);
            status.setText (ok ? "Wrote " + f.getFileName()
                               : "Could not write: " + juce::String (err),
                            juce::dontSendNotification);
        });
    }

    // ---------------------------------------------------------------- misc ----

    const std::vector<std::vector<float>>* bufferFor (Monitor m) const
    {
        switch (m)
        {
            case Monitor::clean:   return clean.empty()   ? nullptr : &clean;
            case Monitor::removed: return removed.empty() ? nullptr : &removed;
            default:               return file.numSamples() == 0 ? nullptr : &file.channels;
        }
    }

    void togglePlay()
    {
        if (file.numSamples() == 0) return;
        const bool now = ! playing.load();
        if (now && playPos.load() >= file.numSamples() - 1) playPos.store (0.0);
        playing.store (now);
        playBtn.setButtonText (now ? "Stop" : "Play");
    }

    void refreshEnablement()
    {
        const bool haveFile = file.numSamples() > 0;
        const bool haveClean = ! clean.empty();
        learnBtn.setEnabled (haveFile && wave.hasSelection() && job == nullptr);
        playBtn.setEnabled (haveFile);
        exportBtn.setEnabled (haveClean);
        cleanBtn.setEnabled (haveClean);
        removedBtn.setEnabled (haveClean);
    }

    void timerCallback() override
    {
        wave.setPlayhead (playing.load() ? (int) playPos.load() : -1);
        if (! playing.load() && playBtn.getButtonText() == "Stop")
            playBtn.setButtonText ("Play");
        if (progress.isVisible()) progress.repaint();
    }

    juce::LookAndFeel_V4 lnf;
    WaveformView wave;
    juce::TextButton openBtn, learnBtn, cancelBtn, playBtn, exportBtn, advancedBtn;
    juce::TextButton origBtn, cleanBtn, removedBtn;
    juce::Slider reductionS, smoothingS, preserveS, oversubS;
    juce::Label reductionL, smoothingL, preserveL, oversubL, status, hint;
    double progressValue = 0.0;
    juce::ProgressBar progress { progressValue };
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<Job> job;

    cv::AudioFile file;
    std::vector<std::vector<float>> clean, removed;
    cv::DenoiseParams params;
    juce::String fileName;

    std::atomic<bool> playing { false };
    std::atomic<double> playPos { 0.0 };
    Monitor monitor = Monitor::original;
    double deviceRate = 48000.0, fileRate = 48000.0;
    bool showAdvanced = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
} // namespace cvapp
