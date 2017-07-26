#!/bin/bash
set -e

# [in] environment
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
VOX_SOURCE_DIR="${ClientVoxSourceDir}"

INSTALL_PATH="opt/$CUSTOMIZATION"

PACKAGES_DIR="${packages.dir}/${rdep.target}"

if [ "$BOX" = "bpi" ]; then
    LIB_INSTALL_PATH="$INSTALL_PATH/lib"
else
    LIB_INSTALL_PATH="$INSTALL_PATH/mediaserver/lib"
fi

#--------------------------------------------------------------------------------------------------

help()
{
    echo "Options:"
    echo "--no-client"
}

# [out] WITH_CLIENT
parseArgs() # "$@"
{
    WITH_CLIENT="1"

    local ARG
    for ARG in "$@"; do
        if [ "$ARG" = "-h" -o "$ARG" = "--help" ]; then
            help
            exit 0
        elif [ "$i" = "--no-client" ] ; then
            WITH_CLIENT=""
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

    if [ "$BOX" = "bpi" ] || [ "$BOX" = "bananapi" ]; then
        LIBS_TO_COPY+=(
            # Put non-raspberry pi (bananapi, nx1) specific server libs here.
            libGLESv2
            libMali
            libUMP
        )
    fi

    # Libs for nx1 lite client.
    if [ "$BOX" = "bpi" ] && [ ! -z "$WITH_CLIENT" ]; then
        LIBS_TO_COPY+=(
            # nx
            libnx_audio
            libnx_client_core
            libnx_media
            libnx_vms_utils

            # third-party
            ldpreloadhook
            libcedrus
            libopenal
            libpixman-1
            libproxydecoder
            libvdpau_sunxi
            libEGL
            libGLESv1_CM
        )
    fi

    # Copy libs.
    local LIB
    for LIB in "${LIBS_TO_COPY[@]}"; do
        echo "Adding lib $LIB"
        cp -r "$LIB_BUILD_DIR/$LIB"* "$LIB_INSTALL_DIR/"
    done
    for LIB in "${OPTIONAL_LIBS_TO_COPY[@]}"; do
        local EXISTING_LIB
        for EXISTING_LIB in "$LIB_BUILD_DIR/$LIB"*; do
            if [ -f "$EXISTING_LIB" ]; then
                echo "Adding optional lib file $(basename $EXISTING_LIB)"
                cp -r "$EXISTING_LIB" "$LIB_INSTALL_DIR/"
            fi
        done
    done
}

# [in] LIB_INSTALL_DIR
copyQtLibs()
{
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

    # Qt libs for nx1 lite client.
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
        local LIB="libQt5$QT_LIB.so"
        echo "Adding Qt lib $LIB"
        cp -r "$QT_DIR/lib/$LIB"* "$LIB_INSTALL_DIR/"
    done
}

