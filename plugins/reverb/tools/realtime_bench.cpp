// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "../src/plugin/PluginProcessor.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <new>
#include <vector>

#if defined (_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <avrt.h>
 #include <malloc.h>
 #include <psapi.h>
#endif

#ifndef NSR_GIT_COMMIT
 #define NSR_GIT_COMMIT "unknown"
#endif
#ifndef NSR_GIT_DIRTY
 #define NSR_GIT_DIRTY 1
#endif
#ifndef NSR_COMPILER_ID
 #define NSR_COMPILER_ID "unknown"
#endif
#ifndef NSR_COMPILER_VERSION
 #define NSR_COMPILER_VERSION "unknown"
#endif

namespace
{
std::atomic<std::size_t> allocationCount { 0 };
std::atomic<std::size_t> deallocationCount { 0 };
}

void* operator new (std::size_t size)
{
    ++allocationCount;
    if (void* memory = std::malloc (size)) return memory;
    throw std::bad_alloc();
}

void* operator new[] (std::size_t size) { return ::operator new (size); }

void* operator new (std::size_t size, const std::nothrow_t&) noexcept
{
    try { return ::operator new (size); }
    catch (...) { return nullptr; }
}

void* operator new[] (std::size_t size, const std::nothrow_t& tag) noexcept
{
    return ::operator new (size, tag);
}

void* operator new (std::size_t size, std::align_val_t alignment)
{
    ++allocationCount;
    void* memory = nullptr;
#if defined (_WIN32)
    memory = _aligned_malloc (size, static_cast<std::size_t> (alignment));
#else
    if (posix_memalign (&memory, static_cast<std::size_t> (alignment), size) != 0)
        memory = nullptr;
#endif
    if (memory != nullptr) return memory;
    throw std::bad_alloc();
}

void* operator new[] (std::size_t size, std::align_val_t alignment)
{
    return ::operator new (size, alignment);
}

void* operator new (std::size_t size, std::align_val_t alignment,
                    const std::nothrow_t&) noexcept
{
    try { return ::operator new (size, alignment); }
    catch (...) { return nullptr; }
}

void* operator new[] (std::size_t size, std::align_val_t alignment,
                      const std::nothrow_t& tag) noexcept
{
    return ::operator new (size, alignment, tag);
}

void operator delete (void* memory) noexcept
{
    ++deallocationCount;
    std::free (memory);
}

void operator delete[] (void* memory) noexcept { ::operator delete (memory); }
void operator delete (void* memory, std::size_t) noexcept { ::operator delete (memory); }
void operator delete[] (void* memory, std::size_t) noexcept { ::operator delete (memory); }
void operator delete (void* memory, const std::nothrow_t&) noexcept { ::operator delete (memory); }
void operator delete[] (void* memory, const std::nothrow_t&) noexcept { ::operator delete (memory); }

void operator delete (void* memory, std::align_val_t) noexcept
{
    ++deallocationCount;
#if defined (_WIN32)
    _aligned_free (memory);
#else
    std::free (memory);
#endif
}

void operator delete[] (void* memory, std::align_val_t alignment) noexcept
{
    ::operator delete (memory, alignment);
}

void operator delete (void* memory, std::size_t, std::align_val_t alignment) noexcept
{
    ::operator delete (memory, alignment);
}

void operator delete[] (void* memory, std::size_t, std::align_val_t alignment) noexcept
{
    ::operator delete (memory, alignment);
}

void operator delete (void* memory, std::align_val_t alignment,
                      const std::nothrow_t&) noexcept
{
    ::operator delete (memory, alignment);
}

void operator delete[] (void* memory, std::align_val_t alignment,
                        const std::nothrow_t&) noexcept
{
    ::operator delete (memory, alignment);
}

