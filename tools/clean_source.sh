#!/bin/bash
#
# Clean all build output of a 4490/6690 source tree, chipcode included.
# Must be run on the host (not inside the build docker): cleaning the chipcode from
# inside the docker would delete the kernel bind mounted in it.
#
# Usage: ./clean_source.sh <4490|6690>

set -euo pipefail

clean_source() {
   local src="${1:-}"
   local source_dir="${HOME}/Working/src/${src}-user"

   if [ ! -d "${source_dir}" ]; then
      echo "ERROR: source dir not found: ${source_dir}" >&2
      exit 1
   fi

   [ ! -f /.dockerenv ] || { echo "ERROR: run this script on the host, not inside docker" >&2; exit 1; }

   echo "[TRACKING] [Clean build output] ${source_dir} ============="
   "${source_dir}/build_scripts/build_clean.sh" --all
   git -C "${source_dir}/chip_code" clean -fd
   git -C "${source_dir}/chip_code" checkout .
   echo "[TRACKING] [Clean build output] FINISHED ============="
}

clean_source "$@"
