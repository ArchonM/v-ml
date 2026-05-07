"""Train a scratch PyTorch sequence classifier for processed workload CSVs.

Each CSV in data/processed is treated as one variable-length sequence. The
filename stem is used as the label, so matrix_multiplication.csv becomes the
matrix_multiplication class.
"""

from __future__ import annotations

import argparse
import csv
import json
import random
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path

import torch
from torch import nn
from torch.nn.utils.rnn import pack_padded_sequence, pad_sequence
from torch.utils.data import DataLoader, Dataset


DEFAULT_PROCESSED_DIR = Path(__file__).resolve().parent / "data" / "processed"
DEFAULT_CHECKPOINT = Path(__file__).resolve().parent / "checkpoints" / "sequence_classifier.pt"
DEFAULT_LOG_DIR = Path(__file__).resolve().parent / "logs"
DEFAULT_FEATURE_COLUMNS = ["Cycles", "Instructions", "Loads", "Stores", "Arithmetic"]
MODEL_NAME = "gru_sequence_classifier"


@dataclass(frozen=True)
class SequenceSample:
    label: str
    source: Path
    values: torch.Tensor


class WorkloadSequenceDataset(Dataset[SequenceSample]):
    def __init__(self, samples: list[SequenceSample]) -> None:
        self.samples = samples

    def __len__(self) -> int:
        return len(self.samples)

    def __getitem__(self, index: int) -> SequenceSample:
        return self.samples[index]


class FeatureNormalizer:
    def __init__(self, mean: torch.Tensor, std: torch.Tensor) -> None:
        self.mean = mean
        self.std = std

    @classmethod
    def fit(cls, samples: list[SequenceSample]) -> "FeatureNormalizer":
        stacked = torch.cat([sample.values for sample in samples], dim=0)
        mean = stacked.mean(dim=0)
        std = stacked.std(dim=0)
        std = torch.where(std == 0, torch.ones_like(std), std)
        return cls(mean=mean, std=std)

    def transform(self, samples: list[SequenceSample]) -> list[SequenceSample]:
        return [
            SequenceSample(
                label=sample.label,
                source=sample.source,
                values=(sample.values - self.mean) / self.std,
            )
            for sample in samples
        ]

    def state_dict(self) -> dict[str, list[float]]:
        return {"mean": self.mean.tolist(), "std": self.std.tolist()}


class GRUClassifier(nn.Module):
    def __init__(
        self,
        input_size: int,
        hidden_size: int,
        num_layers: int,
        num_classes: int,
        dropout: float,
    ) -> None:
        super().__init__()
        gru_dropout = dropout if num_layers > 1 else 0.0
        self.encoder = nn.GRU(
            input_size=input_size,
            hidden_size=hidden_size,
            num_layers=num_layers,
            batch_first=True,
            dropout=gru_dropout,
        )
        self.classifier = nn.Sequential(
            nn.LayerNorm(hidden_size),
            nn.Dropout(dropout),
            nn.Linear(hidden_size, num_classes),
        )

    def forward(self, padded: torch.Tensor, lengths: torch.Tensor) -> torch.Tensor:
        packed = pack_padded_sequence(
            padded,
            lengths.cpu(),
            batch_first=True,
            enforce_sorted=False,
        )
        _, hidden = self.encoder(packed)
        final_hidden = hidden[-1]
        return self.classifier(final_hidden)


def read_csv_sequence(path: Path, feature_columns: list[str]) -> torch.Tensor:
    with path.open("r", encoding="utf-8", newline="") as csv_file:
        reader = csv.DictReader(csv_file)
        missing_columns = [column for column in feature_columns if column not in reader.fieldnames]
        if missing_columns:
            raise ValueError(f"{path}: missing columns: {', '.join(missing_columns)}")

        rows = [
            [float(row[column]) for column in feature_columns]
            for row in reader
            if any(row.values())
        ]

    if not rows:
        raise ValueError(f"{path}: no data rows found")

    return torch.tensor(rows, dtype=torch.float32)


def load_samples(processed_dir: Path, feature_columns: list[str]) -> list[SequenceSample]:
    samples: list[SequenceSample] = []
    for csv_path in sorted(processed_dir.glob("*.csv")):
        samples.append(
            SequenceSample(
                label=csv_path.stem,
                source=csv_path,
                values=read_csv_sequence(csv_path, feature_columns),
            )
        )

    if len(samples) < 2:
        raise ValueError(f"Need at least 2 processed CSV files in {processed_dir}")

    return samples


