// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// Permanent parameter IDs — see docs/parameter-contract.md. Never rename, never reuse.
//
// Kept in their own header, free of JUCE and of the processor, so the UI can key help
// text and layout off the same identifiers the DSP and the state format use, without
// anything having to include PluginProcessor.h.

namespace nsb::pid
{
inline constexpr const char* bypass     = "global.bypass";
inline constexpr const char* azimuth    = "position.azimuth";
inline constexpr const char* elevation  = "position.elevation";
inline constexpr const char* distance   = "position.distance";
inline constexpr const char* width      = "source.width";
inline constexpr const char* mode       = "source.mode";
inline constexpr const char* nearfield  = "nearfield.amount";
inline constexpr const char* headRadius = "head.radius";
inline constexpr const char* roomAmount = "room.amount";
inline constexpr const char* roomSize   = "room.size";
inline constexpr const char* roomDamping= "room.damping";
inline constexpr const char* earlyLate  = "room.early_late";
inline constexpr const char* duckAmount = "duck.amount";
inline constexpr const char* duckRelease= "duck.release";
inline constexpr const char* hrtfProfile= "hrtf.profile";
inline constexpr const char* quality    = "quality.mode";
inline constexpr const char* outputGain = "output.gain";
inline constexpr const char* bypassRoom = "output.bypass_room";
}
