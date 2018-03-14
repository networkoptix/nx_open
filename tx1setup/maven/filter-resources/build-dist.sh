#!/bin/bash
set -e #< Stop on errors.
set -u #< Fail on undefined variables.

# TODO: This script creates a "portable" archive. To be rewritten to create a proper installer.

COMPANY_NAME="@deb.customization.company.name@"
SOURCE_ROOT_PATH="@root.dir@"

TGZ_NAME="tegra.tgz"

STAGEBASE="stagebase"
COMPILER="@CMAKE_CXX_COMPILER@"

BUILD_INFO_TXT="@libdir@/build_info.txt"
LOGS_DIR="@libdir@/build_logs"
LOG_FILE="$LOGS_DIR/tegra-build-dist.log"

BOX_MNT="@CMAKE_CURRENT_BINARY_DIR@/$STAGEBASE"
CMAKE_BUILD_DIR="@CMAKE_BINARY_DIR@"
BOX_INSTALL_DIR="/opt/$COMPANY_NAME"
BOX_DESKTOP_CLIENT_DIR="$BOX_INSTALL_DIR/desktop_client"
BOX_MEDIASERVER_DIR="$BOX_INSTALL_DIR/mediaserver"
BOX_LIBS_DIR="$BOX_INSTALL_DIR/lib"
DESKTOP_CLIENT_BIN="@client.binary.name@"

TEGRA_VIDEO_SRC_PATH="artifacts/tx1/tegra_multimedia_api" #< Relative to source dir.
NVIDIA_MODELS_PATH="$TEGRA_VIDEO_SRC_PATH/data/model" #< Demo neural networks. Relative to source dir.
BOX_NVIDIA_MODELS_DIR="$BOX_MEDIASERVER_DIR/nvidia_models"

PACKAGES_DIR="@packages.dir@/tx1"
PACKAGE_QT="qt-5.6.3"
PACKAGE_SIGAR="sigar-1.7"
PACKAGE_FFMPEG="ffmpeg-3.1.1"

#--------------------------------------------------------------------------------------------------

