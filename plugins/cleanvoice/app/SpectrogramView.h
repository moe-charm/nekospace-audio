// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// Spectrogram of whichever buffer is being monitored, over the waveform's view range.
//
// This is the display that makes the tool judgeable. Hiss is a flat haze; a fan is a
// horizontal line; a fricative is a vertical brush stroke in the top half. Switch the
// monitor to Removed Noise and the question "am I taking the performance out?" becomes a
// picture: broadband haze with nothing in it is right, and vertical strokes or horizontal
// formant bands are the consonants and voice being eaten.
//
// ONE FFT PER PIXEL COLUMN, not one per hop. The cost is then fixed by the width of the
// window rather than by the length of the file - a 22-minute take costs the same as a
// two-second one. It samples the spectrogram rather than averaging it, which is the right
// trade for looking at something you are about to listen to anyway.
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <cmath>
#include "WaveformView.h"          // cvapp::col
#include "../src/dsp/Fft.h"

namespace cvapp
{
class SpectrogramView : public juce::Component
{
public:
    SpectrogramView() = default;

    void setSource (const std::vector<std::vector<float>>* channels, double sampleRate)
    {
        src = channels;
        sr = sampleRate;
        total = (src != nullptr && ! src->empty()) ? (int) (*src)[0].size() : 0;
        dirty = true;
        repaint();
    }

    void setView (int start, int length)
    {
        if (start == viewStart && length == viewLen) return;
        viewStart = start; viewLen = length;
        dirty = true;
        repaint();
    }

    void resized() override { dirty = true; }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds();
        g.setColour (col::panel);
        g.fillRoundedRectangle (b.toFloat(), 4.0f);

        if (src == nullptr || total <= 0 || b.getWidth() < 8 || b.getHeight() < 8)
        {
            g.setColour (col::textDim);
            g.setFont (juce::Font (juce::FontOptions (12.0f)));
            g.drawText ("Open a WAV file", b, juce::Justification::centred);
            return;
        }

        auto plot = b.reduced (44, 4);
        if (dirty) { render (plot.getWidth(), plot.getHeight()); dirty = false; }
        if (image.isValid()) g.drawImageAt (image, plot.getX(), plot.getY());

        // frequency scale
        g.setFont (juce::Font (juce::FontOptions (9.5f)));
        for (double f : { 100.0, 1000.0, 10000.0 })
        {
            if (f > sr * 0.5) continue;
            const int y = plot.getY() + freqToY (f, plot.getHeight());
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.drawHorizontalLine (y, (float) plot.getX(), (float) plot.getRight());
            g.setColour (col::textDim);
            g.drawText (f >= 1000.0 ? juce::String (f / 1000.0, 0) + "k" : juce::String (f, 0),
                        2, y - 7, 38, 14, juce::Justification::centredRight);
        }
        g.setColour (col::textDim);
        g.setFont (juce::Font (juce::FontOptions (10.0f)));
        g.drawText (juce::String ((int) kTopDb) + " .. " + juce::String ((int) kBotDb) + " dBFS",
                    plot.getRight() - 118, plot.getY() + 2, 116, 14,
                    juce::Justification::centredRight);
    }

private:
    static constexpr float kTopDb = -30.0f, kBotDb = -120.0f, kMinHz = 20.0f;

    int freqToY (double f, int h) const
    {
        const double lo = std::log10 (kMinHz), hi = std::log10 (juce::jmax (1000.0, sr * 0.5));
        const double t = (std::log10 (juce::jmax (f, (double) kMinHz)) - lo) / (hi - lo);
        return juce::jlimit (0, h - 1, h - 1 - juce::roundToInt (t * (h - 1)));
    }

