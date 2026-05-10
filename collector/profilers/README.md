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
```

The generated `.riscv` binaries print a pipe-delimited table compatible with
the training data processor. `Cycles`, `Instret`, and all HPM columns are
cumulative snapshots; downstream processing can compute per-IRQ deltas.
