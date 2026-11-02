#!/bin/bash

set -euo pipefail

SYNC_ATTEMPTS=3

sync_source() {
   local src="${1:-}"
   local source_dir="${HOME}/Working/src/${src}-user"
   local sync_ok=false
   local attempt

   if [ ! -d "${source_dir}" ]; then
      echo "ERROR: source dir not found: ${source_dir}" >&2
      exit 1
   fi

   [ -d "${source_dir}/.repo" ] || { echo "ERROR: not a repo root: ${source_dir}" >&2; exit 1; }
   [ -x "${source_dir}/.repo/repo/repo" ] || { echo "ERROR: repo launcher not found: ${source_dir}/.repo/repo/repo" >&2; exit 1; }

   echo "[TRACKING] [Sync source] ${source_dir} ============="
   cd "${source_dir}"

   # --retry-fetches only retries a fetch, a failed checkout or force sync needs a new sync
   for attempt in $(seq 1 "${SYNC_ATTEMPTS}"); do
      echo "sync attempt ${attempt}/${SYNC_ATTEMPTS}"
      if .repo/repo/repo sync -c -j"$(nproc)" --no-tags --no-clone-bundle --retry-fetches=3 \
            --detach --force-sync --force-checkout --force-remove-dirty; then
         sync_ok=true
         break
      fi
   done

   # restored even when the sync failed, the detached checkout is never what we want
   git -C build_scripts checkout customize-local-build
   
   echo "[TRACKING] [Sync source] FINISHED ============="

   if [ "${sync_ok}" != true ]; then
      echo "ERROR: repo sync failed" >&2
      exit 1
   fi
}

sync_source "$@"