    // Dark blue -> blue -> orange -> white, so it reads the way every other spectrogram
    // in this line of work does.
    static juce::Colour heat (float t)
    {
        t = juce::jlimit (0.0f, 1.0f, t);
        if (t < 0.45f)
        {
            const float u = t / 0.45f;
            return juce::Colour::fromFloatRGBA (0.03f + 0.05f * u, 0.05f + 0.18f * u,
                                                0.14f + 0.55f * u, 1.0f);
        }
        if (t < 0.75f)
        {
            const float u = (t - 0.45f) / 0.30f;
            return juce::Colour::fromFloatRGBA (0.08f + 0.83f * u, 0.23f + 0.32f * u,
                                                0.69f - 0.60f * u, 1.0f);
        }
        const float u = (t - 0.75f) / 0.25f;
        return juce::Colour::fromFloatRGBA (0.91f + 0.09f * u, 0.55f + 0.45f * u,
                                            0.09f + 0.85f * u, 1.0f);
    }

    void render (int w, int h)
    {
        image = juce::Image (juce::Image::RGB, juce::jmax (1, w), juce::jmax (1, h), false);
        if (src == nullptr || total <= 0 || viewLen <= 0) return;

        const int n = fft.size();
        const int bins = n / 2 + 1;
        std::vector<cv::Complex> spec ((size_t) n);
        std::vector<float> power ((size_t) bins);
        std::vector<float> colMax ((size_t) h);

        // window once
        if ((int) win.size() != n)
        {
            win.resize ((size_t) n);
            for (int i = 0; i < n; ++i)
                win[(size_t) i] = 0.5f * (1.0f - std::cos (2.0f * cv::kPi * (float) i / (float) n));
        }
        const float refDb = 20.0f * std::log10 ((float) n * 0.25f);   // sine at full scale

        // which y each bin lands on, computed once
        if ((int) binY.size() != bins || binYForRate != sr || binYForHeight != h)
        {
            binY.resize ((size_t) bins);
            for (int k = 0; k < bins; ++k)
                binY[(size_t) k] = freqToY ((double) k * sr / n, h);
            binYForRate = sr; binYForHeight = h;
        }

        juce::Image::BitmapData bmp (image, juce::Image::BitmapData::writeOnly);

        for (int x = 0; x < w; ++x)
        {
            const int centre = viewStart + (int) ((double) x / w * viewLen);
            const int start = centre - n / 2;

            std::fill (power.begin(), power.end(), 0.0f);
            for (const auto& ch : *src)
            {
                for (int i = 0; i < n; ++i)
                {
                    const int s = start + i;
                    const float v = s >= 0 && s < total ? ch[(size_t) s] : 0.0f;
                    spec[(size_t) i] = cv::Complex (v * win[(size_t) i], 0.0f);
                }
                fft.forward (spec);
                for (int k = 1; k < bins; ++k)
                {
                    const float re = spec[(size_t) k].real();
                    const float im = spec[(size_t) k].imag();
                    power[(size_t) k] += re * re + im * im;
                }
            }

            std::fill (colMax.begin(), colMax.end(), kBotDb);
            for (int k = 1; k < bins; ++k)
            {
                // Average power, not the time-domain L+R sum. A binaural recording may
                // have interaural phase differences, and summing first can hide exactly
                // the one-ear residue this view exists to reveal.
                const float meanPower = power[(size_t) k]
                                        / (float) juce::jmax ((int) src->size(), 1);
                const float d = 10.0f * std::log10 (juce::jmax (meanPower, 1.0e-20f))
                                  - refDb;
                float& m = colMax[(size_t) binY[(size_t) k]];
                if (d > m) m = d;
            }
            // fill the gaps a log axis leaves between low-frequency bins
            for (int y = h - 2; y >= 0; --y)
                if (colMax[(size_t) y] <= kBotDb) colMax[(size_t) y] = colMax[(size_t) (y + 1)];

            for (int y = 0; y < h; ++y)
            {
                const float t = (colMax[(size_t) y] - kBotDb) / (kTopDb - kBotDb);
                bmp.setPixelColour (x, y, heat (t));
            }
        }
    }

    const std::vector<std::vector<float>>* src = nullptr;
    double sr = 48000.0;
    int total = 0, viewStart = 0, viewLen = 0;
    bool dirty = true;
    cv::Fft fft { 1024 };
    std::vector<float> win;
    std::vector<int> binY;
    double binYForRate = 0.0; int binYForHeight = 0;
    juce::Image image;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrogramView)
};
} // namespace cvapp
