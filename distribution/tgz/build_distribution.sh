#!/bin/bash
set -e #< Stop on errors.
set -u #< Fail on undefined variables.

source "$(dirname $0)/../../build_utils/linux/build_distribution_utils.sh"

distr_loadConfig "build_distribution.conf"

INSTALL_PATH="opt/$CUSTOMIZATION"

DESKTOP_CLIENT_INSTALL_PATH="$INSTALL_PATH/desktop_client"
MEDIASERVER_INSTALL_PATH="$INSTALL_PATH/mediaserver"

TEGRA_VIDEO_SOURCE_DIR="$SOURCE_DIR/artifacts/tx1/tegra_multimedia_api"
NVIDIA_MODELS_SOURCE_DIR="$TEGRA_VIDEO_SOURCE_DIR/data/model" #< Demo neural networks.
NVIDIA_MODELS_INSTALL_PATH="$MEDIASERVER_INSTALL_PATH/nvidia_models"

PACKAGE_QT="qt-5.6.3"
PACKAGE_SIGAR="sigar-1.7"
PACKAGE_FFMPEG="ffmpeg-3.1.1"

OUTPUT_DIR=${DISTRIBUTION_OUTPUT_DIR:-"$CURRENT_BUILD_DIR"}

TGZ_NAME="tegra.tgz"

LIB_INSTALL_PATH="$INSTALL_PATH/lib"

LOG_FILE="$LOGS_DIR/build_distribution.log"

WORK_DIR="build_distribution_tmp"

#--------------------------------------------------------------------------------------------------

