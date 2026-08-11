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
#include <cstdint>
#include <memory>
#include "WaveformView.h"
#include "NoiseFloorView.h"
#include "SpectrogramView.h"
#include "../src/io/WavFile.h"
#include "../src/dsp/Stft.h"
#include "../src/dsp/NoiseProfile.h"
#include "../src/dsp/Denoiser.h"

namespace cvapp
{
enum class Monitor { original = 1, clean, removed };
using ChannelBuffer = std::vector<std::vector<float>>;

// The audio callback owns one immutable snapshot for the whole block. Replacing a file or
// a completed render on the message thread can therefore never invalidate memory that the
// device thread is still reading.
struct PlaybackState
{
    std::shared_ptr<const ChannelBuffer> channels;
    double sampleRate = 48000.0;
};

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
        // Clicking the waveform moves the playhead, so you can audition around a marked
        // region without losing it.
        wave.onPlayheadMoved = [this] (int s) { playPos.store ((double) s); };
        wave.onViewChanged = [this] { syncSpectrogramView(); };

        // Nothing here takes keyboard focus. If a button had it, Space would press that
        // button instead of starting playback, and which button depends on what you last
        // clicked - the least predictable behaviour available.
        auto button = [this] (juce::TextButton& b, const juce::String& text,
                              const juce::String& shortcut, std::function<void()> fn)
        {
            b.setButtonText (text);
            b.setTooltip (shortcut.isEmpty() ? text : text + "   [" + shortcut + "]");
            b.setWantsKeyboardFocus (false);
            b.onClick = std::move (fn);
            addAndMakeVisible (b);
        };
        button (openBtn,  "Open WAV...", "Ctrl+O", [this] { openFile(); });
        button (learnBtn, "Learn Noise + Process", "Enter", [this] { startProcessing(); });
        button (cancelBtn,"Cancel", "Esc", [this] { cancelProcessing(); });
        button (playBtn,  "Play All", "Shift+Space", [this] { togglePlay (false); });
        button (playSelBtn, "Play Selection", "Space", [this] { togglePlay (true); });
        button (fitBtn,   "Fit", "F", [this] { wave.zoomToFit(); updateViewLabel(); });
        button (zoomSelBtn,"Zoom to Selection", "Z",
                [this] { wave.zoomToSelection(); updateViewLabel(); });
        button (exportBtn,"Export Clean WAV...", "Ctrl+S", [this] { exportClean(); });
        floorBtn.setButtonText ("Noise Floor");
        floorBtn.setClickingTogglesState (true);
        floorBtn.setWantsKeyboardFocus (false);
        floorBtn.setTooltip ("Show the learned noise floor   [N]");
        floorBtn.onClick = [this] { showFloor = floorBtn.getToggleState(); resized(); };
        addAndMakeVisible (floorBtn);
        addChildComponent (floorView);

        specBtn.setButtonText ("Spectrogram");
        specBtn.setClickingTogglesState (true);
        specBtn.setWantsKeyboardFocus (false);
        specBtn.setTooltip ("Show the spectrogram of what you are monitoring   [G]");
        specBtn.onClick = [this]
        {
            showSpec = specBtn.getToggleState();
            resized();
            syncSpectrogramView();
        };
        addAndMakeVisible (specBtn);
        addChildComponent (spectro);
        cancelBtn.setVisible (false);

        for (auto* b : { &origBtn, &cleanBtn, &removedBtn })
        {
            b->setRadioGroupId (7);
            b->setClickingTogglesState (true);
            b->setWantsKeyboardFocus (false);
            addAndMakeVisible (b);
        }
        origBtn.setButtonText ("Original");
        cleanBtn.setButtonText ("Clean");
        removedBtn.setButtonText ("Removed Noise");
        origBtn.setTooltip ("Original   [1]");
        cleanBtn.setTooltip ("Clean   [2]");
        removedBtn.setTooltip ("Removed Noise   [3]");
        origBtn.setToggleState (true, juce::dontSendNotification);
        // Switching source keeps the playhead, which is the entire point: the difference
        // between two renders is only audible if you hear the same moment in each.
        // The waveform follows what you are listening to. Seeing speech in the removed
        // signal is faster than hearing it, and both beat guessing.
        origBtn.onClick    = [this] { applyMonitor (Monitor::original); };
        cleanBtn.onClick   = [this] { applyMonitor (Monitor::clean); };
        removedBtn.onClick = [this] { applyMonitor (Monitor::removed); };

