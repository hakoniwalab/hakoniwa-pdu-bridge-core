#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
BIN_PATH="${BUILD_DIR}/hakoniwa-pdu-web-bridge"
LOG_DIR="${ROOT_DIR}/logs"
TIMESTAMP="$(date +"%Y%m%d-%H%M%S")"
LOG_PATH="${LOG_DIR}/web-bridge-${TIMESTAMP}.log"
LATEST_LINK="${LOG_DIR}/web-bridge-latest.log"

mkdir -p "${LOG_DIR}"

if [[ ! -x "${BIN_PATH}" ]]; then
  echo "[run-web-bridge] binary not found: ${BIN_PATH}" >&2
  echo "[run-web-bridge] build it first: cmake -S . -B build && cmake --build build --target hakoniwa-pdu-web-bridge" >&2
  exit 1
fi

{
  echo "[run-web-bridge] started at ${TIMESTAMP}"
  echo "[run-web-bridge] log file: ${LOG_PATH}"
  echo "[run-web-bridge] command: ${BIN_PATH} $*"
  "${BIN_PATH}" "$@"
} 2>&1 | tee "${LOG_PATH}"

ln -sfn "$(basename "${LOG_PATH}")" "${LATEST_LINK}"
