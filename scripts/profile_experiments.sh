#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROFILE_ROOT_DEFAULT="${ROOT_DIR}/profiling"
PROFILE_ROOT="${TMFQS_PROFILE_ROOT:-${PROFILE_ROOT_DEFAULT}}"
PSRECORD_BIN="${ROOT_DIR}/.venv/bin/psrecord"
DEFAULT_INTERVAL="0.1"
DEFAULT_PRESET="release"
MPLCONFIG_DIR="/tmp/qstest-psrecord-mplconfig"

usage() {
  cat <<'EOF'
Usage:
  scripts/profile_experiments.sh migrate-legacy
  scripts/profile_experiments.sh run [options]

Options for run:
  --preset <name>            Build preset directory under build/ (default: release)
  --algorithms <csv>         qft, grover, or both (default: qft,grover)
  --qft-families <csv>       pattern, random-phase (default: pattern,random-phase)
  --grover-families <csv>    single-target, multi-target (default: single-target,multi-target)
  --qubits <csv>             Qubit counts (default: 22,23,24)
  --strategies <csv>         dense,blosc,zfp,auto (default: dense,blosc,zfp,auto)
  --interval <seconds>       psrecord sampling interval (default: 0.1)
  --profile-root <path>      Output root directory (default: profiling/)
  --overwrite                Replace existing psrecord outputs
  --dry-run                  Print the planned runs without executing them

Examples:
  scripts/profile_experiments.sh migrate-legacy
  scripts/profile_experiments.sh run --algorithms qft --qft-families pattern,random-phase --qubits 22 --strategies dense,blosc
  scripts/profile_experiments.sh run --algorithms grover --grover-families multi-target --strategies zfp --qubits 22,23,24
EOF
}

split_csv() {
  local csv="$1"
  local -n output_ref="$2"
  IFS=',' read -r -a output_ref <<< "${csv}"
}

join_by() {
  local delimiter="$1"
  shift
  local first=1
  local item
  for item in "$@"; do
    if [[ ${first} -eq 1 ]]; then
      printf '%s' "${item}"
      first=0
    else
      printf '%s%s' "${delimiter}" "${item}"
    fi
  done
}

require_file() {
  local path="$1"
  if [[ ! -f "${path}" ]]; then
    printf 'Missing required file: %s\n' "${path}" >&2
    exit 1
  fi
}

ensure_prereqs() {
  require_file "${ROOT_DIR}/environment"
  require_file "${PSRECORD_BIN}"
}

wait_for_exec_target() {
  local pid="$1"
  local expected_exe="$2"
  local attempts=200
  local actual_exe=""

  while (( attempts > 0 )); do
    if [[ ! -e "/proc/${pid}/exe" ]]; then
      return 1
    fi
    actual_exe="$(readlink -f "/proc/${pid}/exe" 2>/dev/null || true)"
    if [[ "${actual_exe}" == "${expected_exe}" ]]; then
      return 0
    fi
    sleep 0.05
    attempts=$((attempts - 1))
  done

  printf 'Timed out waiting for pid %s to exec %s (saw %s)\n' "${pid}" "${expected_exe}" "${actual_exe}" >&2
  return 1
}

