#!/bin/bash
set -e

CUSTOMIZATION=${deb.customization.company.name}
PRODUCT_NAME="${product.name.short}"
MODULE="mediaserver"
VERSION="${release.version}.${buildNumber}"
MAJOR_VERSION="${parsedVersion.majorVersion}"
MINOR_VERSION="${parsedVersion.minorVersion}"
BUILD_VERSION="${parsedVersion.incrementalVersion}"
PACKAGE_NAME="${artifact.name.server}.tar.gz"
UPDATE_NAME="${artifact.name.server_update}.zip"
BUILD_OUTPUT_DIR="${libdir}"
RESOURCE_BUILD_DIR="${project.build.directory}"
LIBS_DIR="$BUILD_OUTPUT_DIR/lib/${build.configuration}"
BINS_DIR="$BUILD_OUTPUT_DIR/bin/${build.configuration}"
STRIP="${packages.dir}/${rdep.target}/gcc-${gcc.version}/bin/arm-linux-gnueabihf-strip"
QT_DIR="${qt.dir}"
INSTALL_PATH="usr/local/apps/$CUSTOMIZATION"
INSTALL_SYMLINK_PATH="opt/$CUSTOMIZATION" #< If not empty, will refer to $INSTALL_PATH.
SDCARD_STUFF_PATH="sdcard/${CUSTOMIZATION}_service"

help()
{
    echo "Options:"
    echo "--target-dir=<dir to copy archive to>"
    echo "--no-strip"
}