# [in] BOX_MNT
cp_files() # src_dir file_mask dst_dir
{
    local SRC_DIR="$1"; shift
    local FILE_MASK="$1"; shift
    local DST_DIR="$1"; shift

    echo "  Copying $FILE_MASK from $SRC_DIR/ to $DST_DIR/"

    mkdir -p "${BOX_MNT}$DST_DIR" || exit $?

    pushd "$SRC_DIR" >/dev/null

    # Here eval expands globs and braces to the array, after we enquote spaces (if any).
    shopt -s extglob #< Enable extended globs: ?(), *(), +(), @(), !().
    eval FILES_LIST=(${FILE_MASK// /\" \"})
    shopt -u extglob #< Disable extended globs.

    if [ ${#FILES_LIST[@]} == 0 ]; then
        echo "ERROR: No match in [$SRC_DIR] of [$FILE_MASK]." >&2
        return -1
    fi

    cp -r "${FILES_LIST[@]}" "${BOX_MNT}$DST_DIR/" || exit $?

    popd >/dev/null
}

# [in] CMAKE_BUILD_DIR
# [in] BOX_LIBS_DIR
cp_libs() # file_mask [file_mask]...
{
    local FILE_MASK
    for FILE_MASK in "$@"; do
        cp_files "$CMAKE_BUILD_DIR/lib" "$FILE_MASK" "$BOX_LIBS_DIR"
    done
}

# [in] PACKAGES_DIR
# [in] BOX_LIBS_DIR
cp_package_libs() # package-name [package-name]...
{
    local PACKAGE
    for PACKAGE in "$@"; do
        if [ ! -z "$PACKAGE" ]; then
            cp_files "$PACKAGES_DIR/$PACKAGE/lib" "*.so*" "$BOX_LIBS_DIR"
        fi
    done
}

# [in] CMAKE_BUILD_DIR
# [in] BOX_MEDIASERVER_DIR
cp_mediaserver_bins() # file_mask [file_mask]...
{
    local FILE_MASK
    for FILE_MASK in "$@"; do
        cp_files "$CMAKE_BUILD_DIR/bin" "$FILE_MASK" "$BOX_MEDIASERVER_DIR/bin"
    done
}

# [in] CMAKE_BUILD_DIR
# [in] BOX_DESKTOP_CLIENT_DIR
cp_desktop_client_bins() # file_mask [file_mask]...
{
    local FILE_MASK
    for FILE_MASK in "$@"; do
        cp_files "$CMAKE_BUILD_DIR/bin" "$FILE_MASK" "$BOX_DESKTOP_CLIENT_DIR/bin"
    done
}

# [in] Library name
# [in] Destination directory
cp_sys_lib()
{
    "$SOURCE_ROOT_PATH"/build_utils/copy_system_library.sh -c "$COMPILER" "$@"
}

buildDistribution()
{
    echo "Copying build_info.txt"
    mkdir -p "${BOX_MNT}$BOX_INSTALL_DIR"
    cp -r "$BUILD_INFO_TXT" "${BOX_MNT}$BOX_INSTALL_DIR/"

    echo "Copying libs"
    mkdir -p "${BOX_MNT}$BOX_LIBS_DIR"
    cp_libs "*.so!(.debug)"

    echo "Copying mediaserver"
    cp_mediaserver_bins "mediaserver"
    cp_mediaserver_bins "external.dat" "plugins" "vox"
    ln -s "../lib" "${BOX_MNT}$BOX_MEDIASERVER_DIR/lib" #< rpath: [$ORIGIN/..lib]

    echo "Copying tegra_video analytics"
    cp_package_libs "tegra_video" #< Tegra-specific plugin for video decoding and neural networks.
    cp_files "$SOURCE_ROOT_PATH/$NVIDIA_MODELS_PATH" "*" "$BOX_NVIDIA_MODELS_DIR" #< Demo neural networks.

    echo "Disabling (renaming) unneeded metadata plugins"
    ( cd "${BOX_MNT}$BOX_MEDIASERVER_DIR/bin/plugins"
        mv libstub_metadata_plugin.so libstub_metadata_plugin.so_DISABLED
        mv libtegra_video_metadata_plugin.so libtegra_video_metadata_plugin.so_DISABLED
    )

    echo "Copying desktop_client"
    cp_desktop_client_bins "$DESKTOP_CLIENT_BIN"
    cp_desktop_client_bins "fonts" "vox" "help"
    ln -s "../lib" "${BOX_MNT}$BOX_DESKTOP_CLIENT_DIR/lib" #< rpath: [$ORIGIN/..lib]
    cp_files "$PACKAGES_DIR/$PACKAGE_QT/plugins" \
        "{imageformats,platforminputcontexts,platforms,xcbglintegrations,audio}" \
        "$BOX_DESKTOP_CLIENT_DIR/bin"
    cp_files "$PACKAGES_DIR/$PACKAGE_QT" "qml" "$BOX_DESKTOP_CLIENT_DIR/bin"

    echo "Copying qt.conf and creating Qt symlinks"
    # TODO: Remove these symlinks when cmake build is fixed.
    cp_desktop_client_bins "qt.conf"
    local -r PLUGINS_DIR="${BOX_MNT}$BOX_DESKTOP_CLIENT_DIR/plugins"
    mkdir "$PLUGINS_DIR"
    local PLUGIN
    for PLUGIN in audio imageformats platforminputcontexts platforms xcbglintegrations
    do
        ln -s "../bin/$PLUGIN" "$PLUGINS_DIR/$PLUGIN"
    done
    ln -s "bin/qml" "${BOX_MNT}$BOX_DESKTOP_CLIENT_DIR/qml"
    # Required structure:
    #     bin/
    #         desktop_client fonts/ help/ vox/
    #     plugins/
    #         audio/ imageformats/ platforminputcontexts/ platforms/ xcbglintegrations/
    #     qml/
    #
    # Actual structure:
    #     bin/
    #         desktop_client fonts/ help/ vox/
    #         qml/
    #         audio/ imageformats/ platforminputcontexts/ platforms/ xcbglintegrations/

    echo "Copying package libs"
    cp_package_libs "$PACKAGE_QT" "$PACKAGE_FFMPEG" "$PACKAGE_SIGAR"

    echo "Copying compiler (system) libs"
    cp_sys_lib libstdc++.so.6 "${STAGEBASE}$BOX_LIBS_DIR"
    cp_sys_lib libatomic.so.1.2.0 "${STAGEBASE}$BOX_LIBS_DIR"

    echo "Creating archive $TGZ_NAME"
    ( cd "$STAGEBASE" && tar czf "$TGZ_NAME" * )

    echo "Moving archive to @distribution_output_dir@"
    mv "$STAGEBASE/$TGZ_NAME" "@distribution_output_dir@/"
}

#--------------------------------------------------------------------------------------------------

showHelp()
{
    echo "Options:"
    echo " -v, --verbose: Do not redirect output to a log file."
    echo " -k, --keep-work-dir: Do not delete work directory on success."
}

# [out] KEEP_WORK_DIR
# [out] VERBOSE
parseArgs() # "$@"
{
    KEEP_WORK_DIR=0
    VERBOSE=0

    local ARG
    for ARG in "$@"; do
        if [ "$ARG" = "-h" -o "$ARG" = "--help" ]; then
            showHelp
            exit 0
        elif [ "$ARG" = "-k" ] || [ "$ARG" = "--keep-work-dir" ]; then
            KEEP_WORK_DIR=1
        elif [ "$ARG" = "-v" ] || [ "$ARG" = "--verbose" ]; then
            VERBOSE=1
        fi
    done
}

redirectOutputToLog()
{
    echo "  See the log in $LOG_FILE"

    mkdir -p "$LOGS_DIR" #< In case log dir is not created yet.
    rm -rf "$LOG_FILE"

    # Redirect only stdout to the log file, leaving stderr as is, because there seems to be no
    # reliable way to duplicate error output to both log file and stderr, keeping the correct order
    # of regular and error lines in the log file.
    exec 1>"$LOG_FILE" #< Open log file for writing as stdout.
}

# Called by trap.
# [in] $?
# [in] VERBOSE
onExit()
{
    local RESULT=$?
    echo "" #< Newline, to separate SUCCESS/FAILURE message from the above log.
    if [ $RESULT != 0 ]; then #< Failure.
        echo "FAILURE (status $RESULT); see the error message(s) above." >&2 #< To stderr.
        if [ $VERBOSE = 0 ]; then #< stdout redirected to log file.
            echo "FAILURE (status $RESULT); see the error message(s) on stderr." #< To log file.
        fi
    else
        echo "SUCCESS"
    fi
    return $RESULT
}

main()
{
    declare -i VERBOSE #< Mot local - needed by onExit().
    parseArgs "$@"

    trap onExit EXIT

    rm -rf "$STAGEBASE"

    if [ KEEP_WORK_DIR = 1 ]; then
        local -r WORK_DIR_NOTE="(ATTENTION: will NOT be deleted)"
    else
        local -r WORK_DIR_NOTE="(will be deleted on success)"
    fi
    echo "Creating archive in $STAGEBASE $WORK_DIR_NOTE."

    if [ $VERBOSE = 0 ]; then
        redirectOutputToLog
    fi

    buildDistribution

    if [ KEEP_WORK_DIR = 0 ]; then
        rm -rf "$STAGEBASE"
    fi
}

main "$@"
