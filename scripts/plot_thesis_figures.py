#!/usr/bin/env python3
"""Generate thesis-ready TMFQS runtime and RAM plots."""

from __future__ import annotations

import argparse
import csv
import math
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


STRATEGY_LABELS = {
    "dense": "Dense",
    "blosc": "Blosc",
    "zfp": "ZFP",
}

STRATEGY_COLORS = {
    "dense": "#1f77b4",
    "blosc": "#2ca02c",
    "zfp": "#d62728",
}

PLOT_GROUPS = [
    {
        "key": "qft_periodic",
        "title": "QFT periodica",
        "workloads": ["qft_pattern"],
    },
    {
        "key": "qft_high_entropy",
        "title": "QFT alta entropia",
        "workloads": ["qft_high_entropy"],
    },
    {
        "key": "grover_single",
        "title": "Grover un objetivo",
        "workloads": ["grover_single_n8", "grover_single_n2", "grover_single_7n8"],
    },
    {
        "key": "grover_multi",
        "title": "Grover multiples objetivos",
        "workloads": ["grover_multi"],
    },
    {
        "key": "grover_lazy_single",
        "title": "Grover lazy un objetivo",
        "workloads": [
            "grover_lazy_single_n8",
            "grover_lazy_single_n2",
            "grover_lazy_single_7n8",
        ],
    },
    {
        "key": "grover_lazy_multi",
        "title": "Grover lazy multiples objetivos",
        "workloads": ["grover_lazy_multi"],
    },
]

METRICS = [
    {
        "key": "time",
        "column": "time_sec",
        "title": "tiempo total",
        "ylabel": "Tiempo total (s)",
    },
    {
        "key": "ram",
        "column": "max_memory_mb",
        "title": "RAM maxima consumida",
        "ylabel": "Memoria maxima (MB)",
    },
]


# Two-sided 95% Student-t critical value for df = 4.  Each curve uses six
# measured qubit counts and two fitted parameters, so df = 6 - 2 = 4.
T_CRITICAL_95_DF4 = 2.7764451051977987


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--max-observed-qubit", default=25, type=int)
    parser.add_argument("--max-predicted-qubit", default=28, type=int)
    return parser.parse_args()


def read_rows(path: Path) -> list[dict[str, str]]:
    with path.open(newline="") as fh:
        rows = list(csv.DictReader(fh))
    return [row for row in rows if row.get("exit_code") == "0"]


def geometric_mean(values: list[float]) -> float:
    if not values:
        raise ValueError("cannot calculate geometric mean of empty values")
    return math.exp(sum(math.log(value) for value in values) / len(values))


def aggregate_by_qubit(
    rows: list[dict[str, str]],
    workloads: list[str],
    strategy: str,
    metric_column: str,
    max_observed_qubit: int,
) -> tuple[np.ndarray, np.ndarray]:
    grouped: dict[int, list[float]] = defaultdict(list)
    workload_set = set(workloads)

    for row in rows:
        if row["workload"] not in workload_set or row["strategy"] != strategy:
            continue
        qubits = int(row["qubits"])
        if qubits > max_observed_qubit:
            continue
        value = float(row[metric_column])
        if value <= 0:
            continue
        grouped[qubits].append(value)

    qubits = np.array(sorted(grouped), dtype=float)
    values = np.array([geometric_mean(grouped[int(q)]) for q in qubits], dtype=float)
    return qubits, values


def log_linear_prediction_interval(
    qubits: np.ndarray,
    values: np.ndarray,
    prediction_qubits: np.ndarray,
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    if len(qubits) < 3:
        raise ValueError("at least three points are required for a prediction interval")

    x = np.column_stack([np.ones_like(qubits), qubits])
    z = np.log(values)
    beta, *_ = np.linalg.lstsq(x, z, rcond=None)
    fitted = x @ beta
    residuals = z - fitted

    parameter_count = x.shape[1]
    degrees_of_freedom = len(qubits) - parameter_count
    sigma_squared = float(np.sum(residuals**2) / degrees_of_freedom)
    xtx_inv = np.linalg.inv(x.T @ x)

    x_pred = np.column_stack([np.ones_like(prediction_qubits), prediction_qubits])
    z_pred = x_pred @ beta
    leverage = np.einsum("ij,jk,ik->i", x_pred, xtx_inv, x_pred)
    se_prediction = np.sqrt(sigma_squared * (1.0 + leverage))
    interval_radius = T_CRITICAL_95_DF4 * se_prediction

    return (
        np.exp(z_pred),
        np.exp(z_pred - interval_radius),
        np.exp(z_pred + interval_radius),
    )


def configure_axes(ax: plt.Axes, title: str, ylabel: str) -> None:
    ax.set_title(title, fontsize=14, pad=10)
    ax.set_xlabel("Qubits (n)", fontsize=12)
    ax.set_ylabel(ylabel, fontsize=12)
    ax.set_yscale("log")
    ax.grid(True, which="major", axis="both", color="#d0d0d0", linewidth=0.8)
    ax.grid(True, which="minor", axis="y", color="#ececec", linewidth=0.5)
    ax.tick_params(axis="both", labelsize=10)


def plot_metric(
    rows: list[dict[str, str]],
    group: dict[str, object],
    metric: dict[str, str],
    output_dir: Path,
    max_observed_qubit: int,
    max_predicted_qubit: int,
) -> None:
    fig, ax = plt.subplots(figsize=(7.2, 4.5), dpi=180)
    prediction_qubits = np.arange(max_observed_qubit, max_predicted_qubit + 1, dtype=float)

    for strategy in ["dense", "blosc", "zfp"]:
        qubits, values = aggregate_by_qubit(
            rows,
            group["workloads"],
            strategy,
            metric["column"],
            max_observed_qubit,
        )
        if len(qubits) == 0:
            continue

        color = STRATEGY_COLORS[strategy]
        label = STRATEGY_LABELS[strategy]
        ax.plot(qubits, values, marker="o", linewidth=2.0, color=color, label=label)

        pred, lower, upper = log_linear_prediction_interval(qubits, values, prediction_qubits)
        ax.plot(
            prediction_qubits,
            pred,
            linestyle="--",
            linewidth=1.8,
            color=color,
            alpha=0.95,
            label=f"{label} proyeccion",
        )
        ax.fill_between(prediction_qubits, lower, upper, color=color, alpha=0.14, linewidth=0)

    ax.axvline(max_observed_qubit, color="#666666", linestyle=":", linewidth=1.1)
    configure_axes(ax, f"{group['title']}: {metric['title']}", metric["ylabel"])
    ax.set_xlim(19.8, max_predicted_qubit + 0.2)
    ax.legend(loc="best", fontsize=9, frameon=True, framealpha=0.92)
    fig.tight_layout()

    output_path = output_dir / f"tmfqs_{group['key']}_{metric['key']}.png"
    fig.savefig(output_path, bbox_inches="tight")
    plt.close(fig)


def main() -> None:
    args = parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    rows = read_rows(args.input)

    for group in PLOT_GROUPS:
        for metric in METRICS:
            plot_metric(
                rows,
                group,
                metric,
                args.output_dir,
                args.max_observed_qubit,
                args.max_predicted_qubit,
            )


if __name__ == "__main__":
    main()
