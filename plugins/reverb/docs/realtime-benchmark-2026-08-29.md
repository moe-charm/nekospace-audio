# Reverb realtime benchmark — 2026-08-29

This is the first full-duration Release measurement of the actual
`NekoSpaceReverbProcessor::processBlock` path. It is evidence for the binding CPU,
allocation, memory and finite-output gates in [validation.md](validation.md), not a
cross-machine performance claim.

## Reproduction

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target nsr_realtime_bench
build\plugins\reverb\Release\nsr_realtime_bench.exe
```

The benchmark runs 1,350,000 callbacks: 30 simulated minutes at 48 kHz with 64-sample
blocks. It hosts the shipping processor, keeps Room Body and the 16-line tail active,
and alternates two legal extreme parameter tuples every 257 blocks. It warms up first,
then measures each callback while the thread uses Windows `Pro Audio` MMCSS critical
priority and one logical-CPU affinity. Global scalar, array, aligned and nothrow
allocation/deallocation forms are counted after warm-up. Output samples are scanned for
NaN/Inf and Windows process private bytes are sampled before prepare, after prepare and
after the stress run.

The executable returns failure unless all of these hold:

- callback allocation and deallocation are both zero;
- every output sample is finite;
- p99 callback time is at most 10% of the 1.333 ms block budget;
- the longest callback is at most 25% of that budget.

## Reference machine and result

| Field | Recorded value |
| --- | --- |
| Git | `99cf6f7d89be`, clean |
| Build | Release, MSVC 19.44.35211.0 |
| CPU | AMD Ryzen 9 9950X, 16 cores / 32 logical processors |
| Format | 48 kHz, 64 samples, stereo |
| Median | 23.0 us / 1.725% of block budget |
| p99 | 37.3 us / **2.7975%** of block budget |
| Worst | 384.2 us / **28.815%** of block budget |
| Calls over 10% | 1,287 of 1,350,000 |
| Calls over 25% | **1 of 1,350,000** |
| Worst automation callback | 233.0 us / 17.475% |
| Worst callback type | steady, block 1,080,549 |
| Callback allocations / frees | **0 / 0** |
| Private bytes after prepare | 13,807,616 |
| Private bytes after stress | 13,705,216 (-102,400) |
| Finite output | yes |
| Formal result | **FAIL — worst-time gate only** |

The p99, allocation, memory and numerical-stability gates pass. The single 384.2 us
steady-block outlier exceeds the 333.3 us worst-time budget; therefore the binding CPU
gate does not pass. It was not a parameter-update block, so this run does not identify
automation or coefficient rebuilding as its cause.

Two immediately preceding runs of the same processor and stress sequence also passed
p99/allocation/memory but exceeded the worst-time limit with a handful of steady-block
outliers. That makes silently discarding the longest call or declaring the clean run a
pass inappropriate. Before a public alpha, either identify and remove a DSP-side source
with a profiler/ETW trace, establish that the outliers are external scheduling latency
with a stronger measurement method and explicitly revise the contract, or reproduce a
clean pass under the existing contract. The threshold is not relaxed by this report.
