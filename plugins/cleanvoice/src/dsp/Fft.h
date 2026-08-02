// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// Minimal radix-2 complex FFT. JUCE-free (Architecture Contract #4), and deliberately
// unoptimised: this runs offline over a file, so clarity is worth more than speed. If a
// realtime version ever needs it, that is the moment to bring in a real FFT, not now.
#include <vector>
#include <complex>
#include <cmath>
#include <cstddef>

namespace cv
{
using Complex = std::complex<float>;
constexpr float kPi = 3.14159265358979323846f;

class Fft
{
public:
    explicit Fft (int size) : n (size)
    {
        // twiddles for each stage, and the bit-reversal permutation, computed once
        rev.resize ((size_t) n);
        int bits = 0;
        while ((1 << bits) < n) ++bits;
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b)
                if (i & (1 << b)) r |= 1 << (bits - 1 - b);
            rev[(size_t) i] = r;
        }
        tw.resize ((size_t) (n / 2));
        for (int i = 0; i < n / 2; ++i)
        {
            const float a = -2.0f * kPi * (float) i / (float) n;
            tw[(size_t) i] = Complex (std::cos (a), std::sin (a));
        }
    }

    int size() const noexcept { return n; }

    void forward (std::vector<Complex>& x) const { run (x, false); }

    // Unnormalised inverse; divides by n so that inverse(forward(x)) == x.
    void inverse (std::vector<Complex>& x) const
    {
        run (x, true);
        const float s = 1.0f / (float) n;
        for (auto& v : x) v *= s;
    }

private:
    void run (std::vector<Complex>& x, bool conj) const
    {
        for (int i = 0; i < n; ++i)
            if (i < rev[(size_t) i])
                std::swap (x[(size_t) i], x[(size_t) rev[(size_t) i]]);

        for (int len = 2; len <= n; len <<= 1)
        {
            const int step = n / len;
            for (int i = 0; i < n; i += len)
            {
                for (int j = 0; j < len / 2; ++j)
                {
                    Complex w = tw[(size_t) (j * step)];
                    if (conj) w = std::conj (w);
                    const Complex u = x[(size_t) (i + j)];
                    const Complex v = x[(size_t) (i + j + len / 2)] * w;
                    x[(size_t) (i + j)] = u + v;
                    x[(size_t) (i + j + len / 2)] = u - v;
                }
            }
        }
    }

    int n;
    std::vector<int> rev;
    std::vector<Complex> tw;
};
} // namespace cv