def split_samples(
    samples: list[SequenceSample],
    validation_fraction: float,
    seed: int,
) -> tuple[list[SequenceSample], list[SequenceSample]]:
    labels = {sample.label for sample in samples}
    if len(samples) <= len(labels):
        print(
            "Only one sequence per label was found; using all samples for training. "
            "Add repeated runs per workload before trusting validation accuracy."
        )
        return samples, []

    shuffled = samples[:]
    random.Random(seed).shuffle(shuffled)
    validation_count = max(1, round(len(shuffled) * validation_fraction))
    return shuffled[validation_count:], shuffled[:validation_count]


def make_label_maps(samples: list[SequenceSample]) -> tuple[dict[str, int], dict[int, str]]:
    labels = sorted({sample.label for sample in samples})
    label_to_index = {label: index for index, label in enumerate(labels)}
    index_to_label = {index: label for label, index in label_to_index.items()}
    return label_to_index, index_to_label


def collate_sequences(
    batch: list[SequenceSample],
    label_to_index: dict[str, int],
) -> tuple[torch.Tensor, torch.Tensor, torch.Tensor]:
    sequences = [sample.values for sample in batch]
    lengths = torch.tensor([sequence.shape[0] for sequence in sequences], dtype=torch.long)
    labels = torch.tensor([label_to_index[sample.label] for sample in batch], dtype=torch.long)
    return pad_sequence(sequences, batch_first=True), lengths, labels


def evaluate(
    model: nn.Module,
    loader: DataLoader[tuple[torch.Tensor, torch.Tensor, torch.Tensor]],
    device: torch.device,
) -> float:
    model.eval()
    correct = 0
    total = 0
    with torch.no_grad():
        for sequences, lengths, labels in loader:
            sequences = sequences.to(device)
            lengths = lengths.to(device)
            labels = labels.to(device)
            predictions = model(sequences, lengths).argmax(dim=1)
            correct += (predictions == labels).sum().item()
            total += labels.numel()
    return correct / total if total else 0.0


def build_training_log_path(log_dir: Path, model_name: str) -> Path:
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    return log_dir / f"{timestamp}_{model_name}.log"


def summarize_samples(samples: list[SequenceSample]) -> list[dict[str, str | int]]:
    return [
        {
            "file": str(sample.source),
            "label": sample.label,
            "sequence_length": sample.values.shape[0],
        }
        for sample in samples
    ]


def write_json_line(log_file, event: str, payload: dict[str, object]) -> None:
    log_file.write(json.dumps({"event": event, **payload}) + "\n")
    log_file.flush()


