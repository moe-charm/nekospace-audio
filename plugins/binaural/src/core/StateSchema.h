// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// Rules for anything this plugin writes into a host project. See docs/state-format.md.
// JUCE-free so the fragile parts of choice restoration are unit-testable.
#include <string>
#include <vector>

namespace nsb
{
// Bump only when the *meaning* of stored data changes, never for additions.
//   1  original flat layout (elevation values as loose properties on the root)
//   2  versioned NEKOSPACE_EXTRA child; choice parameters stored by stable key
//   3  room.size no longer sets the decay time; room.decay does. Adding the parameter
//      would have been an addition, but taking decay away from room.size changes what
//      an existing room.size value MEANS, which is rule 8 - hence a bump and the
//      migration in migratedRoomDecay below.
inline constexpr int kStateSchemaVersion = 3;

// Decay for a state written before schema 3, reconstructed from the room size it was
// saved with. v0.1.0-alpha derived the tail as 0.25 + 2.4*size^2 seconds; replaying that
// formula is what makes an old project reload sounding the way it was mixed, instead of
// picking up the new parameter's default.
//
// sizePercent is the stored room.size (0..100). Kept here rather than in the DSP so the
// migration is unit-testable without instantiating the plugin.
inline float migratedRoomDecay (float sizePercent) noexcept
{
    const float s = sizePercent * 0.01f;
    const float t60 = 0.25f + 2.4f * s * s;
    return t60 < 0.15f ? 0.15f : (t60 > 4.0f ? 4.0f : t60);
}

// Choice parameters are stored by a permanent machine key, never by display name, index,
// or normalised value. Display names may be renamed or translated without changing state.
//
// APVTS stores a choice's denormalised index in its ValueTree, but hosts store automation
// values normalised to 0..1. Keys make plugin state independent of labels/order; they do
// not make it safe to change a released choice list, because automation remains numeric.
//
// Returns fallback when the stored key is unknown or missing.
inline int indexForChoiceKey (const std::vector<std::string>& keys,
                              const std::string& key,
                              int fallback) noexcept
{
    for (size_t i = 0; i < keys.size(); ++i)
        if (keys[i] == key)
            return (int) i;
    return fallback;
}

inline std::string keyForChoiceIndex (const std::vector<std::string>& keys,
                                      int index)
{
    return index >= 0 && index < (int) keys.size() ? keys[(size_t) index]
                                                   : std::string {};
}

// Early development builds of schema 2 wrote the user-facing name. Keep this reader
// while all new states use stable keys.
inline int indexForLegacyChoiceName (const std::vector<std::string>& names,
                                     const std::string& name,
                                     int fallback) noexcept
{
    for (size_t i = 0; i < names.size(); ++i)
        if (names[i] == name)
            return (int) i;
    return fallback;
}

} // namespace nsb
