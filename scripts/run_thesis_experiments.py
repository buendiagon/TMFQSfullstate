#!/usr/bin/env python3
"""Run the thesis experiment matrix and collect resource/error CSVs."""

from __future__ import annotations

import argparse
import csv
import json
import os
import re
import shlex
import shutil
import socket
import subprocess
import sys
import time
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Iterable


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_QUBITS = list(range(20, 29))
STRATEGIES = ("dense", "blosc", "zfp")
COMPRESSED_STRATEGIES = ("blosc", "zfp")
WORKLOADS = (
	"qft_pattern",
	"qft_high_entropy",
	"grover_single_n8",
	"grover_single_n2",
	"grover_single_7n8",
	"grover_multi",
	"grover_lazy_single_n8",
	"grover_lazy_single_n2",
	"grover_lazy_single_7n8",
	"grover_lazy_multi",
)
RESOURCE_FIELDS = (
	"run_id",
	"qubits",
	"workload",
	"family",
	"strategy",
	"marked_states",
	"zfp_mode",
	"zfp_precision",
	"chunk_states",
	"cache_slots",
	"nthreads",
	"exit_code",
	"time_sec",
	"max_memory_mb",
	"measured_state",
	"stdout_path",
	"stderr_path",
	"profile_path",
	"command",
)
ERROR_FIELDS = (
	"qubits",
	"workload",
	"strategy",
	"accuracy_kind",
	"exit_code",
	"bitwise_equal",
	"max_abs_amplitude_error",
	"max_abs_component_error",
	"rel_l2",
	"rmse_amplitude",
	"max_abs_probability_error",
	"total_probability_diff",
	"worst_state",
	"stdout_path",
	"stderr_path",
	"command",
)
SUMMARY_FIELDS = (
	"qubits",
	"scenario",
	"strategy",
	"avg_time_sec",
	"avg_max_memory_mb",
	"memory_saving_percent",
	"relative_time_x",
	"avg_error",
	"worst_error",
)


@dataclass(frozen=True)
class ResourceJob:
	qubits: int
	workload: str
	strategy: str

	@property
	def key(self) -> tuple[str, str, str]:
		return (str(self.qubits), self.workload, self.strategy)


@dataclass(frozen=True)
class ErrorJob:
	qubits: int
	workload: str
	strategy: str

	@property
	def key(self) -> tuple[str, str, str]:
		return (str(self.qubits), self.workload, self.strategy)


def parse_qubits(value: str) -> list[int]:
	if "-" in value:
		start_s, end_s = value.split("-", 1)
		start = int(start_s)
		end = int(end_s)
		if start > end:
			raise argparse.ArgumentTypeError("qubit range start must be <= end")
		return list(range(start, end + 1))
	return [int(part) for part in value.split(",") if part]


def ensure_parent(path: Path) -> None:
	path.parent.mkdir(parents=True, exist_ok=True)


def append_csv(path: Path, fields: Iterable[str], row: dict[str, object]) -> None:
	ensure_parent(path)
	write_header = not path.exists() or path.stat().st_size == 0
	with path.open("a", newline="") as handle:
		writer = csv.DictWriter(handle, fieldnames=list(fields), extrasaction="ignore")
		if write_header:
			writer.writeheader()
		writer.writerow(row)
		handle.flush()
		os.fsync(handle.fileno())


def read_completed(path: Path) -> set[tuple[str, str, str]]:
	if not path.exists():
		return set()
	with path.open(newline="") as handle:
		return {
			(row["qubits"], row["workload"], row["strategy"])
			for row in csv.DictReader(handle)
			if row.get("exit_code") == "0"
		}


def run_checked(args: list[str], cwd: Path) -> None:
	print("+ " + shlex.join(args), flush=True)
	subprocess.run(args, cwd=cwd, check=True)


def build_release(no_build: bool) -> None:
	if no_build:
		return
	run_checked(["cmake", "--build", "build/release", "-j", "4"], REPO_ROOT)


def git_metadata() -> dict[str, object]:
	def read(cmd: list[str]) -> str:
		try:
			return subprocess.check_output(cmd, cwd=REPO_ROOT, text=True).strip()
		except subprocess.SubprocessError:
			return ""

	return {
		"commit": read(["git", "rev-parse", "HEAD"]),
		"branch": read(["git", "rev-parse", "--abbrev-ref", "HEAD"]),
		"status_short": read(["git", "status", "--short"]),
	}


