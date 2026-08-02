// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// Every help string in one table.
//
// The strings say what a control is *for*, not what it is: "high-frequency absorption"
// tells you nothing you can act on, "higher is a softer room — curtains, carpet, bedding"
// lets you choose. They are one line each because they are shown in the footer strip
// while the mouse is over a control, not in a dialog.
//
// Adding a language means adding a column here and a lookup, which is the reason this is
// a table rather than scattered setTooltip calls. The switch itself is deliberately not
// built yet — a UI preference like that belongs in the state tree, never in the parameter
// contract, and there is no point freezing a design for it before a second language
// exists.
#include <juce_core/juce_core.h>
#include "../core/ParameterIds.h"

namespace nsbui
{
struct HelpEntry { const char* key; const char* en; };

// Keys are parameter IDs where one exists, or a "ui." name for everything else.
inline const HelpEntry* helpTable (int& count)
{
    static const HelpEntry entries[] =
    {
        { nsb::pid::azimuth,
          "Direction around you. 0 is straight ahead, positive turns to your right." },
        { nsb::pid::elevation,
          "Height. Positive is above you. A subtle cue on headphones - see the readme." },
        { nsb::pid::distance,
          "How far away the source is. Below about 30 cm the near-field model takes over." },
        { nsb::pid::width,
          "How far apart the two inputs are placed. Linked Stereo only." },
        { nsb::pid::mode,
          "Mono Object collapses the input to one source. Linked Stereo places L and R apart." },
        { nsb::pid::nearfield,
          "How much each ear gets its own distance. 0 is a normal panner, 100 is at-the-ear." },
        { nsb::pid::headRadius,
          "Head size. Larger widens the time difference between the ears." },
        { nsb::pid::roomAmount,
          "How much room is mixed in. At 0 the output is exactly the dry binaural render." },
        { nsb::pid::roomSize,
          "Room dimensions and decay together. Small is a booth, large is a hall." },
        { nsb::pid::roomDamping,
          "High-frequency absorption. Higher is a softer room - curtains, carpet, bedding." },
        { nsb::pid::earlyLate,
          "0 is early reflections only (the room's shape), 100 is late reverb only (its size)." },
        { nsb::pid::duckAmount,
          "Holds the late reverb down while the voice speaks, so a close voice stays close." },
        { nsb::pid::duckRelease,
          "How long the room takes to reappear once a phrase ends." },
        { nsb::pid::hrtfProfile,
          "Which head model renders direction. Analytic B is the default; KU100 needs 48 kHz." },
        { nsb::pid::quality,
          "Standard uses the full HRIR. Economy halves it and costs less CPU." },
        { nsb::pid::outputGain,
          "Output trim, applied before the safety limiter." },
        { nsb::pid::bypassRoom,
          "Mutes the room instantly, for an A/B against the dry render." },

        { "ui.pad",
          "Drag to place the source. Wheel changes distance, double-click returns to front, 1 m." },
        { "ui.presets",
          "Complete scenes - each one sets position, near field and the whole room. Reference presets are diagnostics, not sounds to use." },
        { "ui.elevationSlider",
          "Height, the same control as ELEVATION on the right." },
        { "ui.lab",
          "Tune the height cue by ear on your own headphones, then freeze the result." },
        { "ui.snap",
          "Moves the source only. Room, near field and duck are left exactly as you set them - unlike a preset, which replaces everything." },
        { "ui.meters",
          "Output peak per ear, and GR is how hard the safety limiter is working." },
    };
    count = (int) (sizeof (entries) / sizeof (entries[0]));
    return entries;
}

inline juce::String helpFor (const juce::String& key)
{
    int n = 0;
    const auto* t = helpTable (n);
    for (int i = 0; i < n; ++i)
        if (key == juce::StringRef (t[i].key))
            return juce::String (t[i].en);
    return {};
}
} // namespace nsbui
