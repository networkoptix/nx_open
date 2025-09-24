#!/bin/sh

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# This script compares the version of the bundled libstdc++ with the version of the libstdc++
# installed in the OS, and reports the path of the bundled one if it is newer.
#
# We need this because if some OS component (like a video driver) uses a newer libstdc++, the
# component will fail to load with an older bundled library.
#
# The script should be able to run under busybox shell.

get_glibcxx_version()
{
    local file="$1"

    # `nm` allows us to certainly know the library version, so trying it first.
    if command -v nm > /dev/null; then
        # `sed` searches for something like `0000000000000000 A GLIBCXX_3.4.25` and prints `3.4.25`.
        nm -D "${file}" | sed -n 's/.* GLIBCXX_//p' | sort -uV | tail -n 1
    else
        # If `nm` is not present, guess the version by the library file name.
        # `sed` cuts everything but version part.
        realpath "${file}" | sed 's/.*\.so\.//'
    fi
}

find_system_libstdcpp()
{
    if command -v ldconfig >/dev/null 2>&1; then
        ldconfig -p | grep libstdc++.so.6 | head -n 1 | sed 's/.*=> //'
        return
    fi

    # Fallback: try to find libstdc++.so.6 in standard library paths.
    for dir in /lib64 /lib /usr/lib64 /usr/lib; do
        if [ -f "${dir}/libstdc++.so.6" ]; then
            echo "${dir}/libstdc++.so.6"
            return
        fi
    done
}

get_newest_libstdcpp()
{
    local localStdcppDir="$1"
    if [ -z "${localStdcppDir}" ]; then
        echo "Please specify local libstdc++ directory." >&2
        exit 1
    fi

    local localStdcpp="$localStdcppDir/libstdc++.so.6"
    if [ ! -f "${localStdcpp}" ]; then
        echo "File not found: ${localStdcpp}" >&2
        exit 1
    fi

    local systemStdcpp="$(find_system_libstdcpp)"
    if [ -z "${systemStdcpp}" ]; then
        echo "${localStdcpp}"
        return
    fi

    localVersion=$(get_glibcxx_version "${localStdcpp}")
    systemVersion=$(get_glibcxx_version "${systemStdcpp}")

    if [ "${localVersion}" \> "${systemVersion}" ]; then
        echo "${localStdcppDir}"
    fi
}

create_symlinks_for_files_in_dir()
{
    local srcDir="$1"
    local dstDir="$2"

    for lib in "${srcDir}"/*; do
        libName=$(basename "${lib}")
        echo "Making symlink "${dstDir}/${libName}" for "${lib}""
        ln -sf "${lib}" "${dstDir}/${libName}"
    done
}

remove_symlinks_for_files_in_dir()
{
    local srcDir="$1"
    local dstDir="$2"

    for lib in "${srcDir}"/*; do
        libName=$(basename "${lib}")
        symlink="${dstDir}/${libName}"
        [ -e "${symlink}" ] || continue

        if [ -L "${symlink}" ]; then
            echo "Remove symlink: ${symlink}"
            rm -f "${symlink}"
        else
            echo "Skip: ${symlink}, as it is not a symbolic link"
        fi
    done
}

ensure_newest_libstdcpp()
{
    local libDir="$(dirname "$0")/.."
    local stdcppDir="$(get_newest_libstdcpp "${libDir}/stdcpp")"
    if [ -n "${stdcppDir}" ]; then
        echo "Using bundled libstdc++."
        create_symlinks_for_files_in_dir "${stdcppDir}" "${libDir}"
    else
        echo "Using system libstdc++."
        remove_symlinks_for_files_in_dir "${stdcppDir}" "${libDir}"
    fi
}

remove_bundled_symlinks()
{
    local libDir="$(dirname "$0")/.."
    local stdcppDir="$(get_newest_libstdcpp "${libDir}/stdcpp")"
    remove_symlinks_for_files_in_dir "${stdcppDir}" "${libDir}"
}

if [ "$1" = "ensure_newest_libstdcpp" ]; then
    ensure_newest_libstdcpp
elif [ "$1" = "remove_bundled_symlinks" ]; then
    remove_bundled_symlinks
else
    get_newest_libstdcpp "$@"
fi
