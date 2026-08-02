// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <juce_gui_extra/juce_gui_extra.h>
#include "MainComponent.h"

class CleanVoiceApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "NekoSpace CleanVoice"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise (const juce::String& commandLine) override
    {
        window = std::make_unique<Window> (getApplicationName());

        // A path on the command line opens straight away, which is what makes the app
        // usable from a terminal and scriptable for testing.
        const auto args = juce::JUCEApplication::getCommandLineParameterArray();
        for (const auto& a : args)
        {
            const juce::File f (a.unquoted());
            if (f.existsAsFile())
            {
                window->component().loadFile (f);
                break;
            }
        }
        juce::ignoreUnused (commandLine);
    }

    void shutdown() override { window = nullptr; }
    void systemRequestedQuit() override { quit(); }

private:
    class Window : public juce::DocumentWindow
    {
    public:
        explicit Window (const juce::String& name)
            : DocumentWindow (name, juce::Colour (0xff14171c), DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            main = new cvapp::MainComponent();
            setContentOwned (main, true);
            setResizable (true, true);
            setResizeLimits (860, 520, 3000, 2000);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

        cvapp::MainComponent& component() { return *main; }

    private:
        cvapp::MainComponent* main = nullptr;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Window)
    };

    std::unique_ptr<Window> window;
};

START_JUCE_APPLICATION (CleanVoiceApplication)
