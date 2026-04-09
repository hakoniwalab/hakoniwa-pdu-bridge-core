#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
INSTALL_PREFIX="$(cd "${SCRIPT_DIR}/.." && pwd)"
DEV_BIN_PATH="${ROOT_DIR}/build/hakoniwa-pdu-web-bridge"
INSTALL_BIN_PATH="${SCRIPT_DIR}/hakoniwa-pdu-web-bridge"
DEV_CONFIG_ROOT="${ROOT_DIR}/config/web_bridge"
INSTALL_CONFIG_ROOT="${INSTALL_PREFIX}/share/hakoniwa-pdu-bridge/config/web_bridge"
BIN_PATH=""
DEFAULT_CONFIG_ROOT=""
LOG_DIR="${PWD}/logs"
TIMESTAMP="$(date +"%Y%m%d-%H%M%S")"

has_config_root_arg=false
for arg in "$@"; do
  if [[ "${arg}" == "--config-root" ]]; then
    has_config_root_arg=true
    break
  fi
done

if [[ -x "${INSTALL_BIN_PATH}" && -d "${INSTALL_CONFIG_ROOT}" ]]; then
  BIN_PATH="${INSTALL_BIN_PATH}"
  DEFAULT_CONFIG_ROOT="${INSTALL_CONFIG_ROOT}"
elif [[ -x "${DEV_BIN_PATH}" && -d "${DEV_CONFIG_ROOT}" ]]; then
  BIN_PATH="${DEV_BIN_PATH}"
  DEFAULT_CONFIG_ROOT="${DEV_CONFIG_ROOT}"
else
  echo "[run-web-bridge] binary not found in install or development layout" >&2
  echo "[run-web-bridge] checked install binary: ${INSTALL_BIN_PATH}" >&2
  echo "[run-web-bridge] checked development binary: ${DEV_BIN_PATH}" >&2
  echo "[run-web-bridge] build it first: cmake -S . -B build && cmake --build build --target hakoniwa-pdu-web-bridge" >&2
  exit 1
fi

mkdir -p "${LOG_DIR}"

LOG_PATH="${LOG_DIR}/web-bridge-${TIMESTAMP}.log"
LATEST_LINK="${LOG_DIR}/web-bridge-latest.log"

CMD=("${BIN_PATH}")
if [[ "${has_config_root_arg}" == false ]]; then
  CMD+=("--config-root" "${DEFAULT_CONFIG_ROOT}")
fi
CMD+=("$@")

{
  echo "[run-web-bridge] started at ${TIMESTAMP}"
  echo "[run-web-bridge] log file: ${LOG_PATH}"
  echo "[run-web-bridge] command: ${CMD[*]}"
  "${CMD[@]}"
} 2>&1 | tee "${LOG_PATH}"

ln -sfn "$(basename "${LOG_PATH}")" "${LATEST_LINK}"
