# Hardware Event Profilers

This directory contains benchmark entry points that wrap selected
`collector/benchmarks` programs with the RocketChip hardware performance
counter sampler.

The wrappers intentionally reuse the `setStats(1)` and `setStats(0)` regions
from the original riscv-tests programs. Those calls are redirected to
`profiler_start()` and `profiler_stop()`, so cache preallocation and result
verification are kept outside the sampled region.

Build on a machine with the RISC-V bare-metal toolchain:

```sh
make
```

Useful overrides:

```sh
make RISCV_PREFIX=riscv64-unknown-elf- XLEN=64
make HPM_PROFILER_INTERVAL=1000
make HPM_PROFILER_REPEATS=100
```

Event sweep controls:

```sh
# First 29 events, counters mhpmcounter3..31
make HPM_PROFILER_EVENT_OFFSET=0 HPM_PROFILER_EVENT_COUNT=29

# Remaining 6 events
make HPM_PROFILER_EVENT_OFFSET=29 HPM_PROFILER_EVENT_COUNT=6
```

Named build targets:

```sh
# Event-screening build: repeats=100, events 0..28, summary only
make events0-summary

# Event-screening build: repeats=100, events 29..34, summary only
make events29-summary

# Official ML data build, after selecting the meaningful event window
make ml-data ML_EVENT_OFFSET=0 ML_EVENT_COUNT=29
```

The `ml-data` target intentionally requires `ML_EVENT_OFFSET` and
`ML_EVENT_COUNT`; choose those after checking which event counters are useful
with the two summary targets. It uses `HPM_PROFILER_REPEATS=1000` and prints
all captured rows.

The profiler knows all 35 event masks currently defined in `collector/HPC.h`.
Only 29 HPM counters are available at once, so use `HPM_PROFILER_EVENT_OFFSET`
and `HPM_PROFILER_EVENT_COUNT` to sweep through the list. The printed header is
generated from the selected window, which makes it easier to identify counters
that stay at zero on your design.

Event order:

```text
0 Load, 1 Store, 2 Arith, 3 Branch, 4 Excpt, 5 AMO, 6 Sys,
7 JAL, 8 JALR, 9 Mul, 10 Div, 11 FLd, 12 FSt, 13 FAdd,
14 FMul, 15 FMadd, 16 FDiv, 17 FOth, 18 LdUse, 19 LngLat,
20 CSR, 21 ICBlk, 22 DCBlk, 23 BrMis, 24 TgtMis, 25 Flush,
26 Replay, 27 MDIntk, 28 FPIntk, 29 ICMiss, 30 DCMiss,
31 DCRel, 32 ITLBMiss, 33 DTLBMiss, 34 L2TLBMiss
```

Output speed controls:

```sh
# Print every 10th captured sample
make HPM_PROFILER_PRINT_STRIDE=10

# Print only the first 100 captured samples
make HPM_PROFILER_MAX_PRINT_SAMPLES=100

# Print only diagnostics and final event totals, no IRQ table
make HPM_PROFILER_SUMMARY_ONLY=1
```

Console output through bare-metal `printf`/HTIF is very slow on FPGA-emulated
systems because each character becomes host-visible I/O. For quick event
screening, prefer `HPM_PROFILER_SUMMARY_ONLY=1` or a larger
`HPM_PROFILER_PRINT_STRIDE`. For ML data collection, keep row output enabled
but cap the number of printed samples to the amount you actually need.

Automated collection through `uart_tsi`:

```sh
# From the repository root, after building profiler .riscv binaries on Linux
collector/scripts/collect_profiles.sh
collector/scripts/collect_profiles.sh --tty /dev/ttyUSB1 multiply
collector/scripts/collect_profiles.sh --tty /dev/ttyUSB1 multiply median qsort
collector/scripts/collect_profiles.sh --tty /dev/ttyUSB1 --label events0-28 all
```

The script uses `collector/tools/uart_tsi` by default and saves captured logs
under `ml-train/data/raw/`, which is ignored by Git. It accepts benchmark names
without `.riscv`, full paths to `.riscv` files, or `all` for every profiler
binary in this directory. With no benchmark arguments, it runs every `.riscv`
binary in this directory using `/dev/ttyUSB1`. By default, output filenames
include a timestamp so multiple runs are preserved separately. Use
`--no-timestamp` when you want stable filenames such as `multiply.log`.

The generated `.riscv` binaries print a pipe-delimited table compatible with
the training data processor. `Cycles`, `Instret`, and all HPM columns are
cumulative snapshots; downstream processing can compute per-IRQ deltas.

If `Total Interrupts Caught` is `0`, check the `Profiler Diagnostics` line.
`mtime_delta` is measured in CLINT timer ticks, not CPU cycles. If
`mtime_delta` is less than `interval`, the benchmark finished before the first
timer interrupt could fire; increase `HPM_PROFILER_REPEATS` or lower
`HPM_PROFILER_INTERVAL`.
