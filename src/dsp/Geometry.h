#pragma once
// NekoSpace Binaural — geometry & shared math. JUCE-free (Architecture Contract #4).
#include <cmath>
#include <algorithm>

namespace nsb
{
constexpr float kSpeedOfSound = 343.0f;   // m/s
constexpr float kPi           = 3.14159265358979323846f;
constexpr float kMinDistance  = 0.05f;    // m, from head center
constexpr float kMaxDistance  = 20.0f;    // m
constexpr float kEarAzimuthDeg = 95.0f;   // ear axis slightly behind ±90°

inline float deg2rad (float d) noexcept { return d * (kPi / 180.0f); }
inline float clampf (float v, float lo, float hi) noexcept { return v < lo ? lo : (v > hi ? hi : v); }

// Wrap to (-180, 180]
inline float wrapDeg (float a) noexcept
{
    while (a > 180.0f)  a -= 360.0f;
    while (a <= -180.0f) a += 360.0f;
    return a;
}

// Shortest signed arc from -> to, in degrees (never takes the long way around ±180)
inline float shortestArcDeg (float from, float to) noexcept { return wrapDeg (to - from); }

struct Vec3
{
    float x = 0, y = 0, z = 0; // x = right, y = up, z = front
    Vec3 operator- (const Vec3& o) const noexcept { return { x - o.x, y - o.y, z - o.z }; }
    Vec3 operator* (float s)       const noexcept { return { x * s, y * s, z * s }; }
    float dot (const Vec3& o)      const noexcept { return x * o.x + y * o.y + z * o.z; }
    float length()                 const noexcept { return std::sqrt (dot (*this)); }
};

// azimuth: 0 = front, positive = right. elevation: positive = up.
inline Vec3 directionFromAngles (float azDeg, float elDeg) noexcept
{
    const float az = deg2rad (azDeg), el = deg2rad (elDeg);
    const float ce = std::cos (el);
    return { std::sin (az) * ce, std::sin (el), std::cos (az) * ce };
}

// Unit vector pointing out of an ear. sign: -1 = left ear, +1 = right ear.
inline Vec3 earDirection (float sign) noexcept
{
    return directionFromAngles (sign * kEarAzimuthDeg, 0.0f);
}

// Exact acoustic path length (m) from a source to one ear on a rigid sphere.
// Straight line while the ear is visible; tangent + great-circle arc when occluded.
// Continuous at the shadow boundary. This is the single source of ITD in the engine.
inline float earPathLengthExact (const Vec3& sourcePos, float sourceDist, float headRadius, float earSign) noexcept
{
    const float a = headRadius;
    const float r = std::max (sourceDist, a + 1e-4f);
    const Vec3 earDir = earDirection (earSign);
    const Vec3 earPos = earDir * a;

    const float cosTheta = clampf (sourcePos.dot (earDir) / (sourcePos.length() + 1e-9f), -1.0f, 1.0f);
    const float theta    = std::acos (cosTheta);
    const float thetaVis = kPi * 0.5f + std::acos (clampf (a / r, 0.0f, 1.0f));

    if (theta <= thetaVis)
        return (sourcePos - earPos).length();

    const float tangent = std::sqrt (std::max (r * r - a * a, 0.0f));
    return tangent + a * (theta - thetaVis);
}

// Far-field limit of the same model (Woodworth-style), as an OFFSET relative to r.
// Used to blend near-field exaggeration via nearfield.amount.
inline float earPathOffsetFarField (const Vec3& sourceDir, float headRadius, float earSign) noexcept
{
    const float a = headRadius;
    const Vec3 earDir = earDirection (earSign);
    const float cosTheta = clampf (sourceDir.dot (earDir), -1.0f, 1.0f);
    const float theta = std::acos (cosTheta);
    if (theta <= kPi * 0.5f)
        return -a * cosTheta;              // visible: straight-line advance
    return a * (theta - kPi * 0.5f);       // shadowed: wrap-around delay
}

// -------- smoothing --------

class LinearSmoother
{
public:
    void prepare (float sampleRate, float rampSeconds) noexcept
    {
        stepsTotal = std::max (1, (int) (rampSeconds * sampleRate));
        stepsLeft = 0; current = target; step = 0.0f;
    }
    void snap (float v) noexcept { current = target = v; stepsLeft = 0; step = 0.0f; }
    void setTarget (float v) noexcept
    {
        if (v == target && stepsLeft == 0) return;
        if (v == target) return;
        target = v;
        step = (target - current) / (float) stepsTotal;
        stepsLeft = stepsTotal;
    }
    float next() noexcept
    {
        if (stepsLeft > 0) { current += step; if (--stepsLeft == 0) current = target; }
        return current;
    }
    float value() const noexcept { return current; }
    bool  isSmoothing() const noexcept { return stepsLeft > 0; }

private:
    float current = 0, target = 0, step = 0;
    int stepsTotal = 64, stepsLeft = 0;
};

// One-pole lowpass (air absorption, damping)
class OnePoleLP
{
public:
    void prepare (float sr) noexcept { sampleRate = sr; setCutoff (20000.0f); z = 0; }
    void setCutoff (float fcHz) noexcept
    {
        fcHz = clampf (fcHz, 20.0f, sampleRate * 0.49f);
        const float x = std::exp (-2.0f * kPi * fcHz / sampleRate);
        a = 1.0f - x; b = x;
    }
    float process (float in) noexcept { z = a * in + b * z; return z; }
    void reset() noexcept { z = 0; }

private:
    float sampleRate = 48000.0f, a = 1.0f, b = 0.0f, z = 0.0f;
};
} // namespace nsb
