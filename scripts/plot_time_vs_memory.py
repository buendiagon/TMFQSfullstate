#!/usr/bin/env python3
"""Generate time vs memory trade-off scatter plots for thesis."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path

import matplotlib.lines as mlines
import matplotlib.patches as mpatches
import matplotlib.pyplot as plt


STRATEGY_COLORS = {
    "dense": "#1f77b4",
    "blosc": "#2ca02c",
    "zfp": "#d62728",
}

STRATEGY_LABELS = {
    "dense": "Dense",
    "blosc": "Blosc",
    "zfp": "ZFP",
}

EXPERIMENT_GROUPS = [
    {
        "key": "grover_single",
        "scenario": "grover_single",
        "label": "Grover un objetivo",
    },
    {
        "key": "qft_periodic",
        "scenario": "qft_pattern",
        "label": "QFT periódica",
    },
    {
        "key": "qft_high_entropy",
        "scenario": "qft_high_entropy",
        "label": "QFT alta entropía",
    },
]

# Sub-second runs are reported as time_sec=0; display them at this floor value
# so they appear on the log-scale axis (measurement resolution is 1 s).
TIME_FLOOR = 0.5

# Qubit counts to annotate with "n=X" labels (dense strategy only)
ANNOTATE_QUBITS = {20, 23, 25}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Plot time vs memory trade-off for compressed quantum simulation."
    )
    parser.add_argument(
        "--summary",
        type=Path,
        default=Path("results/thesis_experiments/manual-run/summary.csv"),
    )
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=Path("figures"),
    )
    return parser.parse_args()


def load_summary(path: Path) -> list[dict]:
    with path.open(newline="") as fh:
        return list(csv.DictReader(fh))


def collect_points(
    rows: list[dict], scenario: str, strategy: str
) -> list[tuple[int, float, float, bool]]:
    """Return (qubits, memory_mb, display_time, is_subsecond) for every row with valid memory.

    Sub-second runs (time_sec == 0) are kept and displayed at TIME_FLOOR.
    is_subsecond=True flags that the memory reading may be unreliable because
    psrecord cannot sample peak memory for runs that finish in under one second.
    """
    points = []
    for row in rows:
        if row["scenario"] != scenario or row["strategy"] != strategy:
            continue
        t = float(row["avg_time_sec"])
        m = float(row["avg_max_memory_mb"])
        q = int(row["qubits"])
        if m > 0:
            is_sub = t == 0
            points.append((q, m, t if t > 0 else TIME_FLOOR, is_sub))
    points.sort(key=lambda p: p[0])
    return points


def global_limits(rows: list[dict]) -> tuple[tuple[float, float], tuple[float, float]]:
    """Compute shared axis limits across all experiment groups."""
    all_mem, all_time = [], []
    for group in EXPERIMENT_GROUPS:
        for strategy in STRATEGY_COLORS:
            for _, m, t, _sub in collect_points(rows, group["scenario"], strategy):
                all_mem.append(m)
                all_time.append(t)
    mem_lo = min(all_mem) / 1.8
    mem_hi = max(all_mem) * 1.8
    time_lo = min(all_time) / 1.8
    time_hi = max(all_time) * 1.8
    return (mem_lo, mem_hi), (time_lo, time_hi)


def _plot_group_on_ax(ax: plt.Axes, rows: list[dict], group: dict) -> None:
    for strategy in ["dense", "blosc", "zfp"]:
        color = STRATEGY_COLORS[strategy]
        points = collect_points(rows, group["scenario"], strategy)
        if not points:
            continue

        qubits = [p[0] for p in points]
        mem = [p[1] for p in points]
        times = [p[2] for p in points]
        is_sub = [p[3] for p in points]
        sizes = [(q - 19) * 20 for q in qubits]

        # Trajectory line only through properly-measured points (t > 0).
        # Sub-second points have unreliable memory readings and connecting them
        # into the trajectory creates a misleading non-monotonic appearance.
        measured_mem = [m for m, s in zip(mem, is_sub) if not s]
        measured_times = [t for t, s in zip(times, is_sub) if not s]
        if len(measured_mem) >= 2:
            ax.plot(
                measured_mem, measured_times,
                color=color, linewidth=0.9, linestyle="--", alpha=0.45, zorder=1,
            )

        # Solid circles for measured points, open circles for sub-second floor points
        solid_mem = [m for m, s in zip(mem, is_sub) if not s]
        solid_times = [t for t, s in zip(times, is_sub) if not s]
        solid_sizes = [sz for sz, s in zip(sizes, is_sub) if not s]
        if solid_mem:
            ax.scatter(solid_mem, solid_times, s=solid_sizes, c=color,
                       marker="o", edgecolors="white", linewidths=0.4, alpha=0.88, zorder=2)

        floor_mem = [m for m, s in zip(mem, is_sub) if s]
        floor_times = [t for t, s in zip(times, is_sub) if s]
        floor_sizes = [sz for sz, s in zip(sizes, is_sub) if s]
        if floor_mem:
            ax.scatter(floor_mem, floor_times, s=floor_sizes, facecolors="none",
                       edgecolors=color, linewidths=1.2, alpha=0.85, zorder=2)

        if strategy == "dense":
            for q, m_val, t_val, sub in zip(qubits, mem, times, is_sub):
                if q in ANNOTATE_QUBITS:
                    ax.annotate(
                        f"n={q}",
                        xy=(m_val, t_val),
                        xytext=(5, 3),
                        textcoords="offset points",
                        fontsize=6.5,
                        color="#444444",
                        zorder=3,
                    )


def _configure_ax(
    ax: plt.Axes,
    title: str,
    xlim: tuple[float, float],
    ylim: tuple[float, float],
) -> None:
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlim(*xlim)
    ax.set_ylim(*ylim)
    ax.set_xlabel("Memoria máxima (MB)", fontsize=12)
    ax.set_ylabel("Tiempo total (s)", fontsize=12)
    ax.set_title(title, fontsize=13, pad=10)
    ax.grid(True, which="major", color="#d0d0d0", linewidth=0.8)
    ax.grid(True, which="minor", color="#ececec", linewidth=0.5)
    ax.tick_params(axis="both", labelsize=10)
    # Reference line marking the sub-second floor
    ax.axhline(
        TIME_FLOOR,
        color="#aaaaaa",
        linewidth=0.7,
        linestyle=":",
        zorder=0,
        label=f"< 1 s (piso)",
    )


def _strategy_legend_handles() -> list:
    handles = [
        mpatches.Patch(color=STRATEGY_COLORS[s], label=STRATEGY_LABELS[s])
        for s in ["dense", "blosc", "zfp"]
    ]
    handles.append(
        mlines.Line2D([], [], color="#888888", marker="o", linestyle="None",
                      markersize=7, markerfacecolor="none", markeredgewidth=1.2,
                      label="< 1 s (piso)")
    )
    return handles


def _size_legend_handles() -> list:
    return [
        mlines.Line2D(
            [],
            [],
            color="#888888",
            marker="o",
            linestyle="None",
            markersize=(q - 19) ** 0.5 * 3,
            label=f"n={q}",
        )
        for q in [20, 22, 24, 25]
    ]


def build_per_experiment_figure(
    rows: list[dict],
    group: dict,
    xlim: tuple[float, float],
    ylim: tuple[float, float],
) -> plt.Figure:
    fig, ax = plt.subplots(figsize=(7, 5), dpi=180)
    _plot_group_on_ax(ax, rows, group)
    _configure_ax(ax, f"Compromiso tiempo–memoria: {group['label']}", xlim, ylim)

    leg1 = ax.legend(
        handles=_strategy_legend_handles(),
        title="Estrategia",
        loc="upper left",
        fontsize=9,
        title_fontsize=9,
        frameon=True,
        framealpha=0.92,
    )
    ax.add_artist(leg1)
    ax.legend(
        handles=_size_legend_handles(),
        title="Qubits (tamaño)",
        loc="lower right",
        fontsize=8,
        title_fontsize=8,
        frameon=True,
        framealpha=0.88,
        ncol=2,
    )

    fig.tight_layout()
    return fig


def build_combined_figure(
    rows: list[dict],
    xlim: tuple[float, float],
    ylim: tuple[float, float],
) -> plt.Figure:
    fig, ax = plt.subplots(figsize=(8, 6), dpi=180)

    for group in EXPERIMENT_GROUPS:
        _plot_group_on_ax(ax, rows, group)

    _configure_ax(ax, "Compromiso tiempo–memoria por experimento y estrategia", xlim, ylim)

    experiment_handles = [
        mlines.Line2D(
            [],
            [],
            color="#555555",
            marker="o",
            linestyle="None",
            markersize=8,
            label=g["label"],
        )
        for g in EXPERIMENT_GROUPS
    ]

    leg1 = ax.legend(
        handles=_strategy_legend_handles(),
        title="Estrategia",
        loc="upper left",
        fontsize=9,
        title_fontsize=9,
        frameon=True,
        framealpha=0.92,
    )
    ax.add_artist(leg1)
    leg2 = ax.legend(
        handles=experiment_handles,
        title="Experimento",
        loc="lower right",
        fontsize=9,
        title_fontsize=9,
        frameon=True,
        framealpha=0.92,
    )
    ax.add_artist(leg2)
    ax.legend(
        handles=_size_legend_handles(),
        title="Qubits (tamaño)",
        loc="lower left",
        fontsize=8,
        title_fontsize=8,
        frameon=True,
        framealpha=0.88,
        ncol=2,
    )

    fig.tight_layout()
    return fig


def main() -> None:
    args = parse_args()
    rows = load_summary(args.summary)
    args.out_dir.mkdir(parents=True, exist_ok=True)

    xlim, ylim = global_limits(rows)

    fig = build_combined_figure(rows, xlim, ylim)
    out = args.out_dir / "time_vs_memory_combined.png"
    fig.savefig(out, bbox_inches="tight")
    plt.close(fig)
    print(f"Saved: {out}")

    for group in EXPERIMENT_GROUPS:
        fig = build_per_experiment_figure(rows, group, xlim, ylim)
        out = args.out_dir / f"time_vs_memory_{group['key']}.png"
        fig.savefig(out, bbox_inches="tight")
        plt.close(fig)
        print(f"Saved: {out}")


if __name__ == "__main__":
    main()
