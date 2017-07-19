#!/bin/bash
set -e

CUSTOMIZATION=${deb.customization.company.name}
PRODUCT_NAME=${product.name.short}
VERSION=${release.version}.${buildNumber}
MAJOR_VERSION="${parsedVersion.majorVersion}"
MINOR_VERSION="${parsedVersion.minorVersion}"
BUILD_VERSION="${parsedVersion.incrementalVersion}"
PACKAGE_NAME=${artifact.name.server}.tar.gz
UPDATE_NAME=${artifact.name.server_update}.zip
RESOURCE_BUILD_DIR="${project.build.directory}"
BUILD_OUTPUT_DIR=${libdir}
LIBS_DIR=$BUILD_OUTPUT_DIR/lib/${build.configuration}
BINS_DIR=$BUILD_OUTPUT_DIR/bin/${build.configuration}
QT_DIR="${qt.dir}"
BOX="${box}"
VOX_SOURCE_DIR=${ClientVoxSourceDir}

MODULE="mediaserver"
INSTALL_PATH="opt/$CUSTOMIZATION"

if [ "$BOX" = "bananapi" ]; then #< Bananapi uses bpi toolchain.
    PACKAGES_ROOT="$environment/packages/bpi"
    TOOLCHAIN_ROOT="$environment/packages/bpi/gcc-${gcc.version}"
else
    PACKAGES_ROOT="$environment/packages/$BOX"
    TOOLCHAIN_ROOT="$environment/packages/$BOX/gcc-${gcc.version}"
fi

TOOLCHAIN_PREFIX="$TOOLCHAIN_ROOT/bin/arm-linux-gnueabihf-"
SYSROOT_PREFIX="$PACKAGES_ROOT/sysroot/usr/lib/arm-linux-gnueabihf"

WORK_DIR=$(mktemp -d)
TAR_DIR="$WORK_DIR/tar"
DEBUG_TAR_DIR="$WORK_DIR/debug-tar"
if [ "$BOX" = "bpi" ]; then
    TARGET_LIB_PATH="$INSTALL_PATH/lib"
else
    TARGET_LIB_PATH="$INSTALL_PATH/mediaserver/lib"
fi

DEBS_DIR="$BUILD_OUTPUT_DIR/deb"
UBOOT_DIR="$BUILD_OUTPUT_DIR/root"
USR_DIR="$BUILD_OUTPUT_DIR/usr"

help()
{
    echo "Options:"
    echo "--target-dir=<dir to copy archive to>"
    echo "--no-strip"
    echo "--no-client"
}

