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
INSTALL_PATH="sdcard/$CUSTOMIZATION"

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

copyLibs()
{
    mkdir -p "$ROOT_DIR/$MODULE/lib/"

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
        libnx_network.so
        libnx_streaming.so
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
    [ -e "$LIBS_DIR/libcreateprocess.so" ] && LIBS_TO_COPY+=( libcreateprocess.so )

    local LIB
    for LIB in "${LIBS_TO_COPY[@]}"; do
        echo "Adding lib $LIB"
        cp -r "$LIBS_DIR/$LIB"* "$ROOT_DIR/$MODULE/lib/"
        if [ ! -z "$STRIP" ]; then
            "$STRIP" "$ROOT_DIR/$MODULE/lib/$LIB"
        fi
    done
    # TODO: Use "find" instead of "*" to copy all libs except ".debug".
    rm "$ROOT_DIR/$MODULE/lib"/*.debug #< Delete debug symbol files copied above via "*".

    QT_LIBS_TO_COPY=( Core Gui Xml XmlPatterns Concurrent Network Sql Multimedia )

    local QT_LIB
    for QT_LIB in "${QT_LIBS_TO_COPY[@]}"; do
        local LIB="libQt5$QT_LIB.so"
        echo "Adding Qt lib $LIB"
        cp -r "$QT_DIR/lib/$LIB"* "$ROOT_DIR/$MODULE/lib/"
    done
}

copyBins()
{
    mkdir -p "$ROOT_DIR/$MODULE/bin/"
    cp "$BINS_DIR/mediaserver" "$ROOT_DIR/$MODULE/bin/"
    cp "$BINS_DIR/external.dat" "$ROOT_DIR/$MODILE/bin"

    mkdir -p "$ROOT_DIR/$MODULE/bin/plugins/"
    cp "$BINS_DIR/plugins/libisd_native_plugin.so" "$ROOT_DIR/$MODULE/bin/plugins/"
}

copyConf()
{
    mkdir -p "$ROOT_DIR/$MODULE/etc/"
    cp "sdcard/networkoptix/$MODULE/etc/mediaserver.conf.template" \
        "$ROOT_DIR/$MODULE/etc/mediaserver.conf"
}

copyScripts()
{
    mkdir -p "$WORK_DIR/etc/init.d"
    install -m 755 "$RESOURCE_BUILD_DIR/etc/init.d/S99networkoptix-$MODULE" \
        "$WORK_DIR/etc/init.d/S99$CUSTOMIZATION-$MODULE"
}

buildArchives()
{
    pushd "$WORK_DIR" >/dev/null
    mkdir -p opt
    ln -s "/$INSTALL_PATH" "opt/$CUSTOMIZATION"
    tar czf "$RESOURCE_BUILD_DIR/$PACKAGE_NAME" "$INSTALL_PATH" etc opt
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

    # Create "update" zip.
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
    parseArgs || exit $?

    local WORK_DIR=$(mktemp -d)
    rm -rf "$WORK_DIR"
    local ROOT_DIR="$WORK_DIR/$INSTALL_PATH"
    mkdir -p "$ROOT_DIR"
    echo "Creating distribution in $WORK_DIR (please delete it on failure)."

    echo "$VERSION" >"$ROOT_DIR/version.txt"

    copyLibs
    copyBins
    copyConf
    copyScripts

    buildArchives

    rm -rf "$WORK_DIR"
}

main "$@"
