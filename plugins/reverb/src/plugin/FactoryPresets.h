// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <array>

namespace nsr
{
struct FactoryPreset
{
    const char* name;
    float space;
    float distance;
    float definition;
    float preDelayMs;
    float decaySeconds;
    float bassTailPercent;
    float airTailPercent;
    float mixPercent;
    bool wetMonoInput;
};

inline constexpr std::array<FactoryPreset, 6> factoryPresets {{
    { "Default",         35.0f, 25.0f, 65.0f, 12.0f, 1.40f, 100.0f,  70.0f, 35.0f, false },
    { "Voice Booth",     18.0f, 18.0f, 88.0f,  4.0f, 0.38f,  85.0f,  58.0f, 18.0f, false },
    { "Small Wood Room", 32.0f, 30.0f, 70.0f,  8.0f, 0.85f, 125.0f,  62.0f, 28.0f, false },
    { "Dialogue Stage",  45.0f, 38.0f, 82.0f, 18.0f, 1.15f,  95.0f,  55.0f, 24.0f, false },
    { "Soft Chamber",    52.0f, 22.0f, 48.0f, 10.0f, 1.85f, 135.0f,  42.0f, 34.0f, false },
    { "Open Hall",       78.0f, 58.0f, 42.0f, 32.0f, 3.20f, 120.0f,  78.0f, 38.0f, false },
}};
}
