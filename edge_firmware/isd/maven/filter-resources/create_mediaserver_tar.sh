#!/bin/bash
set -e
shopt -s nullglob

CUSTOMIZATION="${deb.customization.company.name}"
PRODUCT_NAME="${product.name.short}"
VERSION="${release.version}.${buildNumber}"
MAJOR_VERSION="${parsedVersion.majorVersion}"
MINOR_VERSION="${parsedVersion.minorVersion}"
BUILD_VERSION="${parsedVersion.incrementalVersion}"
TAR_FILENAME="${artifact.name.server}.tar.gz"
ZIP_FILENAME="${artifact.name.server_update}.zip"
SRC_DIR="${project.build.directory}"
BUILD_DIR="${libdir}"
LIB_BUILD_DIR="$BUILD_DIR/lib/${build.configuration}"
BIN_BUILD_DIR="$BUILD_DIR/bin/${build.configuration}"
QT_DIR="${qt.dir}"
BOX="${box}"

INSTALL_PATH="usr/local/apps/$CUSTOMIZATION"
SYMLINK_INSTALL_PATH="opt/$CUSTOMIZATION" #< If not empty, will refer to $INSTALL_PATH.
SDCARD_STUFF_PATH="sdcard/${CUSTOMIZATION}_service"

#--------------------------------------------------------------------------------------------------

help()
{
    echo "Options:"
    echo "-h"
}

# [out] TARGET_DIR
parseArgs() # "$@"
{
    local ARG
    for ARG in "$@"; do
        if [ "$ARG" = "-h" -o "$ARG" = "--help"  ]; then
            help
            exit 0
        fi
    done
}

# [in] LIB_INSTALL_DIR
copyBuildLibs()
{
    mkdir -p "$LIB_INSTALL_DIR"

    local LIBS_TO_COPY=(
        # nx
        libappserver2
        libcloud_db_client
        libcommon
        libmediaserver_core
        libnx_email
        libnx_fusion
        libnx_kit
        libnx_network
        libnx_utils

        # ffmpeg
        libavcodec
        libavdevice
        libavfilter
        libavformat
        libavutil
        libswresample
        libswscale
        libpostproc

        # third-party
        liblber
        libldap
        libquazip
        libsasl2
        libsigar
        libudt
    )

    local OPTIONAL_LIBS_TO_COPY=(
        libvpx
    )

    local LIB
    for LIB in "${LIBS_TO_COPY[@]}"; do
        local EXISTING_LIB
        for EXISTING_LIB in "$LIB_BUILD_DIR/$LIB"*; do
            if [[ $EXISTING_LIB != *.debug ]]; then
                echo "Adding $(basename "$EXISTING_LIB")"
                cp -r "$EXISTING_LIB" "$LIB_INSTALL_DIR/"
            fi
        done
    done
    for LIB in "${OPTIONAL_LIBS_TO_COPY[@]}"; do
        local EXISTING_LIB
        for EXISTING_LIB in "$LIB_BUILD_DIR/$LIB"*; do
            if [ -f "$EXISTING_LIB" ]; then
                echo "Adding (optional) $(basename "$EXISTING_LIB")"
                cp -r "$EXISTING_LIB" "$LIB_INSTALL_DIR/"
            fi
        done
    done
}

# [in] LIB_INSTALL_DIR
# [in] SDCARD_STUFF_DIR
copyQtLibs()
{
    # TODO: Implement SD Card support in create_mediaserver_tar.sh for bpi|bananapi|rpi.

    # To save storage space, Qt libs are copied onto sdcard which does not support symlinks.

    mkdir -p "$LIB_INSTALL_DIR"

    local QT_LIBS_TO_COPY=(
        Concurrent
        Core
        Gui
        Xml
        XmlPatterns
        Network
        Multimedia
        Sql
        WebSockets
    )

    local QT_LIB
    for QT_LIB in "${QT_LIBS_TO_COPY[@]}"; do
        local LIB_FILENAME="libQt5$QT_LIB.so"
        echo "Adding (Qt) $LIB_FILENAME"
        local FILE
        for FILE in "$QT_DIR/lib/$LIB_FILENAME"*; do
            if [ -L "$FILE" ]; then
                # FILE is a symlink - put to install dir.
                cp -r "$FILE" "$LIB_INSTALL_DIR/"
            else
                # FILE is not a symlink - put to sdcard.
                cp -r "$FILE" "$SDCARD_STUFF_DIR/"
                ln -s "/$SDCARD_STUFF_PATH/$(basename "$FILE")" \
                    "$LIB_INSTALL_DIR/$(basename "$FILE")"
            fi
        done
    done
}