        auto slider = [this] (juce::Slider& s, juce::Label& l, const juce::String& name,
                              double lo, double hi, double step, double val,
                              const juce::String& suffix)
        {
            s.setSliderStyle (juce::Slider::LinearHorizontal);
            s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 74, 20);
            s.setRange (lo, hi, step);
            s.setValue (val, juce::dontSendNotification);
            s.setTextValueSuffix (suffix);
            s.setWantsKeyboardFocus (false);
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
        // Monitoring only, never written to the exported file. A studio noise floor sits
        // near -77 dBFS and what gets removed from it is quieter still, so at unity the
        // Removed bus is inaudible and the tool looks broken when it is working.
        slider (monitorS, monitorL, "MONITOR GAIN", 0.0, 48.0, 1.0, 0.0, " dB");
        monitorS.setTooltip ("Playback only - does not affect the exported file");

        // Preserve Breath and Oversubtraction start hidden: the first listen has to be the
        // unprotected behaviour, or there is no way to know what the protection is for.
        advancedBtn.setButtonText ("Advanced");
        advancedBtn.setClickingTogglesState (true);
        advancedBtn.setWantsKeyboardFocus (false);
        advancedBtn.onClick = [this] { showAdvanced = advancedBtn.getToggleState(); resized(); };
        addAndMakeVisible (advancedBtn);

        status.setJustificationType (juce::Justification::centredLeft);
        status.setColour (juce::Label::textColourId, col::textDim);
        status.setFont (juce::Font (juce::FontOptions (12.0f)));
        addAndMakeVisible (status);

        viewLabel.setJustificationType (juce::Justification::centredLeft);
        viewLabel.setColour (juce::Label::textColourId, col::textDim);
        viewLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
        addAndMakeVisible (viewLabel);

        hint.setJustificationType (juce::Justification::centredLeft);
        hint.setColour (juce::Label::textColourId, col::textDim);
        hint.setFont (juce::Font (juce::FontOptions (11.5f)));
        hint.setText ("Drag over a stretch of room tone with no voice in it - that range is "
                      "what the noise is learned FROM, and the whole file gets processed. "
                      "Space loops it: if you can hear ANY voice or breath in there, move "
                      "it. Once learned, the profile is kept - clear the selection and "
                      "keep re-processing at different Reduction settings.",
                      juce::dontSendNotification);
        addAndMakeVisible (hint);

        // A shortcut nobody knows about may as well not exist, and this is a window you
        // work in with headphones on rather than one you read a manual for.
        keysLabel.setJustificationType (juce::Justification::centredLeft);
        keysLabel.setColour (juce::Label::textColourId, col::textDim.withAlpha (0.85f));
        keysLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
        keysLabel.setText ("Space play (selection if there is one)   Shift+Space play all   "
                           "1/2/3 Original/Clean/Removed   Enter process   G spectrogram   "
                           "N noise floor   double-click clears selection   F fit   "
                           "Z zoom to selection   +/- zoom   arrows scroll   wheel zoom, "
                           "shift-wheel scroll, right-drag pan",
                           juce::dontSendNotification);
        addAndMakeVisible (keysLabel);

        progress.setColour (juce::ProgressBar::backgroundColourId, col::panel);
        addChildComponent (progress);

