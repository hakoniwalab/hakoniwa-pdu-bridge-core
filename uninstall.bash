#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
MANIFEST="${BUILD_DIR}/install_manifest.txt"

if [[ ! -f "${MANIFEST}" ]]; then
  echo "install_manifest.txt not found. Run install.bash first."
  exit 1
fi

while IFS= read -r file; do
  if [[ -f "${file}" || -L "${file}" ]]; then
    rm -f "${file}"
  fi
done < "${MANIFEST}"

mapfile -t installed_dirs < <(sed 's#/*$##' "${MANIFEST}" | xargs -n1 dirname | awk 'NF' | sort -r | uniq)
for dir in "${installed_dirs[@]}"; do
  if [[ -d "${dir}" ]]; then
    rmdir "${dir}" 2>/dev/null || true
  fi
done
