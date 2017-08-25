#!/bin/bash
set -e
shopt -s nullglob

# ATTENTION: This script works with both maven and cmake builds.

CONF_FILE="create_mediaserver_tar.conf"
if [ -f "$CONF_FILE" ]; then
    source "$CONF_FILE"
fi

if [ ! -z "$BUILD_CONFIGURATION" ]; then
    LIB_AND_BIN_DIR_SUFFIX="/$BUILD_CONFIGURATION"
else
    LIB_AND_BIN_DIR_SUFFIX=""
fi
LIB_BUILD_DIR="$BUILD_DIR/lib$LIB_AND_BIN_DIR_SUFFIX"
BIN_BUILD_DIR="$BUILD_DIR/bin$LIB_AND_BIN_DIR_SUFFIX"

if [ "$BOX" = "edge1" ]; then
    INSTALL_PATH="usr/local/apps/$CUSTOMIZATION"
    SYMLINK_INSTALL_PATH="opt/$CUSTOMIZATION" #< If not empty, will symlink to $INSTALL_PATH.
    QT_LIB_INSTALL_PATH="sdcard/${CUSTOMIZATION}_service" #< If not empty, Qt .so files go here.
else
    INSTALL_PATH="opt/$CUSTOMIZATION"
fi

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

    local LIB
    for LIB in "${LIBS_TO_COPY[@]}"; do
        local EXISTING_LIB
        for EXISTING_LIB in "$LIB_BUILD_DIR/$LIB"*; do
            if [[ $EXISTING_LIB != *.debug ]]; then
                echo "  Adding $(basename "$EXISTING_LIB")"
                cp -r "$EXISTING_LIB" "$LIB_INSTALL_DIR/"
            fi
        done
    done
    for LIB in "${OPTIONAL_LIBS_TO_COPY[@]}"; do
        local EXISTING_LIB
        for EXISTING_LIB in "$LIB_BUILD_DIR/$LIB"*; do
            echo "  Adding optional lib file $(basename "$EXISTING_LIB")"
            cp -r "$EXISTING_LIB" "$LIB_INSTALL_DIR/"
        done
    done
}

# [in] LIB_INSTALL_DIR
copyQtLibs()
{
    # To save storage space on the device, Qt .so files can be copied to an alternative location
    # defined by QT_LIB_INSTALL_PATH (e.g. an sdcard which does not support symlinks), and symlinks
    # to these .so files will be created in the regular LIB_INSTALL_DIR.
    if [ ! -z "$QT_LIB_INSTALL_PATH" ]; then
        local QT_LIB_INSTALL_DIR="$TAR_DIR/$QT_LIB_INSTALL_PATH"
        mkdir -p "$QT_LIB_INSTALL_DIR"
    fi

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
        local LIB_FILENAME="libQt5$QT_LIB.so"
        echo "  Adding Qt $LIB_FILENAME"
        local FILE
        for FILE in "$QT_DIR/lib/$LIB_FILENAME"*; do
            if [ ! -z "$QT_LIB_INSTALL_PATH" ] && [ ! -L "$FILE" ]; then
                # FILE is not a symlink - put to the Qt libs location, and create a symlink to it.
                cp -r "$FILE" "$QT_LIB_INSTALL_DIR/"
                ln -s "/$QT_LIB_INSTALL_PATH/$(basename "$FILE")" \
                    "$LIB_INSTALL_DIR/$(basename "$FILE")"
            else
                # FILE is a symlink, or Qt libs location is not defined - put to install dir.
                cp -r "$FILE" "$LIB_INSTALL_DIR/"
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

    if [ "$BOX" = "edge1" ]; then
        # NOTE: Plugins from $BIN_BUILD_DIR/plugins are not needed on edge1.
        mkdir -p "$MEDIASERVER_BIN_INSTALL_DIR/plugins"
        cp "$LIB_BUILD_DIR/libcpro_ipnc_plugin.so.1.0.0" "$MEDIASERVER_BIN_INSTALL_DIR/plugins/"
    else
        if [ -e "$BIN_BUILD_DIR/plugins" ]; then
            cp -r "$BIN_BUILD_DIR/plugins" "$MEDIASERVER_BIN_INSTALL_DIR/"
        fi
    fi
}

# [in] INSTALL_DIR
copyConf()
{
    if [ "$BOX" = "edge1" ]; then
        local MEDIASERVER_CONF_FILENAME="mediaserver.conf"
    else
        local MEDIASERVER_CONF_FILENAME="mediaserver.conf.template"
    fi

    mkdir -p "$INSTALL_DIR/mediaserver/etc"
    cp "$CUR_BUILD_DIR/opt/networkoptix/mediaserver/etc/$MEDIASERVER_CONF_FILENAME" \
        "$INSTALL_DIR/mediaserver/etc/"
}

# Copy the autostart script and platform-specific scripts.
# [in] TAR_DIR
# [in] INSTALL_DIR
copyScripts()
{
    if [ "$BOX" = "edge1" ]; then
        mkdir -p "$TAR_DIR/etc/init.d"
        install -m 755 "$CUR_BUILD_DIR/etc/init.d/S99networkoptix-mediaserver" \
            "$TAR_DIR/etc/init.d/S99$CUSTOMIZATION-mediaserver"
    else
        # TODO: Consider more general approach: copy all dirs and files, customizing filenames,
        # assigning proper permissions. Top-level dirs (etc, opt, root, etc) should be either moved
        # to e.g. "sysroot" folder, or enumerated manually.

        echo "Copying scripts"
        cp -r "$CUR_BUILD_DIR/etc" "$TAR_DIR"
        chmod -R 755 "$TAR_DIR/etc/init.d"

        cp -r "$CUR_BUILD_DIR/opt/networkoptix/"* "$INSTALL_DIR/"
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
    fi
}