        refreshEnablement();
        setWantsKeyboardFocus (true);
        setSize (1040, 620);
        setAudioChannels (0, 2);
        startTimerHz (30);
    }

    ~MainComponent() override
    {
        if (job != nullptr)
        {
            activeJobId = 0;
            job->signalThreadShouldExit();
            job->waitForThreadToExit (-1);
            job.reset();
        }
        shutdownAudio();
        setLookAndFeel (nullptr);
    }

    // ---------------------------------------------------------------- audio ----

    void prepareToPlay (int, double sampleRate) override { deviceRate.store (sampleRate); }
    void releaseResources() override {}

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override
    {
        info.clearActiveBufferRegion();
        if (! playing.load()) return;

        const auto state = std::atomic_load_explicit (&playbackState,
                                                       std::memory_order_acquire);
        if (state == nullptr || state->channels == nullptr || state->channels->empty()) return;
        const auto& src = *state->channels;

        const int n = (int) src[0].size();
        const int outCh = info.buffer->getNumChannels();
        const int srcCh = (int) src.size();
        // When looping a selection the playhead wraps inside it instead of running on,
        // which is what makes "is there voice in here?" answerable in a few seconds.
        const bool loop = loopSelection.load();
        const int loopA = juce::jlimit (0, n, loopStart.load());
        const int loopB = juce::jlimit (loopA + 1, n, loopEnd.load());
        // If the device could not open at the file's rate, step through at the ratio
        // rather than pretending. It is monitoring, and the mismatch is reported on screen.
        const double currentDeviceRate = deviceRate.load();
        const double step = state->sampleRate > 0 && currentDeviceRate > 0
                              ? state->sampleRate / currentDeviceRate : 1.0;

        double pos = playPos.load();
        for (int i = 0; i < info.numSamples; ++i)
        {
            int idx = (int) pos;
            if (loop && idx >= loopB) { pos = (double) loopA; idx = loopA; }
            if (idx >= n) { playing.store (false); pos = 0; break; }
            const int next = juce::jmin (idx + 1, n - 1);
            const float frac = (float) (pos - std::floor (pos));
            const float mg = monitorGain.load();
            for (int c = 0; c < outCh; ++c)
            {
                const auto& chan = src[(size_t) juce::jmin (c, srcCh - 1)];
                const float sample = chan[(size_t) idx]
                                   + frac * (chan[(size_t) next] - chan[(size_t) idx]);
                info.buffer->setSample (c, info.startSample + i, sample * mg);
            }
            pos += step;
        }
        playPos.store (pos);
    }

    // ---------------------------------------------------------------- keys ----

    // The point of these is the 1/2/3 row. Judging a denoiser means hearing the same
    // moment as Original, Clean and Removed one after another, and hunting for a button
    // with the mouse between each one is long enough to lose what you were comparing.
    bool keyPressed (const juce::KeyPress& k) override
    {
        const auto code = k.getKeyCode();
        const bool ctrl = k.getModifiers().isCommandDown();
        const bool shift = k.getModifiers().isShiftDown();

        // Editor convention: Space plays the selection when there is one. Shift+Space is
        // the way out of it, for comparing renders across the whole take.
        if (code == juce::KeyPress::spaceKey)
        { togglePlay (! shift && wave.hasSelection()); return true; }
        if (code == juce::KeyPress::returnKey)  { if (learnBtn.isEnabled()) startProcessing(); return true; }
        if (code == juce::KeyPress::escapeKey)  { cancelProcessing(); return true; }

        if (ctrl && (code == 'O' || code == 'o')) { openFile(); return true; }
        if (ctrl && (code == 'S' || code == 's')) { exportClean(); return true; }

        // monitor switching - the shortcuts that actually matter
        if (code == '1') { selectMonitor (Monitor::original); return true; }
        if (code == '2') { selectMonitor (Monitor::clean);    return true; }
        if (code == '3') { selectMonitor (Monitor::removed);  return true; }

        if (code == 'G' || code == 'g')
        { specBtn.setToggleState (! specBtn.getToggleState(), juce::sendNotification); return true; }
        if (code == 'N' || code == 'n')
        { floorBtn.setToggleState (! floorBtn.getToggleState(), juce::sendNotification); return true; }
        if (code == 'F' || code == 'f') { wave.zoomToFit(); updateViewLabel(); return true; }
        if (code == 'Z' || code == 'z') { wave.zoomToSelection(); updateViewLabel(); return true; }

        if (code == juce::KeyPress::upKey || code == '+' || code == '=')
        { wave.zoomBy (1.0 / 1.4); updateViewLabel(); return true; }
        if (code == juce::KeyPress::downKey || code == '-')
        { wave.zoomBy (1.4); updateViewLabel(); return true; }

        if (code == juce::KeyPress::leftKey)  { wave.scrollBy (-0.25); updateViewLabel(); return true; }
        if (code == juce::KeyPress::rightKey) { wave.scrollBy ( 0.25); updateViewLabel(); return true; }
        if (code == juce::KeyPress::homeKey)
        {
            playPos.store (wave.hasSelection() ? (double) wave.selectionStart() : 0.0);
            return true;
        }
        return false;
    }

    // Focus has to come back to this component after any dialog or click, or the keys stop
    // working and it looks like they were never there.
    void mouseDown (const juce::MouseEvent&) override { grabKeyboardFocus(); }
    void parentHierarchyChanged() override { grabKeyboardFocus(); }

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
        b.removeFromTop (4);

        auto bottom = b.removeFromBottom (showAdvanced ? 258 : 198);
        keysLabel.setBounds (bottom.removeFromBottom (18));
        bottom.removeFromBottom (4);

        // zoom row sits directly under the waveform, where the thing it controls is
        auto zoomRow = bottom.removeFromTop (26);
        fitBtn.setBounds (zoomRow.removeFromLeft (60));
        zoomRow.removeFromLeft (6);
        zoomSelBtn.setBounds (zoomRow.removeFromLeft (150));
        zoomRow.removeFromLeft (12);
        floorBtn.setBounds (zoomRow.removeFromRight (110));
        zoomRow.removeFromRight (6);
        specBtn.setBounds (zoomRow.removeFromRight (120));
        zoomRow.removeFromRight (12);
        viewLabel.setBounds (zoomRow);
        bottom.removeFromTop (6);

        floorView.setVisible (showFloor);
        if (showFloor)
        {
            floorView.setBounds (b.removeFromBottom (juce::jmax (110, b.getHeight() / 3)));
            b.removeFromBottom (8);
        }
        spectro.setVisible (showSpec);
        if (showSpec)
        {
            spectro.setBounds (b.removeFromBottom (juce::jmax (150, b.getHeight() / 2)));
            b.removeFromBottom (8);
        }
        wave.setBounds (b);

        auto row = bottom.removeFromTop (30);
        playBtn.setBounds (row.removeFromLeft (80));
        row.removeFromLeft (6);
        playSelBtn.setBounds (row.removeFromLeft (130));
        row.removeFromLeft (16);
        origBtn.setBounds (row.removeFromLeft (100));
        cleanBtn.setBounds (row.removeFromLeft (100));
        removedBtn.setBounds (row.removeFromLeft (130));
        row.removeFromLeft (12);
        advancedBtn.setBounds (row.removeFromRight (100));
        row.removeFromRight (8);
        learnBtn.setBounds (row.removeFromRight (190));

        bottom.removeFromTop (8);
        auto lay = [&bottom] (juce::Label& l, juce::Slider& s)
        {
            auto r = bottom.removeFromTop (26);
            l.setBounds (r.removeFromLeft (150));
            s.setBounds (r);
        };
        lay (reductionL, reductionS);
        lay (smoothingL, smoothingS);
        lay (monitorL, monitorS);
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
        Job (MainComponent& o, std::shared_ptr<const cv::AudioFile> source,
             cv::NoiseProfile startingProfile, cv::DenoiseParams settings,
             bool shouldRelearn, std::pair<int, int> selection, uint64_t serial)
            : juce::Thread ("cleanvoice"), owner (o), safeOwner (&o), input (std::move (source)),
              workingProfile (std::move (startingProfile)), parameters (settings),
              relearn (shouldRelearn), selectedSamples (selection), id (serial) {}
        void run() override { owner.runJob (*this); }
        MainComponent& owner;
        juce::Component::SafePointer<MainComponent> safeOwner;
        std::shared_ptr<const cv::AudioFile> input;
        cv::NoiseProfile workingProfile;
        cv::DenoiseParams parameters;
        bool relearn = false;
        std::pair<int, int> selectedSamples { 0, 0 };
        uint64_t id = 0;
    };

    void startProcessing()
    {
        if (file == nullptr || file->numSamples() == 0 || job != nullptr) return;
        // Either learn from the current selection, or re-use the profile already learned.
        // Keeping the profile alive after the selection is cleared is what makes tuning
        // Reduction cheap: that is the loop this tool is actually used in, and forcing a
        // re-select for every attempt would tax the most common action.
        if (! wave.hasSelection() && ! haveProfile) return;
        if (wave.hasSelection() && wave.selectionSeconds() > 30.0)
        {
            status.setText ("Noise learn range is too long - select 1-3 seconds (30 s max)",
                            juce::dontSendNotification);
            return;
        }

        params.reductionDb  = (float) reductionS.getValue();
        params.smoothing    = (float) smoothingS.getValue();
        params.preserve     = (float) preserveS.getValue();
        params.overSubtract = (float) oversubS.getValue();

        learnFromSelection = wave.hasSelection();
        selForJob = { wave.selectionStart(), wave.selectionEnd() };

        progressValue = 0.0;
        progressAtomic.store (0.0);
        progress.setVisible (true);
        cancelBtn.setVisible (true);
        learnBtn.setVisible (false);
        activeJobId = ++nextJobId;
        job = std::make_unique<Job> (*this, file, profile, params, learnFromSelection,
                                     selForJob, activeJobId);
        job->startThread();
        refreshEnablement();
    }

    void cancelProcessing()
    {
        if (job == nullptr) return;
        activeJobId = 0;
        job->signalThreadShouldExit();
        job->waitForThreadToExit (-1);
        job.reset();
        progress.setVisible (false);
        cancelBtn.setVisible (false);
        learnBtn.setVisible (true);
        refreshEnablement();
    }

    void runJob (Job& thread)
    {
        const auto input = thread.input;
        if (input == nullptr) return;
        const int n = input->numSamples();
        const int fftSize = cv::fftSizeForRate (input->sampleRate);
        cv::Stft stft (fftSize, fftSize / 4);

        const bool learned = ! thread.relearn
                             || thread.workingProfile.learn (
                                 stft, input->channels, n,
                                 thread.selectedSamples.first, thread.selectedSamples.second,
                                 [&thread] (float) { return ! thread.threadShouldExit(); });
        if (! learned)
        {
            if (thread.threadShouldExit()) return;
            const auto safe = thread.safeOwner;
            const auto id = thread.id;
            const double rate = input->sampleRate;
            juce::MessageManager::callAsync ([safe, fftSize, rate, id]
            {
                if (safe == nullptr || safe->activeJobId != id) return;
                safe->status.setText ("Selection too short - needs about "
                                      + juce::String (1000.0 * (fftSize * 2) / rate, 0)
                                      + " ms of noise", juce::dontSendNotification);
                safe->finishJob (false, id);
            });
            return;
        }

        auto result = cv::Denoiser::process (
            stft, thread.workingProfile, input->channels, n, thread.parameters,
            [this, &thread] (float p)
            {
                progressAtomic.store ((double) p, std::memory_order_relaxed);
                return ! thread.threadShouldExit();
            });

        if (result.empty() || thread.threadShouldExit()) return;   // cancelled

        ChannelBuffer rem ((size_t) input->numChannels(), std::vector<float> ((size_t) n));
        for (int c = 0; c < input->numChannels(); ++c)
            for (int i = 0; i < n; ++i)
                rem[(size_t) c][(size_t) i] =
                    input->channels[(size_t) c][(size_t) i] - result[(size_t) c][(size_t) i];

        const auto safe = thread.safeOwner;
        const auto id = thread.id;
        auto completedProfile = std::move (thread.workingProfile);
        juce::MessageManager::callAsync (
            [safe, id, r = std::move (result), rm = std::move (rem),
             p = std::move (completedProfile)]() mutable
            {
                if (safe == nullptr || safe->activeJobId != id) return;
                const int frames = p.frames();
                safe->profile = std::move (p);
                safe->clean = std::make_shared<const ChannelBuffer> (std::move (r));
                safe->removed = std::make_shared<const ChannelBuffer> (std::move (rm));
                safe->haveProfile = true;
                safe->publishNoiseFloor();
                safe->status.setText (safe->fileName + "  -  processed from "
                                      + juce::String (frames) + " noise frames"
                                      + safe->monitoringRateNote(),
                                      juce::dontSendNotification);
                safe->finishJob (true, id);
            });
    }

    void finishJob (bool ok, uint64_t id)
    {
        if (activeJobId != id) return;
        if (job != nullptr)
        {
            job->waitForThreadToExit (-1);
            job.reset();
        }
        activeJobId = 0;
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
        if (job != nullptr) return;
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
            if (f == juce::File{} || job != nullptr) return;

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
            file = std::make_shared<const cv::AudioFile> (std::move (loaded));
            clean.reset(); removed.reset();
            haveProfile = false;
            floorView.clear();
            fileName = f.getFileName();
            wave.setAudio (&file->channels, file->sampleRate);
            spectro.setSource (&file->channels, file->sampleRate);
            publishPlayback (Monitor::original);
            syncSpectrogramView();
            origBtn.setToggleState (true, juce::sendNotification);
            monitor = Monitor::original;

            juce::String s = fileName + "  -  " + juce::String (file->numChannels()) + " ch, "
                             + juce::String (file->sampleRate, 0) + " Hz, "
                             + juce::String (file->bitsPerSample) + "-bit"
                             + (file->isFloat ? " float" : "") + ", "
                             + juce::String (file->numSamples() / file->sampleRate, 2) + " s"
                             + monitoringRateNote();
            status.setText (s, juce::dontSendNotification);
            refreshEnablement();
            updateViewLabel();
            grabKeyboardFocus();
        }
    }

    // Dropping a take onto the window is how this actually gets used.
    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        return job == nullptr && files.size() == 1
               && files[0].endsWithIgnoreCase (".wav");
    }
    void filesDropped (const juce::StringArray& files, int, int) override
    {
        if (files.size() == 1) loadFile (juce::File (files[0]));
    }

