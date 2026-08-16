// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <juce_gui_extra/juce_gui_extra.h>
#include "PlayerComponent.h"

class BinauralPlayerApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "NekoSpace Binaural Player"; }
    const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise (const juce::String&) override
    {
        window = std::make_unique<Window> (getApplicationName());

        // A path on the command line opens straight away, so a take can be auditioned
        // from a shortcut or a terminal without going through the file dialog.
        for (const auto& a : juce::JUCEApplication::getCommandLineParameterArray())
        {
            const juce::File f (a.unquoted());
            if (f.existsAsFile()) { window->player().loadFromCommandLine (f); break; }
        }
    }

    void shutdown() override { window = nullptr; }
    void systemRequestedQuit() override { quit(); }

private:
    class Window : public juce::DocumentWindow
    {
    public:
        explicit Window (const juce::String& name)
            : DocumentWindow (name, nsbui::col::bg, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            content = new nsbplayer::PlayerComponent();
            setContentOwned (content, true);
            setResizable (true, true);
            setResizeLimits (900, 620, 3000, 2200);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

        nsbplayer::PlayerComponent& player() { return *content; }

    private:
        nsbplayer::PlayerComponent* content = nullptr;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Window)
    };

    std::unique_ptr<Window> window;
};

START_JUCE_APPLICATION (BinauralPlayerApplication)
