# AGENTS.md

## Project Memory

This workspace is for a new Verilog ML flow with two main parts:

1. `ml-train/`
   - PyTorch training work lives here.
   - Use this folder for datasets, training scripts, model definitions, checkpoints, and evaluation code.

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
- No remote repository has been created yet.
- Do not assume an origin remote exists until the user creates or provides one.

## Working Conventions

- Keep PyTorch training code and generated training artifacts scoped to `ml-train/`.
- Keep hls4ml conversion scripts, generated HLS projects, and Verilog output scoped to `v-converter/`.
- Prefer small, reproducible scripts over notebook-only workflows.
- Avoid committing large generated artifacts, datasets, caches, and build outputs unless the user explicitly asks to track them.
- Before changing existing generated HLS or synthesis outputs, check whether they are source inputs, reference outputs, or disposable build artifacts.