def write_manifest(out_dir: Path, args: argparse.Namespace, qubits: list[int]) -> None:
	manifest = {
		"started_at": datetime.now().isoformat(timespec="seconds"),
		"host": socket.gethostname(),
		"repo_root": str(REPO_ROOT),
		"qubits": qubits,
		"strategies": list(STRATEGIES),
		"workloads": list(WORKLOADS),
		"profiler": args.profiler,
		"psrecord_interval": args.psrecord_interval,
		"compression_defaults": {
			"blosc": {
				"chunkStates": 32768,
				"gateCacheSlots": 8,
				"clevel": 1,
				"nthreads": 4,
				"compcode": 1,
				"useShuffle": True,
			},
			"zfp": {
				"mode": "FixedPrecision",
				"precision": 40,
				"chunkStates": 32768,
				"gateCacheSlots": 8,
				"nthreads": 4,
			},
		},
		"git": git_metadata(),
	}
	(out_dir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")


def family_for(workload: str) -> str:
	if workload.startswith("qft"):
		return "qft"
	if workload.startswith("grover_lazy"):
		return "grover_lazy"
	return "grover"


def marked_states(qubits: int, workload: str) -> str:
	if workload.startswith("qft"):
		return ""
	n = 1 << qubits
	values = {
		"grover_single_n8": [n // 8],
		"grover_single_n2": [n // 2],
		"grover_single_7n8": [(7 * n) // 8],
		"grover_multi": [n // 8, n // 2, (7 * n) // 8],
		"grover_lazy_single_n8": [n // 8],
		"grover_lazy_single_n2": [n // 2],
		"grover_lazy_single_7n8": [(7 * n) // 8],
		"grover_lazy_multi": [n // 8, n // 2, (7 * n) // 8],
	}[workload]
	return ",".join(str(v) for v in values)


def binary_for(workload: str) -> str:
	if workload.startswith("qft"):
		return "qftG"
	if workload.startswith("grover_lazy"):
		return "groverLazy"
	return "grover"


def resource_command(build_dir: Path, job: ResourceJob) -> list[str]:
	binary = build_dir / "bin" / binary_for(job.workload)
	cmd = [str(binary), str(job.qubits)]
	if job.workload == "qft_pattern":
		cmd += ["--input-family", "pattern"]
	elif job.workload == "qft_high_entropy":
		cmd += ["--input-family", "random-phase"]
	else:
		cmd.append(marked_states(job.qubits, job.workload))

	cmd += ["--strategy", job.strategy]
	if job.strategy == "blosc":
		cmd += ["--chunk-states", "32768", "--cache-slots", "8", "--clevel", "1", "--nthreads", "4"]
	elif job.strategy == "zfp":
		cmd += [
			"--zfp-mode",
			"precision",
			"--zfp-precision",
			"40",
			"--zfp-chunk-states",
			"32768",
			"--zfp-cache-slots",
			"8",
			"--nthreads",
			"4",
		]
	return cmd


def compare_command(build_dir: Path, job: ErrorJob) -> list[str]:
	return [
		str(build_dir / "bin" / "experiment_compare"),
		"--qubits",
		str(job.qubits),
		"--workload",
		job.workload,
		"--strategy",
		job.strategy,
	]


def parse_measured_state(stdout: str) -> str:
	for pattern in (r"Grover search result:\s*(\d+)", r"Measured state \(k\):\s*(\d+)"):
		match = re.search(pattern, stdout)
		if match:
			return match.group(1)
	return ""


def parse_psrecord(log_path: Path) -> tuple[str, str]:
	if not log_path.exists():
		return "", ""
	elapsed_values = []
	memory_values = []
	plain_rows = []
	with log_path.open(newline="") as handle:
		for raw_line in handle:
			line = raw_line.strip()
			if not line:
				continue
			if line.startswith("#"):
				continue
			plain_rows.append(line)
			parts = line.split()
			if len(parts) >= 3:
				try:
					elapsed_values.append(float(parts[0]))
					memory_values.append(float(parts[2]))
					continue
				except ValueError:
					pass

	if elapsed_values or memory_values:
		elapsed = f"{elapsed_values[-1]:.6f}" if elapsed_values else ""
		max_memory = f"{max(memory_values):.6f}" if memory_values else ""
		return elapsed, max_memory

	rows: list[dict[str, str]] = []
	if plain_rows:
		reader = csv.DictReader(plain_rows)
		rows = list(reader)
	if not rows:
		return "", ""

	def find_value(row: dict[str, str], candidates: tuple[str, ...]) -> str:
		lowered = {key.lower().strip(): value for key, value in row.items()}
		for candidate in candidates:
			for key, value in lowered.items():
				if candidate in key:
					return value
		return ""

	elapsed = find_value(rows[-1], ("elapsed", "time"))
	memory_values = []
	for row in rows:
		value = find_value(row, ("real", "rss", "memory"))
		if value:
			try:
				memory_values.append(float(value))
			except ValueError:
				pass
	max_memory = max(memory_values) if memory_values else None
	return elapsed, "" if max_memory is None else f"{max_memory:.6f}"


def run_resource_with_time(command: list[str], stdout_path: Path, stderr_path: Path) -> tuple[int, str, str, str]:
	wrapped = ["/usr/bin/time", "-f", "TIME_SEC=%e MAX_RSS_KB=%M"] + command
	with stdout_path.open("w") as stdout_handle, stderr_path.open("w") as stderr_handle:
		proc = subprocess.run(wrapped, cwd=REPO_ROOT, stdout=stdout_handle, stderr=stderr_handle)
	stdout = stdout_path.read_text(errors="replace")
	stderr = stderr_path.read_text(errors="replace")
	time_match = re.search(r"TIME_SEC=([0-9.]+)", stderr)
	rss_match = re.search(r"MAX_RSS_KB=(\d+)", stderr)
	time_sec = time_match.group(1) if time_match else ""
	max_memory_mb = f"{float(rss_match.group(1)) / 1024.0:.6f}" if rss_match else ""
	return proc.returncode, time_sec, max_memory_mb, stdout


def run_resource_with_psrecord(
	command: list[str],
	profile_path: Path,
	stdout_path: Path,
	stderr_path: Path,
	interval: float,
) -> tuple[int, str, str, str]:
	psrecord = shutil.which("psrecord")
	if not psrecord:
		raise RuntimeError("psrecord is not installed; rerun with --profiler time for /usr/bin/time fallback")
	wrapped = [
		psrecord,
		shlex.join(command),
		"--interval",
		str(interval),
		"--log",
		str(profile_path),
	]
	start = time.perf_counter()
	with stdout_path.open("w") as stdout_handle, stderr_path.open("w") as stderr_handle:
		proc = subprocess.run(wrapped, cwd=REPO_ROOT, stdout=stdout_handle, stderr=stderr_handle)
	elapsed_wall = time.perf_counter() - start
	stdout = stdout_path.read_text(errors="replace")
	time_sec, max_memory_mb = parse_psrecord(profile_path)
	if not max_memory_mb:
		return run_resource_with_time(command, stdout_path, stderr_path)
	if not time_sec:
		time_sec = f"{elapsed_wall:.6f}"
	return proc.returncode, time_sec, max_memory_mb, stdout


def run_resource_job(job: ResourceJob, out_dir: Path, build_dir: Path, profiler: str, interval: float) -> dict[str, object]:
	run_id = f"q{job.qubits}_{job.workload}_{job.strategy}"
	stdout_path = out_dir / "logs" / f"{run_id}.stdout.log"
	stderr_path = out_dir / "logs" / f"{run_id}.stderr.log"
	profile_path = out_dir / "profiles" / f"{run_id}.csv"
	ensure_parent(stdout_path)
	ensure_parent(profile_path)
	command = resource_command(build_dir, job)
	print(f"[resource] {run_id}", flush=True)
	if profiler == "psrecord":
		exit_code, time_sec, max_memory_mb, stdout = run_resource_with_psrecord(
			command,
			profile_path,
			stdout_path,
			stderr_path,
			interval,
		)
	else:
		exit_code, time_sec, max_memory_mb, stdout = run_resource_with_time(command, stdout_path, stderr_path)
	return {
		"run_id": run_id,
		"qubits": job.qubits,
		"workload": job.workload,
		"family": family_for(job.workload),
		"strategy": job.strategy,
		"marked_states": marked_states(job.qubits, job.workload),
		"zfp_mode": "FixedPrecision" if job.strategy == "zfp" else "",
		"zfp_precision": 40 if job.strategy == "zfp" else "",
		"chunk_states": 32768 if job.strategy in ("blosc", "zfp") else "",
		"cache_slots": 8 if job.strategy in ("blosc", "zfp") else "",
		"nthreads": 4 if job.strategy in ("blosc", "zfp") else "",
		"exit_code": exit_code,
		"time_sec": time_sec,
		"max_memory_mb": max_memory_mb,
		"measured_state": parse_measured_state(stdout),
		"stdout_path": str(stdout_path),
		"stderr_path": str(stderr_path),
		"profile_path": str(profile_path) if profiler == "psrecord" else "",
		"command": shlex.join(command),
	}


def parse_compare_stdout(stdout: str) -> dict[str, str]:
	lines = [line for line in stdout.splitlines() if line.strip()]
	if len(lines) < 2:
		return {}
	reader = csv.DictReader(lines[-2:])
	return next(reader, {})


def run_error_job(job: ErrorJob, out_dir: Path, build_dir: Path) -> dict[str, object]:
	run_id = f"q{job.qubits}_{job.workload}_{job.strategy}_error"
	stdout_path = out_dir / "logs" / f"{run_id}.stdout.log"
	stderr_path = out_dir / "logs" / f"{run_id}.stderr.log"
	ensure_parent(stdout_path)
	command = compare_command(build_dir, job)
	print(f"[error] {run_id}", flush=True)
	with stdout_path.open("w") as stdout_handle, stderr_path.open("w") as stderr_handle:
		proc = subprocess.run(command, cwd=REPO_ROOT, stdout=stdout_handle, stderr=stderr_handle)
	stdout = stdout_path.read_text(errors="replace")
	parsed = parse_compare_stdout(stdout) if proc.returncode == 0 else {}
	row: dict[str, object] = {
		"qubits": job.qubits,
		"workload": job.workload,
		"strategy": job.strategy,
		"accuracy_kind": "lossless" if job.strategy == "blosc" else "lossy",
		"exit_code": proc.returncode,
		"stdout_path": str(stdout_path),
		"stderr_path": str(stderr_path),
		"command": shlex.join(command),
	}
	for field in ERROR_FIELDS:
		if field not in row:
			row[field] = parsed.get(field, "")
	return row


def dense_error_row(qubits: int, workload: str, out_dir: Path) -> dict[str, object]:
	return {
		"qubits": qubits,
		"workload": workload,
		"strategy": "dense",
		"accuracy_kind": "reference",
		"exit_code": 0,
		"bitwise_equal": "true",
		"max_abs_amplitude_error": 0,
		"max_abs_component_error": 0,
		"rel_l2": 0,
		"rmse_amplitude": 0,
		"max_abs_probability_error": 0,
		"total_probability_diff": 0,
		"worst_state": 0,
		"stdout_path": "",
		"stderr_path": "",
		"command": "reference row",
	}


def read_rows(path: Path) -> list[dict[str, str]]:
	if not path.exists():
		return []
	with path.open(newline="") as handle:
		return list(csv.DictReader(handle))


def write_rows(path: Path, fields: Iterable[str], rows: list[dict[str, object]]) -> None:
	ensure_parent(path)
	with path.open("w", newline="") as handle:
		writer = csv.DictWriter(handle, fieldnames=list(fields), extrasaction="ignore")
		writer.writeheader()
		writer.writerows(rows)


def generate_combined(out_dir: Path) -> None:
	resource_rows = read_rows(out_dir / "runs.csv")
	error_rows = read_rows(out_dir / "errors.csv")
	errors_by_key = {(row["qubits"], row["workload"], row["strategy"]): row for row in error_rows}
	combined = []
	for row in resource_rows:
		error = errors_by_key.get((row["qubits"], row["workload"], row["strategy"]), {})
		merged = dict(row)
		for key in ERROR_FIELDS:
			if key not in ("qubits", "workload", "strategy", "stdout_path", "stderr_path", "command"):
				merged[key] = error.get(key, "")
		combined.append(merged)
	write_rows(out_dir / "combined.csv", list(RESOURCE_FIELDS) + [f for f in ERROR_FIELDS if f not in RESOURCE_FIELDS], combined)


def scenario_for_summary(workload: str) -> str:
	if workload.startswith("grover_lazy_single"):
		return "grover_lazy_single"
	if workload.startswith("grover_single"):
		return "grover_single"
	return workload


def generate_summary(out_dir: Path) -> None:
	rows = read_rows(out_dir / "combined.csv")
	groups: dict[tuple[str, str, str], list[dict[str, str]]] = {}
	for row in rows:
		if row.get("exit_code") != "0":
			continue
		key = (row["qubits"], scenario_for_summary(row["workload"]), row["strategy"])
		groups.setdefault(key, []).append(row)

	summary = []
	for key, group_rows in sorted(groups.items(), key=lambda item: (int(item[0][0]), item[0][1], item[0][2])):
		qubits, scenario, strategy = key
		if scenario in ("grover_single", "grover_lazy_single") and len(group_rows) < 3:
			continue
		times = [float(row["time_sec"]) for row in group_rows if row.get("time_sec")]
		memories = [float(row["max_memory_mb"]) for row in group_rows if row.get("max_memory_mb")]
		errors = [float(row.get("max_abs_amplitude_error") or 0.0) for row in group_rows]
		if not times or not memories:
			continue
		dense_group = groups.get((qubits, scenario, "dense"), [])
		dense_times = [float(row["time_sec"]) for row in dense_group if row.get("time_sec")]
		dense_memories = [float(row["max_memory_mb"]) for row in dense_group if row.get("max_memory_mb")]
		dense_time = sum(dense_times) / len(dense_times) if dense_times else None
		dense_memory = sum(dense_memories) / len(dense_memories) if dense_memories else None
		avg_time = sum(times) / len(times)
		avg_memory = sum(memories) / len(memories)
		summary.append({
			"qubits": qubits,
			"scenario": scenario,
			"strategy": strategy,
			"avg_time_sec": f"{avg_time:.6f}",
			"avg_max_memory_mb": f"{avg_memory:.6f}",
			"memory_saving_percent": "" if dense_memory is None else f"{100.0 * (1.0 - avg_memory / dense_memory):.6f}",
			"relative_time_x": "" if dense_time is None or dense_time == 0.0 else f"{avg_time / dense_time:.6f}",
			"avg_error": f"{sum(errors) / len(errors):.17g}" if errors else "",
			"worst_error": f"{max(errors):.17g}" if errors else "",
		})
	write_rows(out_dir / "summary.csv", SUMMARY_FIELDS, summary)


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--qubits", type=parse_qubits, default=DEFAULT_QUBITS, help="Qubit range like 20-28 or CSV like 20,21")
	parser.add_argument("--output-root", default="results/thesis_experiments")
	parser.add_argument("--output-dir", default="")
	parser.add_argument("--build-dir", default="build/release")
	parser.add_argument("--no-build", action="store_true")
	parser.add_argument("--profiler", choices=("psrecord", "time"), default="psrecord")
	parser.add_argument("--psrecord-interval", type=float, default=1.0)
	parser.add_argument("--stop-on-failure", action="store_true")
	parser.add_argument("--skip-errors", action="store_true")
	args = parser.parse_args()

	qubits = args.qubits
	out_dir = Path(args.output_dir) if args.output_dir else REPO_ROOT / args.output_root / datetime.now().strftime("%Y%m%d-%H%M%S")
	if not out_dir.is_absolute():
		out_dir = REPO_ROOT / out_dir
	out_dir.mkdir(parents=True, exist_ok=True)
	build_dir = Path(args.build_dir)
	if not build_dir.is_absolute():
		build_dir = REPO_ROOT / build_dir

	build_release(args.no_build)
	write_manifest(out_dir, args, qubits)

	runs_csv = out_dir / "runs.csv"
	errors_csv = out_dir / "errors.csv"
	completed_resources = read_completed(runs_csv)
	completed_errors = read_completed(errors_csv)

	for qubit in qubits:
		for workload in WORKLOADS:
			for strategy in STRATEGIES:
				job = ResourceJob(qubit, workload, strategy)
				if job.key not in completed_resources:
					row = run_resource_job(job, out_dir, build_dir, args.profiler, args.psrecord_interval)
					append_csv(runs_csv, RESOURCE_FIELDS, row)
					if row["exit_code"] != 0 and args.stop_on_failure:
						return int(row["exit_code"])
					if row["exit_code"] == 0:
						completed_resources.add(job.key)

				if args.skip_errors:
					continue
				if strategy == "dense":
					if job.key not in completed_errors:
						append_csv(errors_csv, ERROR_FIELDS, dense_error_row(qubit, workload, out_dir))
						completed_errors.add(job.key)
					continue
				error_job = ErrorJob(qubit, workload, strategy)
				if error_job.key not in completed_errors:
					error_row = run_error_job(error_job, out_dir, build_dir)
					append_csv(errors_csv, ERROR_FIELDS, error_row)
					if error_row["exit_code"] != 0 and args.stop_on_failure:
						return int(error_row["exit_code"])
					if error_row["exit_code"] == 0:
						completed_errors.add(error_job.key)

			generate_combined(out_dir)
			generate_summary(out_dir)

	generate_combined(out_dir)
	generate_summary(out_dir)
	print(f"Experiment outputs: {out_dir}", flush=True)
	return 0


if __name__ == "__main__":
	sys.exit(main())
