// SPDX-FileCopyrightText: 2026 TextureVoice
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
inline constexpr int kStateSchemaVersion = 2;

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
