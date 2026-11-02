#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"

sync_and_build() {
    local src="${1:-}"
    local datetime=$(date +%Y%m%d_%H%M%S)
    local log_file="$(pwd)/logs/sync_and_build_${src}_all_${datetime}.log"

    # before the log file, so a wrong src or a missing tool is reported on the terminal
    if [ "${src}" != "4490" ] && [ "${src}" != "6690" ]; then
        echo "Usage: $0 --src <4490|6690>" >&2
        exit 1
    fi

    mkdir -p "$(dirname "${log_file}")"
    echo "Log: ${log_file}"
    exec > "${log_file}" 2>&1

    "${SCRIPT_DIR}/clean_source.sh" "${src}" || { echo "ERROR: step 'clean' FAILED"; exit 1; }
    "${SCRIPT_DIR}/sync_source.sh" "${src}" || { echo "ERROR: step 'sync' FAILED"; exit 1; }
    "${SCRIPT_DIR}/build_source.sh" "${src}" || { echo "ERROR: step 'build' FAILED"; exit 1; }

    echo "${src} all: clean, sync and build completed: SUCCESSFUL"
}

SRC=""
while [ $# -gt 0 ]; do
    case "$1" in
        --src)
            SRC="${2:-}"
            shift 2 || break ;;
        *)
            echo "Usage: $0 --src <4490|6690>" >&2
            exit 1 ;;
    esac
done

sync_and_build "${SRC}"