namespace
{
constexpr double referenceSampleRate = 48000.0;
constexpr int blockSize = 64;
constexpr std::size_t callbackCount = 1350000; // 30 minutes at 48 kHz / 64 samples.
constexpr std::size_t warmupCallbacks = 4096;

class AudioThreadPriority final
{
public:
    AudioThreadPriority() noexcept
    {
#if defined (_WIN32)
        handle = AvSetMmThreadCharacteristicsW (L"Pro Audio", &taskIndex);
        if (handle != nullptr)
            critical = AvSetMmThreadPriority (handle, AVRT_PRIORITY_CRITICAL) != FALSE;
        const auto processorCount = GetActiveProcessorCount (0);
        if (processorCount > 0 && processorCount <= sizeof (DWORD_PTR) * 8)
        {
            affinityMask = static_cast<DWORD_PTR> (1) << (processorCount - 1);
            previousAffinity = SetThreadAffinityMask (GetCurrentThread(), affinityMask);
        }
#endif
    }

    ~AudioThreadPriority()
    {
#if defined (_WIN32)
        if (previousAffinity != 0) SetThreadAffinityMask (GetCurrentThread(), previousAffinity);
        if (handle != nullptr) AvRevertMmThreadCharacteristics (handle);
#endif
    }

    bool enabled() const noexcept
    {
#if defined (_WIN32)
        return handle != nullptr && critical;
#else
        return false;
#endif
    }

    bool affinityEnabled() const noexcept
    {
#if defined (_WIN32)
        return previousAffinity != 0;
#else
        return false;
#endif
    }

private:
#if defined (_WIN32)
    DWORD taskIndex = 0;
    HANDLE handle = nullptr;
    bool critical = false;
    DWORD_PTR affinityMask = 0;
    DWORD_PTR previousAffinity = 0;
#endif
};

std::size_t privateBytes() noexcept
{
#if defined (_WIN32)
    PROCESS_MEMORY_COUNTERS_EX counters {};
    counters.cb = sizeof (counters);
    if (GetProcessMemoryInfo (GetCurrentProcess(),
                              reinterpret_cast<PROCESS_MEMORY_COUNTERS*> (&counters),
                              sizeof (counters)) != FALSE)
        return static_cast<std::size_t> (counters.PrivateUsage);
#endif
    return 0;
}

const char* processorIdentifier() noexcept
{
#if defined (_WIN32)
    static char value[256] {};
    const auto length = GetEnvironmentVariableA ("PROCESSOR_IDENTIFIER", value,
                                                  static_cast<DWORD> (sizeof (value)));
    if (length > 0 && length < sizeof (value)) return value;
#else
    if (const char* value = std::getenv ("PROCESSOR_IDENTIFIER")) return value;
#endif
    return "unknown";
}

void store (NekoSpaceReverbProcessor& processor, const char* id, float plainValue)
{
    if (auto* value = processor.apvts.getRawParameterValue (id))
        value->store (plainValue, std::memory_order_relaxed);
}

void applyStressTuple (NekoSpaceReverbProcessor& processor, bool alternate)
{
    // Both tuples are legal extremes. Room Body remains active, so the benchmark never earns
    // a cheap callback by bypassing either the early reflections or the 16-line tail.
    store (processor, nsr::pid::space, alternate ? 100.0f : 0.0f);
    store (processor, nsr::pid::decay, alternate ? 4.0f : 0.15f);
    store (processor, nsr::pid::bassTail, alternate ? 2.0f : 0.25f);
    store (processor, nsr::pid::airTail, alternate ? 0.25f : 2.0f);
    store (processor, nsr::pid::mix, 100.0f);
    store (processor, nsr::pid::distance, alternate ? 100.0f : 0.0f);
    store (processor, nsr::pid::definition, alternate ? 100.0f : 0.0f);
    store (processor, nsr::pid::preDelay, alternate ? 120.0f : 0.0f);
    store (processor, nsr::pid::wetMonoInput, alternate ? 1.0f : 0.0f);
    store (processor, nsr::pid::bypass, 0.0f);
}

double percentile (const std::vector<double>& sorted, double fraction)
{
    const auto index = static_cast<std::size_t> (
        std::ceil (fraction * static_cast<double> (sorted.size()))) - 1;
    return sorted[std::min (index, sorted.size() - 1)];
}

std::uint64_t cyclePercentile (const std::vector<std::uint64_t>& sorted, double fraction)
{
    const auto index = static_cast<std::size_t> (
        std::ceil (fraction * static_cast<double> (sorted.size()))) - 1;
    return sorted[std::min (index, sorted.size() - 1)];
}

bool queryCurrentThreadCycles (std::uint64_t& cycles) noexcept
{
#if defined (_WIN32)
    ULONG64 value = 0;
    if (QueryThreadCycleTime (GetCurrentThread(), &value) != FALSE)
    {
        cycles = static_cast<std::uint64_t> (value);
        return true;
    }
#endif
    cycles = 0;
    return false;
}

} // namespace

