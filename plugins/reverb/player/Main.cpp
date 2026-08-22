// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <juce_gui_extra/juce_gui_extra.h>
#include "PlayerComponent.h"

class ReverbPlayerApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "NekoSpace Reverb Player"; }
    const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void initialise (const juce::String&) override
    {
        window = std::make_unique<Window> (getApplicationName());
        for (const auto& argument : getCommandLineParameterArray())
        {
            const juce::File file (argument.unquoted());
            if (file.existsAsFile()) { window->player().loadFromCommandLine (file); break; }
        }
    }
    void shutdown() override { window = nullptr; }
    void systemRequestedQuit() override { quit(); }

private:
    class Window final : public juce::DocumentWindow
    {
    public:
        explicit Window (const juce::String& name)
            : DocumentWindow (name, juce::Colour (0xff171615), allButtons)
        {
            setUsingNativeTitleBar (true);
            content = new ReverbPlayerComponent();
            setContentOwned (content, true);
            setResizable (true, true);
            setResizeLimits (780, 454, 2400, 1600);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { JUCEApplication::getInstance()->systemRequestedQuit(); }
        ReverbPlayerComponent& player() { return *content; }

    private:
        ReverbPlayerComponent* content = nullptr;
    };
    std::unique_ptr<Window> window;
};

START_JUCE_APPLICATION (ReverbPlayerApplication)
