// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace nsr::analysis
{
using Complex = std::complex<double>;
inline constexpr double kPi = 3.1415926535897932384626433832795;

inline bool isPowerOfTwo (std::size_t n) noexcept
{
    return n != 0 && (n & (n - 1)) == 0;
}

inline std::size_t nextPowerOfTwo (std::size_t n) noexcept
{
    std::size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

// Small deterministic radix-2 FFT for the offline analyzer. It is intentionally local to
// Reverb Phase 0; sharing or replacing it belongs to Phase 1 and must not alter the
// baseline in the same commit.
class Fft
{
public:
    explicit Fft (std::size_t size) : n (size), reverse (size), twiddle (size / 2)
    {
        if (! isPowerOfTwo (n)) throw std::invalid_argument ("FFT size must be power of two");

        unsigned bits = 0;
        while ((std::size_t { 1 } << bits) < n) ++bits;
        for (std::size_t i = 0; i < n; ++i)
        {
            std::size_t r = 0;
            for (unsigned b = 0; b < bits; ++b)
                if ((i & (std::size_t { 1 } << b)) != 0)
                    r |= std::size_t { 1 } << (bits - 1 - b);
            reverse[i] = r;
        }

        for (std::size_t i = 0; i < n / 2; ++i)
        {
            const double a = -2.0 * kPi * static_cast<double> (i) / static_cast<double> (n);
            twiddle[i] = Complex (std::cos (a), std::sin (a));
        }
    }

    void forward (std::vector<Complex>& values) const { run (values, false); }

    void inverse (std::vector<Complex>& values) const
    {
        run (values, true);
        const double scale = 1.0 / static_cast<double> (n);
        for (auto& value : values) value *= scale;
    }

private:
    void run (std::vector<Complex>& values, bool inverseTransform) const
    {
        if (values.size() != n) throw std::invalid_argument ("FFT buffer has wrong size");

        for (std::size_t i = 0; i < n; ++i)
            if (i < reverse[i]) std::swap (values[i], values[reverse[i]]);

        for (std::size_t length = 2; length <= n; length <<= 1)
        {
            const std::size_t step = n / length;
            for (std::size_t base = 0; base < n; base += length)
                for (std::size_t j = 0; j < length / 2; ++j)
                {
                    Complex w = twiddle[j * step];
                    if (inverseTransform) w = std::conj (w);
                    const Complex even = values[base + j];
                    const Complex odd = values[base + j + length / 2] * w;
                    values[base + j] = even + odd;
                    values[base + j + length / 2] = even - odd;
                }
        }
    }

    std::size_t n;
    std::vector<std::size_t> reverse;
    std::vector<Complex> twiddle;
};
} // namespace nsr::analysis
