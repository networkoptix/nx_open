#!/bin/bash

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set -e

if [[ "$1" == "--help" || "$1" == "-h" ]]
then
    cat << \
EOF
This script runs cmake configuration stage with the necessary parameters for the given target
platform (if the target platform isn't stated eplicitly via the "TARGET_DEVICE" environment
variable it is considered to be the same that the build platform). You may pass additional cmake
configuration options via the script arguments.

Usage:
    $0 [<cmake_configure_options>]
    or
    $0 [-h|--help]

    If you need to build for the target architecture different than default for the build
    platfrom, you should set the "TARGET_DEVICE" variable accordingly. The possible values are:
    linux_x64, linux_arm64, linux_arm32, windows_x64, macos_x64, macos_arm64.

Examples:
    # Debug build
    $0 -DcustomizationPackageFile="\${HOME}/Downloads/custom-package.zip" -DdeveloperBuild=ON

    # Release build with distribution package, unit tests archive and metadata SDK
    $0 \\
        -DcustomizationPackageFile="\${HOME}/Downloads/custom-package.zip" -DdeveloperBuild=OFF \\
        -DwithDistributions=ON -DwithUnitTestsArchive=ON -DwithSdk=ON
EOF

    exit 0
fi

if [[ -z "$TARGET_DEVICE" ]]
then
    case $OSTYPE in
        "darwin"* )
            if uname -p | grep -q arm
            then
                TARGET_DEVICE="macos_arm64"
            else
                TARGET_DEVICE="macos_x64"
            fi;;
        "msys"* ) TARGET_DEVICE="windows_x64";;
        "cygwin"* ) TARGET_DEVICE="windows_x64";;
        * ) TARGET_DEVICE="linux_x64";;
    esac
fi

baseDir=$(dirname "$0")
buildDirName="${baseDir}/../../open-build-${TARGET_DEVICE}"

if [[ -f "$buildDirName/CMakeCache.txt" ]]
then
    rm "$buildDirName/CMakeCache.txt"
fi

cmake -GNinja -B${buildDirName} -DtargetDevice=${TARGET_DEVICE}  $@ ${baseDir}

# "readlink -f" does not work in MacOS, hence an alternative impl via "pwd -P".
normalizedBuildDirName=$(cd "${buildDirName}" && pwd -P)

echo
echo "Run the following command to build the project for target ${TARGET_DEVICE}:"
echo "cmake --build ${normalizedBuildDirName}"
