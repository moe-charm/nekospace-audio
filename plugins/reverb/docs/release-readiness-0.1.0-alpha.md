# Reverb 0.1.0-alpha release readiness

Audit date: 2026-08-29. Candidate source: clean `bc0aaa0b84c6`.

## Decision

**NO-GO under the repository's current release contracts.** Do not create
`reverb-v0.1.0-alpha` yet.

The plug-in, validators, realtime path and distribution path are working. The stop is now
limited to two declared manual Phase 6/7 checks that do not yet have complete records. A
tag would freeze identity and host parameter meaning, so “alpha” is not used to bypass
those contracts.

## Gate table

| Area | Status | Evidence |
| --- | --- | --- |
| Product identity/version | PASS | `NkSp` + `Nksr`, bundle `audio.nekospace.reverb`, binary 0.1.0, independent `reverb-v*` tag route |
| Parameter contract | PASS | ten ordered IDs/ranges/defaults documented in [parameter-contract.md](parameter-contract.md) |
| State compatibility | PASS | formal `NekoSpaceReverbState` schema 1; schema-0 `NekoSpaceReverbPrototypeState` migration and unrelated-root rejection are tested |
| Release CTest | PASS | local clean Release 7/7 |
| Remote CI | PASS | GitHub Actions run `33251812095`: Binaural, CleanVoice and Reverb jobs all succeeded for `07385cc` |
| Steinberg Validator | PASS | VST3 SDK 3.8.1, `-e -l`: 537/537, exit 0 |
| pluginval | PASS | 1.0.4, strictness 10, `--repeat 3 --randomise`, fixed seed `0x7eef060`; configured Steinberg Validator exits 0 three times; no failed markers, final `SUCCESS` |
| Numerical/allocation/memory | PASS | 30-minute-equivalent actual-processor run: finite output, callback alloc/free 0/0, private memory after stress below post-prepare value |
| CPU p99 | PASS | clean `bc0aaa0` run: 28.7 us, 2.1525% of the 48 kHz/64-sample block budget |
| CPU scheduled-work maximum | PASS | callback with greatest thread-cycle delta was 59.8 us / 4.485%; contract limit 25% |
| CPU wall deadline | PASS | absolute maximum 990.7 us / 74.3025%; 0 callbacks exceeded the full 1.333 ms budget |
| Audio-thread lock/source audit | PASS | no mutex/lock primitive in Reverb source; explicit `new` is confined to editor/factory construction |
| Owner sound audition | PASS | current Room Body accepted as natural on 2026-08-28 |
| FL Studio basic smoke | PASS | VST3 loaded and processed audio |
| FL Studio declared matrix | **OPEN** | no complete retained record yet for fixed buffers on/off, save/reload, offline render, bypass, odd/changing blocks and fast automation |
| GUI high-DPI/accessibility | **OPEN** | normal Player layout/control operation checked; Phase 6 high-DPI and accessibility exit evidence is not complete |
| Package | PASS | clean candidate zip contains VST3, Standalone, Player, build identity, product README/changelog, AGPL and third-party notices; SHA-256 matches |
| Private media | PASS | tracked audio/video count 0; candidate zip media count 0 |
| Licensing | PASS | distributed code path is AGPLv3-or-later with compatible JUCE/VST3 dependencies and notices; development-only Remotion/data are absent |
| Public tag/release | PASS (absence) | no `reverb-v*` tag and no Reverb GitHub Release exist |

The CPU investigation is in
[realtime-benchmark-2026-08-29.md](realtime-benchmark-2026-08-29.md). Windows thread-cycle
diagnostics first exposed and led to removal of a real duplicate decay-fitting burst.
Three clean optimization runs then separated the remaining steady-block wall stalls from
scheduled DSP work without deleting the maximum. The final clean contract run passed with
zero cycle-query failures, allocation/free, non-finite samples or deadline misses.

## Remaining order

1. Run and record the full FL Studio matrix against the same release-candidate VST3.
2. Record high-DPI layouts and keyboard/screen-reader-accessible names, fixing any blocker.
3. Change the Reverb changelog date from `Unreleased`, rebuild all three binaries from one
   final clean commit, rerun CTest/pluginval/Steinberg Validator, and reproduce the zip.
4. Only after a fresh explicit publication decision, create `reverb-v0.1.0-alpha`. The
   workflow opens a draft; inspect its archive/checksum/notes before making it public.
