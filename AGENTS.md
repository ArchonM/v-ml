# AGENTS.md

## Project Memory

This workspace is for a new Verilog ML flow with three main parts:

1. `ml-train/`
   - PyTorch training work lives here.
   - Use this folder for datasets, training scripts, model definitions, checkpoints, and evaluation code.
   - Raw workload logs are stored under `ml-train/data/raw/`.
   - Processed training data is generated under `ml-train/data/processed/`.
   - `ml-train/raw_data_processor.py` scans raw `.log` files and creates matching `.csv` files only when the processed version does not already exist.
   - The processor preserves the pipe-delimited log header as a CSV header.
   - In processed CSVs, `IRQ` is kept from the current raw row because it is the time point. The remaining counter columns use consecutive row deltas instead of raw cumulative values.
   - `ml-train/train_sequence_classifier.py` is the scratch PyTorch training workflow.
   - The current starter model is a compact GRU sequence classifier for variable-length workload time series.
   - Each processed CSV is treated as one sequence sample; the filename stem is the label.
   - Default input features are `Cycles`, `Instructions`, `Loads`, `Stores`, and `Arithmetic`; `IRQ` is not used as a model feature by default.
   - Training checkpoints are saved under `ml-train/checkpoints/`.
   - Each training run writes a timestamped newline-delimited JSON log under `ml-train/logs/`, named with the run time and model name.

2. `v-converter/`
   - hls4ml conversion work lives here.
   - Use this folder to convert trained PyTorch models into HLS output and Verilog `.v` files.
   - Existing exploratory hls4ml artifacts and Arty A7 project files are currently under this folder.

3. `collector/`
   - Bare-metal RISC-V benchmark collection work lives here.
   - This folder stores example programs, imported riscv-tests benchmark sources, and hardware event profilers.
   - The target is a RocketChip bare-metal design emulated on Arty A7 100T / Chipyard-compatible systems.
   - `collector/benchmarks/` contains imported riscv-tests style benchmark sources and common bare-metal support code.
   - `collector/profilers/` contains profiled benchmark wrappers and the reusable hardware event profiler runtime.
   - `collector/tools/uart_tsi` is the checked-in host tool used to run `.riscv` binaries through the UART TSI bridge.
   - `collector/scripts/collect_profiles.sh` automates `uart_tsi` runs and records raw logs, defaulting to `ml-train/data/raw/`.
   - `collector/profilers/hpm_profiler.c` configures Rocket-style `mhpmevent` selectors, samples counters on machine timer interrupts, and prints pipe-delimited counter snapshots.
   - The profiler supports sweeping all event masks defined in `collector/HPC.h` with `HPM_PROFILER_EVENT_OFFSET` and `HPM_PROFILER_EVENT_COUNT`; only 29 HPM counters can be active in one binary.
   - Use `HPM_PROFILER_REPEATS` to keep small benchmarks running long enough for timer interrupts to fire.
   - Bare-metal profiler output can be very slow because the riscv-tests syscall path prints character-by-character through host I/O. Use `HPM_PROFILER_SUMMARY_ONLY`, `HPM_PROFILER_PRINT_STRIDE`, or `HPM_PROFILER_MAX_PRINT_SAMPLES` when screening events.

## Environment

- Use the Anaconda environment named `verilog-ml-torch` for project commands.
- PyTorch and hls4ml are expected to already be installed in that environment.
- The RISC-V bare-metal compiler for `collector/profilers/` is not expected to be available on this Windows machine; profiler builds are tested later on a Linux / Chipyard-compatible setup.

Typical activation on Windows PowerShell:

```powershell
conda activate verilog-ml-torch
```

If PowerShell profile loading emits an execution-policy warning, continue using `login: false` for shell commands when possible.

## Git

- This repository was initialized locally first.
- The GitHub remote is `https://github.com/ArchonM/v-ml.git`.
- The main branch is `main`.

## Working Conventions

- Keep PyTorch training code and generated training artifacts scoped to `ml-train/`.
- `ml-train/data/`, `ml-train/checkpoints/`, and `ml-train/logs/` are ignored by Git by default; commit datasets or run artifacts only if the user explicitly asks.
- Keep hls4ml conversion scripts, generated HLS projects, and Verilog output scoped to `v-converter/`.
- Keep bare-metal benchmark/profiler source code scoped to `collector/`.
- Do not commit generated `.riscv`, `.dump`, `.out`, synthesis, or FPGA run artifacts unless the user explicitly asks.
- Prefer compile-time profiler knobs in `collector/profilers/Makefile` over hardcoding event selections or output limits in benchmark wrappers.
- Prefer small, reproducible scripts over notebook-only workflows.
- Avoid committing large generated artifacts, datasets, caches, and build outputs unless the user explicitly asks to track them.
- Before changing existing generated HLS or synthesis outputs, check whether they are source inputs, reference outputs, or disposable build artifacts.
