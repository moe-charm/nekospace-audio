// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// Processing one range of a file instead of all of it.
//
// Deciding a setting by rendering twenty minutes is absurd when the decision is made on ten
// seconds of it. This renders only the chosen span and splices it back, so the rest of the
// file is left exactly as it came in.
//
// THE PRE-ROLL IS THE WHOLE TRICK. The decision-directed SNR estimate starts each run from
// gain = 1 and takes a few frames to settle, so a span rendered cold does not sound like
// the same span rendered inside the full file - the first fraction of a second is softer
// than it will be for real. Feeding it audio from before the range and then discarding
// that part gives the estimator the history it would have had. Without it, a preview is a
// preview of something you will not get.
//
// Outside the range the output IS the input, sample for sample. So a difference signal
// (input - output) is silent outside the range, which reads correctly on a spectrogram:
// nothing was done there, and nothing is claimed to have been.
//
// JUCE-free (Architecture Contract #4).
#include <vector>
#include <algorithm>
#include <functional>
#include "Stft.h"
#include "NoiseProfile.h"
#include "Denoiser.h"

namespace cv
{
struct PreviewSpan { int start = 0; int end = 0; };

// Pre-roll long enough for the decision-directed estimate to converge. Its memory is
// alpha^n with alpha around 0.98, so ~200 frames is several time constants; at the usual
// hop that is a bit over half a second.
inline int previewPreRollSamples (const Stft& stft) noexcept
{
    return std::max (stft.fftSize() * 4, stft.hopSize() * 200);
}

// Returns a full-length copy of `channels` with only [span.start, span.end) processed.
// Empty on cancel, exactly like Denoiser::process.
inline std::vector<std::vector<float>> renderPreview (
    const Stft& stft, const NoiseProfile& profile,
    const std::vector<std::vector<float>>& channels, int numSamples,
    PreviewSpan span, const DenoiseParams& params,
    const std::function<bool (float)>& onProgress = {})
{
    const int nCh = (int) channels.size();
    if (nCh == 0 || numSamples <= 0) return {};

    span.start = std::max (0, std::min (span.start, numSamples));
    span.end   = std::max (span.start, std::min (span.end, numSamples));
    if (span.end <= span.start) return {};

    const int preRoll = previewPreRollSamples (stft);
    // Floor the analysis origin to the hop grid. The STFT frames of the slice then fall on
    // exactly the same sample positions as the frames of a full-file render, which is what
    // makes the two agree bit for bit instead of merely closely. Left unaligned it still
    // sounds the same, but "the preview is what you will get" stops being literally true,
    // and that claim is the reason the feature exists.
    int from = std::max (0, span.start - preRoll);
    from -= from % stft.hopSize();
    const int to   = std::min (numSamples, span.end + stft.fftSize());
    const int len  = to - from;

    std::vector<std::vector<float>> slice ((size_t) nCh, std::vector<float> ((size_t) len));
    for (int c = 0; c < nCh; ++c)
        std::copy (channels[(size_t) c].begin() + from,
                   channels[(size_t) c].begin() + to,
                   slice[(size_t) c].begin());

    auto cleanedSlice = Denoiser::process (stft, profile, slice, len, params, onProgress);
    if (cleanedSlice.empty()) return {};        // cancelled

    // Start from the input and overwrite only the range that was asked for. The pre-roll
    // did its job by existing; its output is discarded.
    auto out = channels;
    for (int c = 0; c < nCh; ++c)
        for (int i = span.start; i < span.end; ++i)
            out[(size_t) c][(size_t) i] = cleanedSlice[(size_t) c][(size_t) (i - from)];
    return out;
}
} // namespace cv
