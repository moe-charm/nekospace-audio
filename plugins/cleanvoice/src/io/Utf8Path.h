// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// Opening a file whose path is not ASCII.
//
// On Windows, fopen() interprets a narrow path in the process code page - CP932 here - so
// a UTF-8 path containing Japanese characters simply fails to open. That is not an edge
// case for this project: the recordings live in folders named after the characters in the
// script. Every path in this codebase is UTF-8, so on Windows it is converted to UTF-16
// and opened with _wfopen.
//
// The conversions are written out rather than pulled from <windows.h> so that this stays a
// self-contained, JUCE-free, dependency-free header like the rest of src/.
#include <cstdio>
#include <string>

namespace cv
{
#ifdef _WIN32
inline std::wstring utf8ToWide (const std::string& s)
{
    std::wstring out;
    out.reserve (s.size());
    size_t i = 0;
    while (i < s.size())
    {
        const unsigned char c = (unsigned char) s[i];
        unsigned int cp = 0;
        int extra = 0;
        if (c < 0x80)          { cp = c;            extra = 0; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1Fu; extra = 1; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0Fu; extra = 2; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07u; extra = 3; }
        else { ++i; continue; }                       // invalid lead byte: skip

        if (i + (size_t) extra >= s.size()) break;
        for (int k = 1; k <= extra; ++k)
        {
            const unsigned char cc = (unsigned char) s[i + (size_t) k];
            if ((cc & 0xC0) != 0x80) { cp = 0xFFFD; break; }
            cp = (cp << 6) | (cc & 0x3Fu);
        }
        i += (size_t) extra + 1;

        if (cp >= 0x10000u)                           // outside the BMP: surrogate pair
        {
            cp -= 0x10000u;
            out.push_back ((wchar_t) (0xD800u + (cp >> 10)));
            out.push_back ((wchar_t) (0xDC00u + (cp & 0x3FFu)));
        }
        else
        {
            out.push_back ((wchar_t) cp);
        }
    }
    return out;
}

inline std::string wideToUtf8 (const std::wstring& w)
{
    std::string out;
    out.reserve (w.size() * 3);
    for (size_t i = 0; i < w.size(); ++i)
    {
        unsigned int cp = (unsigned int) (unsigned short) w[i];
        if (cp >= 0xD800u && cp <= 0xDBFFu && i + 1 < w.size())
        {
            const unsigned int lo = (unsigned int) (unsigned short) w[i + 1];
            if (lo >= 0xDC00u && lo <= 0xDFFFu)
            {
                cp = 0x10000u + ((cp - 0xD800u) << 10) + (lo - 0xDC00u);
                ++i;
            }
        }
        if (cp < 0x80u) out.push_back ((char) cp);
        else if (cp < 0x800u)
        {
            out.push_back ((char) (0xC0u | (cp >> 6)));
            out.push_back ((char) (0x80u | (cp & 0x3Fu)));
        }
        else if (cp < 0x10000u)
        {
            out.push_back ((char) (0xE0u | (cp >> 12)));
            out.push_back ((char) (0x80u | ((cp >> 6) & 0x3Fu)));
            out.push_back ((char) (0x80u | (cp & 0x3Fu)));
        }
        else
        {
            out.push_back ((char) (0xF0u | (cp >> 18)));
            out.push_back ((char) (0x80u | ((cp >> 12) & 0x3Fu)));
            out.push_back ((char) (0x80u | ((cp >> 6) & 0x3Fu)));
            out.push_back ((char) (0x80u | (cp & 0x3Fu)));
        }
    }
    return out;
}
#endif

// Opens a UTF-8 path for reading or writing, correctly on every platform.
inline std::FILE* openUtf8 (const std::string& path, const char* mode)
{
#ifdef _WIN32
    const std::wstring wPath = utf8ToWide (path);
    std::wstring wMode;
    for (const char* m = mode; *m != 0; ++m) wMode.push_back ((wchar_t) *m);
    std::FILE* file = nullptr;
    return _wfopen_s (&file, wPath.c_str(), wMode.c_str()) == 0 ? file : nullptr;
#else
    return std::fopen (path.c_str(), mode);
#endif
}
} // namespace cv
