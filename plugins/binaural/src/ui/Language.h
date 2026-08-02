// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// Which language the written help is shown in.
//
// Deliberately NOT a plugin parameter and NOT part of the project state.
//
// Not a parameter, because it would appear in the host's automation list and would fall
// into the same trap as any released choice list - the option set could never grow again.
//
// Not project state either, and this is the part that is easy to get wrong: the elevation
// macros belong to a project because they are part of how that piece sounds, but a
// language belongs to a *person*. Storing it per project means opening last year's
// session and finding the help back in the wrong language.
//
// So it lives in the user's application settings, shared by every project and every
// instance, and it starts from the operating system's own language so that most people
// never have to find the switch at all.
#include <juce_data_structures/juce_data_structures.h>

namespace nsbui
{
enum class Language { en, ja };

inline juce::PropertiesFile& settingsFile()
{
    // ApplicationProperties is non-copyable, so configure it in place rather than
    // returning one from a lambda.
    static juce::ApplicationProperties props;
    static const bool once = []
    {
        juce::PropertiesFile::Options o;
        o.applicationName     = "NekoSpace";
        o.filenameSuffix      = ".settings";
        o.folderName          = "NekoSpace Audio";
        o.osxLibrarySubFolder = "Application Support";
        props.setStorageParameters (o);
        return true;
    }();
    juce::ignoreUnused (once);
    return *props.getUserSettings();
}

inline Language& languageRef()
{
    static Language lang = []
    {
        const auto stored = settingsFile().getValue ("helpLanguage");
        if (stored == "ja") return Language::ja;
        if (stored == "en") return Language::en;
        // no choice made yet: follow the OS
        return juce::SystemStats::getUserLanguage().startsWithIgnoreCase ("ja")
                   ? Language::ja : Language::en;
    }();
    return lang;
}

inline Language currentLanguage() { return languageRef(); }

inline void setCurrentLanguage (Language l)
{
    if (l == languageRef()) return;
    languageRef() = l;
    settingsFile().setValue ("helpLanguage", l == Language::ja ? "ja" : "en");
    settingsFile().saveIfNeeded();
}

// Picks the right column of a table row. Falls back to English when a string has not
// been translated yet, so a missing entry shows the original rather than nothing.
inline juce::String pick (const char* en, const char* ja)
{
    if (currentLanguage() == Language::ja && ja != nullptr && *ja != 0)
        return juce::String::fromUTF8 (ja);
    return juce::String::fromUTF8 (en);
}
} // namespace nsbui
