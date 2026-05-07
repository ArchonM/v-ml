# AGENTS.md

## Project Memory

This workspace is for a new Verilog ML flow with two main parts:

1. `ml-train/`
   - PyTorch training work lives here.
   - Use this folder for datasets, training scripts, model definitions, checkpoints, and evaluation code.
   - Raw workload logs are stored under `ml-train/data/raw/`.
   - Processed training data is generated under `ml-train/data/processed/`.
   - `ml-train/raw_data_processor.py` scans raw `.log` files and creates matching `.csv` files only when the processed version does not already exist.
   - The processor preserves the pipe-delimited log header as a CSV header and writes consecutive row deltas instead of raw cumulative counter values.

2. `v-converter/`
   - hls4ml conversion work lives here.
   - Use this folder to convert trained PyTorch models into HLS output and Verilog `.v` files.
   - Existing exploratory hls4ml artifacts and Arty A7 project files are currently under this folder.

## Environment

- Use the Anaconda environment named `verilog-ml-torch` for project commands.
- PyTorch and hls4ml are expected to already be installed in that environment.

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
- `ml-train/data/` is ignored by Git by default; commit raw or processed datasets only if the user explicitly asks.
- Keep hls4ml conversion scripts, generated HLS projects, and Verilog output scoped to `v-converter/`.
- Prefer small, reproducible scripts over notebook-only workflows.
- Avoid committing large generated artifacts, datasets, caches, and build outputs unless the user explicitly asks to track them.
- Before changing existing generated HLS or synthesis outputs, check whether they are source inputs, reference outputs, or disposable build artifacts.