# [in] TAR_DIR
copyDebs()
{
    local DEBS_DIR="$BUILD_DIR/deb"
    if [ -d "$DEBS_DIR" ]; then
        cp -r "$DEBS_DIR" "$TAR_DIR/opt/"
    fi
}

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
    cp "$BIN_BUILD_DIR/mobile_client" "$INSTALL_DIR/lite_client/bin/"

    # Create symlink for rpath needed by mediaserver binary.
    ln -s "../lib" "$INSTALL_DIR/mediaserver/lib"

    # Create symlink for rpath needed by mobile_client binary.
    ln -s "../lib" "$INSTALL_DIR/lite_client/lib"

    # Create symlink for rpath needed by Qt plugins.
    ln -s "../../lib" "$INSTALL_DIR/lite_client/bin/lib"

    # Copy directories needed for lite client.
    local DIRS_TO_COPY=(
        # TODO: #mike: cmake: Find and copy dirs to build/bin/:
        fonts # roboto-fonts/bin/
        egldeviceintegrations # $QT_DIR/plugins/
        imageformats # $QT_DIR/plugins/
        platforms # $QT_DIR/plugins/
        qml # $QT_DIR/
        # TODO: #mike: Compile nx_bpi_videonode_plugin into nx_client_core rather than as a .so.
        video # Built common_libs/nx_bpi_videonode_plugin
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
    # Copy uboot.
    cp -r "$BUILD_DIR/root" "$TAR_DIR/"

    # Copy additional binaries.
    cp -r "$BUILD_DIR/usr" "$TAR_DIR/"

    cp -r "$CUR_BUILD_DIR/root" "$TAR_DIR/"

    # Copy default server conf used on "factory reset".
    local TOOLS_DIR="$TAR_DIR/root/tools/nx"
    mkdir -p "$TOOLS_DIR"
    cp "$CUR_BUILD_DIR/opt/networkoptix/mediaserver/etc/mediaserver.conf.template" "$TOOLS_DIR/"
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
            echo "  Adding sysroot lib $LIB"
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
    # TODO: Consider unconditionally copying from the compiler artifact. Decision on usage will
    # then be made in install.sh on the box.
    if [ "$BOX" = "bpi" ] || [ "$BOX" = "bananapi" ]; then
        cp -r "$PACKAGES_DIR/libstdc++-6.0.19/lib/libstdc++.s"* "$LIB_INSTALL_DIR/"
    fi
}

# [in] WORK_DIR
# [in] TAR_DIR
buildInstaller()
{
    echo "Creating main distro .tar.gz"
    if [ ! -z "$SYMLINK_INSTALL_PATH" ]; then
        mkdir -p "$TAR_DIR/$(dirname "$SYMLINK_INSTALL_PATH")"
        ln -s "/$INSTALL_PATH" "$TAR_DIR/$SYMLINK_INSTALL_PATH"
    fi
    ( cd "$TAR_DIR" && tar czf "$CUR_BUILD_DIR/$TAR_FILENAME" * )

    echo "Creating \"update\" .zip"
    local ZIP_DIR="$WORK_DIR/zip"
    mkdir -p "$ZIP_DIR"
    cp -r "$CUR_BUILD_DIR/$TAR_FILENAME" "$ZIP_DIR/"
    cp -r "$CUR_BUILD_DIR"/update.* "$ZIP_DIR/"
    cp -r "$CUR_BUILD_DIR"/install.sh "$ZIP_DIR/"
    ( cd "$ZIP_DIR" && zip "$CUR_BUILD_DIR/$ZIP_FILENAME" * )
}

# [in] WORK_DIR
# [in] TAR_DIR
buildDebugSymbolsArchive()
{
    echo "Creating debug symbols .tar.gz"

    local DEBUG_TAR_DIR="$WORK_DIR/debug-symbols-tar"
    mkdir -p "$DEBUG_TAR_DIR"

    local DEBUG_FILES_EXIST=0
    local FILE
    for FILE in \
        "$LIB_BUILD_DIR"/*.debug \
        "$BIN_BUILD_DIR"/*.debug \
        "$BIN_BUILD_DIR/plugins"/*.debug \
    ; do
        if [ -f "$FILE" ]; then
            DEBUG_FILES_EXIST=1
            echo "  Adding $(basename $FILE)"
            cp -r "$FILE" "$DEBUG_TAR_DIR/"
        fi
    done

    if [ "$DEBUG_FILES_EXIST" == "0" ]; then
        echo "  No .debug files found"
    else
        ( cd "$DEBUG_TAR_DIR" && tar czf "$CUR_BUILD_DIR/$TAR_FILENAME-debug-symbols.tar.gz" * )
    fi
}

#--------------------------------------------------------------------------------------------------

main()
{
    parseArgs "$@"

    local WORK_DIR=$(mktemp -d)
    rm -rf "$WORK_DIR"

    local TAR_DIR="$WORK_DIR/tar"
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

    buildInstaller
    buildDebugSymbolsArchive

    rm -rf "$WORK_DIR"
}

main "$@"
