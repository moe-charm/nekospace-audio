// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// Session Clean, stage S0: the runner.
//
// One worker, one file at a time, reading an immutable RunPlan. Serial on purpose for S0 -
// a batch that is wrong in parallel is harder to reason about than one that is slow, and
// there is nothing here that a second thread would make correct.
//
// Three properties this is built around:
//
// WRITE THROUGH A TEMPORARY, COMMIT BY RENAME. A half-written file must never be able to
// look like a finished one. The output only appears under its real name once the last
// sample is on disk, and the rename fails rather than replaces if something got there
// first, so a completed take from an earlier run cannot be destroyed by a later one.
//
// NOTHING CARRIES BETWEEN FILES. Each item builds its own STFT and its own buffers; the
// only thing shared across the session is the noise profile, which is the point of a
// session. A batched file must come out sample-identical to the same file processed alone.
//
// CANCELLABLE WHERE THE TIME GOES. Between files, and inside the DSP through the progress
// callback the denoiser already takes. Cancelling abandons the temporary and leaves the
// finished outputs of earlier items alone.
//
// JUCE-free (Architecture Contract #4).
#include <functional>
#include <string>
#include <vector>
#include "RunPlan.h"
#include "../dsp/Stft.h"
#include "../dsp/NoiseProfile.h"
#include "../dsp/Denoiser.h"
#include "../io/WavFile.h"
#include "../io/Utf8Path.h"

namespace cv::session
{
enum class ItemResult { written, skipped, failed, cancelled };

struct ItemReport
{
    std::string source, output;
    ItemResult result = ItemResult::skipped;
    std::string detail;
};

struct RunReport
{
    std::vector<ItemReport> items;
    bool cancelled = false;
    std::string failure;          // set when the run could not start at all

    int countOf (ItemResult r) const
    {
        int n = 0;
        for (const auto& i : items) if (i.result == r) ++n;
        return n;
    }
};

struct RunCallbacks
{
    // Return false to stop. Called between files and repeatedly inside each file.
    std::function<bool()> shouldContinue;
    // index, total ready items, 0..1 within the current file, source path
    std::function<void (int, int, float, const std::string&)> onProgress;
    std::function<void (const ItemReport&)> onItemFinished;
};

class SessionRunner
{
public:
    static RunReport run (const RunPlan& plan, const RunCallbacks& cb = {})
    {
        RunReport report;
        if (! plan.ok()) { report.failure = plan.failure(); return report; }

        // The profile is learned once, from the reference, and is the one thing the whole
        // session shares. Learning it per file would make "session" meaningless.
        NoiseProfile profile;
        const int fftSize = fftSizeForRate (plan.referenceInfo().sampleRate);
        {
            const Stft stft (fftSize, fftSize / 4);
            AudioFile ref;
            std::string err;
            if (! wav::read (plan.request().referencePath, ref, err))
            { report.failure = "reference: " + err; return report; }

            const int s0 = (int) (plan.request().noiseStartSec * ref.sampleRate);
            const int s1 = (int) (plan.request().noiseEndSec * ref.sampleRate);
            if (! profile.learn (stft, ref.channels, ref.numSamples(), s0, s1))
            { report.failure = "noise range too short to learn from"; return report; }
        }

        const int total = plan.readyCount();
        int index = 0;

        for (const auto& item : plan.allItems())
        {
            if (item.state != ItemState::ready)
            {
                ItemReport r { item.source, item.output, ItemResult::skipped,
                               std::string (describe (item.state))
                                 + (item.detail.empty() ? "" : ": " + item.detail) };
                report.items.push_back (r);
                if (cb.onItemFinished) cb.onItemFinished (r);
                continue;
            }

            if (cb.shouldContinue && ! cb.shouldContinue())
            {
                report.cancelled = true;
                ItemReport r { item.source, item.output, ItemResult::cancelled, "" };
                report.items.push_back (r);
                if (cb.onItemFinished) cb.onItemFinished (r);
                break;
            }

            auto r = processOne (item, profile, plan.request().params, fftSize,
                                 index, total, cb);
            if (r.result == ItemResult::cancelled) report.cancelled = true;
            report.items.push_back (r);
            if (cb.onItemFinished) cb.onItemFinished (r);
            if (r.result == ItemResult::cancelled) break;
            ++index;
        }
        return report;
    }

private:
    static ItemReport processOne (const PlanItem& item, const NoiseProfile& profile,
                                  const DenoiseParams& params, int fftSize,
                                  int index, int total, const RunCallbacks& cb)
    {
        ItemReport r { item.source, item.output, ItemResult::failed, {} };

        // Fresh state for every file. Nothing is reused but the profile.
        const Stft stft (fftSize, fftSize / 4);

        AudioFile in;
        std::string err;
        if (! wav::read (item.source, in, err)) { r.detail = err; return r; }

        auto clean = Denoiser::process (
            stft, profile, in.channels, in.numSamples(), params,
            [&] (float p)
            {
                if (cb.onProgress) cb.onProgress (index, total, p, item.source);
                return ! (cb.shouldContinue && ! cb.shouldContinue());
            });

        if (clean.empty()) { r.result = ItemResult::cancelled; return r; }

        AudioFile out;
        out.sampleRate = in.sampleRate;
        out.channels = std::move (clean);

        // Write the temporary, then commit. Anything that goes wrong from here removes the
        // temporary and leaves no output at all - a missing file is a correct report of
        // failure, a truncated one is a lie.
        if (! wav::write (item.temp, out, err))
        {
            removeUtf8 (item.temp);
            r.detail = err;
            return r;
        }

        if (! renameNoReplace (item.temp, item.output))
        {
            removeUtf8 (item.temp);
            r.detail = existsUtf8 (item.output)
                         ? "output appeared while this file was being processed; kept"
                         : "could not commit the temporary file";
            return r;
        }

        r.result = ItemResult::written;
        return r;
    }
};
} // namespace cv::session
