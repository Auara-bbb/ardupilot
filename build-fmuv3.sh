#!/usr/bin/env bash

# The Cygwin Bash startup file sources this script to keep ./waf usable
# across branches that have different versions of the tracked launcher.
if [[ ${BASH_SOURCE[0]} != "$0" ]]; then
    function ./waf {
        local repo_dir=${BASH_SOURCE[0]%/*}
        if [[ "$PWD" != "$repo_dir" && "$(pwd -P)" != "$repo_dir" ]]; then
            command ./waf "$@"
            return
        fi

        local active_lock=.lock-waf_fmuv3_cygwin.active
        case ${1-} in
            configure)
                if [[ ${2-} == --board && ${3-} == fmuv3 ]]; then
                    bash "$repo_dir/build-fmuv3.sh" configure "${@:4}"
                elif [[ ${2-} == --board=fmuv3 ]]; then
                    bash "$repo_dir/build-fmuv3.sh" configure "${@:3}"
                else
                    command ./waf "$@" && rm -f -- "$active_lock"
                fi
                ;;
            copter)
                if [[ -f "$active_lock" ]]; then
                    bash "$repo_dir/build-fmuv3.sh" "$@"
                else
                    command ./waf "$@"
                fi
                ;;
            *) command ./waf "$@" ;;
        esac
    }
    return
fi

set -euo pipefail

if ! command -v cygpath >/dev/null || [[ $(uname -s) != CYGWIN* ]]; then
    printf 'Run this script in Cygwin.\n' >&2
    exit 1
fi

repo_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
if [[ ! -f "$repo_dir/wscript" ]]; then
    printf 'ArduPilot wscript was not found in %s.\n' "$repo_dir" >&2
    exit 1
fi

mode=${1-all}
case $mode in
    configure|copter) shift ;;
    all) ;;
    *) printf 'Usage: bash build-fmuv3.sh [configure|copter]\n' >&2; exit 2 ;;
esac

python_dir=/opt/ardupilot-python39
gcc_dir=/opt/ardupilot-arm-gcc6
waf="$python_dir/waf/waf-light"

for required_file in "$python_dir/bin/python" "$waf" "$gcc_dir/bin/arm-none-eabi-gcc.exe"; do
    if [[ ! -f "$required_file" ]]; then
        printf 'Required build tool is missing: %s\n' "$required_file" >&2
        exit 1
    fi
done

repo_win=$(cygpath -am "$repo_dir")
repo_key=$(printf '%s' "$repo_win" | sha256sum)
# Separate Waf invocations must recreate exactly the same configuration path.
mount_dir="/tmp/ardupilot-fmuv3-src.$UID.${repo_key:0:16}"
if ! mkdir -m 700 -- "$mount_dir"; then
    printf 'Could not reserve the build mount at %s.\n' "$mount_dir" >&2
    exit 1
fi
mounted=false

cleanup() {
    if [[ "$mounted" == true ]]; then
        cd /tmp
        if ! umount "$mount_dir"; then
            printf 'Could not unmount %s.\n' "$mount_dir" >&2
            return
        fi
    fi
    rmdir -- "$mount_dir"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

# Keep Waf and native ARM GCC on the same drive while avoiding Cygwin ACL errors.
mount -o binary,noacl "$repo_win" "$mount_dir"
mounted=true
cd "$mount_dir"

export PATH="$python_dir/bin:$python_dir/usr/bin:$gcc_dir/bin:$PATH"
export WAFLOCK=.lock-waf_fmuv3_cygwin
build_dir="$mount_dir/build-fmuv3-cygwin"

if [[ "$mode" != copter ]]; then
    python "$waf" configure "$@" --board fmuv3 --top="$mount_dir" --out="$build_dir" \
        --no-submodule-update --no-autoconfig
    touch "$WAFLOCK.active"
fi
if [[ "$mode" == configure ]]; then
    exit
fi
if [[ ! -f "$WAFLOCK.active" || ! -f "$WAFLOCK" ]]; then
    printf 'Run ./waf configure --board fmuv3 first.\n' >&2
    exit 1
fi

python "$waf" copter -j12 "$@"

firmware="$build_dir/fmuv3/bin/arducopter.apj"
if [[ ! -s "$firmware" ]]; then
    printf 'Firmware was not generated at %s.\n' "$firmware" >&2
    exit 1
fi
printf 'Firmware: %s\n' "$(cygpath -am "$firmware")"
