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

The generated `.riscv` binaries print a pipe-delimited table compatible with
the training data processor. `Cycles`, `Instret`, and all HPM columns are
cumulative snapshots; downstream processing can compute per-IRQ deltas.

If `Total Interrupts Caught` is `0`, check the `Profiler Diagnostics` line.
`mtime_delta` is measured in CLINT timer ticks, not CPU cycles. If
`mtime_delta` is less than `interval`, the benchmark finished before the first
timer interrupt could fire; increase `HPM_PROFILER_REPEATS` or lower
`HPM_PROFILER_INTERVAL`.
