#!/bin/bash

set -euo pipefail

build_source() {
    local src="${1:-}"
    local source_dir="${HOME}/Working/src/${src}-user"

    if [ ! -d "${source_dir}" ]; then
        echo "ERROR: source dir not found: ${source_dir}" >&2
        exit 1
    fi

    # docker_image  : same image as build_scripts/start_docker.sh
    # le_kernel_dir : linux embedded kernel dir, mounted on kernel/kernel_platform

    local docker_image le_kernel_dir 
    case "${src}" in
        4490)
            docker_image="aosp-qcm4490-build-env"
            le_kernel_dir="chip_code/LE.UM.5.3.1/apps_proc/src/kernel-5.10/kernel_platform"
            ;;
        6690)
            docker_image="aosp-qcm4490-la-40-build-env"
            le_kernel_dir="chip_code/LE.VM.1.2.r1/apps_proc/src/kernel-6.6/kernel_platform"
            ;;
        *)
            echo "Usage: $0 <4490|6690>" >&2
            exit 1
            ;;
    esac

    [ -d "${source_dir}/pkg/qct" ] || { echo "ERROR: compilers dir not found: ${source_dir}/pkg/qct" >&2; exit 1; }
    docker image inspect "${docker_image}" > /dev/null 2>&1 \
        || { echo "ERROR: docker image not found: ${docker_image}" >&2; exit 1; }

    echo "[TRACKING] [Build in docker] ${source_dir} (${docker_image}) ============="
    docker run --rm -e USER_ID="$(id -u)" -e GROUP_ID="$(id -g)" \
        -v "${source_dir}":/aosp \
        -v "${source_dir}/pkg":/pkg \
        -v "${HOME}":/home/aosp_builder \
        -v "${source_dir}/kernel/kernel_platform":/aosp/vendor/kernel_platform \
        -v "${source_dir}/kernel/kernel_platform":"/aosp/${le_kernel_dir}" \
        -w /aosp "${docker_image}" ./build_scripts/localbuild.sh

    echo "[TRACKING] [Build in docker] FINISHED ============="
}

build_source "$@"
