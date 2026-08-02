// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// The manual, shown in a window you open on purpose.
//
// This replaced a footer line that updated on hover. That was worse than nothing: it
// moved in the corner of your eye every time the mouse crossed the window, whether or
// not you wanted help, and one line is not enough to explain something like Near Field.
// Help you ask for can be as long as it needs to be.
//
// A second language means a second column here, same as HelpText.h.
#include <juce_core/juce_core.h>
#include <vector>

namespace nsbui
{
struct HelpSection { const char* heading; const char* body; };

inline std::vector<HelpSection> helpSections()
{
    return {
        { "What this is",
          "A binaural spatializer for voice. It takes a mono voice - or a stereo pair - "
          "and places it somewhere around the listener's head, including right up against "
          "an ear. It is built for audio drama and ASMR rather than for music panning, so "
          "the near-field behaviour and the way a close voice survives a room matter more "
          "here than the size of the reverb." },

        { "Getting started",
          "Put it on a voice track, open the Presets menu, and try \"Left Ear 3 cm\". Then "
          "drag the orange dot in the pad: left and right move it around you, the scroll "
          "wheel changes distance, double-click returns it to the front at 1 m.\n"
          "Everything else refines that. If a control is not obvious, it is explained "
          "below." },

        { "Position",
          "AZIMUTH is the direction around you - 0 straight ahead, positive to your right, "
          "and it wraps continuously past +/-180 so a source can circle you without a jump.\n"
          "DISTANCE is how far away. Below roughly 30 cm the near-field model takes over "
          "and the two ears start hearing genuinely different things.\n"
          "ELEVATION is height. Be aware it is the weak axis - see Known limits.\n"
          "WIDTH and SOURCE MODE only matter for stereo material: Mono Object folds the "
          "input down to a single source, Linked Stereo keeps left and right apart by "
          "the Width angle." },

        { "Near Field - the at-the-ear effect",
          "This is the control that makes a whisper feel like it is against your ear "
          "rather than merely panned hard left.\n"
          "At 0 % both ears are attenuated by the same distance, so the difference between "
          "them comes only from your head being in the way. That is what a conventional "
          "panner does, and it is the right setting for music, ambience and anything that "
          "should sit naturally in the mix.\n"
          "At 100 % each ear gets its own distance to the source, worked out along the "
          "actual path around the head. A source 12 cm from the left ear is then about "
          "24 dB louder on that side instead of 6 dB, and arrives noticeably earlier. That "
          "is the ear-whisper effect.\n"
          "It only does anything up close. At two metres the two settings are identical." },

        { "Room",
          "ROOM sets how much of the room you hear. At 0 the output is exactly the dry "
          "binaural render, bit for bit.\n"
          "SIZE moves the room's dimensions and its decay together - small is a booth, "
          "large is a hall.\n"
          "DAMPING is how absorbent the surfaces are. Higher is a softer room: curtains, "
          "carpet, bedding. Lower is tile and glass.\n"
          "EARLY/LATE chooses what you hear of it. Early reflections are the room's shape "
          "and its size cues; late reverb is its tail. Each of the six reflections is "
          "rendered through the head model from the direction its image actually comes "
          "from, so a raised source really does get a floor bounce from below." },

        { "Voice Duck",
          "Reverb is what stops a close voice sounding close. Add enough room for the "
          "scene and the whisper you carefully placed 3 cm from an ear is suddenly a "
          "metre away.\n"
          "VOICE DUCK holds the late reverb down while the voice is speaking and lets it "
          "back up at the end of a phrase. The direct sound and the early reflections are "
          "never touched, so the room keeps its shape and the voice keeps its position; "
          "only the tail gets out of the way.\n"
          "The reverb is still being fed the whole time - it is held down, not switched "
          "off - so what you hear at the end of a line is a tail that has been building "
          "all along, not one starting from silence.\n"
          "DUCK REL is how long that takes. Short (150 ms) is tight and close; long "
          "(1 s+) lets the room bloom gradually after each line.\n"
          "There is no threshold to set. It measures the voice against its own recent "
          "level, so one setting works for a whisper and for a shout." },

        { "HRTF Profile",
          "Which head model turns a direction into what your two ears hear.\n"
          "Analytic B is the default and the one to use.\n"
          "Analytic A is the original model, kept only so the difference can be heard.\n"
          "KU100 is measured from a real dummy head. It is experimental, needs a 48 kHz "
          "session, and is only present in development builds - at other rates the plugin "
          "quietly falls back to Analytic B and says so at the bottom of the window.\n"
          "Custom is whatever you dialled in with the Elevation Lab." },

        { "Elevation Lab",
          "A tuning bench for the height cue, because height is the part that depends most "
          "on your own ears and headphones.\n"
          "Four controls. UP and DOWN set how far a raised or lowered source departs from "
          "ear level, and they are independent - you can chase \"above\" without dragging "
          "\"below\" along with it. BODY is the shoulder reflection, a lower-frequency cue "
          "that survives headphone colouration better than the pinna notches do. FOCUS is "
          "how narrow the notch is: sharp colouring versus a broad tonal shift.\n"
          "1.00 on everything is exactly Analytic B, so Reset really resets. Work with the "
          "room off and real voice material. When something works, Copy as C++ puts the "
          "numbers on the clipboard." },

        { "Quality and Output",
          "QUALITY: Standard uses the full head response. Economy halves it and costs "
          "roughly half the CPU; the difference is small on a voice.\n"
          "OUTPUT is a trim before the safety limiter. The limiter exists because the "
          "near-field gain can reach +32 dB at the skull - GR on the meter shows how hard "
          "it is working. If GR is lit most of the time, pull OUTPUT down.\n"
          "ROOM BYPASS mutes the room instantly for an A/B against the dry render.\n"
          "The plugin reports 2 ms of latency, which is real and gets compensated by the "
          "host. Bypassing it keeps the same delay, so switching does not shift timing." },

        { "Recipes",
          "A whisper at the left ear: Left Ear 3 cm preset, Near Field 100 %, Room around "
          "10 %, Voice Duck 60 %.\n"
          "A voice in a room that still sounds close: Distance 0.8-1.2 m, Room 35-45 %, "
          "Early/Late around 30 %, Voice Duck 50-70 %, Duck Rel 400 ms.\n"
          "Ambience or music that should sit naturally: Near Field 0 %, Linked Stereo, "
          "Width 60-90 deg, Voice Duck 0 %.\n"
          "Someone circling the listener: automate Azimuth. It wraps cleanly and the "
          "filters crossfade, so a full circle has no seam." },

        { "Known limits",
          "Elevation is weak. It makes a real, audible change and a mild sense of vertical "
          "movement, but it is not a dependable \"that is above me\" cue. Four approaches "
          "were built and measured, each improved the numbers, and none produced a "
          "decisive percept. That is the ceiling of static binaural played to arbitrary "
          "listeners: the spectral cues that carry height depend on the shape of your own "
          "pinnae, and headphones colour exactly the band those cues live in. Use "
          "elevation as a colour and let the script, the footsteps and the room carry "
          "height.\n"
          "The measured KU100 profile is 48 kHz only and is not bundled.\n"
          "Windows only so far. No user SOFA import yet." },

        { "Licence",
          "Free software under the GNU Affero General Public License v3.0 or later.\n"
          "Copyright (C) 2026 charmpic. NekoSpace Audio.\n"
          "The audio you make with this is yours. The licence covers the plugin's own "
          "source and binaries; a rendered file is not a derivative work of the software, "
          "so recordings and audio dramas made with it carry no obligation.\n"
          "Source: github.com/moe-charm/nekospace-audio" },
    };
}
} // namespace nsbui
