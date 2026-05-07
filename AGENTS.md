# AGENTS.md

## Project Memory

This workspace is for a new Verilog ML flow with two main parts:

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
- `ml-train/data/`, `ml-train/checkpoints/`, and `ml-train/logs/` are ignored by Git by default; commit datasets or run artifacts only if the user explicitly asks.
- Keep hls4ml conversion scripts, generated HLS projects, and Verilog output scoped to `v-converter/`.
- Prefer small, reproducible scripts over notebook-only workflows.
- Avoid committing large generated artifacts, datasets, caches, and build outputs unless the user explicitly asks to track them.
- Before changing existing generated HLS or synthesis outputs, check whether they are source inputs, reference outputs, or disposable build artifacts.
