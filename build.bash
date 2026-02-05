#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
PREFIX="/usr/local/hakoniwa"

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
  -DHAKO_PDU_ENDPOINT_PREFIX="${PREFIX}"

cmake --build "${BUILD_DIR}"