# [in] WORK_DIR
cp_files() # src_dir file_mask dst_dir
{
    local -r SRC_DIR="$1" && shift
    local -r FILE_MASK="$1" && shift
    local -r DST_DIR="$1" && shift

    echo "  Copying $FILE_MASK from $SRC_DIR/ to $DST_DIR/"

    local -r DST_ABSOLUTE_PATH="$(readlink -f -- "$WORK_DIR")/$DST_DIR"

    mkdir -p "$DST_ABSOLUTE_PATH" || exit $?

    pushd "$SRC_DIR" >/dev/null

    # Here eval expands globs and braces to the array, after we enquote spaces (if any).
    shopt -s extglob #< Enable extended globs: ?(), *(), +(), @(), !().
    eval FILES_LIST=(${FILE_MASK// /\" \"})
    shopt -u extglob #< Disable extended globs.

    if [ ${#FILES_LIST[@]} == 0 ]; then
        echo "ERROR: No match in [$SRC_DIR] of [$FILE_MASK]." >&2
        return -1
    fi

    cp -r "${FILES_LIST[@]}" "$DST_ABSOLUTE_PATH/" || exit $?

    popd >/dev/null
}

# [in] BUILD_DIR
# [in] LIB_INSTALL_PATH
cp_libs() # file_mask [file_mask]...
{
    local FILE_MASK
    for FILE_MASK in "$@"; do
        cp_files "$BUILD_DIR/lib" "$FILE_MASK" "$LIB_INSTALL_PATH"
    done
}

# [in] PACKAGES_DIR
# [in] LIB_INSTALL_PATH
cp_package_libs() # package-name [package-name]...
{
    local PACKAGE
    for PACKAGE in "$@"; do
        if [ ! -z "$PACKAGE" ]; then
            cp_files "$PACKAGES_DIR/$PACKAGE/lib" "*.so*" "$LIB_INSTALL_PATH"
        fi
    done
}

# [in] BUILD_DIR
# [in] MEDIASERVER_INSTALL_PATH
cp_mediaserver_bins() # file_mask [file_mask]...
{
    local FILE_MASK
    for FILE_MASK in "$@"; do
        cp_files "$BUILD_DIR/bin" "$FILE_MASK" "$MEDIASERVER_INSTALL_PATH/bin"
    done
}

# [in] BUILD_DIR
# [in] DESKTOP_CLIENT_INSTALL_PATH
cp_desktop_client_bins() # file_mask [file_mask]...
{
    local FILE_MASK
    for FILE_MASK in "$@"; do
        cp_files "$BUILD_DIR/bin" "$FILE_MASK" "$DESKTOP_CLIENT_INSTALL_PATH/bin"
    done
}

# [in] Library name
# [in] Destination directory
cp_sys_lib()
{
    # TODO: #dklychkov: Why here .sh is used instead of .py as in distr_copySystemLibraries()?
    "$SOURCE_DIR"/build_utils/copy_system_library.sh -c "$COMPILER" "$@"
}

buildDistribution()
{
    echo "Copying build_info.txt"
    mkdir -p "$WORK_DIR/$INSTALL_PATH"
    cp -r "$BUILD_DIR/build_info.txt" "$WORK_DIR/$INSTALL_PATH/"

    echo "Copying libs"
    mkdir -p "$WORK_DIR/$LIB_INSTALL_PATH"
    cp_libs "*.so!(.debug)"

    echo "Copying mediaserver"
    cp_mediaserver_bins "mediaserver"
    cp_mediaserver_bins "external.dat" "plugins" "plugins_optional" "vox"
    ln -s "../lib" "$WORK_DIR/$MEDIASERVER_INSTALL_PATH/lib" #< rpath: [$ORIGIN/..lib]

    echo "Copying tegra_video analytics"
    cp_package_libs "tegra_video" #< Tegra-specific plugin for video decoding and neural networks.
    cp_files "$NVIDIA_MODELS_SOURCE_DIR" "*" "$NVIDIA_MODELS_INSTALL_PATH" #< Demo neural networks.

    echo "Copying desktop_client"
    cp_desktop_client_bins "$DESKTOP_CLIENT_BIN"
    cp_desktop_client_bins "fonts" "vox" "help"
    ln -s "../lib" "$WORK_DIR/$DESKTOP_CLIENT_INSTALL_PATH/lib" #< rpath: [$ORIGIN/..lib]
    cp_files "$PACKAGES_DIR/$PACKAGE_QT/plugins" \
        "{imageformats,platforminputcontexts,platforms,xcbglintegrations,audio}" \
        "$DESKTOP_CLIENT_INSTALL_PATH/bin"
    cp_files "$PACKAGES_DIR/$PACKAGE_QT" "qml" "$DESKTOP_CLIENT_INSTALL_PATH/bin"

    echo "Copying qt.conf and creating Qt symlinks"
    # TODO: Remove these symlinks when cmake build is fixed.
    cp_desktop_client_bins "qt.conf"
    local -r PLUGINS_DIR="$WORK_DIR/$DESKTOP_CLIENT_INSTALL_PATH/plugins"
    mkdir "$PLUGINS_DIR"
    local PLUGIN
    for PLUGIN in audio imageformats platforminputcontexts platforms xcbglintegrations
    do
        ln -s "../bin/$PLUGIN" "$PLUGINS_DIR/$PLUGIN"
    done
    ln -s "bin/qml" "$WORK_DIR/$DESKTOP_CLIENT_INSTALL_PATH/qml"
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
    cp_sys_lib libstdc++.so.6 "$WORK_DIR/$LIB_INSTALL_PATH"
    cp_sys_lib libatomic.so.1.2.0 "$WORK_DIR/$LIB_INSTALL_PATH"

    echo "Creating archive $TGZ_NAME"
    ( cd "$WORK_DIR" && tar czf "$TGZ_NAME" * )

    echo "Moving archive to $OUTPUT_DIR"
    mv "$WORK_DIR/$TGZ_NAME" "$OUTPUT_DIR/"
}

#--------------------------------------------------------------------------------------------------

main()
{
    distr_prepareToBuildDistribution "$WORK_DIR" "$LOG_FILE" "$@"

    buildDistribution
}

main "$@"
