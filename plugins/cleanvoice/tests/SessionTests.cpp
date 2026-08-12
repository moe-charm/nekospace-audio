// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Acceptance tests for Session Clean S0.
//
// Every WAV these tests touch is synthesised here at run time into a scratch directory
// that is removed afterwards. No recording is read, and nothing audio-shaped is ever added
// to the repository.

#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <random>
#include <algorithm>
#ifdef _WIN32
 #include <direct.h>
 #include <process.h>
 #include <sys/stat.h>
#else
 #include <sys/stat.h>
 #include <unistd.h>
#endif
#include "../src/session/RunPlan.h"
#include "../src/session/SessionRunner.h"

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf ("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); ++failures; } } while (0)

using namespace cv;
using namespace cv::session;

// ---------------------------------------------------------------- scratch ----

static std::string scratchDir;

#ifdef _WIN32
static bool existsDir (const std::string& p)
{
    struct _stat64i32 st {};
    return _wstat (utf8ToWide (p).c_str(), &st) == 0;
}
#endif

static bool makeDir (const std::string& p)
{
#ifdef _WIN32
    return _wmkdir (utf8ToWide (p).c_str()) == 0 || existsDir (p);
#else
    return mkdir (p.c_str(), 0777) == 0 || access (p.c_str(), F_OK) == 0;
#endif
}

static std::string uniqueScratch()
{
    const char* base = std::getenv ("TEMP");
    if (base == nullptr) base = std::getenv ("TMPDIR");
    if (base == nullptr) base = ".";
#ifdef _WIN32
    const int pid = _getpid();
#else
    const int pid = (int) getpid();
#endif
    return std::string (base) + "/cv_session_test_" + std::to_string (pid);
}

// ---------------------------------------------------------------- fixtures ----

// Hiss plus a few bursts standing in for speech. Deterministic per seed, so the same
// source always produces the same bytes and a comparison means something.
static AudioFile makeTake (double sr, int channels, double seconds, unsigned seed,
                           bool withBursts = true)
{
    AudioFile f;
    f.sampleRate = sr;
    const int n = (int) (seconds * sr);
    f.channels.assign ((size_t) channels, std::vector<float> ((size_t) n, 0.0f));

    std::mt19937 rng (seed);
    std::normal_distribution<float> g (0.0f, 1.0f);
    std::vector<float> lp ((size_t) channels, 0.0f);

    for (int i = 0; i < n; ++i)
    {
        const double t = (double) i / sr;
        for (int c = 0; c < channels; ++c)
        {
            lp[(size_t) c] = 0.55f * lp[(size_t) c] + 0.45f * g (rng);
            float v = 0.012f * lp[(size_t) c];
            if (withBursts && t > 1.0)
            {
                const double phase = std::fmod (t - 1.0, 0.5);
                if (phase < 0.18)
                {
                    const float env = (float) std::pow (std::sin (kPi * phase / 0.18), 2.0);
                    v += 0.22f * env * g (rng) * (c == 0 ? 1.0f : 0.35f);
                }
            }
            f.channels[(size_t) c][(size_t) i] = v;
        }
    }
    return f;
}

static bool writeTake (const std::string& path, const AudioFile& f)
{
    std::string err;
    return wav::write (path, f, err);
}

static std::vector<unsigned char> readBytes (const std::string& path)
{
    std::vector<unsigned char> out;
    FILE* fp = openUtf8 (path, "rb");
    if (fp == nullptr) return out;
    std::fseek (fp, 0, SEEK_END);
    const long n = std::ftell (fp);
    std::fseek (fp, 0, SEEK_SET);
    out.resize ((size_t) std::max (0L, n));
    if (! out.empty()) { if (std::fread (out.data(), 1, out.size(), fp) != out.size()) out.clear(); }
    std::fclose (fp);
    return out;
}

static RunRequest baseRequest (const std::vector<std::string>& sources,
                               const std::string& reference)
{
    RunRequest req;
    req.sources = sources;
    req.referencePath = reference;
    req.noiseStartSec = 0.05;
    req.noiseEndSec = 0.95;      // the first second is hiss only
    req.params.reductionDb = 10.0f;
    return req;
}

// ---------------------------------------------------------------- tests ----

// The headline promise of a batch: it is not a different processor. A file cleaned in a
// session must be byte-for-byte what it would have been on its own.
static void testBatchMatchesSingle()
{
    const double sr = 48000.0;
    const std::string a = scratchDir + "/a.wav", b = scratchDir + "/b.wav";
    CHECK (writeTake (a, makeTake (sr, 2, 3.0, 1)), "fixture a written");
    CHECK (writeTake (b, makeTake (sr, 2, 2.5, 2)), "fixture b written");

    // batch
    auto plan = RunPlan::build (baseRequest ({ a, b }, a));
    CHECK (plan.ok(), "plan builds");
    auto rep = SessionRunner::run (plan);
    CHECK (rep.countOf (ItemResult::written) == 2, "batch wrote both");

    const auto batchA = readBytes (scratchDir + "/a-clean.wav");
    const auto batchB = readBytes (scratchDir + "/b-clean.wav");
    CHECK (! batchA.empty() && ! batchB.empty(), "batch outputs exist");

    // the same two files, one at a time, into a different directory
    const std::string solo = scratchDir + "/solo";
    makeDir (solo);
    for (const auto& src : { a, b })
    {
        auto req = baseRequest ({ src }, a);
        req.outputDir = solo;
        auto p1 = RunPlan::build (req);
        CHECK (p1.ok(), "single-file plan builds");
        auto r1 = SessionRunner::run (p1);
        CHECK (r1.countOf (ItemResult::written) == 1, "single file written");
    }

    CHECK (batchA == readBytes (solo + "/a-clean.wav"),
           "batch output is byte-identical to processing that file alone");
    CHECK (batchB == readBytes (solo + "/b-clean.wav"),
           "and for the second file too");
}

// If anything survived from one file into the next, order would change the result.
static void testNoStateLeakBetweenFiles()
{
    const double sr = 48000.0;
    const std::string a = scratchDir + "/o_a.wav", b = scratchDir + "/o_b.wav";
    // deliberately unalike: different length and a very different burst level
    CHECK (writeTake (a, makeTake (sr, 2, 3.0, 11)), "fixture written");
    AudioFile loud = makeTake (sr, 2, 2.0, 12);
    for (auto& ch : loud.channels) for (auto& v : ch) v *= 6.0f;
    CHECK (writeTake (b, loud), "loud fixture written");

    const std::string fwd = scratchDir + "/fwd", rev = scratchDir + "/rev";
    makeDir (fwd); makeDir (rev);

    auto reqF = baseRequest ({ a, b }, a); reqF.outputDir = fwd;
    auto reqR = baseRequest ({ b, a }, a); reqR.outputDir = rev;
    SessionRunner::run (RunPlan::build (reqF));
    SessionRunner::run (RunPlan::build (reqR));

    CHECK (readBytes (fwd + "/o_a-clean.wav") == readBytes (rev + "/o_a-clean.wav"),
           "no state leak: a file is unaffected by what was processed before it");
    CHECK (readBytes (fwd + "/o_b-clean.wav") == readBytes (rev + "/o_b-clean.wav"),
           "no state leak: and the same for the other order");
}

static void testSourcesAreNeverModified()
{
    const double sr = 48000.0;
    const std::string a = scratchDir + "/src_a.wav", b = scratchDir + "/src_b.wav";
    writeTake (a, makeTake (sr, 2, 2.0, 21));
    writeTake (b, makeTake (sr, 1, 2.0, 22));      // mono: will be rejected, still untouched

    const auto beforeA = readBytes (a), beforeB = readBytes (b);
    auto req = baseRequest ({ a, b }, a);
    req.outputDir = scratchDir + "/srcout";
    makeDir (req.outputDir);
    SessionRunner::run (RunPlan::build (req));

    CHECK (readBytes (a) == beforeA, "source WAV is unchanged after processing");
    CHECK (readBytes (b) == beforeB, "a rejected source is left alone as well");
}

static void testIncompatibleAndCollisionsAreCaught()
{
    const double sr = 48000.0;
    const std::string ref = scratchDir + "/c_ref.wav";
    const std::string wrongRate = scratchDir + "/c_rate.wav";
    const std::string wrongCh = scratchDir + "/c_mono.wav";
    writeTake (ref, makeTake (sr, 2, 2.0, 31));
    writeTake (wrongRate, makeTake (44100.0, 2, 2.0, 32));
    writeTake (wrongCh, makeTake (sr, 1, 2.0, 33));

    auto plan = RunPlan::build (baseRequest ({ ref, wrongRate, wrongCh }, ref));
    CHECK (plan.readyCount() == 1, "only the compatible source is scheduled");
    int incompatible = 0;
    for (const auto& i : plan.allItems())
        if (i.state == ItemState::incompatible) ++incompatible;
    CHECK (incompatible == 2, "both the wrong rate and the wrong channel count are refused");

    // two sources in different directories whose outputs land in one place
    const std::string d1 = scratchDir + "/d1", d2 = scratchDir + "/d2";
    makeDir (d1); makeDir (d2);
    writeTake (d1 + "/take.wav", makeTake (sr, 2, 2.0, 34));
    writeTake (d2 + "/take.wav", makeTake (sr, 2, 2.0, 35));

    auto req = baseRequest ({ d1 + "/take.wav", d2 + "/take.wav" }, ref);
    req.outputDir = scratchDir + "/collide";
    makeDir (req.outputDir);
    auto cp = RunPlan::build (req);
    int collisions = 0;
    for (const auto& i : cp.allItems())
        if (i.state == ItemState::outputCollision) ++collisions;
    CHECK (collisions == 2, "both halves of an output collision are reported");
    CHECK (cp.readyCount() == 0, "a colliding pair is not silently half-processed");
}

// An output from an earlier run is a finished thing. A later run must not touch it, and
// must say so rather than quietly doing nothing.
static void testExistingOutputIsNeverReplaced()
{
    const double sr = 48000.0;
    const std::string a = scratchDir + "/e_a.wav";
    writeTake (a, makeTake (sr, 2, 2.0, 41));

    auto plan1 = RunPlan::build (baseRequest ({ a }, a));
    SessionRunner::run (plan1);
    const std::string out = scratchDir + "/e_a-clean.wav";
    const auto first = readBytes (out);
    CHECK (! first.empty(), "first run produced an output");

    // change the settings so a second run would produce something different
    auto req = baseRequest ({ a }, a);
    req.params.reductionDb = 24.0f;
    auto plan2 = RunPlan::build (req);
    CHECK (plan2.readyCount() == 0, "an existing output is not scheduled");
    auto rep2 = SessionRunner::run (plan2);
    CHECK (rep2.countOf (ItemResult::written) == 0, "and nothing is written");
    CHECK (readBytes (out) == first, "the existing output is byte-for-byte untouched");
}

// Outputs land beside their sources, so a second pass over the same folder would find the
// first pass's results. Left alone that compounds: take-clean-clean-clean.wav.
static void testPreviousOutputsAreNotResources()
{
    const double sr = 48000.0;
    const std::string dir = scratchDir + "/again";
    makeDir (dir);
    const std::string take = dir + "/take.wav";
    writeTake (take, makeTake (sr, 2, 2.0, 81));

    auto first = RunPlan::build (baseRequest ({ take }, take));
    CHECK (SessionRunner::run (first).countOf (ItemResult::written) == 1, "first pass ran");
    const std::string out = dir + "/take-clean.wav";
    CHECK (existsUtf8 (out), "first pass produced its output");

    // second pass over everything that is now in the folder, exactly as a folder scan
    // would hand it over
    auto second = RunPlan::build (baseRequest ({ take, out }, take));
    int alreadyOutput = 0;
    for (const auto& i : second.allItems())
        if (i.state == ItemState::alreadyOutput) ++alreadyOutput;
    CHECK (alreadyOutput == 1, "the previous output is not treated as a source");
    SessionRunner::run (second);
    CHECK (! existsUtf8 (dir + "/take-clean-clean.wav"),
           "and no doubly-cleaned file is produced");

    // a different suffix is the deliberate way to redo a session at other settings
    auto redo = baseRequest ({ take }, take);
    redo.suffix = "-clean2";
    redo.params.reductionDb = 20.0f;
    auto rp = RunPlan::build (redo);
    CHECK (rp.readyCount() == 1, "changing the suffix allows a second pass");
    CHECK (SessionRunner::run (rp).countOf (ItemResult::written) == 1, "which runs");
    CHECK (existsUtf8 (dir + "/take-clean2.wav"), "and lands under the new name");
    CHECK (existsUtf8 (out), "leaving the first pass's output alone");
}

static void testDuplicateSourcesAreOneJob()
{
    const double sr = 48000.0;
    const std::string a = scratchDir + "/dup.wav";
    writeTake (a, makeTake (sr, 2, 2.0, 91));

    auto plan = RunPlan::build (baseRequest ({ a, a }, a));
    CHECK (plan.readyCount() == 1, "the same file twice is one job, not a collision");
    int dup = 0;
    for (const auto& i : plan.allItems())
        if (i.state == ItemState::duplicateSource) ++dup;
    CHECK (dup == 1, "and the repeat is reported rather than silently dropped");
    CHECK (SessionRunner::run (plan).countOf (ItemResult::written) == 1, "it runs once");
}

// The plan-level check refuses an existing output, so the commit path is never reached in
// a normal run - which means the guarantee that actually protects against a race is the
// one least likely to be exercised. Tested directly: the filesystem must refuse, not us.
static void testCommitRefusesToReplace()
{
    const std::string from = scratchDir + "/r_from.tmp";
    const std::string to   = scratchDir + "/r_to.wav";

    auto put = [] (const std::string& path, const char* text)
    {
        FILE* fp = openUtf8 (path, "wb");
        if (fp != nullptr) { std::fwrite (text, 1, std::strlen (text), fp); std::fclose (fp); }
    };

    put (from, "NEW");
    put (to, "ORIGINAL");
    CHECK (! renameNoReplace (from, to), "the rename refuses when the target exists");
    const auto kept = readBytes (to);
    CHECK (std::string (kept.begin(), kept.end()) == "ORIGINAL",
           "and the file that was already there is untouched");
    CHECK (existsUtf8 (from), "the source of a refused rename is left for the caller to clean");
    removeUtf8 (from); removeUtf8 (to);

    put (from, "NEW");
    CHECK (renameNoReplace (from, to), "and it succeeds when the target is absent");
    CHECK (! existsUtf8 (from), "the temporary is gone after a successful commit");
    removeUtf8 (to);
}

// A run that stops part-way must leave finished outputs and nothing else - no truncated
// file wearing a final name, and no temporary left behind.
static void testFailureLeavesNoPartialOutput()
{
    const double sr = 48000.0;
    const std::string good = scratchDir + "/f_good.wav";
    const std::string bad  = scratchDir + "/f_bad.wav";
    writeTake (good, makeTake (sr, 2, 2.0, 51));
    writeTake (bad,  makeTake (sr, 2, 2.0, 52));

    auto plan = RunPlan::build (baseRequest ({ good, bad }, good));
    CHECK (plan.readyCount() == 2, "both sources planned");

    // Truncate the second source after planning, so the run meets a file it cannot read
    // once it is already under way - the same shape as a disk or network failure mid-batch.
    {
        FILE* fp = openUtf8 (bad, "wb");
        CHECK (fp != nullptr, "could damage the second source");
        if (fp != nullptr) { std::fwrite ("RIFFxxxx", 1, 8, fp); std::fclose (fp); }
    }

    auto rep = SessionRunner::run (plan);
    CHECK (rep.countOf (ItemResult::written) == 1, "the healthy file still completes");
    CHECK (rep.countOf (ItemResult::failed) == 1, "the damaged one is reported as failed");

    CHECK (existsUtf8 (scratchDir + "/f_good-clean.wav"), "the finished output is there");
    CHECK (! existsUtf8 (scratchDir + "/f_bad-clean.wav"),
           "no final file exists for the item that failed");
    CHECK (! existsUtf8 (scratchDir + "/f_bad-clean.wav.part"),
           "and no temporary is left behind");
}

// Cancelling inside the DSP: the file being worked on leaves nothing, earlier ones stay.
static void testCancelDuringDspLeavesNothingBehind()
{
    const double sr = 48000.0;
    const std::string a = scratchDir + "/x_a.wav", b = scratchDir + "/x_b.wav";
    writeTake (a, makeTake (sr, 2, 2.0, 61));
    writeTake (b, makeTake (sr, 2, 6.0, 62));      // long enough to be interrupted

    auto plan = RunPlan::build (baseRequest ({ a, b }, a));
    int progressCalls = 0, callsInSecond = 0;
    bool reachedSecond = false;
    RunCallbacks cb;
    // Keyed on which file is reporting rather than on a call count: how many progress
    // callbacks a file produces depends on its length and the hop size, and a test that
    // depends on those silently stops testing anything when either changes.
    cb.onProgress = [&] (int, int, float, const std::string& src)
    {
        ++progressCalls;
        if (src == b) { reachedSecond = true; ++callsInSecond; }
    };
    cb.shouldContinue = [&] { return ! (reachedSecond && callsInSecond > 2); };

    auto rep = SessionRunner::run (plan, cb);
    CHECK (rep.cancelled, "the run reports that it was cancelled");
    CHECK (progressCalls > 0, "progress was reported from inside the DSP");
    CHECK (existsUtf8 (scratchDir + "/x_a-clean.wav"),
           "a file finished before the cancel is kept");
    CHECK (! existsUtf8 (scratchDir + "/x_b-clean.wav"),
           "the interrupted file produced no output");
    CHECK (! existsUtf8 (scratchDir + "/x_b-clean.wav.part"),
           "and left no temporary");
}

static void testPlanIsInspectableBeforeAnythingHappens()
{
    const double sr = 48000.0;
    const std::string a = scratchDir + "/p_a.wav";
    writeTake (a, makeTake (sr, 2, 2.0, 71));

    auto plan = RunPlan::build (baseRequest ({ a, scratchDir + "/does_not_exist.wav" }, a));
    CHECK (plan.readyCount() == 1, "a missing source is excluded, not fatal");
    CHECK (plan.ok(), "the run can still go ahead with what is left");

    bool sawUnreadable = false;
    for (const auto& i : plan.allItems())
    {
        if (i.state == ItemState::unreadable) sawUnreadable = true;
        if (i.state == ItemState::ready)
        {
            CHECK (! i.output.empty(), "a ready item knows its output path up front");
            CHECK (! i.temp.empty(), "and the temporary it will be written through");
            CHECK (! existsUtf8 (i.output), "planning wrote nothing");
        }
    }
    CHECK (sawUnreadable, "the unreadable source is reported rather than dropped");
}

// ---------------------------------------------------------------- main ----

static void removeTree (const std::string& dir);

int main()
{
    std::printf ("CleanVoice Session Clean S0 tests\n");
    scratchDir = uniqueScratch();
    removeTree (scratchDir);
    if (! makeDir (scratchDir))
    {
        std::printf ("FAIL: could not create scratch directory %s\n", scratchDir.c_str());
        return 1;
    }
    std::printf ("  scratch: %s\n", scratchDir.c_str());

    testPlanIsInspectableBeforeAnythingHappens();
    testBatchMatchesSingle();
    testNoStateLeakBetweenFiles();
    testSourcesAreNeverModified();
    testIncompatibleAndCollisionsAreCaught();
    testPreviousOutputsAreNotResources();
    testDuplicateSourcesAreOneJob();
    testExistingOutputIsNeverReplaced();
    testCommitRefusesToReplace();
    testFailureLeavesNoPartialOutput();
    testCancelDuringDspLeavesNothingBehind();

    removeTree (scratchDir);

    if (failures == 0) { std::printf ("ALL PASS\n"); return 0; }
    std::printf ("%d FAILURES\n", failures);
    return 1;
}

#ifdef _WIN32
 #include <windows.h>
static void removeTree (const std::string& dir)
{
    const std::wstring w = utf8ToWide (dir);
    WIN32_FIND_DATAW fd {};
    HANDLE h = FindFirstFileW ((w + L"\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE)
    {
        do
        {
            const std::wstring name = fd.cFileName;
            if (name == L"." || name == L"..") continue;
            const std::wstring full = w + L"\\" + name;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                removeTree (wideToUtf8 (full));
            else
                _wremove (full.c_str());
        } while (FindNextFileW (h, &fd));
        FindClose (h);
    }
    _wrmdir (w.c_str());
}
#else
 #include <dirent.h>
static void removeTree (const std::string& dir)
{
    DIR* d = opendir (dir.c_str());
    if (d != nullptr)
    {
        while (dirent* e = readdir (d))
        {
            const std::string name = e->d_name;
            if (name == "." || name == "..") continue;
            const std::string full = dir + "/" + name;
            struct stat st {};
            if (stat (full.c_str(), &st) == 0 && S_ISDIR (st.st_mode)) removeTree (full);
            else std::remove (full.c_str());
        }
        closedir (d);
    }
    rmdir (dir.c_str());
}
#endif
