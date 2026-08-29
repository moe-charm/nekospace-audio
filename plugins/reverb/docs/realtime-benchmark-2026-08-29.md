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

The original schema-1 executable returned failure unless all of these held:

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
clean pass under the existing contract. The threshold was not relaxed by that first report.

## Cycle diagnosis and DSP fix

Schema 2 samples Windows `QueryThreadCycleTime` immediately around each measured callback.
The delta includes the two wall-clock reads as small fixed overhead, but excludes time when
the benchmark thread is not scheduled. Raw counts are relative diagnostic values only:
CPU frequency and hardware differences make conversion to microseconds or comparison with
another machine invalid.

The first diagnostic run found a real DSP-side burst. Its 910.3 us maximum callback also
used 8.45 times the median thread cycles and followed an extreme settings update. Source
inspection found that both identical Mid and Side 16-line networks independently ran the
same six-iteration nonlinear decay fit, while every fit evaluation rebuilt fixed 125 Hz,
1 kHz and 8 kHz filter responses. `b5529f0` computes those fixed responses during prepare,
reuses each Newton baseline evaluation, calculates the shared decay targets once and then
applies them to the independent Mid and Side signal states. This preserves separate audio
state and identical targets; the complete seven-test Release set passed.

Three clean `b5529f0` runs then produced:

| Metric | Run 1 | Run 2 | Run 3 |
| --- | ---: | ---: | ---: |
| Median wall time | 23.0 us | 23.2 us | 23.3 us |
| p99 / budget | 2.8800% | 2.8275% | 2.8725% |
| Worst automation | 171.3 us | 85.3 us | 242.1 us |
| Wall time at greatest thread-cycle delta | 315.0 us | 85.3 us | 293.5 us |
| Same, as block budget | 23.6250% | 6.3975% | 22.0125% |
| Absolute wall maximum | 984.9 us | 815.3 us | 1236.3 us |
| Absolute maximum / budget | 73.8675% | 61.1475% | 92.7225% |
| Cycles at wall maximum / median | 1.2009x | 1.7754x | 2.0847x |
| Full-deadline misses | 0 | 0 | 0 |
| Callback allocation / free | 0 / 0 | 0 / 0 | 0 / 0 |
| Finite output | yes | yes | yes |

The automation/DSP burst is removed: the callback with the greatest scheduled CPU work is
below the original 25% budget in every run. The remaining absolute maxima occur on steady
blocks whose cycle deltas are close to the ordinary distribution, which is evidence of
preemption rather than hidden coefficient reconstruction.

The binding reference-machine contract is therefore revised without deleting the maximum:

- p99 wall time at most 10% of the block budget;
- wall time of the callback with the greatest scheduled thread-cycle delta at most 25%;
- absolute wall time at most 100%, meaning zero audio deadlines missed;
- cycle diagnostics available with zero query failures;
- zero callback allocation/free and finite output.

This contract is Windows-reference-machine specific. A future cross-platform acceptance
runner needs an equivalent per-thread execution-time source or a separately reviewed gate.

A final clean schema-2 run at `bc0aaa0b84c6` exercised the executable's revised binding
result: p99 28.7 us / 2.1525%, greatest-cycle callback 59.8 us / 4.485%, absolute maximum
990.7 us / 74.3025%, zero full-budget misses, zero cycle-query failures, zero callback
allocation/free and finite output. The executable returned 0 with `"passed": true`.