run_profile() {
  local output_dir="$1"
  local interval="$2"
  local overwrite="$3"
  shift 3
  local cmd=("$@")

  local stdout_file="${output_dir}/stdout.txt"
  local stderr_file="${output_dir}/stderr.txt"
  local metadata_file="${output_dir}/metadata.txt"
  local command_file="${output_dir}/command.txt"
  local psrecord_log="${output_dir}/psrecord.log"
  local psrecord_plot="${output_dir}/psrecord.png"
  local psrecord_stdout="${output_dir}/psrecord.stdout.txt"
  local psrecord_stderr="${output_dir}/psrecord.stderr.txt"
  local pid_file="${output_dir}/target.pid"

  if [[ -e "${psrecord_log}" && "${overwrite}" != "true" ]]; then
    printf 'Skipping existing run: %s\n' "${output_dir}"
    return 0
  fi

  mkdir -p "${output_dir}"
  rm -f \
    "${stdout_file}" \
    "${stderr_file}" \
    "${metadata_file}" \
    "${command_file}" \
    "${psrecord_log}" \
    "${psrecord_plot}" \
    "${psrecord_stdout}" \
    "${psrecord_stderr}" \
    "${pid_file}"

  printf '%q ' "${cmd[@]}" > "${command_file}"
  printf '\n' >> "${command_file}"

  local resolved_binary
  resolved_binary="$(readlink -f "${cmd[0]}")"
  if [[ ! -x "${resolved_binary}" ]]; then
    printf 'Binary is not executable: %s\n' "${resolved_binary}" >&2
    return 1
  fi

  (
    cd "${ROOT_DIR}"
    exec bash -lc 'source ./environment >/dev/null 2>&1 && exec ./scripts/exec_after_stop.sh "$@"' \
      bash "${pid_file}" "${cmd[@]}"
  ) >"${stdout_file}" 2>"${stderr_file}" &
  local launcher_pid=$!
  local target_pid=""
  local attempts=3000
  while (( attempts > 0 )); do
    if [[ -s "${pid_file}" ]]; then
      target_pid="$(<"${pid_file}")"
      break
    fi
    sleep 0.01
    attempts=$((attempts - 1))
  done
  if [[ -z "${target_pid}" ]]; then
    printf 'Timed out waiting for target pid file: %s\n' "${pid_file}" >&2
    if [[ -s "${stderr_file}" ]]; then
      printf 'Launcher stderr from %s:\n' "${stderr_file}" >&2
      sed -n '1,120p' "${stderr_file}" >&2
    fi
    wait "${launcher_pid}" || true
    return 1
  fi
  if [[ ! -e "/proc/${target_pid}" ]]; then
    printf 'Target pid %s exited before profiling could start\n' "${target_pid}" >&2
    wait "${launcher_pid}" || true
    return 1
  fi

  local tracked_exe
  tracked_exe="$(readlink -f "/proc/${target_pid}/exe" 2>/dev/null || true)"

  {
    printf 'binary=%s\n' "${resolved_binary}"
    printf 'launcher_pid=%s\n' "${launcher_pid}"
    printf 'tracked_pid=%s\n' "${target_pid}"
    printf 'tracked_exe_before_exec=%s\n' "${tracked_exe}"
    printf 'psrecord_interval=%s\n' "${interval}"
    printf 'started_at=%s\n' "$(date --iso-8601=seconds)"
  } > "${metadata_file}"

  mkdir -p "${MPLCONFIG_DIR}"
  MPLCONFIGDIR="${MPLCONFIG_DIR}" \
    "${PSRECORD_BIN}" \
      --include-children \
      --interval "${interval}" \
      --log "${psrecord_log}" \
      --plot "${psrecord_plot}" \
      "${target_pid}" \
      >"${psrecord_stdout}" \
      2>"${psrecord_stderr}" &
  local psrecord_pid=$!

  kill -CONT "${target_pid}"
  wait_for_exec_target "${target_pid}" "${resolved_binary}"
  printf 'tracked_exe=%s\n' "$(readlink -f "/proc/${target_pid}/exe" 2>/dev/null || true)" >> "${metadata_file}"

  local psrecord_status=0
  set +e
  wait "${psrecord_pid}"
  psrecord_status=$?
  set -e

  local target_status=0
  set +e
  wait "${launcher_pid}"
  target_status=$?
  set -e

  {
    printf 'psrecord_status=%s\n' "${psrecord_status}"
    printf 'target_status=%s\n' "${target_status}"
    printf 'finished_at=%s\n' "$(date --iso-8601=seconds)"
  } >> "${metadata_file}"

  if [[ ${psrecord_status} -ne 0 || ${target_status} -ne 0 ]]; then
    printf 'Run failed for %s\n' "${output_dir}" >&2
    return 1
  fi
}

migrate_one() {
  local source_file="$1"
  local dest_file="$2"
  local note_file="$3"

  if [[ ! -f "${source_file}" ]]; then
    return 0
  fi

  mkdir -p "$(dirname "${dest_file}")"
  mv "${source_file}" "${dest_file}"
  {
    printf 'migrated_from=%s\n' "${source_file}"
    printf 'migrated_at=%s\n' "$(date --iso-8601=seconds)"
  } > "${note_file}"
}

