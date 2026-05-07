"""Convert raw workload log files into processed CSV delta files.

The processor scans data/raw for .log files and creates matching .csv files in
data/processed. Existing processed files are skipped unless --force is used.
"""

from __future__ import annotations

import argparse
import csv
from pathlib import Path


DEFAULT_RAW_DIR = Path(__file__).resolve().parent / "data" / "raw"
DEFAULT_PROCESSED_DIR = Path(__file__).resolve().parent / "data" / "processed"


def parse_pipe_row(line: str) -> list[str]:
    return [part.strip() for part in line.split("|")]


def is_separator(line: str) -> bool:
    stripped = line.strip()
    return bool(stripped) and all(char == "-" for char in stripped)


def parse_numeric_value(value: str) -> int | float:
    try:
        return int(value)
    except ValueError:
        return float(value)


def read_log(path: Path) -> tuple[list[str], list[list[int | float]]]:
    header: list[str] | None = None
    rows: list[list[int | float]] = []

    with path.open("r", encoding="utf-8") as raw_file:
        for line_number, line in enumerate(raw_file, start=1):
            stripped = line.strip()
            if not stripped or is_separator(stripped):
                continue

            if "|" not in stripped:
                continue

            columns = parse_pipe_row(stripped)
            if header is None:
                header = columns
                continue

            if len(columns) != len(header):
                raise ValueError(
                    f"{path}: line {line_number} has {len(columns)} columns, "
                    f"expected {len(header)}"
                )

            rows.append([parse_numeric_value(column) for column in columns])

    if header is None:
        raise ValueError(f"{path}: no pipe-delimited header found")

    return header, rows


def row_deltas(rows: list[list[int | float]]) -> list[list[int | float]]:
    return [
        [current - previous for current, previous in zip(rows[index], rows[index - 1])]
        for index in range(1, len(rows))
    ]


def write_csv(path: Path, header: list[str], rows: list[list[int | float]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as processed_file:
        writer = csv.writer(processed_file)
        writer.writerow(header)
        writer.writerows(rows)


def process_log(raw_path: Path, processed_path: Path) -> int:
    header, rows = read_log(raw_path)
    deltas = row_deltas(rows)
    write_csv(processed_path, header, deltas)
    return len(deltas)


def process_new_logs(raw_dir: Path, processed_dir: Path, force: bool = False) -> None:
    raw_files = sorted(raw_dir.glob("*.log"))
    if not raw_files:
        print(f"No .log files found in {raw_dir}")
        return

    processed_dir.mkdir(parents=True, exist_ok=True)

    converted_count = 0
    skipped_count = 0
    for raw_path in raw_files:
        processed_path = processed_dir / f"{raw_path.stem}.csv"
        if processed_path.exists() and not force:
            print(f"Skipping {raw_path.name}: {processed_path.name} already exists")
            skipped_count += 1
            continue

        row_count = process_log(raw_path, processed_path)
        print(f"Converted {raw_path.name} -> {processed_path.name} ({row_count} rows)")
        converted_count += 1

    print(f"Done. Converted: {converted_count}. Skipped: {skipped_count}.")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Convert raw pipe-delimited workload logs into CSV delta files."
    )
    parser.add_argument(
        "--raw-dir",
        type=Path,
        default=DEFAULT_RAW_DIR,
        help=f"Directory containing raw .log files. Default: {DEFAULT_RAW_DIR}",
    )
    parser.add_argument(
        "--processed-dir",
        type=Path,
        default=DEFAULT_PROCESSED_DIR,
        help=f"Directory for processed .csv files. Default: {DEFAULT_PROCESSED_DIR}",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Recreate processed CSV files even when they already exist.",
    )
    return parser


def main() -> None:
    args = build_parser().parse_args()
    process_new_logs(args.raw_dir, args.processed_dir, args.force)


if __name__ == "__main__":
    main()
