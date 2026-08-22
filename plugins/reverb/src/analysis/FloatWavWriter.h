// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "BaselineRenderer.h"

namespace nsr::analysis
{
inline void appendU16 (std::vector<std::uint8_t>& bytes, std::uint16_t value)
{
    bytes.push_back (static_cast<std::uint8_t> (value & 0xffu));
    bytes.push_back (static_cast<std::uint8_t> ((value >> 8u) & 0xffu));
}

inline void appendU32 (std::vector<std::uint8_t>& bytes, std::uint32_t value)
{
    for (unsigned shift = 0; shift < 32; shift += 8)
        bytes.push_back (static_cast<std::uint8_t> ((value >> shift) & 0xffu));
}

inline void appendTag (std::vector<std::uint8_t>& bytes, const char* tag)
{
    for (int i = 0; i < 4; ++i) bytes.push_back (static_cast<std::uint8_t> (tag[i]));
}

inline bool writeFloatStereoWav (const std::filesystem::path& path, const StereoIr& ir,
                                 std::string& error)
{
    if (ir.left.empty() || ir.left.size() != ir.right.size())
    {
        error = "IR channel sizes do not match";
        return false;
    }

    const std::uint64_t dataBytes64 = ir.left.size() * 2u * sizeof (float);
    if (dataBytes64 > 0xffffffffu - 36u)
    {
        error = "WAV exceeds RIFF 32-bit size limit";
        return false;
    }
    const auto dataBytes = static_cast<std::uint32_t> (dataBytes64);
    const auto sampleRate = static_cast<std::uint32_t> (ir.sampleRate + 0.5);

    std::vector<std::uint8_t> header;
    header.reserve (44);
    appendTag (header, "RIFF"); appendU32 (header, 36u + dataBytes); appendTag (header, "WAVE");
    appendTag (header, "fmt "); appendU32 (header, 16u);
    appendU16 (header, 3u);                    // IEEE float
    appendU16 (header, 2u);
    appendU32 (header, sampleRate);
    appendU32 (header, sampleRate * 2u * 4u);
    appendU16 (header, 8u); appendU16 (header, 32u);
    appendTag (header, "data"); appendU32 (header, dataBytes);

    std::ofstream stream (path, std::ios::binary);
    if (! stream)
    {
        error = "cannot create " + path.string();
        return false;
    }
    stream.write (reinterpret_cast<const char*> (header.data()),
                  static_cast<std::streamsize> (header.size()));
    for (std::size_t i = 0; i < ir.left.size(); ++i)
    {
        const float frame[2] { ir.left[i], ir.right[i] };
        stream.write (reinterpret_cast<const char*> (frame), sizeof (frame));
    }
    if (! stream)
    {
        error = "failed while writing " + path.string();
        return false;
    }
    return true;
}
} // namespace nsr::analysis