# [in] MEDIASERVER_BIN_INSTALL_DIR
copyBins()
{
    mkdir -p "$MEDIASERVER_BIN_INSTALL_DIR"
    cp "$BIN_BUILD_DIR/mediaserver" "$MEDIASERVER_BIN_INSTALL_DIR/"
    cp "$BIN_BUILD_DIR/external.dat" "$MEDIASERVER_BIN_INSTALL_DIR/"

    # TODO: bpi|bananapi|rpi.
    if [ -e "$BIN_BUILD_DIR/plugins" ]; then
        mkdir -p "$MEDIASERVER_BIN_INSTALL_DIR/plugins"
        cp "$BIN_BUILD_DIR/plugins"/* "$MEDIASERVER_BIN_INSTALL_DIR/plugins/"
    fi
}

# [in] INSTALL_DIR
copyConf()
{
    # TODO: bpi|rpi|bananapi.
    local MEDIASERVER_CONF_FILENAME="mediaserver.conf.template"

    mkdir -p "$INSTALL_DIR/mediaserver/etc"
    cp "$SRC_DIR/opt/networkoptix/mediaserver/etc/$MEDIASERVER_CONF_FILENAME" \
        "$INSTALL_DIR/mediaserver/etc/"
}

# Copy the autostart script and platform-specific scripts.
# [in] TAR_DIR
# [in] INSTALL_DIR
copyScripts()
{
    # TODO: Consider more general approach: copy all dirs and files, customizing filenames,
    # assigning proper permissions. Top-level dirs (etc, opt, root, etc) should be either moved
    # to e.g. "sysroot" folder, or enumerated manually.

    echo "Copying scripts"
    cp -r "$SRC_DIR/etc" "$TAR_DIR"
    chmod -R 755 "$TAR_DIR/etc/init.d"

    cp -r "$SRC_DIR/opt/networkoptix/"* "$INSTALL_DIR/"
    local SCRIPTS_DIR="$INSTALL_DIR/mediaserver/var/scripts"
    [ -d "$SCRIPTS_DIR" ] && chmod -R 755 "$SCRIPTS_DIR"

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
}

# [in] TAR_DIR
copyDebs()
{
    local DEBS_DIR="$BUILD_DIR/deb"
    if [ -d "$DEBS_DIR" ]; then
        # Dereference symlinks to allow the build system to create .deb symlinks instead of copying
        # the .deb files.
        cp -r -L "$DEBS_DIR" "$TAR_DIR/opt/"
    fi
}

# [in] DEBUG_TAR_DIR
# [in] INSTALL_DIR
# [in] LIB_INSTALL_DIR
copyBpiLiteClient()
{
    # Copy libs of ffmpeg for proxydecoder, if they exist.
    if [ -d "$LIB_BUILD_DIR/ffmpeg" ]; then
        cp -r "$LIB_BUILD_DIR/ffmpeg" "$LIB_INSTALL_DIR/"
    fi

    # Copy lite_client bin.
    mkdir -p "$INSTALL_DIR/lite_client/bin"
    mkdir -p "$DEBUG_TAR_DIR/$INSTALL_PATH/lite_client/bin"
    cp "$BIN_BUILD_DIR/mobile_client" "$INSTALL_DIR/lite_client/bin/"

    # Create symlink for rpath needed by mediaserver binary.
    ln -s "../lib" "$INSTALL_DIR/mediaserver/lib"

    # Create symlink for rpath needed by mobile_client binary.
    ln -s "../lib" "$INSTALL_DIR/lite_client/lib"

    # Create symlink for rpath needed by Qt plugins.
    ln -s "../../lib" "$INSTALL_DIR/lite_client/bin/lib"

    # Copy directories needed for lite client.
    local DIRS_TO_COPY=(
        egldeviceintegrations
        fonts
        imageformats
        mobile_client
        platforms
        qml
        video
    )
    local DIR
    for DIR in "${DIRS_TO_COPY[@]}"; do
        echo "Copying directory $DIR"
        cp -r "$BIN_BUILD_DIR/$DIR" "$INSTALL_DIR/lite_client/bin/"
    done

    # Copy additional platform-specific Qt files.
    cp -r "$QT_DIR/libexec" "$INSTALL_DIR/lite_client/bin/"
    mkdir -p "$INSTALL_DIR/lite_client/bin/translations"
    cp -r "$QT_DIR/translations" "$INSTALL_DIR/lite_client/bin/"
    # TODO: Investigate how to get rid of "resources" duplication.
    cp -r "$QT_DIR/resources" "$INSTALL_DIR/lite_client/bin/"
    cp "$QT_DIR/resources/"* "$INSTALL_DIR/lite_client/bin/libexec/"
}

# [in] TAR_DIR
copyBpiSpecificFiles()
{
    # Copying uboot.
    cp -r "$BUILD_DIR/root" "$TAR_DIR/root/"

    # Copy additional binaries.
    cp -r "$BUILD_DIR/usr" "$TAR_DIR/usr/"

    cp -r "$SRC_DIR/root" "$TAR_DIR/"
    mkdir -p "$TAR_DIR/root/tools/nx"

    # Copy default server conf used on "factory reset".
    cp "$SRC_DIR/opt/networkoptix/mediaserver/etc/mediaserver.conf.template" \
        "$TAR_DIR/root/tools/nx/"
}

# [in] INSTALL_DIR
# [in] LIB_INSTALL_DIR
copyAdditionalSysrootFilesIfNeeded()
{
    local SYSROOT_LIB_DIR="$PACKAGES_DIR/sysroot/usr/lib/arm-linux-gnueabihf"

    if [ "$BOX" = "bpi" ]; then
        local SYSROOT_LIBS_TO_COPY=(
            libopus
            libvpx
            libwebpdemux
            libwebp
        )
        local LIB
        for LIB in "${SYSROOT_LIBS_TO_COPY[@]}"; do
            echo "Adding sysroot lib $LIB"
            cp -r "$SYSROOT_LIB_DIR/$LIB"* "$LIB_INSTALL_DIR/"
        done
    elif [ "$BOX" = "bananapi" ]; then
        # Add files required for bananapi on Debian 8 "Jessie".
        cp -r "$SYSROOT_LIB_DIR/libglib"* "$LIB_INSTALL_DIR/"
        cp -r "$PACKAGES_DIR/sysroot/usr/bin/hdparm" "$INSTALL_DIR/mediaserver/bin/"
    fi
}

# [in] INSTALL_DIR
# [in] VOX_SOURCE_DIR
copyVox()
{
    if [ -d "$VOX_SOURCE_DIR" ]; then
        local VOX_INSTALL_DIR="$INSTALL_DIR/mediaserver/bin/vox"
        mkdir -p "$VOX_INSTALL_DIR"
        cp -r "$VOX_SOURCE_DIR/"* "$VOX_INSTALL_DIR/"
    fi
}

# [in] LIB_INSTALL_DIR
copyLibcIfNeeded()
{
    # TODO: Unconditionally copy from the compiler artifact. Decision on usage will be
    # made in install.sh on the box.
    if [ "$BOX" = "bpi" ] || [ "$BOX" = "bananapi" ]; then
        cp -r "$PACKAGES_DIR/libstdc++-6.0.19/lib/libstdc++.s"* "$LIB_INSTALL_DIR/"
    fi
}

# [in] TAR_DIR
buildArchives()
{
    # Create main distro archive.
    if [ "$BOX" = "bpi" ] && [ ! -z "$WITH_CLIENT" ]; then
        ( cd "$TAR_DIR" && tar czf "$SRC_DIR/$TAR_FILENAME" ./opt ./etc ./root ./usr )
    else
        ( cd "$TAR_DIR" && tar czf "$SRC_DIR/$TAR_FILENAME" ./opt ./etc )
    fi

    # Create "update" zip with the tar inside.
    local ZIP_DIR="$WORK_DIR/zip"
    mkdir -p "$ZIP_DIR"
    cp -r "$SRC_DIR/$TAR_FILENAME" "$ZIP_DIR/"
    cp -r "$SRC_DIR"/update.* "$ZIP_DIR/"
    cp -r "$SRC_DIR"/install.sh "$ZIP_DIR/"
    ( cd "$ZIP_DIR" && zip "$SRC_DIR/$ZIP_FILENAME" * )
}

#--------------------------------------------------------------------------------------------------

main()
{
    parseArgs "$@"

    local WORK_DIR=$(mktemp -d)
    rm -rf "$WORK_DIR"

    local TAR_DIR="$WORK_DIR/tar"
    local DEBUG_TAR_DIR="$WORK_DIR/debug-symbols-tar"
    local INSTALL_DIR="$TAR_DIR/$INSTALL_PATH"
    local LIB_INSTALL_DIR="$TAR_DIR/$LIB_INSTALL_PATH"
    local MEDIASERVER_BIN_INSTALL_DIR="$INSTALL_DIR/mediaserver/bin"
    echo "Creating distribution in $WORK_DIR (please delete it on failure). Current dir: $(pwd)"

    mkdir -p "$INSTALL_DIR"
    echo "$VERSION" >"$INSTALL_DIR/version.txt"

    copyBuildLibs
    copyQtLibs
    copyBins
    copyConf
    copyScripts
    copyDebs
    copyAdditionalSysrootFilesIfNeeded
    copyVox
    copyLibcIfNeeded

    if [ "$BOX" = "bpi" ]; then
        copyBpiSpecificFiles
        if [ ! -z "$WITH_CLIENT" ]; then
            copyBpiLiteClient
        fi
    fi

    buildArchives

    rm -rf "$WORK_DIR"
}

main "$@"