migrate_legacy() {
  local q
  local strategy

  for q in 22 23 24; do
    for strategy in dense blosc zfp; do
      migrate_one \
        "${PROFILE_ROOT}/qft/pattern/qftG_${strategy}_${q}.log" \
        "${PROFILE_ROOT}/qft/pattern/q${q}/${strategy}/legacy_psrecord.log" \
        "${PROFILE_ROOT}/qft/pattern/q${q}/${strategy}/legacy_migration.txt"
      migrate_one \
        "${PROFILE_ROOT}/qft/pattern/qftG_${strategy}_${q}.png" \
        "${PROFILE_ROOT}/qft/pattern/q${q}/${strategy}/legacy_psrecord.png" \
        "${PROFILE_ROOT}/qft/pattern/q${q}/${strategy}/legacy_migration.txt"

      migrate_one \
        "${PROFILE_ROOT}/grover/onemark/grover_${strategy}_${q}.log" \
        "${PROFILE_ROOT}/grover/single-target/marked_6/q${q}/${strategy}/legacy_psrecord.log" \
        "${PROFILE_ROOT}/grover/single-target/marked_6/q${q}/${strategy}/legacy_migration.txt"
      migrate_one \
        "${PROFILE_ROOT}/grover/onemark/grover_${strategy}_${q}.png" \
        "${PROFILE_ROOT}/grover/single-target/marked_6/q${q}/${strategy}/legacy_psrecord.png" \
        "${PROFILE_ROOT}/grover/single-target/marked_6/q${q}/${strategy}/legacy_migration.txt"
    done
  done

  migrate_one \
    "${PROFILE_ROOT}/grover/onemark/grover_zfp_22_fr.log" \
    "${PROFILE_ROOT}/grover/single-target/marked_6/q22/zfp_fr/legacy_psrecord.log" \
    "${PROFILE_ROOT}/grover/single-target/marked_6/q22/zfp_fr/legacy_migration.txt"
  migrate_one \
    "${PROFILE_ROOT}/grover/onemark/grover_zfp_22_fr.png" \
    "${PROFILE_ROOT}/grover/single-target/marked_6/q22/zfp_fr/legacy_psrecord.png" \
    "${PROFILE_ROOT}/grover/single-target/marked_6/q22/zfp_fr/legacy_migration.txt"

  rmdir --ignore-fail-on-non-empty "${PROFILE_ROOT}/qft/pattern" 2>/dev/null || true
  rmdir --ignore-fail-on-non-empty "${PROFILE_ROOT}/qft" 2>/dev/null || true
  rmdir --ignore-fail-on-non-empty "${PROFILE_ROOT}/grover/onemark" 2>/dev/null || true
  rmdir --ignore-fail-on-non-empty "${PROFILE_ROOT}/grover" 2>/dev/null || true
}

build_qft_runs() {
  local preset="$1"
  local family="$2"
  local qubit="$3"
  local strategy="$4"
  local binary="${ROOT_DIR}/build/${preset}/bin/qftG"
  local output_dir="${PROFILE_ROOT}/qft/${family}/q${qubit}/${strategy}"

  run_profile "${output_dir}" "${INTERVAL}" "${OVERWRITE}" \
    "${binary}" "${qubit}" --input-family "${family}" --strategy "${strategy}"
}

build_grover_single_runs() {
  local preset="$1"
  local qubit="$2"
  local strategy="$3"
  local binary="${ROOT_DIR}/build/${preset}/bin/grover"
  local state_count=$((1 << qubit))
  local states=($((state_count / 8)) $((state_count / 2)) $(((7 * state_count) / 8)))
  local state

  for state in "${states[@]}"; do
    run_profile \
      "${PROFILE_ROOT}/grover/single-target/marked_${state}/q${qubit}/${strategy}" \
      "${INTERVAL}" \
      "${OVERWRITE}" \
      "${binary}" "${qubit}" "${state}" --strategy "${strategy}"
  done
}

build_grover_multi_runs() {
  local preset="$1"
  local qubit="$2"
  local strategy="$3"
  local binary="${ROOT_DIR}/build/${preset}/bin/grover"
  local state_count=$((1 << qubit))
  local s1=$((state_count / 8))
  local s2=$((state_count / 2))
  local s3=$(((7 * state_count) / 8))
  local csv="${s1},${s2},${s3}"
  local case_name="marks_${s1}_${s2}_${s3}"

  run_profile \
    "${PROFILE_ROOT}/grover/multi-target/${case_name}/q${qubit}/${strategy}" \
    "${INTERVAL}" \
    "${OVERWRITE}" \
    "${binary}" "${qubit}" "${csv}" --strategy "${strategy}"
}

