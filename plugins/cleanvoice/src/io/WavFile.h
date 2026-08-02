// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// Minimal WAV reader/writer: PCM 16/24/32-bit integer and 32-bit float, any channel count.
// JUCE-free, so the whole v1 prototype builds without pulling a framework in for file I/O.
//
// Deliberately strict: an unrecognised format is an error, never a silent guess. Reading a
// file wrong and then judging the denoiser by the result is the sort of mistake that costs
// a day.
#include <vector>
#include <string>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>

namespace cv
{
struct AudioFile
{
    std::vector<std::vector<float>> channels;   // de-interleaved
    double sampleRate = 48000.0;
    int bitsPerSample = 24;
    bool isFloat = false;

    int numSamples() const
    {
        return channels.empty() ? 0 : (int) channels[0].size();
    }
    int numChannels() const { return (int) channels.size(); }
};

namespace wav
{
inline uint32_t rd32 (const uint8_t* p) { return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24); }
inline uint16_t rd16 (const uint8_t* p) { return (uint16_t) ((uint32_t) p[0] | ((uint32_t) p[1] << 8)); }

inline bool read (const std::string& path, AudioFile& out, std::string& err)
{
    FILE* f = std::fopen (path.c_str(), "rb");
    if (f == nullptr) { err = "cannot open " + path; return false; }
    std::fseek (f, 0, SEEK_END);
    const long len = std::ftell (f);
    std::fseek (f, 0, SEEK_SET);
    std::vector<uint8_t> buf ((size_t) len);
    if (len <= 44 || std::fread (buf.data(), 1, (size_t) len, f) != (size_t) len)
    { std::fclose (f); err = "short or unreadable file"; return false; }
    std::fclose (f);

    if (std::memcmp (buf.data(), "RIFF", 4) != 0 || std::memcmp (buf.data() + 8, "WAVE", 4) != 0)
    { err = "not a RIFF/WAVE file"; return false; }

    size_t pos = 12;
    uint16_t fmt = 0, ch = 0, bits = 0;
    uint32_t rate = 0;
    const uint8_t* data = nullptr;
    size_t dataLen = 0;

    while (pos + 8 <= buf.size())
    {
        const char* id = (const char*) buf.data() + pos;
        const uint32_t sz = rd32 (buf.data() + pos + 4);
        const size_t body = pos + 8;
        if (body + sz > buf.size()) break;

        if (std::memcmp (id, "fmt ", 4) == 0 && sz >= 16)
        {
            fmt  = rd16 (buf.data() + body);
            ch   = rd16 (buf.data() + body + 2);
            rate = rd32 (buf.data() + body + 4);
            bits = rd16 (buf.data() + body + 14);
            if (fmt == 0xFFFE && sz >= 40)          // WAVE_FORMAT_EXTENSIBLE
                fmt = rd16 (buf.data() + body + 24); // first two bytes of the sub-format GUID
        }
        else if (std::memcmp (id, "data", 4) == 0)
        {
            data = buf.data() + body;
            dataLen = sz;
        }
        pos = body + sz + (sz & 1);                  // chunks are word-aligned
    }

    if (data == nullptr || ch == 0) { err = "no data or fmt chunk"; return false; }
    if (fmt != 1 && fmt != 3) { err = "unsupported WAV encoding (not PCM or float)"; return false; }
    if (! (bits == 16 || bits == 24 || bits == 32)) { err = "unsupported bit depth"; return false; }

    const int bytes = bits / 8;
    const size_t frames = dataLen / (size_t) (bytes * ch);
    out.channels.assign (ch, std::vector<float> (frames, 0.0f));
    out.sampleRate = (double) rate;
    out.bitsPerSample = bits;
    out.isFloat = (fmt == 3);

    for (size_t i = 0; i < frames; ++i)
        for (int c = 0; c < ch; ++c)
        {
            const uint8_t* s = data + (i * (size_t) ch + (size_t) c) * (size_t) bytes;
            float v = 0.0f;
            if (fmt == 3) { std::memcpy (&v, s, 4); }
            else if (bits == 16) { const int16_t x = (int16_t) rd16 (s); v = (float) x / 32768.0f; }
            else if (bits == 24)
            {
                int32_t x = (int32_t) ((uint32_t) s[0] | ((uint32_t) s[1] << 8) | ((uint32_t) s[2] << 16));
                if (x & 0x800000) x |= (int32_t) 0xFF000000;
                v = (float) x / 8388608.0f;
            }
            else { const int32_t x = (int32_t) rd32 (s); v = (float) x / 2147483648.0f; }
            out.channels[(size_t) c][i] = v;
        }
    return true;
}

// Always writes 32-bit float: the prototype's output is listened to and measured, not
// mastered, and float removes any question of clipping or dither confusing the result.
inline bool write (const std::string& path, const AudioFile& in, std::string& err)
{
    const int ch = in.numChannels(), n = in.numSamples();
    if (ch == 0) { err = "nothing to write"; return false; }

    const uint32_t dataBytes = (uint32_t) ((size_t) n * (size_t) ch * 4u);
    std::vector<uint8_t> h;
    auto put32 = [&h] (uint32_t v) { h.push_back ((uint8_t) (v & 0xFF)); h.push_back ((uint8_t) ((v >> 8) & 0xFF)); h.push_back ((uint8_t) ((v >> 16) & 0xFF)); h.push_back ((uint8_t) ((v >> 24) & 0xFF)); };
    auto put16 = [&h] (uint16_t v) { h.push_back ((uint8_t) (v & 0xFF)); h.push_back ((uint8_t) ((v >> 8) & 0xFF)); };
    auto tag = [&h] (const char* s) { for (int i = 0; i < 4; ++i) h.push_back ((uint8_t) s[i]); };

    tag ("RIFF"); put32 (36u + dataBytes); tag ("WAVE");
    tag ("fmt "); put32 (16u); put16 (3); put16 ((uint16_t) ch);
    put32 ((uint32_t) (in.sampleRate + 0.5));
    put32 ((uint32_t) ((in.sampleRate + 0.5)) * (uint32_t) ch * 4u);
    put16 ((uint16_t) (ch * 4)); put16 (32);
    tag ("data"); put32 (dataBytes);

    FILE* f = std::fopen (path.c_str(), "wb");
    if (f == nullptr) { err = "cannot write " + path; return false; }
    std::fwrite (h.data(), 1, h.size(), f);

    std::vector<float> row ((size_t) ch);
    for (int i = 0; i < n; ++i)
    {
        for (int c = 0; c < ch; ++c) row[(size_t) c] = in.channels[(size_t) c][(size_t) i];
        std::fwrite (row.data(), 4, (size_t) ch, f);
    }
    std::fclose (f);
    return true;
}
} // namespace wav
} // namespace cv
