#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
collector_dir="$(cd "${script_dir}/.." && pwd)"
repo_dir="$(cd "${collector_dir}/.." && pwd)"

uart_tsi="${collector_dir}/tools/uart_tsi"
tty="/dev/ttyUSB1"
binary_dir="${collector_dir}/profilers"
output_dir="${repo_dir}/ml-train/data/raw"
label=""
timeout_seconds=""
dry_run=0
use_timestamp=1
sleep_seconds=5
wait_for_reset=0
declare -a benchmarks=()
declare -a extra_uart_args=()

usage() {
  cat <<'USAGE'
Usage:
  collector/scripts/collect_profiles.sh [options] [benchmark ...]

Examples:
  collector/scripts/collect_profiles.sh multiply
  collector/scripts/collect_profiles.sh --tty /dev/ttyUSB1 multiply median
  collector/scripts/collect_profiles.sh --label events0-28 multiply
  collector/scripts/collect_profiles.sh --output-dir collector/raw-runs --timeout 300 all
  collector/scripts/collect_profiles.sh --wait-for-reset all

Options:
  -t, --tty PATH          Serial device passed as +tty=PATH. Default: /dev/ttyUSB1
  -u, --uart-tsi PATH     uart_tsi executable. Default: collector/tools/uart_tsi
  -b, --binary-dir DIR    Directory containing *.riscv binaries. Default: collector/profilers
  -o, --output-dir DIR    Directory for captured .log files. Default: ml-train/data/raw
  -l, --label TEXT        Add TEXT to output filenames, useful for event sweeps.
      --timestamp         Add YYYYmmdd_HHMMSS to output filenames. Default behavior.
      --no-timestamp      Write stable filenames such as multiply.log.
      --sleep SECONDS     Sleep before each run so the board can settle. Default: 5
      --wait-for-reset    Prompt for FPGA reset and wait for Enter before each run.
      --timeout SECONDS   Stop each run after SECONDS using timeout(1), if available.
      --dry-run           Print command plan without executing.
      --                 Remaining arguments are passed to uart_tsi before the binary.
  -h, --help              Show this help.

Benchmarks:
  Pass benchmark names without .riscv, paths to .riscv files, or "all".
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -t|--tty)
      tty="$2"
      shift 2
      ;;
    -u|--uart-tsi)
      uart_tsi="$2"
      shift 2
      ;;
    -b|--binary-dir)
      binary_dir="$2"
      shift 2
      ;;
    -o|--output-dir)
      output_dir="$2"
      shift 2
      ;;
    -l|--label)
      label="$2"
      shift 2
      ;;
    --timeout)
      timeout_seconds="$2"
      shift 2
      ;;
    --timestamp)
      use_timestamp=1
      shift
      ;;
    --no-timestamp)
      use_timestamp=0
      shift
      ;;
    --sleep)
      sleep_seconds="$2"
      shift 2
      ;;
    --wait-for-reset)
      wait_for_reset=1
      shift
      ;;
    --dry-run)
      dry_run=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    --)
      shift
      extra_uart_args+=("$@")
      break
      ;;
    -*)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
    *)
      benchmarks+=("$1")
      shift
      ;;
  esac
done

if [[ ${#benchmarks[@]} -eq 0 ]]; then
  benchmarks=(all)
fi

if [[ ! -x "${uart_tsi}" ]]; then
  echo "uart_tsi is not executable: ${uart_tsi}" >&2
  echo "Try: chmod +x ${uart_tsi}" >&2
  exit 1
fi

mkdir -p "${output_dir}"

resolve_benchmark() {
  local item="$1"
  if [[ "${item}" == *.riscv && -f "${item}" ]]; then
    printf '%s\n' "${item}"
  elif [[ "${item}" == *.riscv && -f "${binary_dir}/${item}" ]]; then
    printf '%s\n' "${binary_dir}/${item}"
  elif [[ -f "${binary_dir}/${item}.riscv" ]]; then
    printf '%s\n' "${binary_dir}/${item}.riscv"
  else
    echo "Benchmark binary not found: ${item}" >&2
    return 1
  fi
}

declare -a binaries=()
for benchmark in "${benchmarks[@]}"; do
  if [[ "${benchmark}" == "all" ]]; then
    while IFS= read -r binary; do
      binaries+=("${binary}")
    done < <(find "${binary_dir}" -maxdepth 1 -type f -name '*.riscv' | sort)
  else
    binaries+=("$(resolve_benchmark "${benchmark}")")
  fi
done

if [[ ${#binaries[@]} -eq 0 ]]; then
  echo "No .riscv binaries found in ${binary_dir}" >&2
  exit 1
fi

timestamp=""
if [[ "${use_timestamp}" -eq 1 ]]; then
  timestamp="$(date +%Y%m%d_%H%M%S)"
fi
run_index=0

for binary in "${binaries[@]}"; do
  run_index=$((run_index + 1))
  bench_name="$(basename "${binary}" .riscv)"
  suffix=""
  if [[ -n "${label}" ]]; then
    suffix="${label}"
  fi
  if [[ -n "${timestamp}" ]]; then
    if [[ -n "${suffix}" ]]; then
      suffix="${suffix}_${timestamp}"
    else
      suffix="${timestamp}"
    fi
  fi
  if [[ -n "${suffix}" ]]; then
    log_path="${output_dir}/${bench_name}_${suffix}.log"
  else
    log_path="${output_dir}/${bench_name}.log"
  fi

  cmd=("${uart_tsi}" "+tty=${tty}" "${extra_uart_args[@]}" "${binary}")
  if [[ -n "${timeout_seconds}" ]]; then
    cmd=(timeout "${timeout_seconds}" "${cmd[@]}")
  fi

  echo "[$run_index/${#binaries[@]}] ${bench_name}"
  echo "  binary: ${binary}"
  echo "  log:    ${log_path}"
  echo "  cmd:    ${cmd[*]}"

  if [[ "${dry_run}" -ne 1 ]]; then
    echo "  reset:  Press the FPGA reset button before this run."
    if [[ "${wait_for_reset}" -eq 1 ]]; then
      read -r -p "Press Enter after resetting the FPGA board..."
    elif [[ "${sleep_seconds}" != "0" ]]; then
      echo "  sleep:  Waiting ${sleep_seconds}s before launching uart_tsi..."
      sleep "${sleep_seconds}"
    fi
  fi

  if [[ "${dry_run}" -eq 1 ]]; then
    continue
  fi

  {
    echo "# benchmark=${bench_name}"
    echo "# binary=${binary}"
    echo "# tty=${tty}"
    echo "# reset_notice=Press FPGA reset before run"
    echo "# pre_run_sleep_seconds=${sleep_seconds}"
    echo "# wait_for_reset=${wait_for_reset}"
    echo "# command=${cmd[*]}"
    echo "# started=$(date --iso-8601=seconds)"
    set +e
    "${cmd[@]}"
    status=$?
    set -e
    echo "# finished=$(date --iso-8601=seconds)"
    echo "# exit_status=${status}"
    exit "${status}"
  } 2>&1 | tee "${log_path}"
done