print_plan() {
  local algorithm family qubit strategy

  for algorithm in "${ALGORITHMS[@]}"; do
    if [[ "${algorithm}" == "qft" ]]; then
      for family in "${QFT_FAMILIES[@]}"; do
        for qubit in "${QUBITS[@]}"; do
          for strategy in "${STRATEGIES[@]}"; do
            printf 'qft family=%s qubits=%s strategy=%s\n' "${family}" "${qubit}" "${strategy}"
          done
        done
      done
    elif [[ "${algorithm}" == "grover" ]]; then
      for family in "${GROVER_FAMILIES[@]}"; do
        for qubit in "${QUBITS[@]}"; do
          for strategy in "${STRATEGIES[@]}"; do
            printf 'grover family=%s qubits=%s strategy=%s\n' "${family}" "${qubit}" "${strategy}"
          done
        done
      done
    else
      printf 'Unknown algorithm: %s\n' "${algorithm}" >&2
      return 1
    fi
  done
}

run_matrix() {
  local algorithm family qubit strategy

  for algorithm in "${ALGORITHMS[@]}"; do
    if [[ "${algorithm}" == "qft" ]]; then
      for family in "${QFT_FAMILIES[@]}"; do
        for qubit in "${QUBITS[@]}"; do
          for strategy in "${STRATEGIES[@]}"; do
            build_qft_runs "${PRESET}" "${family}" "${qubit}" "${strategy}"
          done
        done
      done
    elif [[ "${algorithm}" == "grover" ]]; then
      for family in "${GROVER_FAMILIES[@]}"; do
        for qubit in "${QUBITS[@]}"; do
          for strategy in "${STRATEGIES[@]}"; do
            if [[ "${family}" == "single-target" ]]; then
              build_grover_single_runs "${PRESET}" "${qubit}" "${strategy}"
            elif [[ "${family}" == "multi-target" ]]; then
              build_grover_multi_runs "${PRESET}" "${qubit}" "${strategy}"
            else
              printf 'Unknown grover family: %s\n' "${family}" >&2
              return 1
            fi
          done
        done
      done
    else
      printf 'Unknown algorithm: %s\n' "${algorithm}" >&2
      return 1
    fi
  done
}

COMMAND="${1:-}"
if [[ -z "${COMMAND}" ]]; then
  usage
  exit 1
fi
shift || true

PRESET="${DEFAULT_PRESET}"
INTERVAL="${DEFAULT_INTERVAL}"
OVERWRITE="false"
DRY_RUN="false"
ALGORITHMS=(qft grover)
QFT_FAMILIES=(pattern random-phase)
GROVER_FAMILIES=(single-target multi-target)
QUBITS=(22 23 24)
STRATEGIES=(dense blosc zfp auto)

case "${COMMAND}" in
  migrate-legacy)
    migrate_legacy
    ;;
  run)
    while [[ $# -gt 0 ]]; do
      case "$1" in
        --preset)
          PRESET="$2"
          shift 2
          ;;
        --algorithms)
          split_csv "$2" ALGORITHMS
          shift 2
          ;;
        --qft-families)
          split_csv "$2" QFT_FAMILIES
          shift 2
          ;;
        --grover-families)
          split_csv "$2" GROVER_FAMILIES
          shift 2
          ;;
        --qubits)
          split_csv "$2" QUBITS
          shift 2
          ;;
        --strategies)
          split_csv "$2" STRATEGIES
          shift 2
          ;;
        --interval)
          INTERVAL="$2"
          shift 2
          ;;
        --profile-root)
          PROFILE_ROOT="$2"
          shift 2
          ;;
        --overwrite)
          OVERWRITE="true"
          shift
          ;;
        --dry-run)
          DRY_RUN="true"
          shift
          ;;
        *)
          printf 'Unknown option: %s\n' "$1" >&2
          usage
          exit 1
          ;;
      esac
    done

    ensure_prereqs
    require_file "${ROOT_DIR}/build/${PRESET}/bin/qftG"
    require_file "${ROOT_DIR}/build/${PRESET}/bin/grover"

    if [[ "${DRY_RUN}" == "true" ]]; then
      print_plan
    else
      run_matrix
    fi
    ;;
  *)
    usage
    exit 1
    ;;
esac
