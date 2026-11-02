#!/bin/bash

DATE=$(date +%Y%m%d_%H%M%S)
ARTIFACTORY_URL="https://jfrog.devops.datalogic.com/artifactory"
REMOTE_REPO_PREFIX="mob-dl4490-generic"
REMOTE_REPO_SUFFIX="local"
REMOTE_BASE_PATH="mob/bsp-android/dl4490"

LOCAL_FIRMWARES="/media/truongnguyen/1tb/images/4490/LA3"
LOCAL_LOG="$(pwd)/logs/download_artifacts_${DATE}.log"

remotes=("qa")
androids=("a13" "a15")
dates=()
types=("gms" "aosp")
variants=("user" "userdebug")
firmwares=("ota.zip" "fastboot.zip")
auto_download=0

# -------------------------------------
# ----------- UTILITIES ---------------
# -------------------------------------
query_jf() {
    local imageDirectory=$1
    jf rt search "${imageDirectory}/*" --include-dirs=true --recursive=false | jq -r '.[] | (.path)'
}

convert_remote_path() {
    local remote=$1

    if [[ "$remote" == */* ]]; then
        echo "${remote%/}"
    else
        echo "${REMOTE_REPO_PREFIX}-${remote}-${REMOTE_REPO_SUFFIX}/${REMOTE_BASE_PATH}"
    fi
}

download_jf() {
    local path_firmware=$1
    local destination=$2

    mkdir -p "$(dirname "$destination")" \
    || { echo "FAILED TO CREATE DIRECTORY: $(dirname "$destination")" && return 1; }

    jf rt download \
        --flat=true \
        --threads=8 \
        --split-count=8 \
        --retries=2 \
        --fail-no-op=true \
        "$path_firmware" "$destination"

    local rc=$?
    if [[ $rc -ne 0 ]]; then
        echo "FAILED DOWNLOAD (jf exit ${rc}):"
    else
        echo "DOWNLOADED to:"
    fi
    echo "        $destination"

    return $rc
}

# -------------------------------------
# ----------- MAIN LOGIC --------------
# -------------------------------------

usage() {
cat <<EOF
Usage: $(basename "$0") [options]

Selects which images to download. An option takes one or more values, given as
separate words, as a comma separated list, or both:
    -a a13 a15      -a a13,a15      -a=a13,a15
When an option is omitted its default is used.

Options:
-r, --remote    qa,prod,nightly,int    Remote repositories to search
                                        (default: qa). A value containing '/'
                                        is used as a full remote path.
-l, --list-remotes                     List the remote paths --remote accepts
-a, --android   a13,a15                Android versions        (default: a13,a15)
-d, --date      YYYYMMDD[,...]         Exact build dates       (default: today)
    --from      YYYYMMDD               First date of a range   (implies --to today)
    --to        YYYYMMDD               Last date of a range    (used with --from)
-n, --daysago      N                      Last N daysago up to today
-t, --type      gms,aosp               Build types             (default: gms,aosp)
-v, --variant   user,userdebug         Build variants          (default: user,userdebug)
-f, --firmware  ota.zip,fastboot.zip   Image suffix, the part of the file name
                                        after the last '_'      (default: ota.zip,fastboot.zip)
-y, --yes                              Download every image found without asking.
                                        Without it the images found are listed
                                        and the ones to download are chosen.
-h, --help                             Show this help

Examples:
$(basename "$0") --list-remotes
$(basename "$0") -r prod -a a15 -d 20260827
$(basename "$0") --remote qa,nightly --daysago 7 -y
$(basename "$0") -a a13 a15 -d 20260827 -t gms -v userdebug user -f ota.zip
$(basename "$0") -a a15 --daysago 10
$(basename "$0") --from 20260801 --to 20260827 -v userdebug
EOF
}

get_options() {
    local daysago="" 
    local date_from="" 
    local date_to=""
    while [[ $# -gt 0 ]]; do
        local arg=$1
        local opt=${arg%%=*}
        local vals=() chunk=()
        shift

        # Flags take no value
        case "$opt" in
            -y|--yes) auto_download=1; continue ;;
        esac

        if [[ "$arg" == *=* ]]; then
            IFS=',' read -r -a vals <<< "${arg#*=}"
        else
            while [[ $# -gt 0 && "$1" != -* ]]; do
                IFS=',' read -r -a chunk <<< "$1"
                vals+=("${chunk[@]}")
                shift
            done
        fi

        case "$opt" in
            -r|--remote|-a|--android|-d|--date|-t|--type|-v|--variant|-f|--firmware|-n|--daysago|--from|--to)
                if [[ ${#vals[@]} -eq 0 ]]; then
                    echo "Missing value for option: $opt" >&2
                    return 1
                fi
                ;;
            *)
                echo "Unknown option: $opt" >&2
                usage >&2
                return 1
                ;;
        esac

        # Options that accept a single value only
        case "$opt" in
            -n|--daysago|--from|--to)
                if [[ ${#vals[@]} -gt 1 ]]; then
                    echo "Option $opt expects a single value, got: ${vals[*]}" >&2
                    return 1
                fi
                ;;
        esac

        # Remote names end up in a path, keep them free of surprises
        if [[ "$opt" == "-r" || "$opt" == "--remote" ]]; then
            for val in "${vals[@]}"; do
                if [[ ! "$val" =~ ^[A-Za-z0-9._/-]+$ ]]; then
                    echo "Invalid remote: $val" >&2
                    return 1
                fi
            done
        fi

        case "$opt" in
            -r|--remote)   remotes=("${vals[@]}") ;;
            -a|--android)  androids=("${vals[@]}") ;;
            -d|--date)     dates=("${vals[@]}") ;;
            -t|--type)     types=("${vals[@]}") ;;
            -v|--variant)  variants=("${vals[@]}") ;;
            -f|--firmware) firmwares=("${vals[@]}") ;;
            -n|--daysago)  daysago=${vals[0]} ;;
            --from)        date_from=${vals[0]} ;;
            --to)          date_to=${vals[0]} ;;
        esac
    done

    # inner function to generate dates between two given dates
    get_dates() {
        local date_from=$1
        local date_to=$2

        local dates=()
        current="$date_from"
        while [[ "$current" -le "$date_to" ]]; do
            dates+=("$current")
            current=$(date -d "$current + 1 day" +%Y%m%d)
        done
        echo "${dates[@]}"
    }

    is_date_valid() {
        local date=$1
        if [[ ! "$date" =~ ^[0-9]{8}$ ]]; then
            echo "Invalid date (expected YYYYMMDD): $date" >&2
            return 1
        fi
        return 0
    }

    # if dates are already provided, use them
    if [[ ${#dates[@]} -ne 0 ]]; then
        return 0
    fi
    
    # if daysago is provided, use it
    if [[ -n "$daysago" ]]; then
        if [[ ! "$daysago" =~ ^[0-9]+$ ]]; then
            echo "--daysago expects a number, got: $daysago" >&2
            return 1
        fi

        date_from=$(date -d "$daysago days ago" +%Y%m%d)
        date_to=$(date +%Y%m%d)

        dates=($(get_dates "$date_from" "$date_to"))
        return 0
    fi

    # if --from and --to are provided, use them
    if [[ -n "$date_from" && -n "$date_to" ]]; then
        (is_date_valid "$date_from" && is_date_valid "$date_to") || return 1
        if [[ "$date_from" -gt "$date_to" ]]; then
            echo "Invalid date range: $date_from is after $date_to" >&2
            return 1
        fi

        dates=($(get_dates "$date_from" "${date_to:-$(date +%Y%m%d)}"))
        return 0
    fi

    # if no dates are determined by any of the above methods, use today
    dates=("$(date +%Y%m%d)")
}

# -------------------------------------
# ----------- FIRMWARES ---------------
# -------------------------------------

find_firmwares() {
    local remotes=("${!1}")
    local androids=("${!2}")
    local dates=("${!3}")
    local types=("${!4}")
    local variants=("${!5}")
    local firmwares=("${!6}")

    local type_variants=()
    for type in "${types[@]}"; do
        for variant in "${variants[@]}"; do
            type_variants+=("${type}-${variant}")
        done
    done

    for remote in "${remotes[@]}"; do
        local remote_base="$(convert_remote_path "$remote")"
        echo "Searching remote path: $remote_base" >&2

        local androids_found=($(query_jf "$remote_base"))
        if [[ ${#androids_found[@]} -eq 0 ]]; then
            echo "NOTHING FOUND at remote path (run --list-remotes for the usable ones):" >&2
            echo "        $remote_base" >&2
            continue
        fi

        for android in "${androids_found[@]}"; do
            if [[ ! " ${androids[@]} " =~ " $(basename "$android") " ]]; then
                continue
            fi
            echo "Found android directory: $android" >&2
            for date in $(query_jf "${android}"); do
                if [[ ! " ${dates[@]} " =~ " $(basename "$date" | cut -d. -f4 | cut -c1-8) " ]]; then
                    continue
                fi
                echo "Found date directory: $date" >&2
                for type_variant in $(query_jf "${date}"); do
                    if [[ ! " ${type_variants[@]} " =~ " $(basename "$type_variant") " ]]; then
                        continue
                    fi
                    echo "Found type-variant directory: $type_variant" >&2
                    for firmware in $(query_jf "${type_variant}"); do
                        local name="$(basename "$firmware")"
                        if [[ ! " ${firmwares[@]} " =~ " ${name##*_} " ]]; then
                            continue
                        fi
                        echo "$firmware"
                    done
                done
            done
        done
    done
}

print_firmwares() {
    local i=1
    for firmware in "$@"; do
        echo "${i}. $(basename "$firmware")"
        ((i++))
    done
}

select_firmwares() {
    local paths=("$@")
    local count=${#paths[@]}

    while true; do
        echo >&2
        echo "Enter 0 to download all, or the numbers to download (e.g. 1 3 4)." >&2
        echo -n "Empty input cancels: " >&2

        local input
        if ! read -r input; then
            echo >&2
            echo "No input, nothing selected" >&2
            return 1
        fi

        local choices=()
        IFS=', ' read -r -a choices <<< "$input"
        [[ ${#choices[@]} -eq 0 ]] && return 0

        local valid=1
        for choice in "${choices[@]}"; do
            if [[ ! "$choice" =~ ^[0-9]+$ || "$choice" -gt "$count" ]]; then
                echo "Invalid choice: $choice (expected 0 to ${count})" >&2
                valid=0
            fi
        done
        [[ $valid -eq 1 ]] && break
    done

    if [[ " ${choices[*]} " =~ " 0 " ]]; then
        printf '%s\n' "${paths[@]}"
        return 0
    fi

    local i
    for ((i = 1; i <= count; i++)); do
        if [[ " ${choices[*]} " =~ " ${i} " ]]; then
            echo "${paths[i-1]}"
        fi
    done
}

download_firmwares() {
    local failed=0

    for source in "$@"; do
        local relative=$(echo "$source" | rev | cut -d/ -f1-4 | rev)
        local destination="${LOCAL_FIRMWARES}/${relative}"

        echo "Try downloading:"
        echo "        ${ARTIFACTORY_URL}/${source}"

        if [[ -f "$destination" ]]; then
            echo "SKIP DOWNLOAD (File already exists) in:"
            echo "        $destination"
        else
            download_jf "$source" "$destination" || failed=1
        fi
    done

    return $failed
}

main() {
    get_options "$@" || return 1

    echo "Datetime now: $(date)"
    echo "Checking to download images..."

    echo remotes: "${remotes[@]}"
    echo androids: "${androids[@]}"
    echo dates: "${dates[@]}"
    echo types: "${types[@]}"
    echo variants: "${variants[@]}"
    echo firmwares: "${firmwares[@]}"
    echo local path: "$LOCAL_FIRMWARES"
    echo log file: "$LOCAL_LOG"
    echo "----------------------------------------"

    # Find all firmware paths matching the specified filters
    local firmware_paths=()
    mapfile -t firmware_paths < <(find_firmwares remotes[@] androids[@] dates[@] types[@] variants[@] firmwares[@])

    if [[ ${#firmware_paths[@]} -eq 0 ]]; then
        echo "No image matches the filters"
        return 0
    fi

    # Print the found firmware paths
    echo "----------------------------------------"
    echo "Found ${#firmware_paths[@]} image(s)"
    print_firmwares "${firmware_paths[@]}"

    # Ask the user to select firmwares to download if auto download is not enabled
    if [[ $auto_download -eq 0 ]]; then
        mapfile -t firmware_paths < <(select_firmwares "${firmware_paths[@]}")
        if [[ ${#firmware_paths[@]} -eq 0 ]]; then
            echo "Nothing selected, no image downloaded"
            return 0
        fi
    else
        echo "----------------------------------------"
        echo "Auto download enabled, downloading all found images"
    fi

    # Download the selected firmware paths
    echo "---------------Start Download -------------------------"
    download_firmwares "${firmware_paths[@]}"
}

main "$@" 2>&1 | tee "$LOCAL_LOG"

