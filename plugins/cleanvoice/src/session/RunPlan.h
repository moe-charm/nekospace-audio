// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// Session Clean, stage S0: the plan.
//
// Everything that can be decided before a single sample is processed is decided here, and
// then frozen. A RunPlan cannot be modified once built; the runner reads it and nothing
// else. That is not tidiness, it is the property that makes a long batch trustworthy - a
// run cannot change its mind about where a file is going after it has started writing, and
// what will happen is inspectable before anything happens.
//
// What is settled at plan time:
//   * every source is readable and structurally compatible with the reference
//   * every output path is computed, and collisions between two sources are found
//   * outputs that already exist are marked, never scheduled for overwrite
//   * the temporary path each item will be written through
//
// NOT settled here, deliberately: whether the files sound alike. Acoustic similarity is
// out of scope for v1 - see docs/reference-denoise.md. The structural check is a floor,
// not a guarantee that one noise profile suits every take.
//
// JUCE-free (Architecture Contract #4).
#include <string>
#include <vector>
#include <algorithm>
#include "../io/WavFile.h"
#include "../io/Utf8Path.h"
#include "../dsp/Denoiser.h"

namespace cv::session
{
inline std::string directoryOf (const std::string& path)
{
    const size_t slash = path.find_last_of ("/\\");
    return slash == std::string::npos ? std::string {} : path.substr (0, slash + 1);
}

inline std::string fileNameOf (const std::string& path)
{
    const size_t slash = path.find_last_of ("/\\");
    return slash == std::string::npos ? path : path.substr (slash + 1);
}

inline std::string stemOf (const std::string& name)
{
    const size_t dot = name.find_last_of ('.');
    return dot == std::string::npos ? name : name.substr (0, dot);
}

// What the caller asks for. Turned into a RunPlan, which is what actually runs.
struct RunRequest
{
    std::vector<std::string> sources;
    std::string referencePath;         // the take the noise is learned from
    double noiseStartSec = 0.0, noiseEndSec = 0.0;
    std::string outputDir;             // empty = beside each source
    std::string suffix = "-clean";
    DenoiseParams params;
};

enum class ItemState
{
    ready,              // will be processed
    unreadable,         // source could not be probed
    incompatible,       // structurally different from the reference
    outputExists,       // a finished output is already there; never overwritten
    outputCollision,    // two sources want the same output path
    alreadyOutput,      // the source is itself a previous run's output
    duplicateSource     // the same file appears more than once in the list
};

inline const char* describe (ItemState s)
{
    switch (s)
    {
        case ItemState::ready:           return "ready";
        case ItemState::unreadable:      return "unreadable";
        case ItemState::incompatible:    return "incompatible format";
        case ItemState::outputExists:    return "output already exists";
        case ItemState::outputCollision: return "two sources map to one output";
        case ItemState::alreadyOutput:   return "already an output of a previous run";
        case ItemState::duplicateSource: return "listed more than once";
    }
    return "?";
}

struct PlanItem
{
    std::string source;
    std::string output;
    std::string temp;
    ItemState state = ItemState::ready;
    std::string detail;
    wav::WavInfo info;
};

class RunPlan
{
public:
    // The only way to make one. Returns a plan whose contents never change afterwards;
    // ok() is false when there is no work that can safely be done.
    static RunPlan build (const RunRequest& req)
    {
        RunPlan plan;
        plan.req = req;

        if (req.sources.empty()) { plan.error = "no source files"; return plan; }
        if (req.referencePath.empty()) { plan.error = "no reference file"; return plan; }
        if (req.noiseEndSec <= req.noiseStartSec)
        { plan.error = "noise range is empty"; return plan; }

        std::string err;
        if (! wav::probe (req.referencePath, plan.reference, err))
        { plan.error = "reference: " + err; return plan; }

        const double refDur = plan.reference.sampleRate > 0
                                ? (double) plan.reference.frames / plan.reference.sampleRate : 0.0;
        if (req.noiseEndSec > refDur)
        { plan.error = "noise range runs past the end of the reference"; return plan; }

        // Pass 1: probe, check compatibility, compute the output path.
        for (const auto& src : req.sources)
        {
            PlanItem item;
            item.source = src;

            // Outputs land beside their source, so pointing the tool at a folder a second
            // time would otherwise find the first run's results and clean those too, into
            // take-clean-clean.wav. A name already carrying the suffix is not a source.
            if (endsWithSuffix (fileNameOf (src), req.suffix))
            {
                item.state = ItemState::alreadyOutput;
                item.detail = req.suffix;
                plan.items.push_back (std::move (item));
                continue;
            }

            // The same file listed twice is not a collision to report against both - it is
            // one job mentioned twice, and the second mention is the mistake.
            bool seen = false;
            for (const auto& earlier : plan.items)
                if (earlier.state != ItemState::duplicateSource
                    && samePath (earlier.source, src)) { seen = true; break; }
            if (seen)
            {
                item.state = ItemState::duplicateSource;
                plan.items.push_back (std::move (item));
                continue;
            }

            std::string e;
            if (! wav::probe (src, item.info, e))
            {
                item.state = ItemState::unreadable;
                item.detail = e;
                plan.items.push_back (std::move (item));
                continue;
            }

            // A noise profile is per-bin at one sample rate and per-channel. Applying it
            // across a different rate or channel count is not a degraded result, it is a
            // meaningless one, so those files are excluded rather than coerced.
            if (item.info.sampleRate != plan.reference.sampleRate
                || item.info.channels != plan.reference.channels)
            {
                item.state = ItemState::incompatible;
                item.detail = juceless (item.info) + " vs reference "
                                + juceless (plan.reference);
                plan.items.push_back (std::move (item));
                continue;
            }

            const std::string dir = req.outputDir.empty() ? directoryOf (src)
                                                          : withSeparator (req.outputDir);
            item.output = dir + stemOf (fileNameOf (src)) + req.suffix + ".wav";
            plan.items.push_back (std::move (item));
        }

        // Pass 2: collisions between planned outputs. Checked over the whole set rather
        // than as each item is added, because a collision is a property of the pair and
        // both halves of it need to be reported, not just the second one seen.
        for (size_t i = 0; i < plan.items.size(); ++i)
        {
            if (plan.items[i].state != ItemState::ready) continue;
            for (size_t j = i + 1; j < plan.items.size(); ++j)
            {
                if (plan.items[j].state != ItemState::ready) continue;
                if (! samePath (plan.items[i].output, plan.items[j].output)) continue;
                plan.items[i].state = plan.items[j].state = ItemState::outputCollision;
                plan.items[i].detail = plan.items[j].detail = plan.items[i].output;
            }
        }

        // Pass 3: outputs that already exist. A finished file from an earlier run is
        // never replaced - the commit step enforces it too, but saying so up front means
        // the answer is visible before the batch spends an hour arriving at it.
        for (auto& item : plan.items)
        {
            if (item.state != ItemState::ready) continue;
            if (existsUtf8 (item.output))
            {
                item.state = ItemState::outputExists;
                item.detail = item.output;
                continue;
            }
            item.temp = item.output + ".part";
        }

        if (plan.readyCount() == 0 && plan.error.empty())
            plan.error = "nothing to do: no source is both compatible and unclaimed";
        return plan;
    }

