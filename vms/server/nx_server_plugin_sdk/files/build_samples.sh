#!/bin/bash

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set -e #< Exit on error.
set -u #< Prohibit undefined variables.

if [[ $# > 0 && ($1 == "/?" || $1 == "-h" || $1 == "--help") ]]; then
    cat <<EOF
Usage:
$0 \\
    [--no-tests] [--debug] [--no-qt-samples] [--with-rpi-samples] [<cmake-generation-args>...]

 --no-tests: Do not run unit tests.
 --with-rpi-samples Build Raspberry Pi samples (use only when building for 32-bit ARM).
 --no-qt-samples Exclude samples that require the Qt library.
 --debug Compile using Debug configuration (without optimizations) instead of Release.

NOTE: If --no-qt-samples is not specified, and cmake cannot find Qt, set the paths to "qt" and
"qt-host" Conan packages using the environment variables QT_DIR and QT_HOST_PATH repsectively.
EOF
    exit 0
fi

BASE_DIR="$(readlink -f "$(dirname "$0")")" #< Absolute path to this script's dir.
SYSTEM_TYPE="$(uname -s)"
if [[ "${SYSTEM_TYPE}" == "CYGWIN"* || "${SYSTEM_TYPE}" == "MINGW"* ]]; then
    BASE_DIR="$(cygpath -w "${BASE_DIR}")" #< Windows-native cmake requires Windows path
fi

# Parse command line arguments.

if [[ $# > 0 && $1 == "--no-tests" ]]; then
    shift
    NO_TESTS=1
else
    NO_TESTS=0
fi

if [[ $# > 0 && $1 == "--debug" ]]; then
    shift
    BUILD_TYPE=Debug
else
    BUILD_TYPE=Release
fi

if [[ $# > 0 && $1 == "--no-qt-samples" ]]; then
    shift
    NO_QT_SAMPLES=1
else
    NO_QT_SAMPLES=0
fi

if [[ $# > 0 && $1 == "--with-rpi-samples" ]]; then
    shift
    WITH_RPI_SAMPLES=1
else
    WITH_RPI_SAMPLES=0
fi

EXTRA_CMAKE_ARGS=("$@")

# Set up variables for the build system.

GEN_OPTIONS=( -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" )
BUILD_OPTIONS=()
case "${SYSTEM_TYPE}" in #< Check if running in Windows from Cygwin/MinGW.
    CYGWIN*|MINGW*)
        # Assume that the MSVC environment is set up correctly and Ninja is installed. Also
        # specify the compiler explicitly to avoid clashes with gcc.
        GEN_OPTIONS+=( -GNinja -DCMAKE_CXX_COMPILER=cl.exe -DCMAKE_C_COMPILER=cl.exe )
        ;;
    *)
        if command -v ninja >/dev/null; then
            GEN_OPTIONS=( -GNinja ) #< Generate for Ninja and gcc; Ninja uses all CPU cores.
        else
            GEN_OPTIONS=() #< Generate for GNU make and gcc.
            BUILD_OPTIONS+=( -- -j ) #< Use all available CPU cores.
        fi
        if [[ "${BUILD_TYPE}" == "Release" ]]; then
            GEN_OPTIONS+=( -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" )
        fi
        ;;
esac

# Make the build dir at the same level as the parent dir of this script, suffixed with
# "-build".
BUILD_DIR="$BASE_DIR-build"
(set -x #< Log each command.
    rm -rf "${BUILD_DIR}/"
)

# Build samples.

for sdk_samples_dir in "${BASE_DIR}/samples"/*; do
    plugin_type="$(basename "${sdk_samples_dir}")"
    for source_dir in "${sdk_samples_dir}"/*; do
        sample="$(basename "${source_dir}")"
        if [[ "${WITH_RPI_SAMPLES}" == 0 && "${sample}" == "rpi_"* ]]; then
            echo "ATTENTION: Skipping ${sample} because --with-rpi-samples is not specified."
            echo ""
            continue
        fi
        if [[ "${NO_QT_SAMPLES}" == 1 && "${sample}" == "axis_camera_plugin" ]]; then
            echo "ATTENTION: Skipping ${sample} because --no-qt-samples is specified."
            echo ""
            continue
        fi

        # For different plugins the CMakeLists.txt file can be located either in the plugin
        # root directory or in the "src" subdirectory. TODO: Consider changing plugin source
        # files layout to make CMakeLists.txt locations consistent.
        if [[ ! -f "${source_dir}/CMakeLists.txt" ]]; then
            source_dir="${source_dir}/src"
        fi
        sample_build_dir="${BUILD_DIR}/${plugin_type}/${sample}"
        (set -x #< Log each command.
            mkdir -p "${sample_build_dir}"
            cmake -S "${source_dir}" -B "${sample_build_dir}" \
                ${GEN_OPTIONS[@]+"${GEN_OPTIONS[@]}"} `# allow empty array #` \
                "${EXTRA_CMAKE_ARGS[@]+"${EXTRA_CMAKE_ARGS[@]}"}" `# allow empty array #`
            cmake --build "${sample_build_dir}" \
                ${BUILD_OPTIONS[@]+"${BUILD_OPTIONS[@]}"} #< Allow empty array.
        )

        if [[ "${sample}" == "unit_tests" ]]; then
            artifact="$(find "${sample_build_dir}" \
                -name "analytics_plugin_ut.exe" -o -name "analytics_plugin_ut")"
        else
            artifact="$(find "${sample_build_dir}" \
                -name "${sample}.dll" -o -name "lib${sample}.so")"
        fi
        if [[ ! -f "${artifact}" ]]; then
            echo "ERROR: Failed to build ${sample}."
            exit 64
        fi
        echo ""
        echo "Built: ${artifact}"
        echo ""
    done
done

# Run unit tests if needed.

if (( ${NO_TESTS} == 0 )); then
    for unt_test_dir in "${BUILD_DIR}"/*/unit_tests; do
        (set -x #< Log each command.
            cd "${unt_test_dir}"
            ctest --output-on-failure -C "${BUILD_TYPE}"
        )
    done
else
    echo "NOTE: Unit tests were not run."
fi

echo ""
echo "All samples built successfully, see the binaries in ${BUILD_DIR}"