int main()
{
    std::vector<double> callbackMicroseconds (callbackCount);
    std::vector<std::uint64_t> callbackCycles (callbackCount);
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    std::array<float, blockSize> sourceLeft {}, sourceRight {};
    for (int sample = 0; sample < blockSize; ++sample)
    {
        sourceLeft[static_cast<std::size_t> (sample)] =
            0.17f * std::sin (0.071f * static_cast<float> (sample));
        sourceRight[static_cast<std::size_t> (sample)] =
            0.13f * std::cos (0.053f * static_cast<float> (sample));
    }

    NekoSpaceReverbProcessor processor;
    processor.setPlayConfigDetails (2, 2, referenceSampleRate, blockSize);
    const auto memoryBeforePrepare = privateBytes();
    applyStressTuple (processor, true);
    processor.prepareToPlay (referenceSampleRate, blockSize);
    processor.setAuditionMode (nsr::RoomBodyAuditionMode::roomBody);

    auto refill = [&]
    {
        std::memcpy (buffer.getWritePointer (0), sourceLeft.data(), sizeof (sourceLeft));
        std::memcpy (buffer.getWritePointer (1), sourceRight.data(), sizeof (sourceRight));
    };

    for (std::size_t block = 0; block < warmupCallbacks; ++block)
    {
        if ((block % 257) == 0) applyStressTuple (processor, ((block / 257) & 1) != 0);
        refill();
        processor.processBlock (buffer, midi);
    }

    const auto memoryAfterPrepare = privateBytes();
    AudioThreadPriority audioThreadPriority;
    const auto allocationsBefore = allocationCount.load (std::memory_order_relaxed);
    const auto deallocationsBefore = deallocationCount.load (std::memory_order_relaxed);
    bool finite = true;
    double worstSteadyMicroseconds = 0.0;
    double worstAutomationMicroseconds = 0.0;
    bool worstCallbackWasAutomation = false;
    std::size_t worstSteadyBlock = 0;
    std::size_t worstAutomationBlock = 0;
    std::size_t callbacksOverTenPercent = 0;
    std::size_t callbacksOverTwentyFivePercent = 0;
    std::size_t cycleQueryFailures = 0;
    std::size_t worstWallBlock = 0;
    std::size_t worstCycleBlock = 0;
    double worstWallMicroseconds = 0.0;
    double wallMicrosecondsAtWorstCycles = 0.0;
    std::uint64_t cyclesAtWorstWall = 0;
    std::uint64_t worstCycles = 0;
    const double budgetMicroseconds = 1000000.0 * blockSize / referenceSampleRate;

    for (std::size_t block = 0; block < callbackCount; ++block)
    {
        const bool automationBlock = (block % 257) == 0;
        if (automationBlock) applyStressTuple (processor, ((block / 257) & 1) != 0);
        refill();
        std::uint64_t cyclesBefore = 0;
        std::uint64_t cyclesAfter = 0;
        const bool cyclesBeforeValid = queryCurrentThreadCycles (cyclesBefore);
        const auto begin = std::chrono::steady_clock::now();
        processor.processBlock (buffer, midi);
        const auto end = std::chrono::steady_clock::now();
        const bool cyclesAfterValid = queryCurrentThreadCycles (cyclesAfter);
        callbackMicroseconds[block] =
            std::chrono::duration<double, std::micro> (end - begin).count();
        const bool cycleSampleValid = cyclesBeforeValid && cyclesAfterValid
                                   && cyclesAfter >= cyclesBefore;
        if (cycleSampleValid)
        {
            callbackCycles[block] = cyclesAfter - cyclesBefore;
            if (callbackCycles[block] > worstCycles)
            {
                worstCycles = callbackCycles[block];
                worstCycleBlock = block;
                wallMicrosecondsAtWorstCycles = callbackMicroseconds[block];
            }
        }
        else
        {
            ++cycleQueryFailures;
        }
        if (callbackMicroseconds[block] > worstWallMicroseconds)
        {
            worstWallMicroseconds = callbackMicroseconds[block];
            worstWallBlock = block;
            cyclesAtWorstWall = cycleSampleValid ? callbackCycles[block] : 0;
        }
        if (automationBlock)
        {
            if (callbackMicroseconds[block] > worstAutomationMicroseconds)
                worstAutomationBlock = block;
            worstAutomationMicroseconds = (std::max) (worstAutomationMicroseconds,
                                                       callbackMicroseconds[block]);
        }
        else
        {
            if (callbackMicroseconds[block] > worstSteadyMicroseconds)
                worstSteadyBlock = block;
            worstSteadyMicroseconds = (std::max) (worstSteadyMicroseconds,
                                                   callbackMicroseconds[block]);
        }
        if (callbackMicroseconds[block] > budgetMicroseconds * 0.10)
            ++callbacksOverTenPercent;
        if (callbackMicroseconds[block] > budgetMicroseconds * 0.25)
            ++callbacksOverTwentyFivePercent;
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < blockSize; ++sample)
                finite = finite && std::isfinite (buffer.getSample (channel, sample));
    }

    const auto allocationsAfter = allocationCount.load (std::memory_order_relaxed);
    const auto deallocationsAfter = deallocationCount.load (std::memory_order_relaxed);
    const auto memoryAfterStress = privateBytes();
    std::sort (callbackMicroseconds.begin(), callbackMicroseconds.end());

    std::vector<std::uint64_t> validCycleSamples;
    validCycleSamples.reserve (callbackCount - cycleQueryFailures);
    for (const auto cycles : callbackCycles)
        if (cycles != 0) validCycleSamples.push_back (cycles);
    std::sort (validCycleSamples.begin(), validCycleSamples.end());

    const double median = percentile (callbackMicroseconds, 0.50);
    const double p99 = percentile (callbackMicroseconds, 0.99);
    const double worst = callbackMicroseconds.back();
    worstCallbackWasAutomation = worstAutomationMicroseconds >= worstSteadyMicroseconds;
    const double p99Percent = 100.0 * p99 / budgetMicroseconds;
    const double worstPercent = 100.0 * worst / budgetMicroseconds;
    const bool cycleDiagnosticsAvailable = ! validCycleSamples.empty()
                                        && cycleQueryFailures == 0;
    const auto medianCycles = validCycleSamples.empty()
                            ? std::uint64_t { 0 }
                            : cyclePercentile (validCycleSamples, 0.50);
    const auto p99Cycles = validCycleSamples.empty()
                         ? std::uint64_t { 0 }
                         : cyclePercentile (validCycleSamples, 0.99);
    const double worstWallCycleRatio = medianCycles == 0
                                     ? 0.0
                                     : static_cast<double> (cyclesAtWorstWall)
                                         / static_cast<double> (medianCycles);
    const auto callbackAllocations = allocationsAfter - allocationsBefore;
    const auto callbackDeallocations = deallocationsAfter - deallocationsBefore;
    const bool passed = callbackAllocations == 0 && callbackDeallocations == 0 && finite
                     && p99Percent <= 10.0 && worstPercent <= 25.0;

    std::cout << std::fixed << std::setprecision (6)
              << "{\n"
              << "  \"schema_version\": 2,\n"
              << "  \"git_commit\": \"" << NSR_GIT_COMMIT << "\",\n"
              << "  \"git_dirty\": " << (NSR_GIT_DIRTY != 0 ? "true" : "false") << ",\n"
              << "  \"compiler\": \"" << NSR_COMPILER_ID << ' ' << NSR_COMPILER_VERSION << "\",\n"
              << "  \"processor\": \"" << processorIdentifier() << "\",\n"
              << "  \"pro_audio_mmcss\": "
              << (audioThreadPriority.enabled() ? "true" : "false") << ",\n"
              << "  \"single_cpu_affinity\": "
              << (audioThreadPriority.affinityEnabled() ? "true" : "false") << ",\n"
              << "  \"sample_rate\": " << referenceSampleRate << ",\n"
              << "  \"block_size\": " << blockSize << ",\n"
              << "  \"simulated_seconds\": "
              << (static_cast<double> (callbackCount * blockSize) / referenceSampleRate) << ",\n"
              << "  \"callbacks\": " << callbackCount << ",\n"
              << "  \"callback_budget_us\": " << budgetMicroseconds << ",\n"
              << "  \"median_us\": " << median << ",\n"
              << "  \"p99_us\": " << p99 << ",\n"
              << "  \"worst_us\": " << worst << ",\n"
              << "  \"worst_wall_block\": " << worstWallBlock << ",\n"
              << "  \"worst_steady_us\": " << worstSteadyMicroseconds << ",\n"
              << "  \"worst_steady_block\": " << worstSteadyBlock << ",\n"
              << "  \"worst_automation_us\": " << worstAutomationMicroseconds << ",\n"
              << "  \"worst_automation_block\": " << worstAutomationBlock << ",\n"
              << "  \"worst_callback_was_automation\": "
              << (worstCallbackWasAutomation ? "true" : "false") << ",\n"
              << "  \"callbacks_over_10_percent\": " << callbacksOverTenPercent << ",\n"
              << "  \"callbacks_over_25_percent\": " << callbacksOverTwentyFivePercent << ",\n"
              << "  \"p99_realtime_percent\": " << p99Percent << ",\n"
              << "  \"worst_realtime_percent\": " << worstPercent << ",\n"
              << "  \"thread_cycle_diagnostics_available\": "
              << (cycleDiagnosticsAvailable ? "true" : "false") << ",\n"
              << "  \"thread_cycle_query_failures\": " << cycleQueryFailures << ",\n"
              << "  \"median_thread_cycles\": " << medianCycles << ",\n"
              << "  \"p99_thread_cycles\": " << p99Cycles << ",\n"
              << "  \"worst_thread_cycles\": " << worstCycles << ",\n"
              << "  \"worst_thread_cycles_block\": " << worstCycleBlock << ",\n"
              << "  \"wall_us_at_worst_thread_cycles\": "
              << wallMicrosecondsAtWorstCycles << ",\n"
              << "  \"thread_cycles_at_worst_wall\": " << cyclesAtWorstWall << ",\n"
              << "  \"worst_wall_thread_cycle_ratio_to_median\": "
              << worstWallCycleRatio << ",\n"
              << "  \"callback_allocations\": " << callbackAllocations << ",\n"
              << "  \"callback_deallocations\": " << callbackDeallocations << ",\n"
              << "  \"private_bytes_before_prepare\": " << memoryBeforePrepare << ",\n"
              << "  \"private_bytes_after_prepare\": " << memoryAfterPrepare << ",\n"
              << "  \"private_bytes_after_stress\": " << memoryAfterStress << ",\n"
              << "  \"finite_output\": " << (finite ? "true" : "false") << ",\n"
              << "  \"passed\": " << (passed ? "true" : "false") << "\n"
              << "}\n";
    return passed ? 0 : 1;
}
