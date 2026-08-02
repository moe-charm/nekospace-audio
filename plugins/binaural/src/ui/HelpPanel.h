// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// Scrollable manual. Text is laid out with TextLayout so it reflows when the window is
// resized rather than being clipped at a fixed width.
#include <juce_gui_basics/juce_gui_basics.h>
#include "NekoLookAndFeel.h"
#include "HelpContent.h"
#include "Language.h"

namespace nsbui
{
class HelpBody : public juce::Component
{
public:
    HelpBody() : sections (helpSections()) {}

    // Lays out every block for the current width and returns the height needed. The
    // owning viewport calls this to size the scrollable area.
    int layoutForWidth (int width)
    {
        blocks.clear();
        const int textW = juce::jmax (200, width - 2 * kMargin);
        int y = kMargin;

        for (const auto& s : sections)
        {
            blocks.add ({ makeLayout (pick (s.heading, s.headingJa), textW, true), y, true });
            y += (int) blocks.getLast().layout.getHeight() + 6;

            blocks.add ({ makeLayout (pick (s.body, s.bodyJa), textW, false), y, false });
            y += (int) blocks.getLast().layout.getHeight() + kSectionGap;
        }
        contentHeight = y + kMargin;
        return contentHeight;
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (col::bg);
        for (const auto& b : blocks)
        {
            if (b.isHeading)
            {
                g.setColour (col::panelLine);
                g.drawLine ((float) kMargin, (float) b.y - 8.0f,
                            (float) (getWidth() - kMargin), (float) b.y - 8.0f, 1.0f);
            }
            b.layout.draw (g, juce::Rectangle<float> ((float) kMargin, (float) b.y,
                                                      (float) (getWidth() - 2 * kMargin),
                                                      b.layout.getHeight()));
        }
    }

private:
    static constexpr int kMargin = 22;
    static constexpr int kSectionGap = 26;

    static juce::TextLayout makeLayout (const juce::String& text, int width, bool heading)
    {
        juce::AttributedString a;
        a.setWordWrap (juce::AttributedString::byWord);
        a.setLineSpacing (heading ? 0.0f : 3.0f);
        a.append (text,
                  juce::Font (juce::FontOptions (heading ? 15.0f : 12.5f)
                                  .withStyle (heading ? "Bold" : "Regular")),
                  heading ? col::accent : col::text);
        juce::TextLayout l;
        l.createLayout (a, (float) width);
        return l;
    }

    struct Block { juce::TextLayout layout; int y; bool isHeading; };

    std::vector<HelpSection> sections;
    juce::Array<Block> blocks;
    int contentHeight = 0;
};

class HelpPanel : public juce::Component
{
public:
    HelpPanel()
    {
        viewport.setViewedComponent (&body, false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);

        // The switch lives here because the moment you want another language is the
        // moment you are reading the manual - and it costs the main window nothing.
        // The choice is a property of the person, so it is stored in the user's own
        // settings and applies to every project; see Language.h.
        for (auto* b : { &enButton, &jaButton })
        {
            b->setRadioGroupId (1);
            b->setClickingTogglesState (true);
            b->setConnectedEdges (b == &enButton ? juce::Button::ConnectedOnRight
                                                 : juce::Button::ConnectedOnLeft);
            addAndMakeVisible (b);
        }
        enButton.setButtonText ("EN");
        jaButton.setButtonText (juce::String::fromUTF8 ("日本語"));
        enButton.setToggleState (currentLanguage() == Language::en, juce::dontSendNotification);
        jaButton.setToggleState (currentLanguage() == Language::ja, juce::dontSendNotification);
        enButton.onClick = [this] { switchTo (Language::en); };
        jaButton.onClick = [this] { switchTo (Language::ja); };

        setSize (720, 560);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        auto head = b.removeFromTop (30).reduced (10, 3);
        jaButton.setBounds (head.removeFromRight (72));
        enButton.setBounds (head.removeFromRight (44));
        viewport.setBounds (b);
        const int w = viewport.getMaximumVisibleWidth();
        body.setSize (w, body.layoutForWidth (w));
    }

private:
    void switchTo (Language l)
    {
        if (l == currentLanguage()) return;
        setCurrentLanguage (l);
        const int w = viewport.getMaximumVisibleWidth();
        body.setSize (w, body.layoutForWidth (w));
        viewport.setViewPosition (0, 0);
        body.repaint();
    }

    juce::Viewport viewport;
    HelpBody body;
    juce::TextButton enButton, jaButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HelpPanel)
};
} // namespace nsbui
