#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROFILE_ROOT="${ROOT_DIR}/profiling/optimized"

usage() {
  cat <<'EOF'
Usage:
  scripts/profile_grover_optimized.sh [options]

This is a thin wrapper around scripts/profile_experiments.sh for the optimized
Grover runs only. Results are written under profiling/optimized/grover/.

Options:
  --preset <name>            Build preset directory under build/ (default: release)
  --grover-families <csv>    single-target, multi-target (default: single-target,multi-target)
  --qubits <csv>             Qubit counts (default: 22,23,24)
  --strategies <csv>         dense,blosc,zfp (default: dense,blosc,zfp)
  --interval <seconds>       psrecord sampling interval (default: 0.01 for this wrapper)
  --overwrite                Replace existing psrecord outputs
  --dry-run                  Print the planned runs without executing them

Examples:
  scripts/profile_grover_optimized.sh --dry-run
  scripts/profile_grover_optimized.sh --qubits 22 --strategies dense,blosc --overwrite
  scripts/profile_grover_optimized.sh --grover-families multi-target --strategies zfp --overwrite
EOF
}

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
  usage
  exit 0
fi

DEFAULT_INTERVAL_ARGS=(--interval 0.01)
DEFAULT_STRATEGY_ARGS=(--strategies dense,blosc,zfp)
for arg in "$@"; do
  if [[ "${arg}" == "--interval" ]]; then
    DEFAULT_INTERVAL_ARGS=()
  fi
  if [[ "${arg}" == "--strategies" ]]; then
    DEFAULT_STRATEGY_ARGS=()
  fi
done

exec "${ROOT_DIR}/scripts/profile_experiments.sh" \
  run \
  --algorithms grover \
  --profile-root "${PROFILE_ROOT}" \
  "${DEFAULT_INTERVAL_ARGS[@]}" \
  "${DEFAULT_STRATEGY_ARGS[@]}" \
  "$@"