def train(args: argparse.Namespace) -> None:
    torch.manual_seed(args.seed)
    random.seed(args.seed)

    feature_columns = args.feature_columns.split(",")
    samples = load_samples(args.processed_dir, feature_columns)
    label_to_index, index_to_label = make_label_maps(samples)
    train_samples, validation_samples = split_samples(
        samples,
        validation_fraction=args.validation_fraction,
        seed=args.seed,
    )

    normalizer = FeatureNormalizer.fit(train_samples)
    train_samples = normalizer.transform(train_samples)
    validation_samples = normalizer.transform(validation_samples)

    collate_fn = lambda batch: collate_sequences(batch, label_to_index)
    train_loader = DataLoader(
        WorkloadSequenceDataset(train_samples),
        batch_size=args.batch_size,
        shuffle=True,
        collate_fn=collate_fn,
    )
    validation_loader = DataLoader(
        WorkloadSequenceDataset(validation_samples),
        batch_size=args.batch_size,
        shuffle=False,
        collate_fn=collate_fn,
    )

    device = torch.device("cuda" if torch.cuda.is_available() and not args.cpu else "cpu")
    model = GRUClassifier(
        input_size=len(feature_columns),
        hidden_size=args.hidden_size,
        num_layers=args.num_layers,
        num_classes=len(label_to_index),
        dropout=args.dropout,
    ).to(device)
    optimizer = torch.optim.AdamW(model.parameters(), lr=args.learning_rate)
    criterion = nn.CrossEntropyLoss()

    args.checkpoint.parent.mkdir(parents=True, exist_ok=True)
    args.log_dir.mkdir(parents=True, exist_ok=True)
    log_path = build_training_log_path(args.log_dir, MODEL_NAME)

    with log_path.open("w", encoding="utf-8") as log_file:
        write_json_line(
            log_file,
            "run_start",
            {
                "timestamp": datetime.now().isoformat(timespec="seconds"),
                "model_name": MODEL_NAME,
                "processed_dir": str(args.processed_dir),
                "checkpoint": str(args.checkpoint),
                "device": str(device),
                "seed": args.seed,
                "feature_columns": feature_columns,
                "label_to_index": label_to_index,
                "model_config": {
                    "input_size": len(feature_columns),
                    "hidden_size": args.hidden_size,
                    "num_layers": args.num_layers,
                    "num_classes": len(label_to_index),
                    "dropout": args.dropout,
                },
                "training_config": {
                    "epochs": args.epochs,
                    "batch_size": args.batch_size,
                    "learning_rate": args.learning_rate,
                    "validation_fraction": args.validation_fraction,
                    "report_every": args.report_every,
                },
                "dataset": {
                    "sample_count": len(samples),
                    "train_count": len(train_samples),
                    "validation_count": len(validation_samples),
                    "samples": summarize_samples(samples),
                    "train_files": [str(sample.source) for sample in train_samples],
                    "validation_files": [str(sample.source) for sample in validation_samples],
                },
                "normalizer": normalizer.state_dict(),
            },
        )

        for epoch in range(1, args.epochs + 1):
            model.train()
            total_loss = 0.0
            for sequences, lengths, labels in train_loader:
                sequences = sequences.to(device)
                lengths = lengths.to(device)
                labels = labels.to(device)

                optimizer.zero_grad()
                loss = criterion(model(sequences, lengths), labels)
                loss.backward()
                optimizer.step()
                total_loss += loss.item() * labels.numel()

            average_loss = total_loss / len(train_samples)
            train_accuracy = evaluate(model, train_loader, device)
            validation_accuracy = (
                evaluate(model, validation_loader, device) if validation_samples else None
            )
            write_json_line(
                log_file,
                "epoch",
                {
                    "epoch": epoch,
                    "loss": average_loss,
                    "train_accuracy": train_accuracy,
                    "validation_accuracy": validation_accuracy,
                },
            )

            if epoch == 1 or epoch % args.report_every == 0 or epoch == args.epochs:
                message = (
                    f"epoch={epoch:03d} loss={average_loss:.4f} "
                    f"train_acc={train_accuracy:.3f}"
                )
                if validation_accuracy is not None:
                    message += f" val_acc={validation_accuracy:.3f}"
                print(message)

    torch.save(
        {
            "model_state": model.state_dict(),
            "label_to_index": label_to_index,
            "index_to_label": index_to_label,
            "feature_columns": feature_columns,
            "normalizer": normalizer.state_dict(),
            "model_config": {
                "input_size": len(feature_columns),
                "hidden_size": args.hidden_size,
                "num_layers": args.num_layers,
                "num_classes": len(label_to_index),
                "dropout": args.dropout,
            },
        },
        args.checkpoint,
    )
    labels_path = args.checkpoint.with_suffix(".labels.json")
    labels_path.write_text(json.dumps(label_to_index, indent=2), encoding="utf-8")
    with log_path.open("a", encoding="utf-8") as log_file:
        write_json_line(
            log_file,
            "run_end",
            {
                "timestamp": datetime.now().isoformat(timespec="seconds"),
                "checkpoint": str(args.checkpoint),
                "labels_path": str(labels_path),
            },
        )
    print(f"Saved checkpoint to {args.checkpoint}")
    print(f"Saved training log to {log_path}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Train a scratch GRU classifier for processed workload sequences."
    )
    parser.add_argument("--processed-dir", type=Path, default=DEFAULT_PROCESSED_DIR)
    parser.add_argument("--checkpoint", type=Path, default=DEFAULT_CHECKPOINT)
    parser.add_argument("--log-dir", type=Path, default=DEFAULT_LOG_DIR)
    parser.add_argument(
        "--feature-columns",
        default=",".join(DEFAULT_FEATURE_COLUMNS),
        help="Comma-separated CSV columns to use as model inputs.",
    )
    parser.add_argument("--hidden-size", type=int, default=32)
    parser.add_argument("--num-layers", type=int, default=1)
    parser.add_argument("--dropout", type=float, default=0.1)
    parser.add_argument("--batch-size", type=int, default=4)
    parser.add_argument("--epochs", type=int, default=100)
    parser.add_argument("--learning-rate", type=float, default=1e-3)
    parser.add_argument("--validation-fraction", type=float, default=0.2)
    parser.add_argument("--report-every", type=int, default=10)
    parser.add_argument("--seed", type=int, default=7)
    parser.add_argument("--cpu", action="store_true", help="Force CPU even when CUDA is available.")
    return parser


def main() -> None:
    train(build_parser().parse_args())


if __name__ == "__main__":
    main()