parseArgs()
{
    local ARG
    for ARG in "$@"; do
        if [ "$ARG" == "-h" -o "$ARG" == "--help"  ]; then
            help
            exit 0
        elif [[ "$ARG" =~ ^--target-dir= ]] ; then
            TARGET_DIR=${ARG#--target-dir=}
        elif [ "$ARG" == "--no-strip" ] ; then
            STRIP=
        fi
    done
}

# [in] INSTALL_DIR
copyLibsWithoutQt()
{
    mkdir -p "$INSTALL_DIR/$MODULE/lib/"

    LIBS_TO_COPY=(
        libavcodec.so
        libavdevice.so
        libavfilter.so
        libavformat.so
        libavutil.so
        libudt.so
        libcommon.so
        libcloud_db_client.so
        libnx_fusion.so
        libnx_kit.so
        libnx_network.so
        libnx_utils.so
        libnx_email.so
        libappserver2.so
        libmediaserver_core.so
        libquazip.so
        libsasl2.so
        liblber-2.4.so.2
        libldap-2.4.so.2
        libldap_r-2.4.so.2
        libsigar.so
        libswresample.so
        libswscale.so
        libpostproc.so
    )

    [ -e "$LIBS_DIR/libvpx.so" ] && LIBS_TO_COPY+=( libvpx.so )

    local LIB
    for LIB in "${LIBS_TO_COPY[@]}"; do
        echo "Adding lib $LIB"
        cp -r "$LIBS_DIR/$LIB"* "$INSTALL_DIR/$MODULE/lib/"
        if [ ! -z "$STRIP" ]; then
            "$STRIP" "$INSTALL_DIR/$MODULE/lib/$LIB"
        fi
    done
    # TODO: Use "find" instead of "*" to copy all libs except ".debug".
    rm "$INSTALL_DIR/$MODULE/lib"/*.debug #< Delete debug symbol files copied above via "*".
}

# [in] INSTALL_DIR
# [in] SDCARD_STUFF_DIR
copyQtLibs()
{
    # To save storage space, Qt libs are copied onto sdcard which does not support symlinks.

    mkdir -p "$INSTALL_DIR/$MODULE/lib/"

    QT_LIBS_TO_COPY=( Core Gui Xml XmlPatterns Concurrent Network Sql Multimedia )

    local QT_LIB
    for QT_LIB in "${QT_LIBS_TO_COPY[@]}"; do
        local LIB="libQt5$QT_LIB.so"
        echo "Adding Qt lib $LIB"
        local FILE
        for FILE in "$QT_DIR/lib/$LIB"*; do
            if [ -L "$FILE" ]; then
                # FILE is a symlink - put to install dir.
                cp -r "$FILE" "$INSTALL_DIR/$MODULE/lib/"
            else
                # FILE is not a symlink - put to sdcard.
                cp -r "$FILE" "$SDCARD_STUFF_DIR/"
                ln -s "/$SDCARD_STUFF_PATH/$(basename "$FILE")" \
                    "$INSTALL_DIR/$MODULE/lib/$(basename "$FILE")"
            fi
        done
    done
}

# [in] INSTALL_DIR
copyBins()
{
    mkdir -p "$INSTALL_DIR/$MODULE/bin/"
    cp "$BINS_DIR/mediaserver" "$INSTALL_DIR/$MODULE/bin/"
    cp "$BINS_DIR/external.dat" "$INSTALL_DIR/$MODULE/bin/"

    mkdir -p "$INSTALL_DIR/$MODULE/bin/plugins/"
    cp "$LIBS_DIR/libcpro_ipnc_plugin.so.1.0.0" "$INSTALL_DIR/$MODULE/bin/plugins/"
}

# [in] INSTALL_DIR
copyConf()
{
    mkdir -p "$INSTALL_DIR/$MODULE/etc/"
    cp "mediaserver.conf.template" "$INSTALL_DIR/$MODULE/etc/mediaserver.conf"
}

# [in] TAR_DIR
copyScripts()
{
    mkdir -p "$TAR_DIR/etc/init.d"
    install -m 755 "$RESOURCE_BUILD_DIR/etc/init.d/S99networkoptix-$MODULE" \
        "$TAR_DIR/etc/init.d/S99$CUSTOMIZATION-$MODULE"
}

# [in] WORK_DIR
# [in] TAR_DIR
buildArchives()
{
    # Create main distro archive.
    pushd "$TAR_DIR" >/dev/null
    mkdir -p $(dirname "$INSTALL_SYMLINK_PATH")
    ln -s "/$INSTALL_PATH" "$INSTALL_SYMLINK_PATH"
    tar czf "$RESOURCE_BUILD_DIR/$PACKAGE_NAME" *
    popd >/dev/null

    [ ! -z "$TARGET_DIR" ] && cp "$RESOURCE_BUILD_DIR/$PACKAGE_NAME" "$TARGET_DIR"

    # Create debug symbols archive.
    local SYMBOLS_DIR="$WORK_DIR/debug-symbols"
    mkdir -p "$SYMBOLS_DIR"
    cp -r "$LIBS_DIR"/*.debug "$SYMBOLS_DIR/" || true
    cp -r "$BINS_DIR"/*.debug "$SYMBOLS_DIR/" || true
    cp -r "$BINS_DIR/plugins"/*.debug "$SYMBOLS_DIR/" || true
    pushd "$SYMBOLS_DIR" >/dev/null
    tar czf "$RESOURCE_BUILD_DIR/$PACKAGE_NAME-debug-symbols.tar.gz" ./*
    popd >/dev/null

    # Create "update" zip with the tar inside.
    local ZIP_DIR="$WORK_DIR/zip"
    mkdir -p "$ZIP_DIR"
    cp -r "$RESOURCE_BUILD_DIR/$PACKAGE_NAME" "$ZIP_DIR/"
    cp -r "$RESOURCE_BUILD_DIR"/update.* "$ZIP_DIR/"
    cp -r "$RESOURCE_BUILD_DIR"/install.sh "$ZIP_DIR/"
    pushd "$ZIP_DIR" >/dev/null
    zip "$RESOURCE_BUILD_DIR/$UPDATE_NAME" ./*
    popd >/dev/null
}

#--------------------------------------------------------------------------------------------------

main()
{
    parseArgs

    local WORK_DIR=$(mktemp -d)
    rm -rf "$WORK_DIR"
    local TAR_DIR="$WORK_DIR/tar"
    mkdir -p "$TAR_DIR"
    local INSTALL_DIR="$TAR_DIR/$INSTALL_PATH"
    mkdir -p "$INSTALL_DIR"
    local SDCARD_STUFF_DIR="$TAR_DIR/$SDCARD_STUFF_PATH"
    mkdir -p "$SDCARD_STUFF_DIR"
    echo "Creating distribution in $WORK_DIR (please delete it on failure)."

    echo "$VERSION" >"$INSTALL_DIR/version.txt"

    copyLibsWithoutQt
    copyQtLibs
    copyBins
    copyConf
    copyScripts

    buildArchives

    rm -rf "$WORK_DIR"
}

main "$@"