# [out] TARGET_DIR
# [out] STRIP
# [out] WITH_CLIENT
parseArgs() # "$@"
{
    STRIP=""
    WITH_CLIENT="1"

    local ARG
    for ARG in "$@"; do
        if [ "$ARG" = "-h" -o "$ARG" = "--help" ]; then
            help
            exit 0
        elif [[ "$ARG" =~ ^--target-dir= ]] ; then
            TARGET_DIR=${ARG#--target-dir=}
        elif [ "$ARG" = "--no-strip" ] ; then
            STRIP=""
        elif [ "$i" = "--no-client" ] ; then
            WITH_CLIENT=""
        fi
    done
}

# Copy debug version of LIB to DEBUG_LIB via objcopy, and strip the original LIB.
copyAndStripIfNeeded() # LIB DEBUG_LIB
{
    local LIB="$1"
    local DEBUG_LIB="$2"

    if [ ! -z "$STRIP" ]; then
        "${TOOLCHAIN_PREFIX}objcopy" --only-keep-debug "$LIB" "$DEBUG_LIB.debug"
        "${TOOLCHAIN_PREFIX}objcopy" --add-gnu-debuglink="$DEBUG_LIB.debug" "$LIB"
        "${TOOLCHAIN_PREFIX}strip" -g "$LIB"
    fi
}

copyNonQtLibs()
{
    local LIBS_TO_COPY=(
        libavcodec
        libavdevice
        libavfilter
        libavformat
        libavutil
        liblber-2.4
        libldap-2.4
        libldap_r-2.4
        libquazip
        libsasl2
        libsigar
        libswresample
        libswscale
        libappserver2
        libcloud_db_client
        libcommon
        libmediaserver_core
        libnx_email
        libnx_fusion
        libnx_kit
        libnx_network
        libnx_utils
        libpostproc
        libudt
    )

    if [ "$BOX" = "bpi" ] || [ "$BOX" = "bananapi" ]; then
        LIBS_TO_COPY+=(
            # Put non-raspberry pi (bananapi, nx1) specific server libs here.
            libGLESv2
            libMali
            libUMP
        )
    fi

    # Additional libs for nx1.
    if [ "$BOX" = "bpi" ] && [ ! -z "$WITH_CLIENT" ]; then
        LIBS_TO_COPY+=(
            ldpreloadhook
            libcedrus
            libnx_vms_utils
            libnx_client_core
            libnx_audio
            libnx_media
            libopenal
            libproxydecoder
            libEGL
            libGLESv1_CM
            libpixman-1
            libvdpau_sunxi
        )
    fi

    [ -e "$LIBS_DIR/libvpx.so.1.2.0" ] && LIBS_TO_COPY+=(libvpx.so)

    if [ ! "$CUSTOMIZATION" = "networkoptix" ]; then
        mv -f "opt/networkoptix" "opt/$CUSTOMIZATION"
    fi
    rm -rf "$TAR_DIR"
    mkdir -p "$TAR_DIR/$INSTALL_PATH"
    echo "$VERSION" >"$TAR_DIR/$INSTALL_PATH/version.txt"

    # Copy libs.
    mkdir -p "$TAR_DIR/$TARGET_LIB_PATH"
    mkdir -p "$DEBUG_TAR_DIR/$TARGET_LIB_PATH"
    for LIB in "${LIBS_TO_COPY[@]}"; do
        echo "Adding lib $LIB"
        cp -r "$LIBS_DIR/$LIB"* "$TAR_DIR/$TARGET_LIB_PATH/"
        copyAndStripIfNeeded "$TAR_DIR/$TARGET_LIB_PATH/$LIB" "$DEBUG_TAR_DIR/$TARGET_LIB_PATH/$LIB"
    done
}

copyQtLibs()
{
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

    if [ "$BOX" = "bpi" ] && [ ! -z "$WITH_CLIENT" ]; then
        QT_LIBS_TO_COPY+=(
            WebChannel
            WebEngine
            WebEngineCore
            WebView
            MultimediaQuick_p
            Qml
            Quick
            LabsTemplates
            DBus
            EglDeviceIntegration
        )
    fi

    local QT_LIB
    for QT_LIB in "${QT_LIBS_TO_COPY[@]}"; do
        local LIB=libQt5$QT_LIB.so
        echo "Adding Qt lib $LIB"
        cp -r "$QT_DIR/lib/$LIB"* "$TAR_DIR/$TARGET_LIB_PATH/"
    done
}

copyBins()
{
    mkdir -p "$TAR_DIR/$INSTALL_PATH/mediaserver/bin"
    mkdir -p "$DEBUG_TAR_DIR/$INSTALL_PATH/mediaserver/bin"
    cp "$BINS_DIR/mediaserver" "$TAR_DIR/$INSTALL_PATH/mediaserver/bin/"
    cp "$BINS_DIR/external.dat" "$TAR_DIR/$INSTALL_PATH/mediaserver/bin/"
    copyAndStripIfNeeded "$TAR_DIR/$INSTALL_PATH/mediaserver/bin/mediaserver" \
        "$DEBUG_TAR_DIR/$INSTALL_PATH/mediaserver/bin/mediaserver"

    if [ -e "$BINS_DIR/plugins" ]; then
        mkdir -p "$TAR_DIR/$INSTALL_PATH/mediaserver/bin/plugins"
        mkdir -p "$DEBUG_TAR_DIR/$INSTALL_PATH/mediaserver/bin/plugins"
        cp "$BINS_DIR/plugins"/*.* "$TAR_DIR/$INSTALL_PATH/mediaserver/bin/plugins/"
        # TODO: mshevchenko: Eliminate "ls".
        for FILE in $(ls $TAR_DIR/$INSTALL_PATH/mediaserver/bin/plugins/); do
            copyAndStripIfNeeded "$TAR_DIR/$INSTALL_PATH/mediaserver/bin/plugins/$FILE" \
                "$DEBUG_TAR_DIR/$INSTALL_PATH/mediaserver/bin/plugins/$FILE"
        done
    fi
}

copyConf()
{
    mkdir -p $TAR_DIR/$INSTALL_PATH/mediaserver/etc/
    cp opt/$CUSTOMIZATION/mediaserver/etc/mediaserver.conf.template \
        $TAR_DIR/$INSTALL_PATH/mediaserver/etc
}

# Copy the autostart script and platform-specific scripts.
copyScripts()
{
    cp -r ./etc "$TAR_DIR"
    cp -r ./opt "$TAR_DIR"

    NON_CUSTOMIZED_MEDIASERVER_STARTUP_SCRIPT="$TAR_DIR/etc/init.d/networkoptix-mediaserver"
    MEDIASERVER_STARTUP_SCRIPT="$TAR_DIR/etc/init.d/$CUSTOMIZATION-mediaserver"
    NON_CUSTOMIZED_LITE_CLIENT_STARTUP_SCRIPT="$TAR_DIR/etc/init.d/networkoptix-lite-client"
    LITE_CLIENT_STARTUP_SCRIPT="$TAR_DIR/etc/init.d/$CUSTOMIZATION-lite-client"

    if [ ! "$CUSTOMIZATION" = "networkoptix" ]; then
        if [ -f "$NON_CUSTOMIZED_MEDIASERVER_STARTUP_SCRIPT" ]; then
            mv "$NON_CUSTOMIZED_MEDIASERVER_STARTUP_SCRIPT" "$MEDIASERVER_STARTUP_SCRIPT"
        fi
        if [ -f "$NON_CUSTOMIZED_LITE_CLIENT_STARTUP_SCRIPT" ]; then
            mv "$NON_CUSTOMIZED_LITE_CLIENT_STARTUP_SCRIPT" "$LITE_CLIENT_STARTUP_SCRIPT"
        fi
    fi

    chmod -R 755 "$TAR_DIR/etc/init.d"
}

copyDebs()
{
    cp -r "$DEBS_DIR" "$TAR_DIR/opt/"
}

copyBpiLiteClient()
{
    # Copy libs of ffmpeg for proxydecoder, if they exist.
    if [ -d "$LIBS_DIR/ffmpeg" ]; then
        cp -r "$LIBS_DIR/ffmpeg" "$TAR_DIR/$TARGET_LIB_PATH/"
    fi

    # Copy lite_client bin.
    mkdir -p "$TAR_DIR/$INSTALL_PATH/lite_client/bin"
    mkdir -p "$DEBUG_TAR_DIR/$INSTALL_PATH/lite_client/bin"
    cp "$BINS_DIR/mobile_client" "$TAR_DIR/$INSTALL_PATH/lite_client/bin/"
    copyAndStripIfNeeded "$TAR_DIR/$INSTALL_PATH/lite_client/bin/mobile_client" \
        "$DEBUG_TAR_DIR/$INSTALL_PATH/lite_client/bin/mobile_client"

    # Create symlink for rpath needed by mediaserver binary.
    ln -s "../lib" "$TAR_DIR/$INSTALL_PATH/mediaserver/lib"

    # Create symlink for rpath needed by mobile_client binary.
    ln -s "../lib" "$TAR_DIR/$INSTALL_PATH/lite_client/lib"

    # Create symlink for rpath needed by Qt plugins.
    ln -s "../../lib" "$TAR_DIR/$INSTALL_PATH/lite_client/bin/lib"

    # Copy directories needed by lite client.
    local DIRS_TO_COPY=(
        egldeviceintegrations
        fonts
        imageformats
        mobile_client
        platforms
        qml
        video
    )
    for DIR in "${DIRS_TO_COPY[@]}"; do
        echo "Copying directory $DIR"
        cp -r "$BINS_DIR/$DIR" "$TAR_DIR/$INSTALL_PATH/lite_client/bin/"
    done

    # Copying uboot.
    cp -r "$UBOOT_DIR" "$TAR_DIR/root/"

    # Copy additional binaries.
    cp -r "$USR_DIR" "$TAR_DIR/usr/"

    # Copy additional platform-specific files.
    cp -r "$QT_DIR/libexec" "$TAR_DIR/$INSTALL_PATH/lite_client/bin/"
    mkdir -p "$TAR_DIR/$INSTALL_PATH/lite_client/bin/translations"
    cp -r "$QT_DIR/translations" "$TAR_DIR/$INSTALL_PATH/lite_client/bin/"
    cp -r "$QT_DIR/resources" "$TAR_DIR/$INSTALL_PATH/lite_client/bin/"
    cp "$QT_DIR/resources/"* "$TAR_DIR/$INSTALL_PATH/lite_client/bin/libexec/"
    cp -r ./root "$TAR_DIR/"
    mkdir -p "$TAR_DIR/root/tools/nx"
    cp "opt/$CUSTOMIZATION/mediaserver/etc/mediaserver.conf.template" "$TAR_DIR/root/tools/nx/"
    chmod -R 755 "$TAR_DIR/$INSTALL_PATH/mediaserver/var/scripts"
}

copyAdditionalSysrootFilesIfNeeded()
{
    if [ "$BOX" = "bpi" ]; then
        local SYSROOT_LIBS_TO_COPY=(
            libopus
            libvpx
            libwebpdemux
            libwebp
        )
        for LIB in ${SYSROOT_LIBS_TO_COPY[@]}; do
            echo "Adding lib $LIB"
            cp -r "$SYSROOT_PREFIX/$LIB"* "$TAR_DIR/$TARGET_LIB_PATH/"
        done
    elif [ "$BOX" = "bananapi" ]; then
        # Add files required for bananapi on Debian 8 "Jessie".
        cp -r "$SYSROOT_PREFIX/libglib"* "$TAR_DIR/$TARGET_LIB_PATH/"
        cp -r "$PACKAGES_ROOT/sysroot/usr/bin/hdparm" "$TAR_DIR/$INSTALL_PATH/mediaserver/bin/"
    fi
}

copyVox()
{
    local VOX_TARGET_DIR="$TAR_DIR/$INSTALL_PATH/$MODULE/bin/vox"
    mkdir -p "$VOX_TARGET_DIR"
    cp -r "$VOX_SOURCE_DIR/"* "$VOX_TARGET_DIR/"
}

copyLibcIfNeeded()
{
    if [ "$BOX" = "bpi" ] || [ "$BOX" = "bananapi" ]; then
        # Uncomment to enable libc/libstdc++ upgrade.
        #cp -P "$PACKAGES_ROOT/libstdc++-6.0.20/lib/libstdc++.s"* "$TAR_DIR/$TARGET_LIB_PATH/"
        cp -P "$PACKAGES_ROOT/libstdc++-6.0.19/lib/libstdc++.s"* "$TAR_DIR/$TARGET_LIB_PATH/"
    fi
}

buildArchives()
{
    pushd "$TAR_DIR"
    if [ "$BOX" = "bpi" ]; then
        if [ ! -z "$WITH_CLIENT" ]; then
            tar czf "$PACKAGE_NAME" ./opt ./etc ./root ./usr
        else
            tar czf "$PACKAGE_NAME" ./opt ./etc
        fi
    else
        tar czf "$PACKAGE_NAME" ./opt ./etc
    fi
    cp "$PACKAGE_NAME" "$RESOURCE_BUILD_DIR/"
    popd

    if [ ! -z "$STRIP" ]; then
        # TODO: #mshevchenko: Also add Lite Client bin(s) to the debug archive.
        pushd "$DEBUG_TAR_DIR/$INSTALL_PATH/mediaserver"
        tar czf "$PACKAGE-debug-symbols.tar.gz" ./bin ./lib
        cp "$PACKAGE-debug-symbols.tar.gz" "$RESOURCE_BUILD_DIR/"
        popd
    fi

    mkdir -p zip
    mv "$PACKAGE_NAME" ./zip
    mv update.* ./zip
    mv install.sh ./zip
    cd zip
    if [ ! -f "$PACKAGE_NAME" ]; then
        echo "ERROR: Distribution is not created."
        exit 1
    fi
    zip "./$UPDATE_NAME" ./*
    mv ./* ../
    cd ..
}

main()
{
    parseArgs "$@"

    copyNonQtLibs
    copyQtLibs
    copyBins
    copyConf
    copyScripts
    copyDebs
    copyAdditionalSysrootFilesIfNeeded
    copyVox
    copyLibcIfNeeded

    if [ "$BOX" = "bpi" ] && [ ! -z "$WITH_CLIENT" ]; then
        copyBpiLiteClient
    fi

    buildArchives

    rm -rf zip
    rm -rf "$TAR_DIR"
    rm -rf "$WORK_DIR"
    rm -rf "$DEBUG_TAR_DIR"
}

main "$@"
