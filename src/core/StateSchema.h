// SPDX-FileCopyrightText: 2026 TextureVoice
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// Rules for anything this plugin writes into a host project. See docs/state-format.md.
// JUCE-free so the fragile part — resolving a choice by name — is unit-testable.
#include <string>
#include <vector>

namespace nsb
{
// Bump only when the *meaning* of stored data changes, never for additions.
//   1  original flat layout (elevation values as loose properties on the root)
//   2  versioned NEKOSPACE_EXTRA child; choice parameters stored by name
inline constexpr int kStateSchemaVersion = 2;

// Choice parameters are stored by NAME, never by index and never by normalised value.
//
// A choice parameter normalises as index / (count - 1), so adding one option silently
// changes what every existing project restores: "Analytic B" saved as 0.5 out of three
// options becomes index 2 of four — a different profile — on reload. This plugin has
// already grown that list from two options to four, so the hazard is not hypothetical.
//
// Returns fallback when the stored name is unknown, which is what happens when an
// option is renamed or removed in a later version.
inline int indexForChoiceName (const std::vector<std::string>& choices,
                               const std::string& name,
                               int fallback) noexcept
{
    for (size_t i = 0; i < choices.size(); ++i)
        if (choices[i] == name)
            return (int) i;
    return fallback;
}
} // namespace nsb