    bool ok() const noexcept { return error.empty() && readyCount() > 0; }
    const std::string& failure() const noexcept { return error; }

    const std::vector<PlanItem>& allItems() const noexcept { return items; }
    const RunRequest& request() const noexcept { return req; }
    const wav::WavInfo& referenceInfo() const noexcept { return reference; }

    int readyCount() const noexcept
    {
        int n = 0;
        for (const auto& i : items) if (i.state == ItemState::ready) ++n;
        return n;
    }

private:
    RunPlan() = default;

    static std::string withSeparator (const std::string& dir)
    {
        if (dir.empty()) return dir;
        const char last = dir.back();
        return (last == '/' || last == '\\') ? dir : dir + "/";
    }

    // Enough for collisions inside one plan: the paths are built by the same rule, so
    // they differ only in case on the platforms where that does not matter.
    static bool samePath (const std::string& a, const std::string& b)
    {
#ifdef _WIN32
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i)
        {
            char x = a[i], y = b[i];
            if (x == '\\') x = '/';
            if (y == '\\') y = '/';
            if (std::tolower ((unsigned char) x) != std::tolower ((unsigned char) y))
                return false;
        }
        return true;
#else
        return a == b;
#endif
    }

    static bool endsWithSuffix (const std::string& fileName, const std::string& suffix)
    {
        if (suffix.empty()) return false;
        const std::string stem = stemOf (fileName);
        if (stem.size() < suffix.size()) return false;
        return samePath (stem.substr (stem.size() - suffix.size()), suffix);
    }

    static std::string juceless (const wav::WavInfo& i)
    {
        return std::to_string ((long long) i.sampleRate) + " Hz / "
                 + std::to_string (i.channels) + " ch";
    }

    RunRequest req;
    wav::WavInfo reference;
    std::vector<PlanItem> items;
    std::string error;
};
} // namespace cv::session