private:

    void exportClean()
    {
        if (clean == nullptr || file == nullptr) return;
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
            out.sampleRate = file->sampleRate;
            out.channels = *clean;
            std::string err;
            const bool ok = cv::wav::write (f.getFullPathName().toStdString(), out, err);
            status.setText (ok ? "Wrote " + f.getFileName()
                               : "Could not write: " + juce::String (err),
                            juce::dontSendNotification);
        });
    }

    // ---------------------------------------------------------------- misc ----

    // Copies the learned per-bin power out of the profile for display. Cheap, and it is
    // the only way to see that a fan tone or a stray transient got into the selection.
    void publishNoiseFloor()
    {
        if (! haveProfile || file == nullptr) { floorView.clear(); return; }
        const int fftSize = cv::fftSizeForRate (file->sampleRate);
        const cv::Stft stft (fftSize, fftSize / 4);
        const int bins = stft.numBins();
        std::vector<std::vector<float>> psd ((size_t) profile.channels(),
                                             std::vector<float> ((size_t) bins, 0.0f));
        for (int c = 0; c < profile.channels(); ++c)
            for (int k = 0; k < bins; ++k)
                psd[(size_t) c][(size_t) k] = profile.power (c, k);
        floorView.setProfile (std::move (psd), file->sampleRate, stft.windowSum());
    }

    void applyMonitor (Monitor m)
    {
        monitor = m;
        const auto owner = bufferOwnerFor (m);
        wave.setAudioKeepSelection (owner.get());
        spectro.setSource (owner.get(), file != nullptr ? file->sampleRate : 48000.0);
        publishPlayback (m);
        syncSpectrogramView();
    }

    void syncSpectrogramView()
    {
        if (showSpec)
            spectro.setView (wave.viewStartSample(), wave.viewLengthSamples());
    }

    // Used by the 1/2/3 keys: moves the radio button too, so the window always shows what
    // you are hearing.
    void selectMonitor (Monitor m)
    {
        auto* b = m == Monitor::clean ? &cleanBtn
                : m == Monitor::removed ? &removedBtn : &origBtn;
        if (! b->isEnabled()) return;
        b->setToggleState (true, juce::dontSendNotification);
        applyMonitor (m);
    }

    std::shared_ptr<const ChannelBuffer> bufferOwnerFor (Monitor m) const
    {
        switch (m)
        {
            case Monitor::clean:   return clean;
            case Monitor::removed: return removed;
            default:
                return file == nullptr ? nullptr
                                       : std::shared_ptr<const ChannelBuffer> (file, &file->channels);
        }
    }

    void publishPlayback (Monitor m)
    {
        auto next = std::make_shared<PlaybackState>();
        next->channels = bufferOwnerFor (m);
        next->sampleRate = file != nullptr ? file->sampleRate : 48000.0;
        std::atomic_store_explicit (&playbackState,
                                    std::shared_ptr<const PlaybackState> (std::move (next)),
                                    std::memory_order_release);
    }

    juce::String monitoringRateNote() const
    {
        if (file == nullptr) return {};
        const double outputRate = deviceRate.load();
        if (std::abs (outputRate - file->sampleRate) <= 1.0) return {};
        return "   [monitoring at " + juce::String (outputRate, 0)
               + " Hz - resampled for playback only]";
    }

    void togglePlay (bool selectionOnly)
    {
        if (file == nullptr || file->numSamples() == 0) return;
        if (selectionOnly && ! wave.hasSelection()) return;

        const bool wasPlaying = playing.load();
        const bool wasLooping = loopSelection.load();
        // Pressing the other transport button switches mode rather than stopping.
        const bool now = ! wasPlaying || wasLooping != selectionOnly;

        if (now)
        {
            loopSelection.store (selectionOnly);
            if (selectionOnly)
            {
                loopStart.store (wave.selectionStart());
                loopEnd.store (wave.selectionEnd());
                playPos.store ((double) wave.selectionStart());
            }
            else if (playPos.load() >= file->numSamples() - 1)
            {
                playPos.store (0.0);
            }
        }
        playing.store (now);
        refreshTransportText();
    }

    void refreshTransportText()
    {
        const bool p = playing.load(), sel = loopSelection.load();
        playBtn.setButtonText (p && ! sel ? "Stop" : "Play");
        playSelBtn.setButtonText (p && sel ? "Stop" : "Play Selection");
    }

    void updateViewLabel()
    {
        viewLabel.setText (wave.viewDescription(), juce::dontSendNotification);
    }

    void refreshEnablement()
    {
        const bool haveFile = file != nullptr && file->numSamples() > 0;
        const bool haveClean = clean != nullptr && ! clean->empty();
        const bool canLearn = wave.hasSelection();
        learnBtn.setEnabled (haveFile && (canLearn || haveProfile) && job == nullptr);
        learnBtn.setButtonText (canLearn ? "Learn Noise + Process"
                               : haveProfile ? "Process (keeps learned noise)"
                                             : "Select Noise Range");
        playBtn.setEnabled (haveFile);
        playSelBtn.setEnabled (haveFile && wave.hasSelection());
        zoomSelBtn.setEnabled (haveFile && wave.hasSelection());
        fitBtn.setEnabled (haveFile);
        updateViewLabel();
        exportBtn.setEnabled (haveClean);
        cleanBtn.setEnabled (haveClean);
        removedBtn.setEnabled (haveClean);
        openBtn.setEnabled (job == nullptr);
    }

    void timerCallback() override
    {
        monitorGain.store (juce::Decibels::decibelsToGain ((float) monitorS.getValue()));
        progressValue = progressAtomic.load (std::memory_order_relaxed);
        wave.setPlayhead ((int) playPos.load());
        if (! playing.load()
            && (playBtn.getButtonText() == "Stop" || playSelBtn.getButtonText() == "Stop"))
            refreshTransportText();
        if (progress.isVisible()) progress.repaint();
        updateViewLabel();          // wheel zoom has no callback of its own
    }

    juce::LookAndFeel_V4 lnf;
    juce::TooltipWindow tooltips { this, 700 };
    WaveformView wave;
    juce::TextButton openBtn, learnBtn, cancelBtn, playBtn, playSelBtn, exportBtn,
                     advancedBtn, fitBtn, zoomSelBtn;
    juce::TextButton origBtn, cleanBtn, removedBtn;
    juce::Slider reductionS, smoothingS, preserveS, oversubS, monitorS;
    juce::Label monitorL;
    NoiseFloorView floorView;
    SpectrogramView spectro;
    juce::TextButton floorBtn, specBtn;
    bool showSpec = false;
    std::atomic<float> monitorGain { 1.0f };
    bool showFloor = false;
    juce::Label reductionL, smoothingL, preserveL, oversubL, status, hint, viewLabel,
                keysLabel;
    double progressValue = 0.0;
    std::atomic<double> progressAtomic { 0.0 };
    juce::ProgressBar progress { progressValue };
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<Job> job;

    std::shared_ptr<const cv::AudioFile> file;
    std::shared_ptr<const ChannelBuffer> clean, removed;
    std::shared_ptr<const PlaybackState> playbackState;
    cv::DenoiseParams params;
    juce::String fileName;
    // The learned profile outlives the selection on purpose; see startProcessing.
    cv::NoiseProfile profile;
    bool haveProfile = false, learnFromSelection = true;
    std::pair<int, int> selForJob { 0, 0 };
    uint64_t nextJobId = 0, activeJobId = 0;

    std::atomic<bool> playing { false };
    std::atomic<bool> loopSelection { false };
    std::atomic<int> loopStart { 0 }, loopEnd { 1 };
    std::atomic<double> playPos { 0.0 };
    Monitor monitor = Monitor::original;
    std::atomic<double> deviceRate { 48000.0 };
    bool showAdvanced = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
} // namespace cvapp
