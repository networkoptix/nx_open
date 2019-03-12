#!/bin/bash
set -e #< Stop on errors.
set -u #< Fail on undefined variables.

source "$(dirname $0)/../build_distribution_utils.sh"

distrib_loadConfig "build_distribution.conf"

WORK_DIR="build_distribution_tmp"
LOG_FILE="$LOGS_DIR/build_distribution.log"

VMS_INSTALL_PATH="opt/$CUSTOMIZATION"

DESKTOP_CLIENT_INSTALL_PATH="$VMS_INSTALL_PATH/desktop_client"
MEDIASERVER_INSTALL_PATH="$VMS_INSTALL_PATH/mediaserver"

TEGRA_VIDEO_SOURCE_DIR="$SOURCE_DIR/artifacts/tx1/tegra_multimedia_api"
NVIDIA_MODELS_SOURCE_DIR="$TEGRA_VIDEO_SOURCE_DIR/data/model" #< Demo neural networks.
NVIDIA_MODELS_INSTALL_PATH="$MEDIASERVER_INSTALL_PATH/nvidia_models"

PACKAGE_SIGAR="sigar-1.7"
PACKAGE_FFMPEG="ffmpeg-3.1.1"

LIB_INSTALL_PATH="$VMS_INSTALL_PATH/lib"

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

copyMediaserverPlugins()
{
    echo ""
    echo "Copying mediaserver plugins"

    local -r BIN_DIR="$WORK_DIR/$MEDIASERVER_INSTALL_PATH/bin"

    distrib_copyMediaserverPlugins "plugins" "$BIN_DIR" "${SERVER_PLUGINS[@]}"
    distrib_copyMediaserverPlugins "plugins_optional" "$BIN_DIR" "${SERVER_PLUGINS_OPTIONAL[@]}"
}

buildDistribution()
{
    echo "Copying build_info.txt"
    mkdir -p "$WORK_DIR/$VMS_INSTALL_PATH"
    cp -r "$BUILD_DIR/build_info.txt" "$WORK_DIR/$VMS_INSTALL_PATH/"

    echo "Copying libs"
    mkdir -p "$WORK_DIR/$LIB_INSTALL_PATH"
    cp_libs "*.so!(.debug)"

    echo "Copying mediaserver"
    cp_mediaserver_bins "mediaserver" "external.dat" "vox"
    ln -s "../lib" "$WORK_DIR/$MEDIASERVER_INSTALL_PATH/lib" #< rpath: [$ORIGIN/..lib]

    echo "Copying Qt plugins for mediaserver"
    local -r QT_PLUGINS_INSTALL_DIR="$WORK_DIR/$MEDIASERVER_INSTALL_PATH/plugins"
    mkdir -p "$QT_PLUGINS_INSTALL_DIR"
    local -r PLUGINS=(
        sqldrivers/libqsqlite.so
    )
    for PLUGIN in "${PLUGINS[@]}"
    do
        echo "  Copying (Qt plugin) $PLUGIN"

        mkdir -p "$QT_PLUGINS_INSTALL_DIR/$(dirname $PLUGIN)"
        cp -r "$QT_DIR/plugins/$PLUGIN" "$QT_PLUGINS_INSTALL_DIR/$PLUGIN"
    done

    echo "Copying qt.conf"
    cp -r "$CURRENT_BUILD_DIR/qt.conf" "$WORK_DIR/$MEDIASERVER_INSTALL_PATH/bin/"

    copyMediaserverPlugins

    echo "Copying tegra_video analytics"
    cp_package_libs "tegra_video" #< Tegra-specific plugin for video decoding and neural networks.
    cp_files "$NVIDIA_MODELS_SOURCE_DIR" "*" "$NVIDIA_MODELS_INSTALL_PATH" #< Demo neural networks.

    echo "Copying desktop_client"
    cp_desktop_client_bins "$DESKTOP_CLIENT_BIN"
    cp_desktop_client_bins "fonts" "vox" "help"
    ln -s "../lib" "$WORK_DIR/$DESKTOP_CLIENT_INSTALL_PATH/lib" #< rpath: [$ORIGIN/..lib]
    cp_files "$QT_DIR/plugins" \
        "{imageformats,platforminputcontexts,platforms,xcbglintegrations,audio}" \
        "$DESKTOP_CLIENT_INSTALL_PATH/bin"
    cp_files "$QT_DIR" "qml" "$DESKTOP_CLIENT_INSTALL_PATH/bin"

    echo "Copying qt.conf and creating Qt symlinks"
    # TODO: Remove these symlinks when cmake build is improved accordingly.
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
    cp_package_libs "$(basename "$QT_DIR")" "$PACKAGE_FFMPEG" "$PACKAGE_SIGAR"

    echo "Copying compiler (system) libs"
    distrib_copySystemLibs "$WORK_DIR/$LIB_INSTALL_PATH" \
        "libstdc++.so.6" "libgcc_s.so.1" "libatomic.so.1"

    echo "Creating main distribution tgz"
    distrib_createArchive "$DISTRIBUTION_OUTPUT_DIR/$DISTRIBUTION_TGZ" "$WORK_DIR" tar czf
}

#--------------------------------------------------------------------------------------------------

main()
{
    distrib_prepareToBuildDistribution "$WORK_DIR" "$LOG_FILE" "$@"

    buildDistribution
}

main "$@"