# [in] MEDIASERVER_BIN_INSTALL_DIR
copyBins()
{
    mkdir -p "$MEDIASERVER_BIN_INSTALL_DIR"
    cp "$BIN_BUILD_DIR/mediaserver" "$MEDIASERVER_BIN_INSTALL_DIR/"
    cp "$BIN_BUILD_DIR/external.dat" "$MEDIASERVER_BIN_INSTALL_DIR/"

    # TODO: edge1. Plugins from $BIN_BUILD_DIR/plugins are not copied.
    mkdir -p "$MEDIASERVER_BIN_INSTALL_DIR/plugins/"
    cp "$LIB_BUILD_DIR/libcpro_ipnc_plugin.so.1.0.0" "$MEDIASERVER_BIN_INSTALL_DIR/plugins/"
}

# [in] INSTALL_DIR
copyConf()
{
    # TODO: edge1.
    local MEDIASERVER_CONF_FILENAME="mediaserver.conf"

    mkdir -p "$INSTALL_DIR/mediaserver/etc"
    cp "$SRC_DIR/opt/networkoptix/mediaserver/etc/$MEDIASERVER_CONF_FILENAME" \
        "$INSTALL_DIR/mediaserver/etc/"
}

# [in] TAR_DIR
copyScripts()
{
    # TODO: edge1.
    mkdir -p "$TAR_DIR/etc/init.d"
    install -m 755 "$SRC_DIR/etc/init.d/S99networkoptix-mediaserver" \
        "$TAR_DIR/etc/init.d/S99$CUSTOMIZATION-mediaserver"
}

# [in] WORK_DIR
# [in] TAR_DIR
buildArchives()
{
    echo "Creating main distro .tar.gz"
    mkdir -p "$TAR_DIR/$(dirname "$SYMLINK_INSTALL_PATH")"
    ln -s "/$INSTALL_PATH" "$TAR_DIR/$SYMLINK_INSTALL_PATH"
    ( cd "$TAR_DIR" && tar czf "$SRC_DIR/$TAR_FILENAME" * )

    echo "Creating \"update\" .zip"
    local ZIP_DIR="$WORK_DIR/zip"
    mkdir -p "$ZIP_DIR"
    cp -r "$SRC_DIR/$TAR_FILENAME" "$ZIP_DIR/"
    cp -r "$SRC_DIR"/update.* "$ZIP_DIR/"
    cp -r "$SRC_DIR"/install.sh "$ZIP_DIR/"
    ( cd "$ZIP_DIR" && zip "$SRC_DIR/$ZIP_FILENAME" * )

    echo "Creating debug symbols .tar.gz"
    local DEBUG_TAR_DIR="$WORK_DIR/debug-symbols-tar"
    mkdir -p "$DEBUG_TAR_DIR"
    local FILE
    for FILE in \
        "$LIB_BUILD_DIR"/*.debug \
        "$BIN_BUILD_DIR"/*.debug \
        "$BIN_BUILD_DIR/plugins"/*.debug \
    ; do
        cp -r "$FILE" "$DEBUG_TAR_DIR/"
    done
    ( cd "$DEBUG_TAR_DIR" && tar czf "$SRC_DIR/$TAR_FILENAME-debug-symbols.tar.gz" * )
}

#--------------------------------------------------------------------------------------------------

main()
{
    parseArgs "$@"

    local WORK_DIR=$(mktemp -d)
    rm -rf "$WORK_DIR"

    local TAR_DIR="$WORK_DIR/tar"
    local INSTALL_DIR="$TAR_DIR/$INSTALL_PATH"
    local LIB_INSTALL_DIR="$INSTALL_DIR/mediaserver/lib"
    local MEDIASERVER_BIN_INSTALL_DIR="$INSTALL_DIR/mediaserver/bin"
    local SDCARD_STUFF_DIR="$TAR_DIR/$SDCARD_STUFF_PATH"
    mkdir -p "$SDCARD_STUFF_DIR"
    echo "Creating distribution in $WORK_DIR (please delete it on failure). Current dir: $(pwd)"

    mkdir -p "$INSTALL_DIR"
    echo "$VERSION" >"$INSTALL_DIR/version.txt"

    copyBuildLibs
    copyQtLibs
    copyBins
    copyConf
    copyScripts

    buildArchives

    rm -rf "$WORK_DIR"
}

main "$@"
